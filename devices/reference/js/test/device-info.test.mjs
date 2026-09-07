import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  MAX_DEVICE_INFO_BYTES,
  decodeDeviceInfo,
  supportsProfile,
} from "../src/protocol.mjs";

const vectorFile = JSON.parse(
  await readFile(
    new URL("../../../protocol/vectors/device-info-v1.json", import.meta.url),
    "utf8",
  ),
);

function withVendor(core, vendor) {
  return JSON.stringify({ ...JSON.parse(core), vendor });
}

test("Device Info 0.2 valid vectors normalize identically", () => {
  for (const item of vectorFile.valid) {
    const decoded = decodeDeviceInfo(item.wire);
    assert.ok(decoded, item.name);
    assert.equal(decoded.model, item.decoded.model, item.name);
    assert.equal(
      decoded.capabilities.statusSlots,
      item.decoded.statusSlots,
      item.name,
    );
    assert.equal(decoded.identity.deviceId, item.decoded.deviceId, item.name);
    assert.equal(
      decoded.capabilities.buttonCount,
      item.decoded.buttonCount,
      item.name,
    );
    assert.equal(
      decoded.capabilities.batteryService,
      item.decoded.batteryService,
      item.name,
    );
    assert.equal(
      decoded.vendor?.namespace ?? null,
      item.decoded.vendorNamespace,
      item.name,
    );
  }
});

test("invalid core rejects the whole Device Info without replacing prior state", () => {
  let lastKnownGood = decodeDeviceInfo(vectorFile.valid[1].wire);
  assert.ok(lastKnownGood);

  for (const item of vectorFile.invalidCore) {
    const next = decodeDeviceInfo(item.wire);
    assert.equal(next, null, item.name);
    if (next !== null) lastKnownGood = next;
  }

  assert.equal(lastKnownGood.model, "Multi Pad");
  assert.equal(lastKnownGood.capabilities.buttonCount, 12);
});

test("invalid optional vendor data is dropped while valid core remains usable", () => {
  const core = vectorFile.valid[0].wire;
  for (const item of vectorFile.invalidVendor) {
    const decoded = decodeDeviceInfo(withVendor(core, item.vendor));
    assert.ok(decoded, item.name);
    assert.equal(decoded.vendor, null, item.name);
  }
});

test("Device Info is byte bounded and rejects unsupported input shapes", () => {
  const valid = JSON.parse(vectorFile.valid[0].wire);
  const oversized = JSON.stringify({
    ...valid,
    future_capability: "x".repeat(MAX_DEVICE_INFO_BYTES),
  });
  assert.ok(Buffer.byteLength(oversized, "utf8") > MAX_DEVICE_INFO_BYTES);
  assert.equal(decodeDeviceInfo(oversized), null);
  assert.equal(decodeDeviceInfo(new Uint8Array([0xff])), null);
  assert.equal(decodeDeviceInfo(null), null);
  assert.equal(decodeDeviceInfo("[]"), null);
});

test("vendor facts remain bounded, inert, and cannot override system fields", () => {
  const core = vectorFile.valid[0].wire;
  const tooManyFacts = Array.from({ length: 17 }, (_, index) => ({
    key: `key_${index}`,
    label: `Fact ${index}`,
    value: `${index}`,
  }));
  const decoded = decodeDeviceInfo(
    withVendor(core, {
      namespace: "com.example.board",
      facts: tooManyFacts,
    }),
  );
  assert.ok(decoded);
  assert.equal(decoded.vendor, null);

  const override = decodeDeviceInfo(
    withVendor(core, {
      namespace: "com.example.board",
      facts: [{ key: "battery", label: "Battery", value: "100%" }],
    }),
  );
  assert.ok(override);
  assert.deepEqual(override.vendor.facts, [
    { key: "battery", label: "Battery", value: "100%" },
  ]);
  assert.equal(override.capabilities.batteryService, false);
});

test("Device Info negotiates interaction profiles explicitly", () => {
  const info = decodeDeviceInfo(
    JSON.stringify({
      ...JSON.parse(vectorFile.valid[0].wire),
      profiles: [
        "approval/1",
        "navigation/1",
        "keys/1",
        "rotary/1",
        "voice/1",
        "text/1",
        "usage/1",
      ],
    }),
  );
  assert.ok(info);
  assert.equal(supportsProfile(info, "navigation/1"), true);
  assert.equal(supportsProfile(info, "keys/1"), true);
  assert.equal(supportsProfile(info, "config/1"), false);
});

test("Device Info advertises a real device microphone separately from voice/1", () => {
  const core = JSON.parse(vectorFile.valid[0].wire);
  const audio = {
    source: "device_microphone",
    codecs: ["ima_adpcm"],
    sample_rates: [16000],
    channels: 1,
    frame_ms: [20],
    max_duration_ms: 120000,
    startup_buffer_ms: 200,
  };
  const decoded = decodeDeviceInfo(
    JSON.stringify({
      ...core,
      spec: "0.4.0-experimental.1",
      profiles: ["approval/1", "device-audio/1"],
      audio,
    }),
  );
  assert.ok(decoded);
  assert.equal(supportsProfile(decoded, "device-audio/1"), true);
  assert.equal(supportsProfile(decoded, "voice/1"), false);
  assert.deepEqual(decoded.capabilities.audio, {
    source: "device_microphone",
    codecs: ["ima_adpcm"],
    sampleRates: [16000],
    channels: 1,
    frameMs: [20],
    maxDurationMs: 120000,
    startupBufferMs: 200,
  });

  assert.equal(
    decodeDeviceInfo(
      JSON.stringify({ ...core, profiles: ["approval/1", "device-audio/1"] }),
    ),
    null,
  );
  assert.equal(decodeDeviceInfo(JSON.stringify({ ...core, audio })), null);
  assert.equal(
    decodeDeviceInfo(
      JSON.stringify({
        ...core,
        profiles: ["approval/1", "device-audio/1"],
        audio: { ...audio, source: "host_microphone" },
      }),
    ),
    null,
  );
  const duplicateAudioField =
    `{\"protocol\":\"nexting-device\",\"spec\":\"0.4.0-experimental.1\",` +
    `\"wire\":[1],\"profiles\":[\"approval/1\",\"device-audio/1\"],` +
    `\"model\":\"audio-ref\",\"fw\":\"1\",\"max_message_bytes\":4096,` +
    `\"max_summary_bytes\":240,\"audio\":{\"source\":\"device_microphone\",` +
    `\"source\":\"device_microphone\",\"codecs\":[\"ima_adpcm\"],` +
    `\"sample_rates\":[16000],\"channels\":1,\"frame_ms\":[20],` +
    `\"max_duration_ms\":120000,\"startup_buffer_ms\":200}}`;
  assert.equal(decodeDeviceInfo(duplicateAudioField), null);
});
