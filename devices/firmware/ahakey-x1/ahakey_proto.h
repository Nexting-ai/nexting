#ifndef AHAKEY_PROTO_H
#define AHAKEY_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Portable codec for the AhaKey X1 BLE configuration protocol (GATT service
 * 0x7340), as published by the AhaKey team in AhakeyAI/desktop
 * docs/ble-protocol.md. This file only builds and parses frames; the board
 * port owns GATT, HID, and timing.
 *
 * Frame layout: [0xAA 0xBB] [cmd:1] [data:N] [0xCC 0xDD] (minimum 5 bytes).
 * Responses arrive on the 0x7344 notify characteristic as
 * [0xAA 0xBB] [cmd_echo:1] [status:1] [data:N] [0xCC 0xDD], except the
 * status-query response, whose data starts immediately after the echo.
 */

#define AHAKEY_PROTO_HEADER_0 0xAAU
#define AHAKEY_PROTO_HEADER_1 0xBBU
#define AHAKEY_PROTO_TAIL_0 0xCCU
#define AHAKEY_PROTO_TAIL_1 0xDDU
#define AHAKEY_PROTO_MIN_FRAME_BYTES 5U
#define AHAKEY_PROTO_MAX_DATA_BYTES 104U
#define AHAKEY_PROTO_MAX_FRAME_BYTES \
  (AHAKEY_PROTO_MIN_FRAME_BYTES + AHAKEY_PROTO_MAX_DATA_BYTES)

#define AHAKEY_PROTO_NAME_MAX_BYTES 21U
#define AHAKEY_PROTO_KEY_DESCRIPTION_MAX_BYTES 20U
#define AHAKEY_PROTO_KEY_BINDING_MAX_BYTES 98U
#define AHAKEY_PROTO_MODE_COUNT 3U
#define AHAKEY_PROTO_KEY_COUNT 4U

typedef enum {
  AHAKEY_PROTO_CMD_STATUS_QUERY = 0x00,
  AHAKEY_PROTO_CMD_CHANGE_NAME = 0x01,
  AHAKEY_PROTO_CMD_CHANGE_APPEARANCE = 0x02,
  AHAKEY_PROTO_CMD_SAVE_CONFIG = 0x04,
  AHAKEY_PROTO_CMD_UPDATE_CUSTOM_KEY = 0x73,
  AHAKEY_PROTO_CMD_PREPARE_WRITE = 0x80,
  AHAKEY_PROTO_CMD_WRITE_RESULT = 0x81,
  AHAKEY_PROTO_CMD_UPDATE_PIC = 0x82,
  AHAKEY_PROTO_CMD_READ_PIC_STATE = 0x83,
  AHAKEY_PROTO_CMD_UPDATE_STATE = 0x90
} ahakey_proto_cmd_t;

typedef enum {
  AHAKEY_PROTO_KEY_SUB_SHORTCUT = 0x73,
  AHAKEY_PROTO_KEY_SUB_MACRO = 0x74,
  AHAKEY_PROTO_KEY_SUB_DESCRIPTION = 0x75
} ahakey_proto_key_subtype_t;

/* IDE state values written with UPDATE_STATE (0x90) to drive the LED strip. */
typedef enum {
  AHAKEY_STATE_NOTIFICATION = 0,
  AHAKEY_STATE_PERMISSION_REQUEST = 1,
  AHAKEY_STATE_POST_TOOL_USE = 2,
  AHAKEY_STATE_PRE_TOOL_USE = 3,
  AHAKEY_STATE_SESSION_START = 4,
  AHAKEY_STATE_STOP = 5,
  AHAKEY_STATE_TASK_COMPLETED = 6,
  AHAKEY_STATE_USER_PROMPT_SUBMIT = 7,
  AHAKEY_STATE_SESSION_END = 8
} ahakey_state_t;

typedef enum {
  AHAKEY_PROTO_OK = 0,
  AHAKEY_PROTO_BAD_FRAME,
  AHAKEY_PROTO_BUFFER_TOO_SMALL
} ahakey_proto_result_t;

typedef struct {
  uint8_t cmd;
  uint8_t data[AHAKEY_PROTO_MAX_DATA_BYTES];
  size_t data_length;
} ahakey_proto_frame_t;

typedef struct {
  uint8_t battery_percent;
  uint8_t signal_strength;
  uint8_t firmware_major;
  uint8_t firmware_minor;
  uint8_t work_mode;
  uint8_t light_mode;
  /* Physical approval lever: 0 = at rest (auto), non-zero = switched (ask). */
  uint8_t switch_state;
} ahakey_proto_status_t;

/*
 * Validate one complete frame and copy out cmd + data. The input must hold
 * exactly one frame including header and tail.
 */
ahakey_proto_result_t ahakey_proto_decode(const uint8_t *bytes, size_t length,
                                          ahakey_proto_frame_t *frame);

/* Build any command frame. data may be NULL when data_length is 0. */
ahakey_proto_result_t ahakey_proto_build(uint8_t cmd, const uint8_t *data,
                                         size_t data_length, uint8_t *output,
                                         size_t output_capacity,
                                         size_t *output_length);

/* UPDATE_STATE (0x90): sync one IDE state to the keyboard LED strip. */
ahakey_proto_result_t ahakey_proto_build_update_state(ahakey_state_t state,
                                                      uint8_t *output,
                                                      size_t output_capacity,
                                                      size_t *output_length);

/*
 * Key description (UPDATE_CUSTOM_KEY subtype 0x75): an ASCII label of at most
 * 20 bytes shown on the OLED for one key in one mode. Non-ASCII or overlong
 * input is rejected, never truncated silently.
 */
ahakey_proto_result_t ahakey_proto_build_key_description(
    uint8_t mode, uint8_t key_index, const char *ascii, uint8_t *output,
    size_t output_capacity, size_t *output_length);

/* CHANGE_NAME (0x01): UTF-8 name of at most 21 bytes; needs SAVE_CONFIG. */
ahakey_proto_result_t ahakey_proto_build_change_name(const uint8_t *name,
                                                     size_t name_length,
                                                     uint8_t *output,
                                                     size_t output_capacity,
                                                     size_t *output_length);

/*
 * Parse the response to a command. For most commands the first data byte is a
 * status byte (0 = success); the status query (0x00) is the exception and its
 * payload starts immediately. Use ahakey_proto_parse_status for that one.
 */
ahakey_proto_result_t ahakey_proto_parse_response(
    const ahakey_proto_frame_t *frame, uint8_t expected_cmd, uint8_t *status);

/*
 * Parse the status-query payload into fields. The published payload carries
 * 7 fields plus one reserved byte; 7 or more data bytes are accepted.
 */
ahakey_proto_result_t ahakey_proto_parse_status(
    const ahakey_proto_frame_t *frame, ahakey_proto_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
