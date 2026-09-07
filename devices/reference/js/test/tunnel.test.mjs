import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  encodeTunnelPacket,
  parseTransportDescriptor,
  parseTunnelPacket,
} from "../src/tunnel.mjs";

const vectors = JSON.parse(
  await readFile(
    new URL(
      "../../../protocol/vectors/compatibility-tunnel-v1.json",
      import.meta.url,
    ),
    "utf8",
  ),
);
const base = vectors.valid[0].descriptor;

function withPath(source, path, value) {
  const copy = structuredClone(source);
  const segments = path.split(".");
  let cursor = copy;
  for (const segment of segments.slice(0, -1)) cursor = cursor[segment];
  cursor[segments.at(-1)] = value;
  return copy;
}

test("valid compatibility descriptors normalize four logical channels", () => {
  const descriptor = parseTransportDescriptor(base);
  assert.equal(descriptor.integrationId, "example.controls-x1");
  assert.equal(descriptor.channels.audioUp.opcode, 0x12);
  assert.equal(descriptor.requirements.minAttPayload, 64);
  assert.equal(Object.keys(descriptor.channels).length, 4);
});

test("AhaKey integration descriptor is inert and conforms to the shared tunnel", async () => {
  const raw = await readFile(
    new URL(
      "../../../firmware/ahakey-x1/transport-descriptor.nexting-device.json",
      import.meta.url,
    ),
    "utf8",
  );
  const descriptor = parseTransportDescriptor(raw);
  assert.equal(descriptor?.integrationId, "ai.ahakey.x1");
  assert.equal(descriptor?.channels.controlDown.opcode, 0xa0);
  assert.equal(descriptor?.channels.controlUp.opcode, 0xa1);
  assert.equal(descriptor?.channels.audioUp.opcode, 0xa2);
  assert.equal(descriptor?.channels.flowDown.opcode, 0xa3);
  assert.equal(descriptor?.requirements.encrypted, true);
});

test("raw descriptors reject duplicate keys before JSON normalization", () => {
  const raw = JSON.stringify(base);
  assert.ok(parseTransportDescriptor(raw));
  assert.equal(
    parseTransportDescriptor(raw.replace('"version":1', '"version":1,"version":1')),
    null,
  );
  assert.equal(
    parseTransportDescriptor(
      raw.replace('"opcode":16', '"opcode":16,"opcode":16'),
    ),
    null,
  );
});

test("descriptors reject executable, unsafe, and ambiguous data", () => {
  for (const item of vectors.invalid) {
    assert.equal(
      parseTransportDescriptor(withPath(base, item.path, item.value)),
      null,
      item.name,
    );
  }
});

test("the fixed opcode envelope cannot inject a parser or a fifth channel", () => {
  const descriptor = parseTransportDescriptor(base);
  const payload = Uint8Array.of(1, 2, 3);
  assert.deepEqual(
    [...encodeTunnelPacket(descriptor, "controlDown", payload)],
    [0x10, 1, 2, 3],
  );
  assert.deepEqual(parseTunnelPacket(descriptor, Uint8Array.of(0x12, 9, 8)), {
    channel: "audioUp",
    payload: Uint8Array.of(9, 8),
  });
  assert.equal(parseTunnelPacket(descriptor, Uint8Array.of(0x1f, 1)), null);
  assert.throws(() => encodeTunnelPacket(descriptor, "unknown", payload));
});

test("the public descriptor schema is strict data, not executable plugin code", async () => {
  const schema = JSON.parse(
    await readFile(
      new URL("../../../schemas/transport-descriptor.schema.json", import.meta.url),
      "utf8",
    ),
  );
  assert.equal(schema.$defs.descriptor.additionalProperties, false);
  assert.deepEqual(schema.$defs.envelope.properties.strategy.enum, [
    "opcode-prefix-v1",
  ]);
  assert.equal(schema.$defs.requirements.properties.min_att_payload.minimum, 64);
  assert.equal(schema.$defs.channels.additionalProperties, false);
  const keys = [];
  const visit = (value) => {
    if (!value || typeof value !== "object") return;
    for (const [key, child] of Object.entries(value)) {
      keys.push(key.toLowerCase());
      visit(child);
    }
  };
  visit(schema);
  assert.equal(keys.includes("script"), false);
  assert.equal(keys.includes("url"), false);
});
