import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  AUDIO_FRAME_BYTES,
  AUDIO_FRAGMENT_HEADER_BYTES,
  crc32c,
  decodeAudioFrame,
  decodeImaAdpcmBlock,
  encodeAudioFrame,
  encodeImaAdpcmBlock,
  fragmentAudioFrame,
  parseAudioFragment,
} from "../src/audio.mjs";

const vectors = JSON.parse(
  await readFile(
    new URL("../../../protocol/vectors/device-audio-v1.json", import.meta.url),
    "utf8",
  ),
);
const golden = vectors.valid[0];
const goldenBytes = Uint8Array.from(Buffer.from(golden.frame_hex, "hex"));

test("CRC32C uses the Castagnoli check value", () => {
  assert.equal(crc32c(new TextEncoder().encode("123456789")), 0xe3069283);
});

test("a zero PCM block matches the cross-language golden frame", () => {
  const samples = new Int16Array(320).fill(golden.pcm_fill);
  const encoded = encodeAudioFrame({
    epoch: golden.epoch,
    streamId: golden.stream_id,
    sequence: golden.sequence,
    sampleIndex: golden.sample_index,
    stepIndex: golden.step_index,
    samples,
  });
  assert.equal(encoded.length, AUDIO_FRAME_BYTES);
  assert.equal(Buffer.from(encoded).toString("hex"), golden.frame_hex);

  const decoded = decodeAudioFrame(encoded);
  assert.equal(decoded.epoch, golden.epoch);
  assert.equal(decoded.streamId, golden.stream_id);
  assert.equal(decoded.sequence, 0);
  assert.equal(decoded.sampleIndex, 0);
  assert.deepEqual([...decoded.samples], [...samples]);
});

test("logical frame metadata is continuous and bounded to 120 seconds", () => {
  const samples = new Int16Array(320);
  assert.throws(() =>
    encodeAudioFrame({
      epoch: 1,
      streamId: 1,
      sequence: 1,
      sampleIndex: 0,
      samples,
    }),
  );
  assert.throws(() =>
    encodeAudioFrame({
      epoch: 1,
      streamId: 1,
      sequence: 6000,
      sampleIndex: 1_920_000,
      samples,
    }),
  );
});

test("IMA ADPCM encodes low nibbles first and decodes exactly 320 samples", () => {
  const samples = Int16Array.from({ length: 320 }, (_, index) =>
    Math.max(-32768, Math.min(32767, index * 97 - 15000)),
  );
  const block = encodeImaAdpcmBlock(samples, 7);
  assert.equal(block.predictor, samples[0]);
  assert.equal(block.stepIndex, 7);
  assert.equal(block.payload.length, 160);
  assert.equal(block.payload[159] & 0xf0, 0);

  const decoded = decodeImaAdpcmBlock({ ...block, sampleCount: 320 });
  assert.equal(decoded.length, 320);
  assert.equal(decoded[0], samples[0]);
  const maxError = Math.max(
    ...decoded.map((sample, index) => Math.abs(sample - samples[index])),
  );
  assert.ok(maxError < 4000, `unexpected ADPCM error ${maxError}`);
});

test("logical frames split into at most four bounded non-interleavable fragments", () => {
  const fragments = fragmentAudioFrame(goldenBytes, {
    streamId: golden.stream_id,
    sequence: 0,
    maxPacketBytes: 63,
  });
  assert.equal(fragments.length, 4);
  const parsed = fragments.map(parseAudioFragment);
  assert.deepEqual(parsed.map((item) => item.fragmentIndex), [0, 1, 2, 3]);
  assert.ok(fragments.every((item) => item.length <= 63));
  assert.deepEqual(
    Buffer.concat(parsed.map((item) => Buffer.from(item.payload))),
    Buffer.from(goldenBytes),
  );
  assert.throws(() =>
    fragmentAudioFrame(goldenBytes, {
      streamId: golden.stream_id,
      sequence: 0,
      maxPacketBytes: 59,
    }),
  );
  assert.equal(
    parseAudioFragment(
      new Uint8Array(AUDIO_FRAME_BYTES + AUDIO_FRAGMENT_HEADER_BYTES + 1),
    ),
    null,
  );
  assert.throws(() =>
    fragmentAudioFrame(goldenBytes, {
      streamId: golden.stream_id + 1,
      sequence: 0,
      maxPacketBytes: 63,
    }),
  );
  assert.throws(() =>
    fragmentAudioFrame(goldenBytes, {
      streamId: golden.stream_id,
      sequence: 1,
      maxPacketBytes: 63,
    }),
  );
});

test("malformed and corrupted frame vectors fail closed", () => {
  for (const item of vectors.invalid) {
    let bytes;
    if (item.frame_hex) bytes = Uint8Array.from(Buffer.from(item.frame_hex, "hex"));
    else {
      bytes = goldenBytes.slice();
      bytes[item.mutate.offset] = item.mutate.value;
      if (item.recompute_crc) {
        new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).setUint32(
          bytes.length - 4,
          crc32c(bytes.subarray(0, -4)),
          true,
        );
      }
    }
    assert.equal(decodeAudioFrame(bytes), null, item.name);
  }
});
