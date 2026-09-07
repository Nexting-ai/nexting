#include "ahakey_proto.h"

#include <string.h>

ahakey_proto_result_t ahakey_proto_decode(const uint8_t *bytes, size_t length,
                                          ahakey_proto_frame_t *frame) {
  if (bytes == NULL || frame == NULL || length < AHAKEY_PROTO_MIN_FRAME_BYTES ||
      length > AHAKEY_PROTO_MAX_FRAME_BYTES)
    return AHAKEY_PROTO_BAD_FRAME;
  if (bytes[0] != AHAKEY_PROTO_HEADER_0 || bytes[1] != AHAKEY_PROTO_HEADER_1 ||
      bytes[length - 2U] != AHAKEY_PROTO_TAIL_0 ||
      bytes[length - 1U] != AHAKEY_PROTO_TAIL_1)
    return AHAKEY_PROTO_BAD_FRAME;
  frame->cmd = bytes[2];
  frame->data_length = length - AHAKEY_PROTO_MIN_FRAME_BYTES;
  if (frame->data_length != 0U)
    memcpy(frame->data, bytes + 3U, frame->data_length);
  return AHAKEY_PROTO_OK;
}

ahakey_proto_result_t ahakey_proto_build(uint8_t cmd, const uint8_t *data,
                                         size_t data_length, uint8_t *output,
                                         size_t output_capacity,
                                         size_t *output_length) {
  if (output == NULL || (data == NULL && data_length != 0U) ||
      data_length > AHAKEY_PROTO_MAX_DATA_BYTES)
    return AHAKEY_PROTO_BAD_FRAME;
  const size_t frame_length = AHAKEY_PROTO_MIN_FRAME_BYTES + data_length;
  if (output_capacity < frame_length)
    return AHAKEY_PROTO_BUFFER_TOO_SMALL;
  output[0] = AHAKEY_PROTO_HEADER_0;
  output[1] = AHAKEY_PROTO_HEADER_1;
  output[2] = cmd;
  if (data_length != 0U)
    memcpy(output + 3U, data, data_length);
  output[3U + data_length] = AHAKEY_PROTO_TAIL_0;
  output[4U + data_length] = AHAKEY_PROTO_TAIL_1;
  if (output_length != NULL)
    *output_length = frame_length;
  return AHAKEY_PROTO_OK;
}

ahakey_proto_result_t ahakey_proto_build_update_state(ahakey_state_t state,
                                                      uint8_t *output,
                                                      size_t output_capacity,
                                                      size_t *output_length) {
  if (state > AHAKEY_STATE_SESSION_END)
    return AHAKEY_PROTO_BAD_FRAME;
  const uint8_t data = (uint8_t)state;
  return ahakey_proto_build(AHAKEY_PROTO_CMD_UPDATE_STATE, &data, 1U, output,
                            output_capacity, output_length);
}

static bool is_printable_ascii(const char *text, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const uint8_t byte = (uint8_t)text[i];
    if (byte < 0x20U || byte > 0x7EU)
      return false;
  }
  return true;
}

ahakey_proto_result_t ahakey_proto_build_key_description(
    uint8_t mode, uint8_t key_index, const char *ascii, uint8_t *output,
    size_t output_capacity, size_t *output_length) {
  if (ascii == NULL || mode >= AHAKEY_PROTO_MODE_COUNT ||
      key_index >= AHAKEY_PROTO_KEY_COUNT)
    return AHAKEY_PROTO_BAD_FRAME;
  const size_t text_length = strlen(ascii);
  if (text_length == 0U ||
      text_length > AHAKEY_PROTO_KEY_DESCRIPTION_MAX_BYTES ||
      !is_printable_ascii(ascii, text_length))
    return AHAKEY_PROTO_BAD_FRAME;
  uint8_t data[3U + AHAKEY_PROTO_KEY_DESCRIPTION_MAX_BYTES];
  data[0] = AHAKEY_PROTO_KEY_SUB_DESCRIPTION;
  data[1] = mode;
  data[2] = key_index;
  memcpy(data + 3U, ascii, text_length);
  return ahakey_proto_build(AHAKEY_PROTO_CMD_UPDATE_CUSTOM_KEY, data,
                            3U + text_length, output, output_capacity,
                            output_length);
}

ahakey_proto_result_t ahakey_proto_build_change_name(const uint8_t *name,
                                                     size_t name_length,
                                                     uint8_t *output,
                                                     size_t output_capacity,
                                                     size_t *output_length) {
  if (name == NULL || name_length == 0U ||
      name_length > AHAKEY_PROTO_NAME_MAX_BYTES)
    return AHAKEY_PROTO_BAD_FRAME;
  return ahakey_proto_build(AHAKEY_PROTO_CMD_CHANGE_NAME, name, name_length,
                            output, output_capacity, output_length);
}

ahakey_proto_result_t ahakey_proto_parse_response(
    const ahakey_proto_frame_t *frame, uint8_t expected_cmd, uint8_t *status) {
  if (frame == NULL || status == NULL || frame->cmd != expected_cmd ||
      frame->data_length == 0U)
    return AHAKEY_PROTO_BAD_FRAME;
  *status = frame->data[0];
  return AHAKEY_PROTO_OK;
}

ahakey_proto_result_t ahakey_proto_parse_status(
    const ahakey_proto_frame_t *frame, ahakey_proto_status_t *status) {
  if (frame == NULL || status == NULL ||
      frame->cmd != AHAKEY_PROTO_CMD_STATUS_QUERY || frame->data_length < 7U)
    return AHAKEY_PROTO_BAD_FRAME;
  status->battery_percent = frame->data[0];
  status->signal_strength = frame->data[1];
  status->firmware_major = frame->data[2];
  status->firmware_minor = frame->data[3];
  status->work_mode = frame->data[4];
  status->light_mode = frame->data[5];
  status->switch_state = frame->data[6];
  return AHAKEY_PROTO_OK;
}
