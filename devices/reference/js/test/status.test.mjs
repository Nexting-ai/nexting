import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  decode,
  encode,
  MAX_STATUS_AGENTS,
  MAX_STATUS_LABEL_BYTES,
  STATUS_PROFILE,
  STATUS_STATES,
} from "../src/protocol.mjs";

const vectors = JSON.parse(
  await readFile(
    new URL("../../../protocol/vectors/status-v1.json", import.meta.url),
    "utf8",
  ),
);

test("status/1 vectors are versioned and cover every state", () => {
  assert.equal(vectors.spec, "0.1.0-experimental");
  assert.equal(vectors.wire, 1);
  assert.equal(vectors.profile, "status/1");
  assert.deepEqual(
    [...new Set(vectors.valid.map((item) => item.decoded.type))],
    ["status"],
  );
  assert.deepEqual(
    [
      ...new Set(
        vectors.valid.flatMap((item) =>
          item.decoded.agents.map((agent) => agent.state),
        ),
      ),
    ].sort(),
    [...STATUS_STATES].sort(),
  );
  assert.ok(
    vectors.valid.some((item) => item.decoded.agents.length === 0),
    "must cover the empty clear message",
  );
  assert.ok(
    vectors.valid.some((item) => item.decoded.agents.length === MAX_STATUS_AGENTS),
    "must cover the slot-count boundary",
  );
  assert.ok(
    vectors.valid.some((item) =>
      item.decoded.agents.some(
        (agent) =>
          agent.label !== undefined &&
          Buffer.byteLength(agent.label, "utf8") === MAX_STATUS_LABEL_BYTES,
      ),
    ),
    "must cover the label byte boundary",
  );
  assert.ok(vectors.invalid.length >= 8);
});

test("status valid vectors round-trip canonically", () => {
  for (const item of vectors.valid) {
    assert.deepEqual(decode(item.wire), item.decoded);
    assert.equal(encode(item.decoded), item.wire);
  }
});

test("status invalid vectors fail closed", () => {
  for (const wire of vectors.invalid) assert.equal(decode(wire), null);
});

test("status schema def stays aligned with protocol constants", async () => {
  const schema = JSON.parse(
    await readFile(
      new URL("../../../schemas/message.schema.json", import.meta.url),
      "utf8",
    ),
  );
  const status = schema.$defs.status;

  assert.deepEqual(status.required, ["v", "t", "agents"]);
  assert.equal(status.properties.t.const, "status");
  assert.equal(status.properties.agents.maxItems, MAX_STATUS_AGENTS);
  assert.deepEqual(
    status.properties.agents.items.properties.state.enum,
    STATUS_STATES,
  );
  assert.equal(status.properties.agents.items.properties.slot.minimum, 0);
  assert.equal(
    status.properties.agents.items.properties.slot.maximum,
    MAX_STATUS_AGENTS - 1,
  );
  assert.equal(
    status.properties.agents.items.properties.label["x-maxUtf8Bytes"],
    MAX_STATUS_LABEL_BYTES,
  );
});

test("protocol exports the status profile identity", () => {
  assert.equal(STATUS_PROFILE, "status/1");
  assert.deepEqual(STATUS_STATES, [
    "idle",
    "thinking",
    "working",
    "complete",
    "needs_input",
    "error",
  ]);
  assert.equal(MAX_STATUS_AGENTS, 8);
  assert.equal(MAX_STATUS_LABEL_BYTES, 64);
});

test("status encode rejects bad slot, state, and label shapes", () => {
  const base = { type: "status", agents: [{ slot: 0, state: "idle" }] };
  assert.equal(
    encode({ ...base, agents: [{ slot: 8, state: "idle" }] }),
    null,
  );
  assert.equal(
    encode({ ...base, agents: [{ slot: 0, state: "paused" }] }),
    null,
  );
  assert.equal(
    encode({
      ...base,
      agents: [
        { slot: 0, state: "idle" },
        { slot: 0, state: "working" },
      ],
    }),
    null,
  );
  assert.equal(
    encode({
      ...base,
      agents: [{ slot: 0, state: "idle", label: "x".repeat(65) }],
    }),
    null,
  );
  assert.equal(
    encode({
      ...base,
      agents: [{ slot: 0, state: "idle", label: "badell" }],
    }),
    null,
  );
  assert.equal(
    encode({
      ...base,
      agents: [{ slot: 0, state: "idle", label: "" }],
    }),
    null,
  );
  assert.equal(encode({ type: "status" }), null);
});

test("status decode ignores unknown fields but never approval state", () => {
  assert.deepEqual(
    decode(
      '{"v":1,"t":"status","agents":[{"slot":2,"state":"working",' +
        '"label":"build","future":{"note":"ignored"}}],"future":true}\n',
    ),
    {
      type: "status",
      agents: [{ slot: 2, state: "working", label: "build" }],
    },
  );
});
