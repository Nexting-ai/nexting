import assert from "node:assert/strict";
import test from "node:test";

import {
  DeviceAudioReceiver,
  DeviceAudioSimulator,
} from "./device-audio-simulator.mjs";

function readyDevice() {
  let now = 0;
  const device = new DeviceAudioSimulator({ now: () => now });
  device.configure({ authorized: true, encrypted: true, attPayload: 64, epoch: 7 });
  return {
    device,
    setNow(value) {
      now = value;
    },
  };
}

test("Host configuration prepares reception but cannot open the microphone", () => {
  const { device } = readyDevice();
  assert.equal(device.state, "ready");
  assert.equal(device.handleHostControl({ type: "audioConfig" }), false);
  assert.equal(device.state, "ready");
  assert.equal(device.audioInRamBytes, 0);
});

test("late Host cancellation cannot affect a different stream", () => {
  const { device } = readyDevice();
  device.physicalVoiceDown();
  assert.equal(
    device.handleHostControl({
      type: "audioCancel",
      epoch: 7,
      streamId: device.streamId + 1,
    }),
    false,
  );
  assert.equal(device.state, "capturing");
  assert.equal(
    device.handleHostControl({
      type: "audioCancel",
      epoch: 7,
      streamId: device.streamId,
    }),
    true,
  );
  assert.equal(device.state, "ready");
});

test("physical short press latches and the next press submits", () => {
  const { device, setNow } = readyDevice();
  assert.equal(device.physicalVoiceDown(), true);
  assert.equal(device.state, "capturing");
  setNow(200);
  device.physicalVoiceUp();
  assert.equal(device.state, "latchedRecording");
  assert.equal(device.physicalVoiceDown(), true);
  assert.equal(device.state, "waitingResult");
  assert.equal(device.events.at(-1).type, "audioEnd");
});

test("physical hold, Submit, Reject, disconnect, and duration limit are deterministic", () => {
  const held = readyDevice();
  held.device.physicalVoiceDown();
  held.setNow(450);
  held.device.physicalVoiceUp();
  assert.equal(held.device.state, "waitingResult");

  const submitted = readyDevice();
  submitted.device.physicalVoiceDown();
  submitted.device.localSubmit();
  assert.equal(submitted.device.state, "waitingResult");

  for (const action of ["localReject", "disconnect"]) {
    const fixture = readyDevice();
    fixture.device.physicalVoiceDown();
    fixture.device.captureFrame(new Uint8Array(192));
    fixture.device[action]();
    assert.equal(fixture.device.state, action === "disconnect" ? "unavailable" : "ready");
    assert.equal(fixture.device.audioInRamBytes, 0);
    assert.equal(fixture.device.events.at(-1).type, "audioCancel");
  }

  const limited = readyDevice();
  limited.device.physicalVoiceDown();
  limited.setNow(120_000);
  limited.device.tick();
  assert.equal(limited.device.state, "waitingResult");
  assert.equal(limited.device.events.at(-1).reason, "max_duration");
});

test("eight credits and a ten-frame RAM queue fail closed on overflow", () => {
  const { device } = readyDevice();
  device.physicalVoiceDown();
  for (let index = 0; index < 8; index += 1) {
    assert.equal(device.captureFrame(new Uint8Array(192).fill(index)), true);
  }
  assert.equal(device.sentFrames.length, 8);
  assert.equal(Object.hasOwn(device.sentFrames[0], "bytes"), false);
  for (let index = 0; index < 10; index += 1) {
    assert.equal(device.captureFrame(new Uint8Array(192).fill(index + 8)), true);
  }
  assert.equal(device.queuedFrames, 10);
  assert.equal(device.captureFrame(new Uint8Array(192)), false);
  assert.equal(device.state, "ready");
  assert.equal(device.audioInRamBytes, 0);
  assert.equal(device.events.at(-1).reason, "transport");
});

test("credits drain queued frames and a 500 ms stall cancels", () => {
  const draining = readyDevice();
  draining.device.physicalVoiceDown();
  for (let index = 0; index < 10; index += 1)
    draining.device.captureFrame(new Uint8Array(192));
  assert.equal(draining.device.queuedFrames, 2);
  assert.equal(
    draining.device.grantCredit({
      epoch: 7,
      streamId: draining.device.streamId,
      ackSequence: 7,
      credits: 2,
    }),
    true,
  );
  assert.equal(draining.device.queuedFrames, 0);
  assert.equal(draining.device.sentFrames.length, 10);

  const stalled = readyDevice();
  stalled.device.physicalVoiceDown();
  for (let index = 0; index < 9; index += 1)
    stalled.device.captureFrame(new Uint8Array(192));
  stalled.setNow(499);
  stalled.device.tick();
  assert.equal(stalled.device.state, "capturing");
  stalled.setNow(500);
  stalled.device.tick();
  assert.equal(stalled.device.state, "ready");
  assert.equal(stalled.device.events.at(-1).reason, "timeout");
});

test("credits are matching, cumulative, and cannot exceed the outstanding window", () => {
  const { device } = readyDevice();
  device.physicalVoiceDown();
  for (let index = 0; index < 8; index += 1)
    device.captureFrame(new Uint8Array(192));
  const current = { epoch: 7, streamId: device.streamId };
  assert.equal(
    device.grantCredit({ ...current, ackSequence: 8, credits: 1 }),
    false,
  );
  assert.equal(
    device.grantCredit({ ...current, ackSequence: 7, credits: 8 }),
    true,
  );
  assert.equal(
    device.grantCredit({ ...current, ackSequence: 7, credits: 1 }),
    false,
  );
  assert.equal(
    device.grantCredit({ ...current, ackSequence: 7, credits: 9 }),
    false,
  );
  assert.equal(
    device.grantCredit({
      epoch: 8,
      streamId: device.streamId,
      ackSequence: 7,
      credits: 1,
    }),
    false,
  );
});

test("sample-count bounds stop both device and receiver at 120 seconds", () => {
  const { device } = readyDevice();
  device.physicalVoiceDown();
  device.sequence = 6000;
  assert.equal(device.captureFrame(new Uint8Array(192)), false);
  assert.equal(device.state, "waitingResult");
  assert.equal(device.events.at(-1).reason, "max_duration");

  const receiver = new DeviceAudioReceiver({ maxFrames: 2 });
  assert.equal(
    receiver.accept({ sequence: 0, samples: new Int16Array(320) }).accepted,
    true,
  );
  assert.equal(
    receiver.accept({ sequence: 1, samples: new Int16Array(320) }).accepted,
    true,
  );
  assert.deepEqual(
    receiver.accept({ sequence: 2, samples: new Int16Array(320) }),
    { accepted: false, terminated: true, reason: "max_duration" },
  );
  assert.equal(receiver.samples.length, 0);
});

test("receiver inserts one silence frame but rejects excessive loss", () => {
  const receiver = new DeviceAudioReceiver();
  assert.deepEqual(receiver.accept({ sequence: 0, samples: new Int16Array(320).fill(1) }), {
    accepted: true,
    insertedSilenceFrames: 0,
  });
  assert.deepEqual(receiver.accept({ sequence: 2, samples: new Int16Array(320).fill(2) }), {
    accepted: true,
    insertedSilenceFrames: 1,
  });
  assert.equal(receiver.samples.length, 960);
  assert.equal(receiver.samples[320], 0);
  assert.deepEqual(receiver.accept({ sequence: 2, samples: new Int16Array(320) }), {
    accepted: false,
    duplicate: true,
  });
  assert.deepEqual(receiver.accept({ sequence: 6, samples: new Int16Array(320) }), {
    accepted: false,
    terminated: true,
    reason: "missing_frames",
  });
  assert.equal(receiver.samples.length, 0);
});

test("receiver terminates after ten total isolated missing frames", () => {
  const receiver = new DeviceAudioReceiver();
  receiver.accept({ sequence: 0, samples: new Int16Array(320) });
  for (let index = 1; index <= 9; index += 1) {
    const sequence = index * 2;
    assert.equal(
      receiver.accept({ sequence, samples: new Int16Array(320) }).accepted,
      true,
    );
  }
  assert.equal(receiver.missingFrames, 9);
  assert.equal(
    receiver.accept({ sequence: 20, samples: new Int16Array(320) }).terminated,
    true,
  );
  assert.equal(receiver.samples.length, 0);
});
