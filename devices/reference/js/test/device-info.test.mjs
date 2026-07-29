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
    assert.equal(decoded.capabilities.statusSlots, item.decoded.statusSlots, item.name);
    assert.equal(decoded.identity.deviceId, item.decoded.deviceId, item.name);
    assert.equal(decoded.capabilities.buttonCount, item.decoded.buttonCount, item.name);
    assert.equal(
      decoded.capabilities.batteryService,
      item.decoded.batteryService,
      item.name,
    );
    assert.equal(decoded.vendor?.namespace ?? null, item.decoded.vendorNamespace, item.name);
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
  const decoded = decodeDeviceInfo(withVendor(core, {
    namespace: "com.example.board",
    facts: tooManyFacts,
  }));
  assert.ok(decoded);
  assert.equal(decoded.vendor, null);

  const override = decodeDeviceInfo(withVendor(core, {
    namespace: "com.example.board",
    facts: [{ key: "battery", label: "Battery", value: "100%" }],
  }));
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
