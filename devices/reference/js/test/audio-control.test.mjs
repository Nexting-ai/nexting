import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  DEVICE_AUDIO_PROFILE,
  INTERACTION_PROFILES,
  decode,
  encode,
} from "../src/protocol.mjs";

const vectors = JSON.parse(
  await readFile(
    new URL("../../../protocol/vectors/device-audio-v1.json", import.meta.url),
    "utf8",
  ),
);

test("device-audio/1 control messages round-trip canonically", () => {
  assert.equal(DEVICE_AUDIO_PROFILE, "device-audio/1");
  assert.equal(INTERACTION_PROFILES.includes(DEVICE_AUDIO_PROFILE), true);
  for (const item of vectors.control.valid) {
    assert.deepEqual(decode(item.wire), item.decoded, item.name);
    assert.equal(encode(item.decoded), item.wire, item.name);
  }
});

test("device audio controls reject unsupported or microphone-opening fields", () => {
  for (const wire of vectors.control.invalid) {
    assert.equal(decode(wire), null, wire);
  }
});

test("message schema distinguishes device audio from Host microphone voice", async () => {
  const schema = JSON.parse(
    await readFile(
      new URL("../../../schemas/message.schema.json", import.meta.url),
      "utf8",
    ),
  );
  const audioRefs = schema.oneOf
    .map((item) => item.$ref)
    .filter((ref) => ref?.includes("audio"));
  assert.deepEqual(audioRefs, [
    "#/$defs/audioConfig",
    "#/$defs/audioBegin",
    "#/$defs/audioCredit",
    "#/$defs/audioEnd",
    "#/$defs/audioCancel",
    "#/$defs/audioState",
  ]);
  assert.equal(schema.$defs.deviceAudioCapabilities.properties.source.const, "device_microphone");
  assert.deepEqual(schema.$defs.deviceAudioCapabilities.properties.codecs.const, [
    "ima_adpcm",
  ]);
  assert.equal(schema.$defs.audioConfig.properties.max_duration_ms.const, 120000);
  assert.equal(schema.$defs.audioEnd.properties.last_seq.maximum, 5999);
  assert.equal(schema.$defs.audioEnd.properties.sample_count.multipleOf, 320);
  assert.equal(schema.$defs.voiceEvent.properties.t.const, "voice_event");
  assert.deepEqual(schema.$defs.deviceInfo.allOf[0].then.required, ["audio"]);
  assert.deepEqual(schema.$defs.deviceInfo.allOf[0].else.not.required, ["audio"]);
});
