import assert from "node:assert/strict";
import test from "node:test";

import { decode } from "../src/protocol.mjs";
import { createApprovalRelay } from "../src/relay.mjs";

function harness() {
  let clock = 1_000;
  const sent = [];
  const answered = [];
  const relay = createApprovalRelay({
    sendToDevice: (wire) => sent.push(decode(wire)),
    answerPrompt: (choice) => answered.push(choice),
    now: () => clock,
  });
  return {
    relay,
    sent,
    answered,
    setNow(value) {
      clock = value;
    },
  };
}

test("present sends one bounded approval and tracks it", () => {
  const { relay, sent } = harness();
  assert.equal(
    relay.present({ requestId: "r1", summary: "Allow push?", ttlMs: 30_000 }),
    true,
  );
  assert.equal(relay.hasPending, true);
  assert.equal(relay.pendingRequestId, "r1");
  assert.deepEqual(sent, [
    {
      type: "present",
      requestId: "r1",
      summary: "Allow push?",
      options: ["allow", "deny"],
      ttlMs: 30_000,
    },
  ]);
});

test("authorized answer resolves only after commit and retries after failure", () => {
  const { relay, sent, answered } = harness();
  relay.present({ requestId: "r1", summary: "Allow?", ttlMs: 30_000 });
  assert.deepEqual(
    relay.onDeviceAnswer({
      requestId: "r1",
      choice: "allow",
      authorized: true,
    }),
    { accepted: true, choice: "allow" },
  );
  assert.deepEqual(answered, ["allow"]);
  assert.equal(sent.length, 1);
  assert.equal(relay.hasPending, true);

  relay.answerFailed("r1");
  assert.deepEqual(
    relay.onDeviceAnswer({ requestId: "r1", choice: "deny", authorized: true }),
    { accepted: false, reason: "choice_locked" },
  );
  assert.deepEqual(
    relay.onDeviceAnswer({
      requestId: "r1",
      choice: "allow",
      authorized: true,
    }),
    { accepted: true, choice: "allow" },
  );
  assert.deepEqual(answered, ["allow", "allow"]);
  assert.equal(sent.length, 1);

  relay.answerSucceeded("stale");
  assert.equal(relay.hasPending, true);
  relay.answerSucceeded("r1");
  assert.deepEqual(sent.at(-1), {
    type: "resolved",
    requestId: "r1",
    reason: "answered",
  });
  assert.equal(relay.hasPending, false);
  assert.deepEqual(
    relay.onDeviceAnswer({
      requestId: "r1",
      choice: "allow",
      authorized: true,
    }),
    { accepted: false, reason: "no_pending" },
  );
  assert.deepEqual(answered, ["allow", "allow"]);
});

test("unauthorized, stale, and invalid answers fail closed", () => {
  const { relay, answered } = harness();
  relay.present({ requestId: "r1", summary: "Allow?", ttlMs: 30_000 });
  assert.deepEqual(
    relay.onDeviceAnswer({
      requestId: "r1",
      choice: "allow",
      authorized: false,
    }),
    { accepted: false, reason: "unauthorized" },
  );
  assert.deepEqual(
    relay.onDeviceAnswer({
      requestId: "stale",
      choice: "allow",
      authorized: true,
    }),
    { accepted: false, reason: "stale_or_unknown" },
  );
  assert.deepEqual(
    relay.onDeviceAnswer({
      requestId: "r1",
      choice: "maybe",
      authorized: true,
    }),
    { accepted: false, reason: "bad_choice" },
  );
  assert.deepEqual(answered, []);
  assert.equal(relay.hasPending, true);
});

test("new present resolves the old request as replaced", () => {
  const { relay, sent } = harness();
  relay.present({ requestId: "r1", summary: "first", ttlMs: 30_000 });
  relay.present({ requestId: "r2", summary: "second", ttlMs: 30_000 });
  assert.deepEqual(sent[1], {
    type: "resolved",
    requestId: "r1",
    reason: "replaced",
  });
  assert.equal(relay.pendingRequestId, "r2");
  assert.deepEqual(
    relay.onDeviceAnswer({
      requestId: "r1",
      choice: "allow",
      authorized: true,
    }),
    { accepted: false, reason: "stale_or_unknown" },
  );
});

test("phone-first race resolves hardware only after API success", () => {
  const { relay, sent, answered } = harness();
  relay.present({ requestId: "r1", summary: "Allow?", ttlMs: 30_000 });
  assert.equal(relay.phoneAnswerStarted("r1"), true);
  assert.equal(sent.length, 1);
  assert.deepEqual(
    relay.onDeviceAnswer({ requestId: "r1", choice: "deny", authorized: true }),
    { accepted: false, reason: "answer_in_flight" },
  );
  assert.deepEqual(answered, []);

  relay.answerFailed("r1");
  assert.deepEqual(
    relay.onDeviceAnswer({ requestId: "r1", choice: "deny", authorized: true }),
    { accepted: true, choice: "deny" },
  );
  relay.answerSucceeded("r1");
  assert.equal(sent.at(-1).reason, "answered");
});

test("default monotonic clock expires despite wall-clock rollback", () => {
  const originalDateNow = Date.now;
  const originalPerformance = globalThis.performance;
  let wallClock = 1_000;
  let monotonicClock = 1_000;
  Date.now = () => wallClock;
  Object.defineProperty(globalThis, "performance", {
    configurable: true,
    value: { now: () => monotonicClock },
  });

  try {
    const sent = [];
    const relay = createApprovalRelay({
      sendToDevice: (wire) => sent.push(decode(wire)),
      answerPrompt: () => {},
    });
    relay.present({ requestId: "r1", summary: "Allow?", ttlMs: 10 });
    wallClock = 0;
    monotonicClock = 1_010;
    relay.tick();
    assert.equal(sent.at(-1).reason, "expired");
  } finally {
    Date.now = originalDateNow;
    Object.defineProperty(globalThis, "performance", {
      configurable: true,
      value: originalPerformance,
    });
  }
});

test("expiry rejects a boundary-time answer and emits expired", () => {
  const { relay, sent, answered, setNow } = harness();
  relay.present({ requestId: "r1", summary: "Allow?", ttlMs: 10 });
  setNow(1_010);
  assert.deepEqual(
    relay.onDeviceAnswer({
      requestId: "r1",
      choice: "allow",
      authorized: true,
    }),
    { accepted: false, reason: "expired" },
  );
  assert.equal(sent.at(-1).reason, "expired");
  assert.deepEqual(answered, []);
});

test("an in-flight answer may succeed after its original deadline", () => {
  const { relay, sent, setNow } = harness();
  relay.present({ requestId: "r1", summary: "Allow?", ttlMs: 10 });
  assert.equal(
    relay.onDeviceAnswer({ requestId: "r1", choice: "allow", authorized: true })
      .accepted,
    true,
  );
  setNow(1_010);
  relay.tick();
  assert.equal(sent.length, 1);
  relay.answerSucceeded("r1");
  assert.equal(sent.at(-1).reason, "answered");
});

test("a failed in-flight answer expires after its deadline", () => {
  const { relay, sent, setNow } = harness();
  relay.present({ requestId: "r1", summary: "Allow?", ttlMs: 10 });
  relay.onDeviceAnswer({ requestId: "r1", choice: "allow", authorized: true });
  setNow(1_010);
  relay.answerFailed("r1");
  assert.equal(sent.at(-1).reason, "expired");
});

test("tick, cancel, and disconnect have deterministic cleanup", () => {
  const first = harness();
  first.relay.present({ requestId: "r1", summary: "Allow?", ttlMs: 10 });
  first.setNow(1_010);
  first.relay.tick();
  assert.equal(first.sent.at(-1).reason, "expired");

  const second = harness();
  second.relay.present({ requestId: "r2", summary: "Allow?", ttlMs: 10 });
  second.relay.cancel("r2");
  assert.equal(second.sent.at(-1).reason, "cancelled");

  const third = harness();
  third.relay.present({ requestId: "r3", summary: "Allow?", ttlMs: 10 });
  third.relay.disconnect();
  assert.equal(third.relay.hasPending, false);
  assert.equal(third.sent.length, 1);
});

test("invalid present is rejected without changing the current request", () => {
  const { relay, sent } = harness();
  relay.present({ requestId: "r1", summary: "valid", ttlMs: 30_000 });
  assert.equal(
    relay.present({ requestId: "bad id", summary: "invalid", ttlMs: 30_000 }),
    false,
  );
  assert.equal(relay.pendingRequestId, "r1");
  assert.equal(sent.length, 1);
});
