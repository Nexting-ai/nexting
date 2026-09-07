#include "ahakey_proto.h"

#include <assert.h>
#include <string.h>

static void test_round_trip(uint8_t cmd, const uint8_t *data,
                            size_t data_length) {
  uint8_t wire[AHAKEY_PROTO_MAX_FRAME_BYTES];
  size_t wire_length = 0;
  assert(ahakey_proto_build(cmd, data, data_length, wire, sizeof wire,
                            &wire_length) == AHAKEY_PROTO_OK);
  assert(wire_length == AHAKEY_PROTO_MIN_FRAME_BYTES + data_length);
  assert(wire[0] == 0xAAU && wire[1] == 0xBBU);
  assert(wire[wire_length - 2U] == 0xCCU);
  assert(wire[wire_length - 1U] == 0xDDU);
  ahakey_proto_frame_t frame;
  assert(ahakey_proto_decode(wire, wire_length, &frame) == AHAKEY_PROTO_OK);
  assert(frame.cmd == cmd);
  assert(frame.data_length == data_length);
  if (data_length != 0U)
    assert(memcmp(frame.data, data, data_length) == 0);
}

int main(void) {
  /* Status query matches the published capture AA BB 00 CC DD. */
  uint8_t wire[AHAKEY_PROTO_MAX_FRAME_BYTES];
  size_t wire_length = 0;
  assert(ahakey_proto_build(AHAKEY_PROTO_CMD_STATUS_QUERY, NULL, 0U, wire,
                            sizeof wire, &wire_length) == AHAKEY_PROTO_OK);
  static const uint8_t expect_query[] = {0xAA, 0xBB, 0x00, 0xCC, 0xDD};
  assert(wire_length == sizeof expect_query);
  assert(memcmp(wire, expect_query, sizeof expect_query) == 0);

  /* Published capture: battery 74%, signal 50, fw 1.0, mode/light/switch 0. */
  static const uint8_t capture[] = {0xAA, 0xBB, 0x00, 0x4A, 0x32, 0x01,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0xCC,
                                    0xDD};
  ahakey_proto_frame_t frame;
  assert(ahakey_proto_decode(capture, sizeof capture, &frame) ==
         AHAKEY_PROTO_OK);
  ahakey_proto_status_t status;
  assert(ahakey_proto_parse_status(&frame, &status) == AHAKEY_PROTO_OK);
  assert(status.battery_percent == 74U);
  assert(status.signal_strength == 50U);
  assert(status.firmware_major == 1U);
  assert(status.firmware_minor == 0U);
  assert(status.work_mode == 0U);
  assert(status.light_mode == 0U);
  assert(status.switch_state == 0U);

  /* UPDATE_STATE carries exactly one ClaudeState byte. */
  assert(ahakey_proto_build_update_state(AHAKEY_STATE_PERMISSION_REQUEST,
                                         wire, sizeof wire,
                                         &wire_length) == AHAKEY_PROTO_OK);
  static const uint8_t expect_state[] = {0xAA, 0xBB, 0x90, 0x01, 0xCC, 0xDD};
  assert(wire_length == sizeof expect_state);
  assert(memcmp(wire, expect_state, sizeof expect_state) == 0);
  assert(ahakey_proto_build_update_state((ahakey_state_t)9, wire, sizeof wire,
                                         &wire_length) ==
         AHAKEY_PROTO_BAD_FRAME);

  /* Key description: subtype, mode, key index, then ASCII. */
  assert(ahakey_proto_build_key_description(0, 0, "EchoWrite", wire,
                                            sizeof wire,
                                            &wire_length) == AHAKEY_PROTO_OK);
  assert(ahakey_proto_decode(wire, wire_length, &frame) == AHAKEY_PROTO_OK);
  assert(frame.cmd == AHAKEY_PROTO_CMD_UPDATE_CUSTOM_KEY);
  assert(frame.data[0] == AHAKEY_PROTO_KEY_SUB_DESCRIPTION);
  assert(frame.data[1] == 0U);
  assert(frame.data[2] == 0U);
  assert(frame.data_length == 3U + 9U);
  assert(memcmp(frame.data + 3U, "EchoWrite", 9U) == 0);

  /* Descriptions reject overlong, non-ASCII, and out-of-range selectors. */
  assert(ahakey_proto_build_key_description(
             0, 0, "this-description-is-way-too-long", wire, sizeof wire,
             &wire_length) == AHAKEY_PROTO_BAD_FRAME);
  assert(ahakey_proto_build_key_description(0, 0, "中文", wire, sizeof wire,
                                            &wire_length) ==
         AHAKEY_PROTO_BAD_FRAME);
  assert(ahakey_proto_build_key_description(3, 0, "ok", wire, sizeof wire,
                                            &wire_length) ==
         AHAKEY_PROTO_BAD_FRAME);
  assert(ahakey_proto_build_key_description(0, 4, "ok", wire, sizeof wire,
                                            &wire_length) ==
         AHAKEY_PROTO_BAD_FRAME);

  /* Names are bounded at 21 bytes and save config is an empty frame. */
  assert(ahakey_proto_build_change_name((const uint8_t *)"Nexting X1", 10U,
                                        wire, sizeof wire,
                                        &wire_length) == AHAKEY_PROTO_OK);
  assert(ahakey_proto_build_change_name(
             (const uint8_t *)"a-name-that-is-far-too-long", 27U, wire,
             sizeof wire, &wire_length) == AHAKEY_PROTO_BAD_FRAME);
  assert(ahakey_proto_build(AHAKEY_PROTO_CMD_SAVE_CONFIG, NULL, 0U, wire,
                            sizeof wire, &wire_length) == AHAKEY_PROTO_OK);
  static const uint8_t expect_save[] = {0xAA, 0xBB, 0x04, 0xCC, 0xDD};
  assert(memcmp(wire, expect_save, sizeof expect_save) == 0);

  /* Generic responses expose the status byte after the command echo. */
  static const uint8_t ack[] = {0xAA, 0xBB, 0x90, 0x00, 0xCC, 0xDD};
  assert(ahakey_proto_decode(ack, sizeof ack, &frame) == AHAKEY_PROTO_OK);
  uint8_t response_status = 0xFFU;
  assert(ahakey_proto_parse_response(&frame, AHAKEY_PROTO_CMD_UPDATE_STATE,
                                     &response_status) == AHAKEY_PROTO_OK);
  assert(response_status == 0U);
  assert(ahakey_proto_parse_response(&frame, AHAKEY_PROTO_CMD_SAVE_CONFIG,
                                     &response_status) ==
         AHAKEY_PROTO_BAD_FRAME);

  /* Malformed frames are rejected, never guessed. */
  static const uint8_t bad_tail[] = {0xAA, 0xBB, 0x00, 0xCC, 0xDE};
  assert(ahakey_proto_decode(bad_tail, sizeof bad_tail, &frame) ==
         AHAKEY_PROTO_BAD_FRAME);
  static const uint8_t bad_head[] = {0xAB, 0xBB, 0x00, 0xCC, 0xDD};
  assert(ahakey_proto_decode(bad_head, sizeof bad_head, &frame) ==
         AHAKEY_PROTO_BAD_FRAME);
  static const uint8_t too_short[] = {0xAA, 0xBB, 0xCC, 0xDD};
  assert(ahakey_proto_decode(too_short, sizeof too_short, &frame) ==
         AHAKEY_PROTO_BAD_FRAME);
  assert(ahakey_proto_build(0x00, NULL, 0U, wire, 4U, &wire_length) ==
         AHAKEY_PROTO_BUFFER_TOO_SMALL);

  /* Round-trip every documented command class. */
  test_round_trip(AHAKEY_PROTO_CMD_UPDATE_PIC,
                  (const uint8_t[]){0x00, 0x00, 0x00, 0x01, 0x00, 0x64, 0x00},
                  7U);
  test_round_trip(AHAKEY_PROTO_CMD_READ_PIC_STATE,
                  (const uint8_t[]){0x00}, 1U);

  return 0;
}
