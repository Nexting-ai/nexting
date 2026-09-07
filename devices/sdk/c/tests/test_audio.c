#include "nexting_device.h"
#include "nexting_device_audio.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t hex_nibble(char value) {
  if (value >= '0' && value <= '9')
    return (uint8_t)(value - '0');
  if (value >= 'a' && value <= 'f')
    return (uint8_t)(value - 'a' + 10);
  return (uint8_t)(value - 'A' + 10);
}

static size_t decode_hex(const char *hex, uint8_t *output, size_t capacity) {
  const size_t length = strlen(hex) / 2U;
  assert(length <= capacity);
  for (size_t index = 0; index < length; ++index) {
    output[index] = (uint8_t)((hex_nibble(hex[index * 2U]) << 4U) |
                              hex_nibble(hex[index * 2U + 1U]));
  }
  return length;
}

static void test_crc32c(void) {
  static const uint8_t check[] = "123456789";
  assert(nexting_device_crc32c(check, sizeof(check) - 1U) == UINT32_C(0xe3069283));
}

static void test_zero_golden_frame(void) {
  static const char *golden_hex =
      "0101010044332211ddccbbaa0000000000000000400100000000a00000000000"
      "0000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000"
      "00000000000000000000000000000000000000000000000000000000c78c4635";
  int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME] = {0};
  uint8_t encoded[NEXTING_DEVICE_AUDIO_FRAME_BYTES];
  uint8_t expected[NEXTING_DEVICE_AUDIO_FRAME_BYTES];
  nexting_device_audio_frame_t decoded;
  size_t encoded_length = 0U;

  assert(decode_hex(golden_hex, expected, sizeof(expected)) == sizeof(expected));
  assert(nexting_device_audio_encode_frame(
             UINT32_C(0x11223344), UINT32_C(0xaabbccdd), 0U, 0U, samples,
             NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME, 0U, encoded,
             sizeof(encoded), &encoded_length) == NEXTING_DEVICE_AUDIO_OK);
  assert(encoded_length == sizeof(encoded));
  assert(memcmp(encoded, expected, sizeof(encoded)) == 0);

  assert(nexting_device_audio_decode_frame(encoded, sizeof(encoded), &decoded) ==
         NEXTING_DEVICE_AUDIO_OK);
  assert(decoded.epoch == UINT32_C(0x11223344));
  assert(decoded.stream_id == UINT32_C(0xaabbccdd));
  assert(decoded.sequence == 0U);
  assert(decoded.sample_index == 0U);
  assert(decoded.predictor == 0);
  assert(decoded.step_index == 0U);
  for (size_t index = 0; index < NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME; ++index)
    assert(decoded.samples[index] == 0);
}

static void test_ramp_round_trip(void) {
  int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME];
  uint8_t payload[NEXTING_DEVICE_AUDIO_PAYLOAD_BYTES];
  int16_t decoded[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME];
  int16_t predictor = 0;
  for (size_t index = 0; index < NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME; ++index)
    samples[index] = (int16_t)((int32_t)index * 97 - 15000);

  assert(nexting_device_audio_encode_block(
             samples, NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME, 7U, &predictor,
             payload, sizeof(payload)) == NEXTING_DEVICE_AUDIO_OK);
  assert(predictor == samples[0]);
  assert((payload[sizeof(payload) - 1U] & 0xf0U) == 0U);
  assert(nexting_device_audio_decode_block(
             predictor, 7U, payload, sizeof(payload),
             NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME, decoded,
             NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME) == NEXTING_DEVICE_AUDIO_OK);
  assert(decoded[0] == samples[0]);
  for (size_t index = 0; index < NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME; ++index) {
    int32_t error = (int32_t)decoded[index] - samples[index];
    if (error < 0)
      error = -error;
    assert(error < 4000);
  }
}

static void test_invalid_frames_fail_closed(void) {
  int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME] = {0};
  uint8_t frame[NEXTING_DEVICE_AUDIO_FRAME_BYTES];
  nexting_device_audio_frame_t decoded;
  size_t length = 0U;
  assert(nexting_device_audio_encode_frame(
             1U, 2U, 1U, 0U, samples,
             NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME, 0U, frame, sizeof(frame),
             &length) == NEXTING_DEVICE_AUDIO_BAD_ARGUMENT);
  assert(nexting_device_audio_encode_frame(
             1U, 2U, 6000U, 1920000U, samples,
             NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME, 0U, frame, sizeof(frame),
             &length) == NEXTING_DEVICE_AUDIO_BAD_ARGUMENT);
  assert(nexting_device_audio_encode_frame(
             1U, 2U, 0U, 0U, samples, NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME,
             0U, frame, sizeof(frame), &length) == NEXTING_DEVICE_AUDIO_OK);
  assert(nexting_device_audio_decode_frame(frame, length - 1U, &decoded) ==
         NEXTING_DEVICE_AUDIO_BAD_FRAME);
  frame[24] = 89U;
  {
    const uint32_t crc = nexting_device_crc32c(frame, length - 4U);
    frame[length - 4U] = (uint8_t)crc;
    frame[length - 3U] = (uint8_t)(crc >> 8U);
    frame[length - 2U] = (uint8_t)(crc >> 16U);
    frame[length - 1U] = (uint8_t)(crc >> 24U);
  }
  assert(nexting_device_audio_decode_frame(frame, length, &decoded) ==
         NEXTING_DEVICE_AUDIO_BAD_FRAME);
  frame[24] = 0U;
  {
    const uint32_t crc = nexting_device_crc32c(frame, length - 4U);
    frame[length - 4U] = (uint8_t)crc;
    frame[length - 3U] = (uint8_t)(crc >> 8U);
    frame[length - 2U] = (uint8_t)(crc >> 16U);
    frame[length - 1U] = (uint8_t)(crc >> 24U);
  }
  frame[length - 1U] ^= 1U;
  assert(nexting_device_audio_decode_frame(frame, length, &decoded) ==
         NEXTING_DEVICE_AUDIO_BAD_FRAME);

  assert(nexting_device_audio_encode_frame(
             1U, 2U, 0U, 0U, samples, NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME,
             0U, frame, sizeof(frame), &length) == NEXTING_DEVICE_AUDIO_OK);
  frame[16] = 1U;
  {
    const uint32_t crc = nexting_device_crc32c(frame, length - 4U);
    frame[length - 4U] = (uint8_t)crc;
    frame[length - 3U] = (uint8_t)(crc >> 8U);
    frame[length - 2U] = (uint8_t)(crc >> 16U);
    frame[length - 1U] = (uint8_t)(crc >> 24U);
  }
  assert(nexting_device_audio_decode_frame(frame, length, &decoded) ==
         NEXTING_DEVICE_AUDIO_BAD_FRAME);
}

static void assert_control_round_trip(const char *wire,
                                      nexting_device_message_type_t type) {
  nexting_device_message_t message = {0};
  char encoded[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES];
  size_t encoded_length = 0U;
  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(message.type == type);
  assert(nexting_device_encode(&message, encoded, sizeof(encoded),
                               &encoded_length) == NEXTING_DEVICE_OK);
  assert(encoded_length == strlen(wire));
  assert(memcmp(encoded, wire, encoded_length) == 0);
}

static void test_audio_control_messages(void) {
  assert_control_round_trip(
      "{\"v\":1,\"t\":\"audio_config\",\"epoch\":287454020,\"codec\":"
      "\"ima_adpcm\",\"sample_rate\":16000,\"channels\":1,\"frame_ms\":20,"
      "\"max_duration_ms\":120000,\"startup_buffer_ms\":200,\"credits\":8}\n",
      NEXTING_DEVICE_MESSAGE_AUDIO_CONFIG);
  assert_control_round_trip(
      "{\"v\":1,\"t\":\"audio_begin\",\"epoch\":287454020,\"stream_id\":"
      "2864434397,\"codec\":\"ima_adpcm\",\"sample_rate\":16000,"
      "\"channels\":1,\"frame_ms\":20}\n",
      NEXTING_DEVICE_MESSAGE_AUDIO_BEGIN);
  assert_control_round_trip(
      "{\"v\":1,\"t\":\"audio_credit\",\"epoch\":287454020,\"stream_id\":"
      "2864434397,\"ack_seq\":7,\"credits\":4}\n",
      NEXTING_DEVICE_MESSAGE_AUDIO_CREDIT);
  assert_control_round_trip(
      "{\"v\":1,\"t\":\"audio_end\",\"epoch\":287454020,\"stream_id\":"
      "2864434397,\"last_seq\":49,\"sample_count\":16000,\"reason\":"
      "\"submitted\"}\n",
      NEXTING_DEVICE_MESSAGE_AUDIO_END);
  assert_control_round_trip(
      "{\"v\":1,\"t\":\"audio_cancel\",\"epoch\":287454020,\"stream_id\":"
      "2864434397,\"reason\":\"rejected\"}\n",
      NEXTING_DEVICE_MESSAGE_AUDIO_CANCEL);
  assert_control_round_trip(
      "{\"v\":1,\"t\":\"audio_state\",\"epoch\":287454020,\"stream_id\":"
      "2864434397,\"state\":\"transcribing\"}\n",
      NEXTING_DEVICE_MESSAGE_AUDIO_STATE);
  assert_control_round_trip(
      "{\"v\":1,\"t\":\"audio_state\",\"epoch\":287454020,\"state\":"
      "\"ready\"}\n",
      NEXTING_DEVICE_MESSAGE_AUDIO_STATE);

  nexting_device_message_t invalid = {0};
  static const char remote_start[] =
      "{\"v\":1,\"t\":\"audio_begin\",\"epoch\":1,\"stream_id\":1,"
      "\"codec\":\"ima_adpcm\",\"sample_rate\":16000,\"channels\":1,"
      "\"frame_ms\":20,\"remote_start\":true}\n";
  assert(nexting_device_decode(remote_start, strlen(remote_start), &invalid) ==
         NEXTING_DEVICE_BAD_MESSAGE);
  static const char inconsistent_end[] =
      "{\"v\":1,\"t\":\"audio_end\",\"epoch\":1,\"stream_id\":1,"
      "\"last_seq\":1,\"sample_count\":320,\"reason\":\"submitted\"}\n";
  assert(nexting_device_decode(inconsistent_end, strlen(inconsistent_end),
                               &invalid) == NEXTING_DEVICE_BAD_MESSAGE);
}

static void test_device_info_audio_capability(void) {
  static const char valid[] =
      "{\"protocol\":\"nexting-device\",\"spec\":\"0.4.0-experimental.1\","
      "\"wire\":[1],\"profiles\":[\"approval/1\",\"device-audio/1\"],"
      "\"model\":\"audio-reference\",\"fw\":\"0.3.0\","
      "\"max_message_bytes\":4096,\"max_summary_bytes\":240,\"audio\":{"
      "\"source\":\"device_microphone\",\"codecs\":[\"ima_adpcm\"],"
      "\"sample_rates\":[16000],\"channels\":1,\"frame_ms\":[20],"
      "\"max_duration_ms\":120000,\"startup_buffer_ms\":200}}";
  static const char missing_capability[] =
      "{\"protocol\":\"nexting-device\",\"spec\":\"0.4.0-experimental.1\","
      "\"wire\":[1],\"profiles\":[\"approval/1\",\"device-audio/1\"],"
      "\"model\":\"audio-reference\",\"fw\":\"0.3.0\","
      "\"max_message_bytes\":4096,\"max_summary_bytes\":240}";
  nexting_device_info_t info = {0};
  assert(nexting_device_info_decode(valid, sizeof(valid) - 1U, &info) ==
         NEXTING_DEVICE_OK);
  assert(info.has_device_audio);
  assert(info.supports_device_audio_v1);
  assert(nexting_device_info_decode(missing_capability,
                                    sizeof(missing_capability) - 1U,
                                    &info) == NEXTING_DEVICE_BAD_MESSAGE);
}

int main(void) {
  test_crc32c();
  test_zero_golden_frame();
  test_ramp_round_trip();
  test_invalid_frames_fail_closed();
  test_audio_control_messages();
  test_device_info_audio_capability();
  puts("audio tests passed");
  return 0;
}
