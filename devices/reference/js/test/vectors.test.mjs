import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  CHOICES,
  decode,
  ERROR_CODES,
  MAX_REQUEST_ID_BYTES,
  MAX_SUMMARY_BYTES,
  MAX_TTL_MS,
  RESOLUTION_REASONS,
} from "../src/protocol.mjs";

const interactionProfiles = [
  "navigation",
  "keys",
  "rotary",
  "voice",
  "text",
  "usage",
  "config",
];

test("interaction profile vectors share the strict reference decoder", async () => {
  for (const profile of interactionProfiles) {
    const raw = await readFile(
      new URL(`../../../protocol/vectors/${profile}-v1.json`, import.meta.url),
      "utf8",
    );
    const vectors = JSON.parse(raw);
    assert.equal(vectors.wire, 1);
    assert.equal(vectors.profile, `${profile}/1`);
    for (const vector of vectors.valid) {
      assert.deepEqual(decode(vector.wire), vector.decoded, vector.name);
    }
    for (const vector of vectors.invalid) {
      assert.equal(decode(vector.wire), null, vector.name);
    }
  }
});

test("approval/1 vectors are versioned and cover every message type", async () => {
  const raw = await readFile(
    new URL("../../../protocol/vectors/approval-v1.json", import.meta.url),
    "utf8",
  );
  const vectors = JSON.parse(raw);

  assert.equal(vectors.spec, "0.1.0-experimental");
  assert.equal(vectors.wire, 1);
  assert.equal(vectors.profile, "approval/1");
  assert.deepEqual(
    [...new Set(vectors.valid.map((item) => item.decoded.type))].sort(),
    ["answer", "error", "present", "resolved"],
  );
  assert.deepEqual(
    [...new Set(vectors.valid.map((item) => item.decoded.choice).filter(Boolean))].sort(),
    [...CHOICES].sort(),
  );
  assert.deepEqual(
    [...new Set(vectors.valid.map((item) => item.decoded.reason).filter(Boolean))].sort(),
    [...RESOLUTION_REASONS].sort(),
  );
  assert.deepEqual(
    [...new Set(vectors.valid.map((item) => item.decoded.code).filter(Boolean))].sort(),
    [...ERROR_CODES].sort(),
  );
  assert.ok(vectors.invalid.length >= 8);
});

test("schema bounds and enums stay aligned with official vectors", async () => {
  const [schemaRaw, vectorsRaw] = await Promise.all([
    readFile(
      new URL("../../../schemas/message.schema.json", import.meta.url),
      "utf8",
    ),
    readFile(
      new URL("../../../protocol/vectors/approval-v1.json", import.meta.url),
      "utf8",
    ),
  ]);
  const schema = JSON.parse(schemaRaw);
  const vectors = JSON.parse(vectorsRaw);
  const present = schema.$defs.present.allOf[1].properties;

  assert.equal(schema.$defs.id.maxLength, MAX_REQUEST_ID_BYTES);
  assert.equal(present.sum["x-maxUtf8Bytes"], MAX_SUMMARY_BYTES);
  assert.match(present.sum.$comment, /UTF-8 byte/);
  assert.equal(present.ttl.maximum, MAX_TTL_MS);
  assert.deepEqual(present.opt.const, CHOICES);
  assert.deepEqual(schema.$defs.answer.allOf[1].properties.ch.enum, CHOICES);
  assert.deepEqual(
    schema.$defs.resolved.allOf[1].properties.r.enum,
    RESOLUTION_REASONS,
  );
  assert.deepEqual(schema.$defs.error.properties.code.enum, ERROR_CODES);

  for (const { wire } of vectors.valid) {
    const message = JSON.parse(wire);
    assert.ok(
      Buffer.byteLength(message.id ?? "", "utf8") <= MAX_REQUEST_ID_BYTES,
    );
    if (message.t === "present") {
      assert.ok(Buffer.byteLength(message.sum, "utf8") <= MAX_SUMMARY_BYTES);
      assert.ok(message.ttl <= MAX_TTL_MS);
    }
  }
});
