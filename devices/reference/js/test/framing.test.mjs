import assert from "node:assert/strict";
import test from "node:test";

import { createLineDecoder } from "../src/framing.mjs";

const answerWire = '{"v":1,"t":"answer","id":"r1","ch":"allow"}\n';
const answerMessage = { type: "answer", requestId: "r1", choice: "allow" };

test("line decoder buffers a message split across writes", () => {
  const stream = createLineDecoder({ maxMessageBytes: 512 });
  assert.deepEqual(stream.push(answerWire.slice(0, 9)), []);
  assert.deepEqual(stream.push(answerWire.slice(9)), [answerMessage]);
});

test("line decoder handles a multibyte character split across byte chunks", () => {
  const stream = createLineDecoder({ maxMessageBytes: 512 });
  const wire = new TextEncoder().encode(
    '{"v":1,"t":"present","id":"r1","sum":"批准?","opt":["allow","deny"],"ttl":1000}\n',
  );
  const split = wire.indexOf(0xe6) + 1;
  assert.deepEqual(stream.push(wire.slice(0, split)), []);
  assert.deepEqual(stream.push(wire.slice(split)), [
    {
      type: "present",
      requestId: "r1",
      summary: "批准?",
      options: ["allow", "deny"],
      ttlMs: 1000,
    },
  ]);
});

test("line decoder emits every complete line in one chunk", () => {
  const stream = createLineDecoder({ maxMessageBytes: 512 });
  assert.deepEqual(stream.push(answerWire + answerWire), [
    answerMessage,
    answerMessage,
  ]);
});

test("oversize input emits one error then discards through newline", () => {
  const stream = createLineDecoder({ maxMessageBytes: 64 });
  assert.deepEqual(stream.push("x".repeat(64)), [
    { type: "error", requestId: null, code: "message_too_large" },
  ]);
  assert.deepEqual(stream.push("still discarded\n"), []);
  assert.deepEqual(stream.push(answerWire), [answerMessage]);
});

test("message limit counts the terminating newline", () => {
  const exact = createLineDecoder({
    maxMessageBytes: Buffer.byteLength(answerWire, "utf8"),
  });
  assert.deepEqual(exact.push(answerWire), [answerMessage]);

  const tooSmall = createLineDecoder({
    maxMessageBytes: Buffer.byteLength(answerWire, "utf8") - 1,
  });
  assert.deepEqual(tooSmall.push(answerWire), [
    { type: "error", requestId: null, code: "message_too_large" },
  ]);
});

test("malformed line reports bad_message and reset clears partial state", () => {
  const stream = createLineDecoder({ maxMessageBytes: 512 });
  assert.deepEqual(stream.push("not json\n"), [
    { type: "error", requestId: null, code: "bad_message" },
  ]);
  stream.push(answerWire.slice(0, 10));
  stream.reset();
  assert.deepEqual(stream.push(answerWire.slice(10)), [
    { type: "error", requestId: null, code: "bad_message" },
  ]);
});

test("line decoder rejects invalid limits before accepting input", () => {
  for (const maxMessageBytes of [0, -1, 1.5, Number.NaN]) {
    assert.throws(
      () => createLineDecoder({ maxMessageBytes }),
      /integer from 1 to 4096/,
    );
  }
  assert.throws(
    () => createLineDecoder({ maxMessageBytes: 4097 }),
    /integer from 1 to 4096/,
  );
});

test("line decoder fails closed for unsupported chunk types", () => {
  const stream = createLineDecoder({ maxMessageBytes: 512 });
  for (const chunk of [null, undefined, 42, {}, new ArrayBuffer(4)]) {
    assert.deepEqual(stream.push(chunk), [
      { type: "error", requestId: null, code: "bad_message" },
    ]);
  }
});

test("one hostile chunk cannot amplify into unbounded result objects", () => {
  const stream = createLineDecoder();
  assert.deepEqual(stream.push("\n".repeat(4097)), [
    { type: "error", requestId: null, code: "message_too_large" },
  ]);
});
