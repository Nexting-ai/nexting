import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  decode,
  encode,
  ERROR_CODES,
  MAX_MESSAGE_BYTES,
  PROFILE,
  WIRE_VERSION,
} from "../src/protocol.mjs";

const vectors = JSON.parse(
  await readFile(
    new URL("../../../protocol/vectors/approval-v1.json", import.meta.url),
    "utf8",
  ),
);

test("shared valid vectors round-trip canonically", () => {
  for (const item of vectors.valid) {
    assert.deepEqual(decode(item.wire), item.decoded);
    assert.equal(encode(item.decoded), item.wire);
  }
});

test("shared invalid vectors fail closed", () => {
  for (const wire of vectors.invalid) assert.equal(decode(wire), null);
});

test("protocol exports its negotiated identity", () => {
  assert.equal(WIRE_VERSION, 1);
  assert.equal(PROFILE, "approval/1");
  assert.ok(ERROR_CODES.includes("message_too_large"));
  assert.equal(MAX_MESSAGE_BYTES, 4096);
});

test("direct codec rejects a complete frame above the host limit", () => {
  const wire = `${JSON.stringify({
    v: 1,
    t: "answer",
    id: "r1",
    ch: "allow",
    future: "x".repeat(MAX_MESSAGE_BYTES),
  })}\n`;
  assert.ok(Buffer.byteLength(wire, "utf8") > MAX_MESSAGE_BYTES);
  assert.equal(decode(wire), null);
  assert.equal(decode(new TextEncoder().encode(wire)), null);
});

test("direct codec reserves one byte for the required newline", () => {
  const prefix = '{"v":1,"t":"answer","id":"r1","ch":"allow","future":"';
  const suffix = '"}';
  const exactPayload = `${prefix}${"x".repeat(
    MAX_MESSAGE_BYTES - 1 - prefix.length - suffix.length,
  )}${suffix}`;
  assert.equal(Buffer.byteLength(`${exactPayload}\n`, "utf8"), MAX_MESSAGE_BYTES);
  assert.equal(decode(`${exactPayload}\n`)?.type, "answer");
  assert.equal(decode(exactPayload)?.type, "answer");

  const tooLargeWithoutNewline = `${prefix}${"x".repeat(
    MAX_MESSAGE_BYTES - prefix.length - suffix.length,
  )}${suffix}`;
  assert.equal(Buffer.byteLength(tooLargeWithoutNewline, "utf8"), MAX_MESSAGE_BYTES);
  assert.equal(decode(tooLargeWithoutNewline), null);
});

test("summary bound counts UTF-8 bytes rather than JavaScript characters", () => {
  const summary = "批".repeat(81);
  const message = {
    type: "present",
    requestId: "r1",
    summary,
    options: ["allow", "deny"],
    ttlMs: 30_000,
  };
  assert.equal(Buffer.byteLength(summary, "utf8"), 243);
  assert.equal(encode(message), null);
  assert.equal(
    decode(
      `${JSON.stringify({
        v: 1,
        t: "present",
        id: "r1",
        sum: summary,
        opt: ["allow", "deny"],
        ttl: 30_000,
      })}\n`,
    ),
    null,
  );
});

test("summary rejects NUL on encode and decode", () => {
  const message = {
    type: "present",
    requestId: "r1",
    summary: "approve\0deny",
    options: ["allow", "deny"],
    ttlMs: 30_000,
  };
  assert.equal(encode(message), null);
  assert.equal(
    decode(
      '{"v":1,"t":"present","id":"r1","sum":"approve\\u0000deny",' +
        '"opt":["allow","deny"],"ttl":30000}\n',
    ),
    null,
  );
});

test("error may omit a request id when the input had no usable id", () => {
  const message = {
    type: "error",
    requestId: null,
    code: "bad_message",
  };
  const wire = '{"v":1,"t":"error","code":"bad_message"}\n';
  assert.equal(encode(message), wire);
  assert.deepEqual(decode(wire), message);
});

test("error rejects an explicitly null request id", () => {
  assert.equal(
    decode('{"v":1,"t":"error","id":null,"code":"bad_message"}\n'),
    null,
  );
});

test("decoder ignores unknown fields but rejects unknown types and extra lines", () => {
  assert.deepEqual(
    decode(
      '{"v":1,"t":"answer","id":"r1","ch":"deny",' +
        '"future":{"note":"comma, brace } and quote \\\" remain data"}}\n',
    ),
    { type: "answer", requestId: "r1", choice: "deny" },
  );
  const longUnknownKey = "k".repeat(241);
  assert.equal(
    decode(`{"v":1,"t":"answer","id":"r1","ch":"allow","${longUnknownKey}":true}`)?.choice,
    "allow",
  );
  assert.equal(decode('{"v":1,"t":"future","id":"r1"}\n'), null);
  assert.equal(
    decode(
      '{"v":1,"t":"answer","id":"r1","ch":"deny"}\n' +
        '{"v":1,"t":"answer","id":"r2","ch":"allow"}\n',
    ),
    null,
  );
});
