#include "nexting_device.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  const char *bytes;
  size_t length;
  size_t position;
} parser_t;

typedef struct {
  char *bytes;
  size_t capacity;
  size_t length;
  bool overflow;
} writer_t;

#define FIELD_V (UINT64_C(1) << 0)
#define FIELD_T (UINT64_C(1) << 1)
#define FIELD_ID (UINT64_C(1) << 2)
#define FIELD_SUM (UINT64_C(1) << 3)
#define FIELD_OPT (UINT64_C(1) << 4)
#define FIELD_TTL (UINT64_C(1) << 5)
#define FIELD_CH (UINT64_C(1) << 6)
#define FIELD_R (UINT64_C(1) << 7)
#define FIELD_CODE (UINT64_C(1) << 8)
#define FIELD_AGENTS (UINT64_C(1) << 9)
#define FIELD_ITEMS (UINT64_C(1) << 10)
#define FIELD_CURSOR (UINT64_C(1) << 11)
#define FIELD_DIR (UINT64_C(1) << 12)
#define FIELD_SEQ (UINT64_C(1) << 13)
#define FIELD_INDEX (UINT64_C(1) << 14)
#define FIELD_REV (UINT64_C(1) << 15)
#define FIELD_KEYS (UINT64_C(1) << 16)
#define FIELD_SLOT (UINT64_C(1) << 17)
#define FIELD_EVENT (UINT64_C(1) << 18)
#define FIELD_CONTROLS (UINT64_C(1) << 19)
#define FIELD_DELTA (UINT64_C(1) << 20)
#define FIELD_STATE (UINT64_C(1) << 21)
#define FIELD_LABEL (UINT64_C(1) << 22)
#define FIELD_CHANNEL (UINT64_C(1) << 23)
#define FIELD_TITLE (UINT64_C(1) << 24)
#define FIELD_CONTENT (UINT64_C(1) << 25)
#define FIELD_MODEL (UINT64_C(1) << 26)
#define FIELD_INPUT_TOKENS (UINT64_C(1) << 27)
#define FIELD_OUTPUT_TOKENS (UINT64_C(1) << 28)
#define FIELD_CACHED_TOKENS (UINT64_C(1) << 29)
#define FIELD_CONTEXT_USED (UINT64_C(1) << 30)
#define FIELD_CONTEXT_LIMIT (UINT64_C(1) << 31)
#define FIELD_ENTRIES (UINT64_C(1) << 32)
#define FIELD_STATUS (UINT64_C(1) << 33)
#define FIELD_UNKNOWN (UINT64_C(1) << 34)

static bool is_space(char value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

static void skip_space(parser_t *parser) {
  while (parser->position < parser->length &&
         is_space(parser->bytes[parser->position])) {
    parser->position += 1;
  }
}

static bool take(parser_t *parser, char expected) {
  skip_space(parser);
  if (parser->position >= parser->length ||
      parser->bytes[parser->position] != expected) {
    return false;
  }
  parser->position += 1;
  return true;
}

static int hex_value(char value) {
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  return -1;
}

static bool parse_hex_quad(parser_t *parser, uint32_t *value) {
  uint32_t result = 0;
  if (parser->length - parser->position < 4)
    return false;
  for (size_t i = 0; i < 4; ++i) {
    int digit = hex_value(parser->bytes[parser->position++]);
    if (digit < 0)
      return false;
    result = (result << 4) | (uint32_t)digit;
  }
  *value = result;
  return true;
}

static bool append_byte(char *output, size_t capacity, size_t *length,
                        uint8_t byte) {
  if (output != NULL) {
    if (*length + 1 >= capacity)
      return false;
    output[*length] = (char)byte;
  }
  *length += 1;
  return true;
}

static bool append_codepoint(char *output, size_t capacity, size_t *length,
                             uint32_t codepoint) {
  if (codepoint == 0U)
    return false;
  if (codepoint <= 0x7FU) {
    return append_byte(output, capacity, length, (uint8_t)codepoint);
  }
  if (codepoint <= 0x7FFU) {
    return append_byte(output, capacity, length,
                       (uint8_t)(0xC0U | (codepoint >> 6))) &&
           append_byte(output, capacity, length,
                       (uint8_t)(0x80U | (codepoint & 0x3FU)));
  }
  if (codepoint >= 0xD800U && codepoint <= 0xDFFFU)
    return false;
  if (codepoint <= 0xFFFFU) {
    return append_byte(output, capacity, length,
                       (uint8_t)(0xE0U | (codepoint >> 12))) &&
           append_byte(output, capacity, length,
                       (uint8_t)(0x80U | ((codepoint >> 6) & 0x3FU))) &&
           append_byte(output, capacity, length,
                       (uint8_t)(0x80U | (codepoint & 0x3FU)));
  }
  if (codepoint <= 0x10FFFFU) {
    return append_byte(output, capacity, length,
                       (uint8_t)(0xF0U | (codepoint >> 18))) &&
           append_byte(output, capacity, length,
                       (uint8_t)(0x80U | ((codepoint >> 12) & 0x3FU))) &&
           append_byte(output, capacity, length,
                       (uint8_t)(0x80U | ((codepoint >> 6) & 0x3FU))) &&
           append_byte(output, capacity, length,
                       (uint8_t)(0x80U | (codepoint & 0x3FU)));
  }
  return false;
}

static bool copy_utf8_sequence(parser_t *parser, char *output, size_t capacity,
                               size_t *output_length) {
  const uint8_t first = (uint8_t)parser->bytes[parser->position];
  size_t count = 0;
  uint32_t codepoint = 0;
  uint32_t minimum = 0;
  if (first >= 0xC2U && first <= 0xDFU) {
    count = 2;
    codepoint = first & 0x1FU;
    minimum = 0x80U;
  } else if (first >= 0xE0U && first <= 0xEFU) {
    count = 3;
    codepoint = first & 0x0FU;
    minimum = 0x800U;
  } else if (first >= 0xF0U && first <= 0xF4U) {
    count = 4;
    codepoint = first & 0x07U;
    minimum = 0x10000U;
  } else {
    return false;
  }
  if (parser->length - parser->position < count)
    return false;
  for (size_t i = 1; i < count; ++i) {
    uint8_t continuation = (uint8_t)parser->bytes[parser->position + i];
    if ((continuation & 0xC0U) != 0x80U)
      return false;
    codepoint = (codepoint << 6) | (continuation & 0x3FU);
  }
  if (codepoint < minimum || codepoint > 0x10FFFFU ||
      (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!append_byte(output, capacity, output_length,
                     (uint8_t)parser->bytes[parser->position + i])) {
      return false;
    }
  }
  parser->position += count;
  return true;
}

static bool parse_string(parser_t *parser, char *output, size_t capacity) {
  size_t output_length = 0;
  if (!take(parser, '"'))
    return false;
  while (parser->position < parser->length) {
    uint8_t value = (uint8_t)parser->bytes[parser->position++];
    if (value == '"') {
      if (output != NULL)
        output[output_length] = '\0';
      return true;
    }
    if (value < 0x20U)
      return false;
    if (value >= 0x80U) {
      parser->position -= 1;
      if (!copy_utf8_sequence(parser, output, capacity, &output_length))
        return false;
      continue;
    }
    if (value != '\\') {
      if (!append_byte(output, capacity, &output_length, value))
        return false;
      continue;
    }
    if (parser->position >= parser->length)
      return false;
    value = (uint8_t)parser->bytes[parser->position++];
    switch (value) {
    case '"':
    case '\\':
    case '/':
      if (!append_byte(output, capacity, &output_length, value))
        return false;
      break;
    case 'b':
      if (!append_byte(output, capacity, &output_length, '\b'))
        return false;
      break;
    case 'f':
      if (!append_byte(output, capacity, &output_length, '\f'))
        return false;
      break;
    case 'n':
      if (!append_byte(output, capacity, &output_length, '\n'))
        return false;
      break;
    case 'r':
      if (!append_byte(output, capacity, &output_length, '\r'))
        return false;
      break;
    case 't':
      if (!append_byte(output, capacity, &output_length, '\t'))
        return false;
      break;
    case 'u': {
      uint32_t high = 0;
      if (!parse_hex_quad(parser, &high))
        return false;
      if (high >= 0xD800U && high <= 0xDBFFU) {
        uint32_t low = 0;
        if (parser->length - parser->position < 6 ||
            parser->bytes[parser->position] != '\\' ||
            parser->bytes[parser->position + 1] != 'u')
          return false;
        parser->position += 2;
        if (!parse_hex_quad(parser, &low) || low < 0xDC00U || low > 0xDFFFU)
          return false;
        high = 0x10000U + ((high - 0xD800U) << 10) + (low - 0xDC00U);
      } else if (high >= 0xDC00U && high <= 0xDFFFU) {
        return false;
      }
      if (!append_codepoint(output, capacity, &output_length, high))
        return false;
      break;
    }
    default:
      return false;
    }
  }
  return false;
}

static bool parse_object_key(parser_t *parser, char *output, size_t capacity) {
  const size_t start = parser->position;
  if (parse_string(parser, output, capacity))
    return true;
  parser->position = start;
  if (!parse_string(parser, NULL, 0))
    return false;
  output[0] = '\0';
  return true;
}

static bool parse_uint(parser_t *parser, uint64_t *value) {
  uint64_t result = 0;
  size_t start;
  skip_space(parser);
  start = parser->position;
  if (start >= parser->length || parser->bytes[start] < '0' ||
      parser->bytes[start] > '9')
    return false;
  if (parser->bytes[start] == '0' && start + 1 < parser->length &&
      parser->bytes[start + 1] >= '0' && parser->bytes[start + 1] <= '9')
    return false;
  while (parser->position < parser->length) {
    char digit = parser->bytes[parser->position];
    if (digit < '0' || digit > '9')
      break;
    if (result > (UINT64_MAX - (uint64_t)(digit - '0')) / 10U)
      return false;
    result = result * 10U + (uint64_t)(digit - '0');
    parser->position += 1;
  }
  *value = result;
  return parser->position > start;
}

static bool parse_int32(parser_t *parser, int32_t *value) {
  bool negative = false;
  uint64_t magnitude = 0;
  skip_space(parser);
  if (parser->position < parser->length &&
      parser->bytes[parser->position] == '-') {
    negative = true;
    parser->position += 1U;
  }
  if (!parse_uint(parser, &magnitude))
    return false;
  if ((!negative && magnitude > INT32_MAX) ||
      (negative && magnitude > (uint64_t)INT32_MAX + 1U))
    return false;
  *value = negative ? (magnitude == (uint64_t)INT32_MAX + 1U
                           ? INT32_MIN
                           : -(int32_t)magnitude)
                    : (int32_t)magnitude;
  return true;
}

static bool skip_number(parser_t *parser) {
  size_t start;
  skip_space(parser);
  start = parser->position;
  if (parser->position < parser->length &&
      parser->bytes[parser->position] == '-')
    parser->position += 1;
  if (parser->position >= parser->length)
    return false;
  if (parser->bytes[parser->position] == '0') {
    parser->position += 1;
  } else {
    if (parser->bytes[parser->position] < '1' ||
        parser->bytes[parser->position] > '9')
      return false;
    while (parser->position < parser->length &&
           parser->bytes[parser->position] >= '0' &&
           parser->bytes[parser->position] <= '9')
      parser->position += 1;
  }
  if (parser->position < parser->length &&
      parser->bytes[parser->position] == '.') {
    parser->position += 1;
    if (parser->position >= parser->length ||
        parser->bytes[parser->position] < '0' ||
        parser->bytes[parser->position] > '9')
      return false;
    while (parser->position < parser->length &&
           parser->bytes[parser->position] >= '0' &&
           parser->bytes[parser->position] <= '9')
      parser->position += 1;
  }
  if (parser->position < parser->length &&
      (parser->bytes[parser->position] == 'e' ||
       parser->bytes[parser->position] == 'E')) {
    parser->position += 1;
    if (parser->position < parser->length &&
        (parser->bytes[parser->position] == '+' ||
         parser->bytes[parser->position] == '-'))
      parser->position += 1;
    if (parser->position >= parser->length ||
        parser->bytes[parser->position] < '0' ||
        parser->bytes[parser->position] > '9')
      return false;
    while (parser->position < parser->length &&
           parser->bytes[parser->position] >= '0' &&
           parser->bytes[parser->position] <= '9')
      parser->position += 1;
  }
  return parser->position > start;
}

static bool skip_value(parser_t *parser, unsigned depth);

static bool skip_array(parser_t *parser, unsigned depth) {
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return true;
  for (;;) {
    if (!skip_value(parser, depth + 1))
      return false;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool skip_object(parser_t *parser, unsigned depth) {
  if (!take(parser, '{'))
    return false;
  skip_space(parser);
  if (take(parser, '}'))
    return true;
  for (;;) {
    if (!parse_string(parser, NULL, 0) || !take(parser, ':') ||
        !skip_value(parser, depth + 1))
      return false;
    skip_space(parser);
    if (take(parser, '}'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool take_literal(parser_t *parser, const char *literal) {
  const size_t length = strlen(literal);
  skip_space(parser);
  if (parser->length - parser->position < length ||
      memcmp(parser->bytes + parser->position, literal, length) != 0)
    return false;
  parser->position += length;
  return true;
}

static bool skip_value(parser_t *parser, unsigned depth) {
  if (depth > 8)
    return false;
  skip_space(parser);
  if (parser->position >= parser->length)
    return false;
  switch (parser->bytes[parser->position]) {
  case '"':
    return parse_string(parser, NULL, 0);
  case '{':
    return depth < 8 && skip_object(parser, depth);
  case '[':
    return depth < 8 && skip_array(parser, depth);
  case 't':
    return take_literal(parser, "true");
  case 'f':
    return take_literal(parser, "false");
  case 'n':
    return take_literal(parser, "null");
  default:
    return skip_number(parser);
  }
}

static bool bounded_length(const char *value, size_t capacity, size_t *length) {
  const char *terminator = memchr(value, '\0', capacity);
  if (terminator == NULL)
    return false;
  *length = (size_t)(terminator - value);
  return true;
}

static bool valid_request_id(const char *request_id) {
  size_t length = 0;
  if (!bounded_length(request_id, NEXTING_DEVICE_REQUEST_ID_CAPACITY, &length) ||
      length == 0)
    return false;
  for (size_t i = 0; i < length; ++i) {
    const char value = request_id[i];
    const bool allowed = (value >= 'A' && value <= 'Z') ||
                         (value >= 'a' && value <= 'z') ||
                         (value >= '0' && value <= '9') || value == '.' ||
                         value == '_' || value == ':' || value == '-';
    if (!allowed)
      return false;
  }
  return true;
}

static bool valid_utf8_cstring(const char *value, size_t capacity) {
  size_t length = 0;
  if (!bounded_length(value, capacity, &length))
    return false;
  parser_t parser = {value, length, 0};
  size_t ignored = 0;
  while (parser.position < parser.length) {
    const uint8_t byte = (uint8_t)parser.bytes[parser.position];
    if (byte == 0)
      return false;
    if (byte < 0x80U) {
      parser.position += 1;
    } else if (!copy_utf8_sequence(&parser, NULL, 0, &ignored)) {
      return false;
    }
  }
  return true;
}

static bool valid_status_label(const char *label) {
  size_t length = 0;
  if (!bounded_length(label, NEXTING_DEVICE_STATUS_LABEL_CAPACITY, &length) ||
      length == 0)
    return false;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t byte = (uint8_t)label[i];
    if (byte < 0x20U || byte == 0x7FU)
      return false;
  }
  return valid_utf8_cstring(label, NEXTING_DEVICE_STATUS_LABEL_CAPACITY);
}

static bool valid_status_agents(const nexting_device_message_t *message) {
  if (message->agent_count > NEXTING_DEVICE_STATUS_MAX_AGENTS)
    return false;
  unsigned seen_slots = 0;
  for (size_t i = 0; i < message->agent_count; ++i) {
    const nexting_device_agent_status_t *agent = &message->agents[i];
    if (agent->slot > 7U || agent->state == NEXTING_DEVICE_AGENT_STATE_NONE)
      return false;
    const unsigned bit = 1U << agent->slot;
    if ((seen_slots & bit) != 0U)
      return false;
    seen_slots |= bit;
    if (agent->has_label && !valid_status_label(agent->label))
      return false;
  }
  return true;
}

static nexting_device_choice_t parse_choice(const char *value) {
  if (strcmp(value, "allow") == 0)
    return NEXTING_DEVICE_CHOICE_ALLOW;
  if (strcmp(value, "deny") == 0)
    return NEXTING_DEVICE_CHOICE_DENY;
  return NEXTING_DEVICE_CHOICE_NONE;
}

static nexting_device_resolution_t parse_resolution(const char *value) {
  if (strcmp(value, "answered") == 0)
    return NEXTING_DEVICE_RESOLUTION_ANSWERED;
  if (strcmp(value, "expired") == 0)
    return NEXTING_DEVICE_RESOLUTION_EXPIRED;
  if (strcmp(value, "cancelled") == 0)
    return NEXTING_DEVICE_RESOLUTION_CANCELLED;
  if (strcmp(value, "replaced") == 0)
    return NEXTING_DEVICE_RESOLUTION_REPLACED;
  return NEXTING_DEVICE_RESOLUTION_NONE;
}

static nexting_device_error_code_t parse_error(const char *value) {
  if (strcmp(value, "bad_message") == 0)
    return NEXTING_DEVICE_ERROR_BAD_MESSAGE;
  if (strcmp(value, "message_too_large") == 0)
    return NEXTING_DEVICE_ERROR_MESSAGE_TOO_LARGE;
  if (strcmp(value, "unsupported_version") == 0)
    return NEXTING_DEVICE_ERROR_UNSUPPORTED_VERSION;
  if (strcmp(value, "unsupported_profile") == 0)
    return NEXTING_DEVICE_ERROR_UNSUPPORTED_PROFILE;
  if (strcmp(value, "unknown_request") == 0)
    return NEXTING_DEVICE_ERROR_UNKNOWN_REQUEST;
  if (strcmp(value, "not_authorized") == 0)
    return NEXTING_DEVICE_ERROR_NOT_AUTHORIZED;
  if (strcmp(value, "busy") == 0)
    return NEXTING_DEVICE_ERROR_BUSY;
  return NEXTING_DEVICE_ERROR_NONE;
}

static bool parse_options(parser_t *parser) {
  char first[8];
  char second[8];
  if (!take(parser, '[') || !parse_string(parser, first, sizeof first) ||
      !take(parser, ',') || !parse_string(parser, second, sizeof second) ||
      !take(parser, ']'))
    return false;
  return strcmp(first, "allow") == 0 && strcmp(second, "deny") == 0;
}

static nexting_device_agent_state_t parse_agent_state(const char *value) {
  if (strcmp(value, "idle") == 0)
    return NEXTING_DEVICE_AGENT_STATE_IDLE;
  if (strcmp(value, "thinking") == 0)
    return NEXTING_DEVICE_AGENT_STATE_THINKING;
  if (strcmp(value, "working") == 0)
    return NEXTING_DEVICE_AGENT_STATE_WORKING;
  if (strcmp(value, "complete") == 0)
    return NEXTING_DEVICE_AGENT_STATE_COMPLETE;
  if (strcmp(value, "needs_input") == 0)
    return NEXTING_DEVICE_AGENT_STATE_NEEDS_INPUT;
  if (strcmp(value, "error") == 0)
    return NEXTING_DEVICE_AGENT_STATE_ERROR;
  return NEXTING_DEVICE_AGENT_STATE_NONE;
}

static bool parse_agent(parser_t *parser, nexting_device_agent_status_t *agent) {
  nexting_device_agent_status_t parsed = {0};
  bool has_slot = false;
  bool has_state = false;
  if (!take(parser, '{'))
    return false;
  skip_space(parser);
  if (take(parser, '}'))
    return false;
  for (;;) {
    char key[16];
    if (!parse_object_key(parser, key, sizeof key) || !take(parser, ':'))
      return false;
    if (strcmp(key, "slot") == 0) {
      uint64_t slot = 0;
      if (!parse_uint(parser, &slot) || slot > 7U)
        return false;
      parsed.slot = (uint8_t)slot;
      has_slot = true;
    } else if (strcmp(key, "state") == 0) {
      char state[16];
      if (!parse_string(parser, state, sizeof state))
        return false;
      parsed.state = parse_agent_state(state);
      has_state = true;
    } else if (strcmp(key, "label") == 0) {
      if (!parse_string(parser, parsed.label, sizeof parsed.label))
        return false;
      parsed.has_label = true;
    } else if (!skip_value(parser, 1)) {
      return false;
    }
    skip_space(parser);
    if (take(parser, '}'))
      break;
    if (!take(parser, ','))
      return false;
  }
  if (!has_slot || !has_state)
    return false;
  *agent = parsed;
  return true;
}

static bool parse_agents(parser_t *parser, nexting_device_message_t *message) {
  if (!take(parser, '['))
    return false;
  message->agent_count = 0;
  skip_space(parser);
  if (take(parser, ']'))
    return true;
  for (;;) {
    if (message->agent_count >= NEXTING_DEVICE_STATUS_MAX_AGENTS)
      return false;
    if (!parse_agent(parser, &message->agents[message->agent_count]))
      return false;
    message->agent_count += 1;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool parse_boolean(parser_t *parser, bool *value);

static bool valid_profile_text(const char *value, size_t capacity,
                               bool allow_layout, bool allow_empty) {
  size_t length = 0;
  if (!bounded_length(value, capacity, &length) ||
      (!allow_empty && length == 0U) || !valid_utf8_cstring(value, capacity))
    return false;
  for (size_t index = 0; index < length; ++index) {
    const uint8_t byte = (uint8_t)value[index];
    if (byte == 0x7FU || (byte < 0x20U &&
                          !(allow_layout && (byte == '\n' || byte == '\t'))))
      return false;
  }
  return true;
}

static nexting_device_direction_t parse_direction(const char *value) {
  if (strcmp(value, "prev") == 0)
    return NEXTING_DEVICE_DIRECTION_PREV;
  if (strcmp(value, "next") == 0)
    return NEXTING_DEVICE_DIRECTION_NEXT;
  if (strcmp(value, "up") == 0)
    return NEXTING_DEVICE_DIRECTION_UP;
  if (strcmp(value, "down") == 0)
    return NEXTING_DEVICE_DIRECTION_DOWN;
  if (strcmp(value, "left") == 0)
    return NEXTING_DEVICE_DIRECTION_LEFT;
  if (strcmp(value, "right") == 0)
    return NEXTING_DEVICE_DIRECTION_RIGHT;
  return NEXTING_DEVICE_DIRECTION_NONE;
}

static nexting_device_nav_resolution_t
parse_nav_resolution(const char *value) {
  if (strcmp(value, "selected") == 0)
    return NEXTING_DEVICE_NAV_RESOLUTION_SELECTED;
  if (strcmp(value, "cancelled") == 0)
    return NEXTING_DEVICE_NAV_RESOLUTION_CANCELLED;
  if (strcmp(value, "expired") == 0)
    return NEXTING_DEVICE_NAV_RESOLUTION_EXPIRED;
  if (strcmp(value, "replaced") == 0)
    return NEXTING_DEVICE_NAV_RESOLUTION_REPLACED;
  return NEXTING_DEVICE_NAV_RESOLUTION_NONE;
}

static nexting_device_gesture_t parse_gesture(const char *value) {
  if (strcmp(value, "press") == 0)
    return NEXTING_DEVICE_GESTURE_PRESS;
  if (strcmp(value, "release") == 0)
    return NEXTING_DEVICE_GESTURE_RELEASE;
  if (strcmp(value, "hold") == 0)
    return NEXTING_DEVICE_GESTURE_HOLD;
  if (strcmp(value, "double") == 0)
    return NEXTING_DEVICE_GESTURE_DOUBLE;
  return NEXTING_DEVICE_GESTURE_NONE;
}

static nexting_device_light_t parse_light(const char *value) {
  if (strcmp(value, "off") == 0)
    return NEXTING_DEVICE_LIGHT_OFF;
  if (strcmp(value, "dim") == 0)
    return NEXTING_DEVICE_LIGHT_DIM;
  if (strcmp(value, "solid") == 0)
    return NEXTING_DEVICE_LIGHT_SOLID;
  if (strcmp(value, "pulse") == 0)
    return NEXTING_DEVICE_LIGHT_PULSE;
  return NEXTING_DEVICE_LIGHT_NONE;
}

static nexting_device_voice_event_t parse_voice_event(const char *value) {
  if (strcmp(value, "start") == 0)
    return NEXTING_DEVICE_VOICE_EVENT_START;
  if (strcmp(value, "stop") == 0)
    return NEXTING_DEVICE_VOICE_EVENT_STOP;
  if (strcmp(value, "cancel") == 0)
    return NEXTING_DEVICE_VOICE_EVENT_CANCEL;
  return NEXTING_DEVICE_VOICE_EVENT_NONE;
}

static nexting_device_voice_state_t parse_voice_state(const char *value) {
  if (strcmp(value, "idle") == 0)
    return NEXTING_DEVICE_VOICE_IDLE;
  if (strcmp(value, "listening") == 0)
    return NEXTING_DEVICE_VOICE_LISTENING;
  if (strcmp(value, "transcribing") == 0)
    return NEXTING_DEVICE_VOICE_TRANSCRIBING;
  if (strcmp(value, "submitted") == 0)
    return NEXTING_DEVICE_VOICE_SUBMITTED;
  if (strcmp(value, "error") == 0)
    return NEXTING_DEVICE_VOICE_ERROR;
  return NEXTING_DEVICE_VOICE_NONE;
}

static nexting_device_config_status_t parse_config_status(const char *value) {
  if (strcmp(value, "applied") == 0)
    return NEXTING_DEVICE_CONFIG_APPLIED;
  if (strcmp(value, "rejected") == 0)
    return NEXTING_DEVICE_CONFIG_REJECTED;
  return NEXTING_DEVICE_CONFIG_STATUS_NONE;
}

static nexting_device_config_error_t parse_config_error(const char *value) {
  if (strcmp(value, "unknown_key") == 0)
    return NEXTING_DEVICE_CONFIG_UNKNOWN_KEY;
  if (strcmp(value, "invalid_value") == 0)
    return NEXTING_DEVICE_CONFIG_INVALID_VALUE;
  if (strcmp(value, "storage_error") == 0)
    return NEXTING_DEVICE_CONFIG_STORAGE_ERROR;
  if (strcmp(value, "unsupported") == 0)
    return NEXTING_DEVICE_CONFIG_UNSUPPORTED;
  return NEXTING_DEVICE_CONFIG_ERROR_NONE;
}

static bool parse_navigation_items(parser_t *parser,
                                   nexting_device_message_t *message) {
  nexting_device_navigation_payload_t *navigation =
      &message->interaction.navigation;
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return false;
  for (;;) {
    if (navigation->item_count >= NEXTING_DEVICE_NAV_MAX_ITEMS ||
        !parse_string(parser, navigation->items[navigation->item_count],
                      sizeof navigation->items[navigation->item_count]) ||
        !valid_profile_text(navigation->items[navigation->item_count],
                            sizeof navigation->items[navigation->item_count],
                            false, false))
      return false;
    for (size_t index = 0; index < navigation->item_count; ++index) {
      if (strcmp(navigation->items[index],
                 navigation->items[navigation->item_count]) == 0)
        return false;
    }
    navigation->item_count += 1U;
    skip_space(parser);
    if (take(parser, ']'))
      return navigation->item_count >= 2U;
    if (!take(parser, ','))
      return false;
  }
}

static bool parse_rgb(parser_t *parser, uint8_t rgb[3]) {
  if (!take(parser, '['))
    return false;
  for (size_t index = 0; index < 3U; ++index) {
    uint64_t value = 0;
    if (!parse_uint(parser, &value) || value > 255U)
      return false;
    rgb[index] = (uint8_t)value;
    if (index < 2U && !take(parser, ','))
      return false;
  }
  return take(parser, ']');
}

static bool parse_key_presentation(parser_t *parser,
                                   nexting_device_key_presentation_t *key) {
  bool has_slot = false;
  bool has_label = false;
  bool has_enabled = false;
  bool has_light = false;
  if (!take(parser, '{'))
    return false;
  for (;;) {
    char field[16];
    if (!parse_object_key(parser, field, sizeof field) || !take(parser, ':'))
      return false;
    if (strcmp(field, "slot") == 0) {
      uint64_t value = 0;
      if (has_slot || !parse_uint(parser, &value) || value > 63U)
        return false;
      key->slot = (uint8_t)value;
      has_slot = true;
    } else if (strcmp(field, "label") == 0) {
      if (has_label ||
          !parse_string(parser, key->label, sizeof key->label) ||
          !valid_profile_text(key->label, sizeof key->label, false, false))
        return false;
      has_label = true;
    } else if (strcmp(field, "enabled") == 0) {
      if (has_enabled || !parse_boolean(parser, &key->enabled))
        return false;
      has_enabled = true;
    } else if (strcmp(field, "light") == 0) {
      char value[16];
      if (has_light || !parse_string(parser, value, sizeof value))
        return false;
      key->light = parse_light(value);
      has_light = true;
    } else if (strcmp(field, "rgb") == 0) {
      if (key->has_rgb || !parse_rgb(parser, key->rgb))
        return false;
      key->has_rgb = true;
    } else {
      return false;
    }
    skip_space(parser);
    if (take(parser, '}'))
      break;
    if (!take(parser, ','))
      return false;
  }
  return has_slot && has_label && has_enabled && has_light &&
         key->light != NEXTING_DEVICE_LIGHT_NONE;
}

static bool parse_keymap(parser_t *parser, nexting_device_message_t *message) {
  nexting_device_keymap_payload_t *keymap = &message->interaction.keymap;
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return true;
  for (;;) {
    if (keymap->key_count >= NEXTING_DEVICE_KEYS_MAX ||
        !parse_key_presentation(parser, &keymap->keys[keymap->key_count]))
      return false;
    for (size_t index = 0; index < keymap->key_count; ++index) {
      if (keymap->keys[index].slot == keymap->keys[keymap->key_count].slot)
        return false;
    }
    keymap->key_count += 1U;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool parse_rotary_control(parser_t *parser,
                                 nexting_device_rotary_control_t *control) {
  bool has_slot = false;
  bool has_label = false;
  bool has_value = false;
  bool has_minimum = false;
  bool has_maximum = false;
  bool has_wrap = false;
  if (!take(parser, '{'))
    return false;
  for (;;) {
    char field[16];
    if (!parse_object_key(parser, field, sizeof field) || !take(parser, ':'))
      return false;
    if (strcmp(field, "slot") == 0) {
      uint64_t value = 0;
      if (has_slot || !parse_uint(parser, &value) || value > 15U)
        return false;
      control->slot = (uint8_t)value;
      has_slot = true;
    } else if (strcmp(field, "label") == 0) {
      if (has_label ||
          !parse_string(parser, control->label, sizeof control->label) ||
          !valid_profile_text(control->label, sizeof control->label, false,
                              false))
        return false;
      has_label = true;
    } else if (strcmp(field, "value") == 0) {
      if (has_value || !parse_int32(parser, &control->value))
        return false;
      has_value = true;
    } else if (strcmp(field, "min") == 0) {
      if (has_minimum || !parse_int32(parser, &control->minimum))
        return false;
      has_minimum = true;
    } else if (strcmp(field, "max") == 0) {
      if (has_maximum || !parse_int32(parser, &control->maximum))
        return false;
      has_maximum = true;
    } else if (strcmp(field, "wrap") == 0) {
      if (has_wrap || !parse_boolean(parser, &control->wrap))
        return false;
      has_wrap = true;
    } else {
      return false;
    }
    skip_space(parser);
    if (take(parser, '}'))
      break;
    if (!take(parser, ','))
      return false;
  }
  return has_slot && has_label && has_value && has_minimum && has_maximum &&
         has_wrap && control->minimum >= -1000000 &&
         control->maximum <= 1000000 &&
         control->minimum <= control->value &&
         control->value <= control->maximum;
}

static bool parse_rotary_map(parser_t *parser,
                             nexting_device_message_t *message) {
  nexting_device_rotary_map_payload_t *map = &message->interaction.rotary_map;
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return true;
  for (;;) {
    if (map->control_count >= NEXTING_DEVICE_ROTARY_MAX ||
        !parse_rotary_control(parser, &map->controls[map->control_count]))
      return false;
    for (size_t index = 0; index < map->control_count; ++index) {
      if (map->controls[index].slot == map->controls[map->control_count].slot)
        return false;
    }
    map->control_count += 1U;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool valid_config_key(const char *value) {
  size_t length = 0;
  if (!bounded_length(value, NEXTING_DEVICE_CONFIG_KEY_CAPACITY, &length) ||
      length == 0U)
    return false;
  for (size_t index = 0; index < length; ++index) {
    const char byte = value[index];
    const bool alpha_numeric =
        (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
        (byte >= '0' && byte <= '9');
    if (!alpha_numeric &&
        (index == 0U || (byte != '.' && byte != '_' && byte != '-')))
      return false;
  }
  return true;
}

static bool parse_config_value(parser_t *parser,
                               nexting_device_config_entry_t *entry) {
  const size_t start = parser->position;
  if (parse_boolean(parser, &entry->boolean_value)) {
    entry->type = NEXTING_DEVICE_CONFIG_BOOLEAN;
    return true;
  }
  parser->position = start;
  if (parse_int32(parser, &entry->integer_value)) {
    if (entry->integer_value < -1000000 || entry->integer_value > 1000000)
      return false;
    entry->type = NEXTING_DEVICE_CONFIG_INTEGER;
    return true;
  }
  parser->position = start;
  if (parse_string(parser, entry->string_value, sizeof entry->string_value) &&
      valid_profile_text(entry->string_value, sizeof entry->string_value,
                         false, true)) {
    entry->type = NEXTING_DEVICE_CONFIG_STRING;
    return true;
  }
  return false;
}

static bool parse_config_entry(parser_t *parser,
                               nexting_device_config_entry_t *entry) {
  bool has_key = false;
  bool has_value = false;
  if (!take(parser, '{'))
    return false;
  for (;;) {
    char field[16];
    if (!parse_object_key(parser, field, sizeof field) || !take(parser, ':'))
      return false;
    if (strcmp(field, "key") == 0) {
      if (has_key || !parse_string(parser, entry->key, sizeof entry->key))
        return false;
      has_key = true;
    } else if (strcmp(field, "value") == 0) {
      if (has_value || !parse_config_value(parser, entry))
        return false;
      has_value = true;
    } else {
      return false;
    }
    skip_space(parser);
    if (take(parser, '}'))
      break;
    if (!take(parser, ','))
      return false;
  }
  return has_key && has_value && valid_config_key(entry->key);
}

static bool parse_config_entries(parser_t *parser,
                                 nexting_device_message_t *message) {
  nexting_device_config_payload_t *config = &message->interaction.config;
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return true;
  for (;;) {
    if (config->entry_count >= NEXTING_DEVICE_CONFIG_MAX_ENTRIES ||
        !parse_config_entry(parser, &config->entries[config->entry_count]))
      return false;
    for (size_t index = 0; index < config->entry_count; ++index) {
      if (strcmp(config->entries[index].key,
                 config->entries[config->entry_count].key) == 0)
        return false;
    }
    config->entry_count += 1U;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool claim_field(uint64_t *fields, uint64_t field) {
  if ((*fields & field) != 0U)
    return false;
  *fields |= field;
  return true;
}

static bool parse_known_field(parser_t *parser, const char *key,
                              uint64_t *fields, uint64_t *version, char *type,
                              size_t type_capacity,
                              nexting_device_message_t *message) {
  char value[32];
  uint64_t number = 0;
  if (strcmp(key, "v") == 0) {
    return claim_field(fields, FIELD_V) && parse_uint(parser, version);
  }
  if (strcmp(key, "t") == 0) {
    return claim_field(fields, FIELD_T) &&
           parse_string(parser, type, type_capacity);
  }
  if (strcmp(key, "id") == 0) {
    if (!claim_field(fields, FIELD_ID) ||
        !parse_string(parser, message->request_id, sizeof message->request_id))
      return false;
    message->has_request_id = true;
    return true;
  }
  if (strcmp(key, "sum") == 0) {
    return claim_field(fields, FIELD_SUM) &&
           parse_string(parser, message->summary, sizeof message->summary);
  }
  if (strcmp(key, "opt") == 0) {
    return claim_field(fields, FIELD_OPT) && parse_options(parser);
  }
  if (strcmp(key, "ttl") == 0) {
    if (!claim_field(fields, FIELD_TTL) || !parse_uint(parser, &number) ||
        number > UINT32_MAX)
      return false;
    message->ttl_ms = (uint32_t)number;
    return true;
  }
  if (strcmp(key, "ch") == 0) {
    if (!claim_field(fields, FIELD_CH) ||
        !parse_string(parser, value, sizeof value))
      return false;
    message->choice = parse_choice(value);
    return true;
  }
  if (strcmp(key, "r") == 0) {
    if (!claim_field(fields, FIELD_R) ||
        !parse_string(parser, value, sizeof value))
      return false;
    message->resolution = parse_resolution(value);
    message->interaction.navigation.resolution = parse_nav_resolution(value);
    return true;
  }
  if (strcmp(key, "code") == 0) {
    if (!claim_field(fields, FIELD_CODE) ||
        !parse_string(parser, value, sizeof value))
      return false;
    message->error_code = parse_error(value);
    message->interaction.config_error = parse_config_error(value);
    return true;
  }
  if (strcmp(key, "agents") == 0) {
    return claim_field(fields, FIELD_AGENTS) && parse_agents(parser, message);
  }
  if (strcmp(key, "items") == 0) {
    return claim_field(fields, FIELD_ITEMS) &&
           parse_navigation_items(parser, message);
  }
  if (strcmp(key, "cursor") == 0 || strcmp(key, "index") == 0 ||
      strcmp(key, "slot") == 0 || strcmp(key, "channel") == 0) {
    const uint64_t field = strcmp(key, "cursor") == 0
                               ? FIELD_CURSOR
                               : strcmp(key, "index") == 0
                                     ? FIELD_INDEX
                                     : strcmp(key, "slot") == 0 ? FIELD_SLOT
                                                                : FIELD_CHANNEL;
    if (!claim_field(fields, field) || !parse_uint(parser, &number) ||
        number > UINT8_MAX)
      return false;
    if (field == FIELD_CURSOR)
      message->interaction.navigation.cursor = (uint8_t)number;
    else if (field == FIELD_INDEX)
      message->interaction.navigation.index = (uint8_t)number;
    else if (field == FIELD_SLOT)
      message->interaction.slot = (uint8_t)number;
    else
      message->interaction.channel = (uint8_t)number;
    return true;
  }
  if (strcmp(key, "dir") == 0) {
    if (!claim_field(fields, FIELD_DIR) ||
        !parse_string(parser, value, sizeof value))
      return false;
    message->interaction.navigation.direction = parse_direction(value);
    return true;
  }
  if (strcmp(key, "seq") == 0 || strcmp(key, "rev") == 0) {
    const uint64_t field = strcmp(key, "seq") == 0 ? FIELD_SEQ : FIELD_REV;
    if (!claim_field(fields, field) || !parse_uint(parser, &number) ||
        number > UINT32_MAX)
      return false;
    if (field == FIELD_SEQ)
      message->interaction.sequence = (uint32_t)number;
    else
      message->interaction.revision = (uint32_t)number;
    return true;
  }
  if (strcmp(key, "keys") == 0) {
    return claim_field(fields, FIELD_KEYS) && parse_keymap(parser, message);
  }
  if (strcmp(key, "event") == 0) {
    if (!claim_field(fields, FIELD_EVENT) ||
        !parse_string(parser, value, sizeof value))
      return false;
    message->interaction.gesture = parse_gesture(value);
    message->interaction.voice_event = parse_voice_event(value);
    return true;
  }
  if (strcmp(key, "controls") == 0) {
    return claim_field(fields, FIELD_CONTROLS) &&
           parse_rotary_map(parser, message);
  }
  if (strcmp(key, "delta") == 0) {
    int32_t delta = 0;
    if (!claim_field(fields, FIELD_DELTA) || !parse_int32(parser, &delta) ||
        delta < INT16_MIN || delta > INT16_MAX)
      return false;
    message->interaction.delta = (int16_t)delta;
    return true;
  }
  if (strcmp(key, "state") == 0) {
    if (!claim_field(fields, FIELD_STATE) ||
        !parse_string(parser, value, sizeof value))
      return false;
    message->interaction.voice_state = parse_voice_state(value);
    return true;
  }
  if (strcmp(key, "label") == 0) {
    if (!claim_field(fields, FIELD_LABEL) ||
        !parse_string(parser, message->interaction.label,
                      sizeof message->interaction.label))
      return false;
    message->interaction.has_label = true;
    return true;
  }
  if (strcmp(key, "title") == 0) {
    if (!claim_field(fields, FIELD_TITLE) ||
        !parse_string(parser, message->interaction.text.title,
                      sizeof message->interaction.text.title))
      return false;
    message->interaction.text.has_title = true;
    return true;
  }
  if (strcmp(key, "content") == 0) {
    return claim_field(fields, FIELD_CONTENT) &&
           parse_string(parser, message->interaction.text.content,
                        sizeof message->interaction.text.content);
  }
  if (strcmp(key, "model") == 0) {
    return claim_field(fields, FIELD_MODEL) &&
           parse_string(parser, message->interaction.usage.model,
                        sizeof message->interaction.usage.model);
  }
  if (strcmp(key, "input_tokens") == 0 ||
      strcmp(key, "output_tokens") == 0 ||
      strcmp(key, "cached_tokens") == 0 ||
      strcmp(key, "context_used") == 0 ||
      strcmp(key, "context_limit") == 0) {
    const uint64_t field =
        strcmp(key, "input_tokens") == 0
            ? FIELD_INPUT_TOKENS
            : strcmp(key, "output_tokens") == 0
                  ? FIELD_OUTPUT_TOKENS
                  : strcmp(key, "cached_tokens") == 0
                        ? FIELD_CACHED_TOKENS
                        : strcmp(key, "context_used") == 0
                              ? FIELD_CONTEXT_USED
                              : FIELD_CONTEXT_LIMIT;
    if (!claim_field(fields, field) || !parse_uint(parser, &number) ||
        number > UINT64_C(9007199254740991))
      return false;
    if (field == FIELD_INPUT_TOKENS)
      message->interaction.usage.input_tokens = number;
    else if (field == FIELD_OUTPUT_TOKENS)
      message->interaction.usage.output_tokens = number;
    else if (field == FIELD_CACHED_TOKENS) {
      message->interaction.usage.cached_tokens = number;
      message->interaction.usage.has_cached_tokens = true;
    } else if (field == FIELD_CONTEXT_USED) {
      message->interaction.usage.context_used = number;
      message->interaction.usage.has_context = true;
    } else {
      message->interaction.usage.context_limit = number;
    }
    return true;
  }
  if (strcmp(key, "entries") == 0) {
    return claim_field(fields, FIELD_ENTRIES) &&
           parse_config_entries(parser, message);
  }
  if (strcmp(key, "status") == 0) {
    if (!claim_field(fields, FIELD_STATUS) ||
        !parse_string(parser, value, sizeof value))
      return false;
    message->interaction.config_status = parse_config_status(value);
    return true;
  }
  *fields |= FIELD_UNKNOWN;
  return skip_value(parser, 0);
}

static bool validate_message(nexting_device_message_t *message,
                             const char *type, uint64_t fields,
                             uint64_t version) {
  if ((fields & (FIELD_V | FIELD_T)) != (FIELD_V | FIELD_T) || version != 1)
    return false;
  if ((fields & FIELD_ID) != 0U && !valid_request_id(message->request_id))
    return false;
  if (strcmp(type, "present") == 0) {
    const unsigned required = FIELD_ID | FIELD_SUM | FIELD_OPT | FIELD_TTL;
    if ((fields & required) != required || message->ttl_ms < 1 ||
        message->ttl_ms > NEXTING_DEVICE_MAX_TTL_MS)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_PRESENT;
    return true;
  }
  if (strcmp(type, "answer") == 0) {
    if ((fields & (FIELD_ID | FIELD_CH)) != (FIELD_ID | FIELD_CH) ||
        message->choice == NEXTING_DEVICE_CHOICE_NONE)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_ANSWER;
    return true;
  }
  if (strcmp(type, "resolved") == 0) {
    if ((fields & (FIELD_ID | FIELD_R)) != (FIELD_ID | FIELD_R) ||
        message->resolution == NEXTING_DEVICE_RESOLUTION_NONE)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_RESOLVED;
    return true;
  }
  if (strcmp(type, "error") == 0) {
    if ((fields & FIELD_CODE) == 0U ||
        message->error_code == NEXTING_DEVICE_ERROR_NONE)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_ERROR;
    return true;
  }
  if (strcmp(type, "status") == 0) {
    if ((fields & FIELD_AGENTS) == 0U || !valid_status_agents(message))
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_STATUS;
    return true;
  }
  if (strcmp(type, "nav_present") == 0) {
    const uint64_t allowed = FIELD_V | FIELD_T | FIELD_ID | FIELD_ITEMS |
                             FIELD_CURSOR | FIELD_TTL;
    if (fields != allowed ||
        message->interaction.navigation.item_count < 2U ||
        message->interaction.navigation.cursor >=
            message->interaction.navigation.item_count ||
        message->ttl_ms < 1U ||
        message->ttl_ms > NEXTING_DEVICE_MAX_TTL_MS)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_NAV_PRESENT;
    return true;
  }
  if (strcmp(type, "nav_move") == 0) {
    const uint64_t allowed =
        FIELD_V | FIELD_T | FIELD_ID | FIELD_DIR | FIELD_SEQ;
    if (fields != allowed ||
        message->interaction.navigation.direction ==
            NEXTING_DEVICE_DIRECTION_NONE)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_NAV_MOVE;
    return true;
  }
  if (strcmp(type, "nav_select") == 0) {
    const uint64_t allowed =
        FIELD_V | FIELD_T | FIELD_ID | FIELD_INDEX | FIELD_SEQ;
    if (fields != allowed || message->interaction.navigation.index > 7U)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_NAV_SELECT;
    return true;
  }
  if (strcmp(type, "nav_resolved") == 0) {
    const uint64_t allowed = FIELD_V | FIELD_T | FIELD_ID | FIELD_R;
    if (fields != allowed ||
        message->interaction.navigation.resolution ==
            NEXTING_DEVICE_NAV_RESOLUTION_NONE)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_NAV_RESOLVED;
    return true;
  }
  if (strcmp(type, "keymap") == 0) {
    if (fields != (FIELD_V | FIELD_T | FIELD_REV | FIELD_KEYS))
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_KEYMAP;
    return true;
  }
  if (strcmp(type, "key_event") == 0) {
    if (fields != (FIELD_V | FIELD_T | FIELD_SLOT | FIELD_EVENT | FIELD_SEQ) ||
        message->interaction.slot > 63U ||
        message->interaction.gesture == NEXTING_DEVICE_GESTURE_NONE)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_KEY_EVENT;
    return true;
  }
  if (strcmp(type, "rotary_map") == 0) {
    if (fields != (FIELD_V | FIELD_T | FIELD_REV | FIELD_CONTROLS))
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_ROTARY_MAP;
    return true;
  }
  if (strcmp(type, "rotary_event") == 0) {
    if (fields !=
            (FIELD_V | FIELD_T | FIELD_SLOT | FIELD_DELTA | FIELD_SEQ) ||
        message->interaction.slot > 15U || message->interaction.delta == 0 ||
        message->interaction.delta < -127 || message->interaction.delta > 127)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_ROTARY_EVENT;
    return true;
  }
  if (strcmp(type, "rotary_press") == 0) {
    if (fields !=
            (FIELD_V | FIELD_T | FIELD_SLOT | FIELD_EVENT | FIELD_SEQ) ||
        message->interaction.slot > 15U ||
        message->interaction.gesture == NEXTING_DEVICE_GESTURE_NONE)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_ROTARY_PRESS;
    return true;
  }
  if (strcmp(type, "voice_event") == 0) {
    if (fields != (FIELD_V | FIELD_T | FIELD_EVENT | FIELD_SEQ) ||
        message->interaction.voice_event == NEXTING_DEVICE_VOICE_EVENT_NONE)
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_VOICE_EVENT;
    return true;
  }
  if (strcmp(type, "voice_state") == 0) {
    const uint64_t required = FIELD_V | FIELD_T | FIELD_STATE;
    const uint64_t allowed = required | FIELD_LABEL;
    if ((fields & required) != required || (fields & ~allowed) != 0U ||
        message->interaction.voice_state == NEXTING_DEVICE_VOICE_NONE ||
        (message->interaction.has_label &&
         !valid_profile_text(message->interaction.label,
                             sizeof message->interaction.label, false, false)))
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_VOICE_STATE;
    return true;
  }
  if (strcmp(type, "text") == 0) {
    const uint64_t required = FIELD_V | FIELD_T | FIELD_CHANNEL | FIELD_CONTENT;
    const uint64_t allowed = required | FIELD_TITLE;
    if ((fields & required) != required || (fields & ~allowed) != 0U ||
        message->interaction.channel > 7U ||
        !valid_profile_text(message->interaction.text.content,
                            sizeof message->interaction.text.content, true,
                            true) ||
        (message->interaction.text.has_title &&
         !valid_profile_text(message->interaction.text.title,
                             sizeof message->interaction.text.title, false,
                             false)))
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_TEXT;
    return true;
  }
  if (strcmp(type, "usage") == 0) {
    const uint64_t required =
        FIELD_V | FIELD_T | FIELD_MODEL | FIELD_INPUT_TOKENS |
        FIELD_OUTPUT_TOKENS;
    const uint64_t allowed = required | FIELD_CACHED_TOKENS |
                             FIELD_CONTEXT_USED | FIELD_CONTEXT_LIMIT;
    const bool context_pair =
        (fields & (FIELD_CONTEXT_USED | FIELD_CONTEXT_LIMIT)) == 0U ||
        (fields & (FIELD_CONTEXT_USED | FIELD_CONTEXT_LIMIT)) ==
            (FIELD_CONTEXT_USED | FIELD_CONTEXT_LIMIT);
    if ((fields & required) != required || (fields & ~allowed) != 0U ||
        !context_pair ||
        !valid_profile_text(message->interaction.usage.model,
                            sizeof message->interaction.usage.model, false,
                            false) ||
        ((fields & FIELD_CONTEXT_USED) != 0U &&
         message->interaction.usage.context_used >
             message->interaction.usage.context_limit))
      return false;
    message->interaction.usage.has_context =
        (fields & FIELD_CONTEXT_USED) != 0U;
    message->type = NEXTING_DEVICE_MESSAGE_USAGE;
    return true;
  }
  if (strcmp(type, "usage_clear") == 0) {
    if (fields != (FIELD_V | FIELD_T))
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_USAGE_CLEAR;
    return true;
  }
  if (strcmp(type, "config") == 0) {
    if (fields != (FIELD_V | FIELD_T | FIELD_REV | FIELD_ENTRIES))
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_CONFIG;
    return true;
  }
  if (strcmp(type, "config_result") == 0) {
    const uint64_t required = FIELD_V | FIELD_T | FIELD_REV | FIELD_STATUS;
    const uint64_t allowed = required | FIELD_CODE;
    if ((fields & required) != required || (fields & ~allowed) != 0U ||
        message->interaction.config_status ==
            NEXTING_DEVICE_CONFIG_STATUS_NONE ||
        (message->interaction.config_status == NEXTING_DEVICE_CONFIG_APPLIED &&
         (fields & FIELD_CODE) != 0U) ||
        (message->interaction.config_status ==
             NEXTING_DEVICE_CONFIG_REJECTED &&
         ((fields & FIELD_CODE) == 0U ||
          message->interaction.config_error ==
              NEXTING_DEVICE_CONFIG_ERROR_NONE)))
      return false;
    message->type = NEXTING_DEVICE_MESSAGE_CONFIG_RESULT;
    return true;
  }
  return false;
}

nexting_device_result_t
nexting_device_decode(const char *wire, size_t wire_length,
                      nexting_device_message_t *output) {
  parser_t parser;
  nexting_device_message_t message = {0};
  char type[16] = {0};
  uint64_t fields = 0;
  uint64_t version = 0;
  if (wire == NULL || output == NULL || wire_length == 0)
    return NEXTING_DEVICE_BAD_MESSAGE;
  if (wire_length > NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES ||
      (wire_length == NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES &&
       wire[wire_length - 1] != '\n'))
    return NEXTING_DEVICE_MESSAGE_TOO_LARGE;
  if (wire[wire_length - 1] == '\n')
    wire_length -= 1;
  if (wire_length == 0)
    return NEXTING_DEVICE_BAD_MESSAGE;
  for (size_t i = 0; i < wire_length; ++i) {
    if (wire[i] == '\n' || wire[i] == '\r')
      return NEXTING_DEVICE_BAD_MESSAGE;
  }
  parser.bytes = wire;
  parser.length = wire_length;
  parser.position = 0;
  if (!take(&parser, '{'))
    return NEXTING_DEVICE_BAD_MESSAGE;
  skip_space(&parser);
  if (take(&parser, '}'))
    return NEXTING_DEVICE_BAD_MESSAGE;
  for (;;) {
    char key[16];
    if (!parse_object_key(&parser, key, sizeof key) || !take(&parser, ':') ||
        !parse_known_field(&parser, key, &fields, &version, type, sizeof type,
                           &message))
      return NEXTING_DEVICE_BAD_MESSAGE;
    skip_space(&parser);
    if (take(&parser, '}'))
      break;
    if (!take(&parser, ','))
      return NEXTING_DEVICE_BAD_MESSAGE;
  }
  skip_space(&parser);
  if (parser.position != parser.length ||
      !validate_message(&message, type, fields, version))
    return NEXTING_DEVICE_BAD_MESSAGE;
  *output = message;
  return NEXTING_DEVICE_OK;
}

enum {
  DI_FIELD_PROTOCOL = 1U << 0,
  DI_FIELD_SPEC = 1U << 1,
  DI_FIELD_WIRE = 1U << 2,
  DI_FIELD_PROFILES = 1U << 3,
  DI_FIELD_MODEL = 1U << 4,
  DI_FIELD_FW = 1U << 5,
  DI_FIELD_MAX_MESSAGE = 1U << 6,
  DI_FIELD_MAX_SUMMARY = 1U << 7,
  DI_FIELD_STATUS_SLOTS = 1U << 8,
  DI_FIELD_DEVICE_ID = 1U << 9,
  DI_FIELD_MANUFACTURER = 1U << 10,
  DI_FIELD_DISPLAY_NAME = 1U << 11,
  DI_FIELD_SERIAL_NUMBER = 1U << 12,
  DI_FIELD_BUTTON_COUNT = 1U << 13,
  DI_FIELD_APPROVAL_BUTTON_COUNT = 1U << 14,
  DI_FIELD_CUSTOM_BUTTON_COUNT = 1U << 15,
  DI_FIELD_ROTARY_COUNT = 1U << 16,
  DI_FIELD_ROTARY_PRESS_COUNT = 1U << 17,
  DI_FIELD_BATTERY_SERVICE = 1U << 18,
  DI_FIELD_DISPLAY = 1U << 19,
  DI_FIELD_HAPTICS = 1U << 20,
  DI_FIELD_VENDOR = 1U << 21
};

static bool parse_boolean(parser_t *parser, bool *value) {
  if (take_literal(parser, "true")) {
    *value = true;
    return true;
  }
  if (take_literal(parser, "false")) {
    *value = false;
    return true;
  }
  return false;
}

static bool parse_wire_versions(parser_t *parser,
                                nexting_device_info_t *info) {
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return false;
  for (;;) {
    uint64_t value = 0;
    if (info->wire_version_count >= NEXTING_DEVICE_INFO_MAX_WIRE_VERSIONS ||
        !parse_uint(parser, &value) || value < 1U || value > UINT16_MAX)
      return false;
    info->wire_versions[info->wire_version_count++] = (uint16_t)value;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool parse_profiles(parser_t *parser, nexting_device_info_t *info) {
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return false;
  for (;;) {
    if (info->profile_count >= NEXTING_DEVICE_INFO_MAX_PROFILES ||
        !parse_string(parser, info->profiles[info->profile_count],
                      sizeof info->profiles[info->profile_count]))
      return false;
    info->profile_count += 1U;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool parse_haptics(parser_t *parser, nexting_device_info_t *info) {
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return false;
  for (;;) {
    if (info->haptic_count >= NEXTING_DEVICE_INFO_MAX_HAPTICS ||
        !parse_string(parser, info->haptics[info->haptic_count],
                      sizeof info->haptics[info->haptic_count]))
      return false;
    info->haptic_count += 1U;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool parse_display(parser_t *parser, nexting_device_info_t *info) {
  bool has_type = false;
  bool has_width = false;
  bool has_height = false;
  if (!take(parser, '{'))
    return false;
  skip_space(parser);
  if (take(parser, '}'))
    return false;
  for (;;) {
    char key[16];
    if (!parse_object_key(parser, key, sizeof key) || !take(parser, ':'))
      return false;
    if (strcmp(key, "type") == 0) {
      if (has_type ||
          !parse_string(parser, info->display_type,
                        sizeof info->display_type))
        return false;
      has_type = true;
    } else if (strcmp(key, "width") == 0 ||
               strcmp(key, "height") == 0) {
      uint64_t value = 0;
      bool *present = strcmp(key, "width") == 0 ? &has_width : &has_height;
      if (*present || !parse_uint(parser, &value) || value < 1U ||
          value > 4096U)
        return false;
      if (strcmp(key, "width") == 0)
        info->display_width = (uint16_t)value;
      else
        info->display_height = (uint16_t)value;
      *present = true;
    } else if (!skip_value(parser, 1)) {
      return false;
    }
    skip_space(parser);
    if (take(parser, '}'))
      break;
    if (!take(parser, ','))
      return false;
  }
  info->has_display = has_type && has_width && has_height;
  return info->has_display;
}

static bool valid_descriptor(const char *value, size_t capacity) {
  size_t length = 0;
  if (!bounded_length(value, capacity, &length) || length == 0 ||
      !valid_utf8_cstring(value, capacity))
    return false;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t byte = (uint8_t)value[i];
    if (byte < 0x20U || byte == 0x7FU)
      return false;
  }
  return true;
}

static bool valid_vendor_key(const char *value) {
  size_t length = 0;
  if (!bounded_length(value, NEXTING_DEVICE_INFO_VENDOR_KEY_CAPACITY,
                      &length) ||
      length == 0)
    return false;
  for (size_t i = 0; i < length; ++i) {
    const char byte = value[i];
    const bool alpha_numeric =
        (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
        (byte >= '0' && byte <= '9');
    if (!alpha_numeric && (i == 0U || (byte != '.' && byte != '_' &&
                                      byte != '-')))
      return false;
  }
  return true;
}

static bool valid_vendor_namespace(const char *value) {
  size_t length = 0;
  bool saw_dot = false;
  bool segment_start = true;
  if (!bounded_length(value, NEXTING_DEVICE_INFO_VENDOR_NAMESPACE_CAPACITY,
                      &length) ||
      length == 0 || !valid_descriptor(
                         value, NEXTING_DEVICE_INFO_VENDOR_NAMESPACE_CAPACITY))
    return false;
  for (size_t i = 0; i < length; ++i) {
    const char byte = value[i];
    if (byte == '.') {
      if (segment_start || i == length - 1U || value[i - 1U] == '-')
        return false;
      saw_dot = true;
      segment_start = true;
      continue;
    }
    if (!((byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
          (byte == '-' && !segment_start)))
      return false;
    segment_start = false;
  }
  return saw_dot && value[length - 1U] != '-';
}

static bool valid_inert_vendor_text(const char *value, size_t capacity) {
  size_t length = 0;
  if (!valid_descriptor(value, capacity) ||
      !bounded_length(value, capacity, &length))
    return false;
  for (size_t i = 0; i < length; ++i) {
    switch (value[i]) {
    case '<':
    case '>':
    case '`':
    case '*':
    case '_':
    case '#':
    case '[':
    case ']':
    case '(':
    case ')':
      return false;
    default:
      break;
    }
  }
  return strstr(value, "://") == NULL && strstr(value, "www.") == NULL;
}

static bool parse_vendor_value(parser_t *parser, char *output,
                               size_t capacity) {
  const size_t start = parser->position;
  uint64_t number = 0;
  if (parse_string(parser, output, capacity))
    return true;
  parser->position = start;
  if (!parse_uint(parser, &number))
    return false;
  const int written = snprintf(output, capacity, "%" PRIu64, number);
  return written > 0 && (size_t)written < capacity;
}

static bool parse_vendor_fact(parser_t *parser,
                              nexting_device_vendor_fact_t *fact) {
  bool has_key = false;
  bool has_label = false;
  bool has_value = false;
  if (!take(parser, '{'))
    return false;
  skip_space(parser);
  if (take(parser, '}'))
    return false;
  for (;;) {
    char key[16];
    if (!parse_object_key(parser, key, sizeof key) || !take(parser, ':'))
      return false;
    if (strcmp(key, "key") == 0) {
      if (has_key || !parse_string(parser, fact->key, sizeof fact->key))
        return false;
      has_key = true;
    } else if (strcmp(key, "label") == 0) {
      if (has_label ||
          !parse_string(parser, fact->label, sizeof fact->label))
        return false;
      has_label = true;
    } else if (strcmp(key, "value") == 0) {
      if (has_value ||
          !parse_vendor_value(parser, fact->value, sizeof fact->value))
        return false;
      has_value = true;
    } else if (!skip_value(parser, 2)) {
      return false;
    }
    skip_space(parser);
    if (take(parser, '}'))
      break;
    if (!take(parser, ','))
      return false;
  }
  return has_key && has_label && has_value && valid_vendor_key(fact->key) &&
         valid_inert_vendor_text(fact->label, sizeof fact->label) &&
         valid_inert_vendor_text(fact->value, sizeof fact->value);
}

static bool parse_vendor_facts(parser_t *parser,
                               nexting_device_info_t *info) {
  if (!take(parser, '['))
    return false;
  skip_space(parser);
  if (take(parser, ']'))
    return false;
  for (;;) {
    if (info->vendor_fact_count >= NEXTING_DEVICE_INFO_MAX_VENDOR_FACTS ||
        !parse_vendor_fact(parser,
                           &info->vendor_facts[info->vendor_fact_count]))
      return false;
    for (size_t i = 0; i < info->vendor_fact_count; ++i) {
      if (strcmp(info->vendor_facts[i].key,
                 info->vendor_facts[info->vendor_fact_count].key) == 0)
        return false;
    }
    info->vendor_fact_count += 1U;
    skip_space(parser);
    if (take(parser, ']'))
      return true;
    if (!take(parser, ','))
      return false;
  }
}

static bool parse_vendor(parser_t *parser, nexting_device_info_t *info) {
  bool has_namespace = false;
  bool has_facts = false;
  if (!take(parser, '{'))
    return false;
  skip_space(parser);
  if (take(parser, '}'))
    return false;
  for (;;) {
    char key[16];
    if (!parse_object_key(parser, key, sizeof key) || !take(parser, ':'))
      return false;
    if (strcmp(key, "namespace") == 0) {
      if (has_namespace ||
          !parse_string(parser, info->vendor_namespace,
                        sizeof info->vendor_namespace))
        return false;
      has_namespace = true;
    } else if (strcmp(key, "facts") == 0) {
      if (has_facts || !parse_vendor_facts(parser, info))
        return false;
      has_facts = true;
    } else if (!skip_value(parser, 1)) {
      return false;
    }
    skip_space(parser);
    if (take(parser, '}'))
      break;
    if (!take(parser, ','))
      return false;
  }
  info->has_vendor =
      has_namespace && has_facts && valid_vendor_namespace(info->vendor_namespace);
  return info->has_vendor;
}

static bool list_contains_wire(const nexting_device_info_t *info,
                               uint16_t value) {
  for (size_t i = 0; i < info->wire_version_count; ++i) {
    if (info->wire_versions[i] == value)
      return true;
  }
  return false;
}

static bool list_contains_profile(const nexting_device_info_t *info,
                                  const char *value) {
  for (size_t i = 0; i < info->profile_count; ++i) {
    if (strcmp(info->profiles[i], value) == 0)
      return true;
  }
  return false;
}

static bool unique_info_lists(const nexting_device_info_t *info) {
  for (size_t i = 0; i < info->wire_version_count; ++i) {
    for (size_t j = i + 1U; j < info->wire_version_count; ++j) {
      if (info->wire_versions[i] == info->wire_versions[j])
        return false;
    }
  }
  for (size_t i = 0; i < info->profile_count; ++i) {
    if (!valid_descriptor(info->profiles[i], sizeof info->profiles[i]))
      return false;
    for (size_t j = i + 1U; j < info->profile_count; ++j) {
      if (strcmp(info->profiles[i], info->profiles[j]) == 0)
        return false;
    }
  }
  for (size_t i = 0; i < info->haptic_count; ++i) {
    if (!valid_descriptor(info->haptics[i], sizeof info->haptics[i]))
      return false;
    for (size_t j = i + 1U; j < info->haptic_count; ++j) {
      if (strcmp(info->haptics[i], info->haptics[j]) == 0)
        return false;
    }
  }
  return true;
}

static bool valid_uuid(const char *value) {
  size_t length = 0;
  if (!bounded_length(value, NEXTING_DEVICE_INFO_UUID_CAPACITY, &length))
    return false;
  if (length == 0)
    return true;
  if (length != 36U)
    return false;
  for (size_t i = 0; i < length; ++i) {
    if (i == 8U || i == 13U || i == 18U || i == 23U) {
      if (value[i] != '-')
        return false;
      continue;
    }
    if (hex_value(value[i]) < 0)
      return false;
  }
  const int version = hex_value(value[14]);
  const char variant = value[19];
  return version >= 1 && version <= 5 &&
         (variant == '8' || variant == '9' || variant == 'a' ||
          variant == 'A' || variant == 'b' || variant == 'B');
}

static bool valid_optional_descriptor(const char *value, size_t capacity) {
  size_t length = 0;
  if (!bounded_length(value, capacity, &length))
    return false;
  return length == 0 || valid_descriptor(value, capacity);
}

static bool validate_device_info(nexting_device_info_t *info,
                                 uint32_t fields) {
  const uint32_t required =
      DI_FIELD_PROTOCOL | DI_FIELD_SPEC | DI_FIELD_WIRE | DI_FIELD_PROFILES |
      DI_FIELD_MODEL | DI_FIELD_FW | DI_FIELD_MAX_MESSAGE |
      DI_FIELD_MAX_SUMMARY;
  if ((fields & required) != required ||
      strcmp(info->protocol_name, "nexting-device") != 0 ||
      !valid_descriptor(info->spec, sizeof info->spec) ||
      !valid_descriptor(info->model, sizeof info->model) ||
      !valid_descriptor(info->firmware_version,
                        sizeof info->firmware_version) ||
      !valid_optional_descriptor(info->manufacturer,
                                 sizeof info->manufacturer) ||
      !valid_optional_descriptor(info->display_name,
                                 sizeof info->display_name) ||
      !valid_optional_descriptor(info->serial_number,
                                 sizeof info->serial_number) ||
      !valid_uuid(info->device_id) || info->wire_version_count == 0U ||
      info->profile_count == 0U || !unique_info_lists(info) ||
      !list_contains_wire(info, 1U) ||
      !list_contains_profile(info, "approval/1") ||
      info->max_message_bytes < 512U || info->max_summary_bytes < 1U ||
      info->max_summary_bytes > NEXTING_DEVICE_SUMMARY_CAPACITY - 1U ||
      info->status_slots > NEXTING_DEVICE_STATUS_MAX_AGENTS ||
      (info->status_slots > 0U &&
       !list_contains_profile(info, "status/1")) ||
      (info->has_approval_button_count && info->has_button_count &&
       info->approval_button_count > info->button_count) ||
      (info->has_custom_button_count && info->has_button_count &&
       info->custom_button_count > info->button_count) ||
      (info->has_rotary_press_count && info->has_rotary_count &&
       info->rotary_press_count > info->rotary_count))
    return false;
  info->supports_approval_v1 = true;
  info->supports_status_v1 =
      info->status_slots > 0U && list_contains_profile(info, "status/1");
  info->supports_navigation_v1 =
      list_contains_profile(info, "navigation/1");
  info->supports_keys_v1 = list_contains_profile(info, "keys/1");
  info->supports_rotary_v1 = list_contains_profile(info, "rotary/1");
  info->supports_voice_v1 = list_contains_profile(info, "voice/1");
  info->supports_text_v1 = list_contains_profile(info, "text/1");
  info->supports_usage_v1 = list_contains_profile(info, "usage/1");
  info->supports_config_v1 = list_contains_profile(info, "config/1");
  return true;
}

static bool claim_info_field(uint32_t *fields, uint32_t field) {
  if ((*fields & field) != 0U)
    return false;
  *fields |= field;
  return true;
}

static bool parse_device_info_field(parser_t *parser, const char *key,
                                    uint32_t *fields,
                                    nexting_device_info_t *info) {
  uint64_t number = 0;
#define CLAIM_INFO_FIELD(bit) claim_info_field(fields, (uint32_t)(bit))
  if (strcmp(key, "protocol") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_PROTOCOL) &&
           parse_string(parser, info->protocol_name,
                        sizeof info->protocol_name);
  if (strcmp(key, "spec") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_SPEC) &&
           parse_string(parser, info->spec, sizeof info->spec);
  if (strcmp(key, "wire") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_WIRE) &&
           parse_wire_versions(parser, info);
  if (strcmp(key, "profiles") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_PROFILES) &&
           parse_profiles(parser, info);
  if (strcmp(key, "model") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_MODEL) &&
           parse_string(parser, info->model, sizeof info->model);
  if (strcmp(key, "fw") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_FW) &&
           parse_string(parser, info->firmware_version,
                        sizeof info->firmware_version);
  if (strcmp(key, "max_message_bytes") == 0) {
    if (!CLAIM_INFO_FIELD(DI_FIELD_MAX_MESSAGE) ||
        !parse_uint(parser, &number) || number > UINT32_MAX)
      return false;
    info->max_message_bytes = (uint32_t)number;
    return true;
  }
  if (strcmp(key, "max_summary_bytes") == 0) {
    if (!CLAIM_INFO_FIELD(DI_FIELD_MAX_SUMMARY) ||
        !parse_uint(parser, &number) || number > UINT16_MAX)
      return false;
    info->max_summary_bytes = (uint16_t)number;
    return true;
  }
  if (strcmp(key, "statusSlots") == 0) {
    if (!CLAIM_INFO_FIELD(DI_FIELD_STATUS_SLOTS) ||
        !parse_uint(parser, &number) ||
        number > NEXTING_DEVICE_STATUS_MAX_AGENTS)
      return false;
    info->status_slots = (uint8_t)number;
    return true;
  }
  if (strcmp(key, "device_id") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_DEVICE_ID) &&
           parse_string(parser, info->device_id, sizeof info->device_id);
  if (strcmp(key, "manufacturer") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_MANUFACTURER) &&
           parse_string(parser, info->manufacturer,
                        sizeof info->manufacturer);
  if (strcmp(key, "display_name") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_DISPLAY_NAME) &&
           parse_string(parser, info->display_name,
                        sizeof info->display_name);
  if (strcmp(key, "serial_number") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_SERIAL_NUMBER) &&
           parse_string(parser, info->serial_number,
                        sizeof info->serial_number);
  if (strcmp(key, "button_count") == 0 ||
      strcmp(key, "approval_button_count") == 0 ||
      strcmp(key, "custom_button_count") == 0) {
    const uint32_t bit = strcmp(key, "button_count") == 0
                             ? DI_FIELD_BUTTON_COUNT
                             : strcmp(key, "approval_button_count") == 0
                                   ? DI_FIELD_APPROVAL_BUTTON_COUNT
                                   : DI_FIELD_CUSTOM_BUTTON_COUNT;
    if (!CLAIM_INFO_FIELD(bit) || !parse_uint(parser, &number) ||
        number > 1024U)
      return false;
    if (bit == DI_FIELD_BUTTON_COUNT) {
      info->has_button_count = true;
      info->button_count = (uint16_t)number;
    } else if (bit == DI_FIELD_APPROVAL_BUTTON_COUNT) {
      info->has_approval_button_count = true;
      info->approval_button_count = (uint16_t)number;
    } else {
      info->has_custom_button_count = true;
      info->custom_button_count = (uint16_t)number;
    }
    return true;
  }
  if (strcmp(key, "rotary_count") == 0 ||
      strcmp(key, "rotary_press_count") == 0) {
    const bool press = strcmp(key, "rotary_press_count") == 0;
    if (!CLAIM_INFO_FIELD(press ? DI_FIELD_ROTARY_PRESS_COUNT
                                : DI_FIELD_ROTARY_COUNT) ||
        !parse_uint(parser, &number) || number > 64U)
      return false;
    if (press) {
      info->has_rotary_press_count = true;
      info->rotary_press_count = (uint8_t)number;
    } else {
      info->has_rotary_count = true;
      info->rotary_count = (uint8_t)number;
    }
    return true;
  }
  if (strcmp(key, "battery_service") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_BATTERY_SERVICE) &&
           parse_boolean(parser, &info->has_battery_service);
  if (strcmp(key, "display") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_DISPLAY) &&
           parse_display(parser, info);
  if (strcmp(key, "haptics") == 0)
    return CLAIM_INFO_FIELD(DI_FIELD_HAPTICS) &&
           parse_haptics(parser, info);
  if (strcmp(key, "vendor") == 0) {
    const size_t start = parser->position;
    nexting_device_info_t candidate = {0};
    if (!CLAIM_INFO_FIELD(DI_FIELD_VENDOR))
      return false;
    if (parse_vendor(parser, &candidate) &&
        parser->position - start <= 1024U) {
      info->has_vendor = true;
      (void)strcpy(info->vendor_namespace, candidate.vendor_namespace);
      info->vendor_fact_count = candidate.vendor_fact_count;
      memcpy(info->vendor_facts, candidate.vendor_facts,
             candidate.vendor_fact_count * sizeof candidate.vendor_facts[0]);
      return true;
    }
    parser->position = start;
    info->has_vendor = false;
    info->vendor_fact_count = 0U;
    return skip_value(parser, 0);
  }
#undef CLAIM_INFO_FIELD
  return skip_value(parser, 0);
}

nexting_device_result_t
nexting_device_info_decode(const char *wire, size_t wire_length,
                           nexting_device_info_t *output) {
  parser_t parser;
  nexting_device_info_t info = {0};
  uint32_t fields = 0;
  if (wire == NULL || output == NULL || wire_length == 0U)
    return NEXTING_DEVICE_BAD_MESSAGE;
  if (wire_length > NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES)
    return NEXTING_DEVICE_MESSAGE_TOO_LARGE;
  parser.bytes = wire;
  parser.length = wire_length;
  parser.position = 0U;
  if (!take(&parser, '{'))
    return NEXTING_DEVICE_BAD_MESSAGE;
  skip_space(&parser);
  if (take(&parser, '}'))
    return NEXTING_DEVICE_BAD_MESSAGE;
  for (;;) {
    char key[32];
    if (!parse_object_key(&parser, key, sizeof key) || !take(&parser, ':') ||
        !parse_device_info_field(&parser, key, &fields, &info))
      return NEXTING_DEVICE_BAD_MESSAGE;
    skip_space(&parser);
    if (take(&parser, '}'))
      break;
    if (!take(&parser, ','))
      return NEXTING_DEVICE_BAD_MESSAGE;
  }
  skip_space(&parser);
  if (parser.position != parser.length ||
      !validate_device_info(&info, fields))
    return NEXTING_DEVICE_BAD_MESSAGE;
  *output = info;
  return NEXTING_DEVICE_OK;
}

static void write_byte(writer_t *writer, char value) {
  if (writer->length < writer->capacity)
    writer->bytes[writer->length] = value;
  else
    writer->overflow = true;
  writer->length += 1;
}

static void write_text(writer_t *writer, const char *value) {
  while (*value != '\0')
    write_byte(writer, *value++);
}

static void write_json_string(writer_t *writer, const char *value) {
  static const char hex[] = "0123456789abcdef";
  write_byte(writer, '"');
  for (const uint8_t *cursor = (const uint8_t *)value; *cursor != 0; ++cursor) {
    switch (*cursor) {
    case '"':
      write_text(writer, "\\\"");
      break;
    case '\\':
      write_text(writer, "\\\\");
      break;
    case '\b':
      write_text(writer, "\\b");
      break;
    case '\f':
      write_text(writer, "\\f");
      break;
    case '\n':
      write_text(writer, "\\n");
      break;
    case '\r':
      write_text(writer, "\\r");
      break;
    case '\t':
      write_text(writer, "\\t");
      break;
    default:
      if (*cursor < 0x20U) {
        write_text(writer, "\\u00");
        write_byte(writer, hex[*cursor >> 4]);
        write_byte(writer, hex[*cursor & 0x0FU]);
      } else {
        write_byte(writer, (char)*cursor);
      }
    }
  }
  write_byte(writer, '"');
}

static const char *choice_string(nexting_device_choice_t choice) {
  if (choice == NEXTING_DEVICE_CHOICE_ALLOW)
    return "allow";
  if (choice == NEXTING_DEVICE_CHOICE_DENY)
    return "deny";
  return NULL;
}

static const char *resolution_string(nexting_device_resolution_t resolution) {
  switch (resolution) {
  case NEXTING_DEVICE_RESOLUTION_ANSWERED:
    return "answered";
  case NEXTING_DEVICE_RESOLUTION_EXPIRED:
    return "expired";
  case NEXTING_DEVICE_RESOLUTION_CANCELLED:
    return "cancelled";
  case NEXTING_DEVICE_RESOLUTION_REPLACED:
    return "replaced";
  default:
    return NULL;
  }
}

static const char *error_string(nexting_device_error_code_t error) {
  switch (error) {
  case NEXTING_DEVICE_ERROR_BAD_MESSAGE:
    return "bad_message";
  case NEXTING_DEVICE_ERROR_MESSAGE_TOO_LARGE:
    return "message_too_large";
  case NEXTING_DEVICE_ERROR_UNSUPPORTED_VERSION:
    return "unsupported_version";
  case NEXTING_DEVICE_ERROR_UNSUPPORTED_PROFILE:
    return "unsupported_profile";
  case NEXTING_DEVICE_ERROR_UNKNOWN_REQUEST:
    return "unknown_request";
  case NEXTING_DEVICE_ERROR_NOT_AUTHORIZED:
    return "not_authorized";
  case NEXTING_DEVICE_ERROR_BUSY:
    return "busy";
  default:
    return NULL;
  }
}

static const char *agent_state_string(nexting_device_agent_state_t state) {
  switch (state) {
  case NEXTING_DEVICE_AGENT_STATE_IDLE:
    return "idle";
  case NEXTING_DEVICE_AGENT_STATE_THINKING:
    return "thinking";
  case NEXTING_DEVICE_AGENT_STATE_WORKING:
    return "working";
  case NEXTING_DEVICE_AGENT_STATE_COMPLETE:
    return "complete";
  case NEXTING_DEVICE_AGENT_STATE_NEEDS_INPUT:
    return "needs_input";
  case NEXTING_DEVICE_AGENT_STATE_ERROR:
    return "error";
  default:
    return NULL;
  }
}

static const char *direction_string(nexting_device_direction_t direction) {
  switch (direction) {
  case NEXTING_DEVICE_DIRECTION_PREV:
    return "prev";
  case NEXTING_DEVICE_DIRECTION_NEXT:
    return "next";
  case NEXTING_DEVICE_DIRECTION_UP:
    return "up";
  case NEXTING_DEVICE_DIRECTION_DOWN:
    return "down";
  case NEXTING_DEVICE_DIRECTION_LEFT:
    return "left";
  case NEXTING_DEVICE_DIRECTION_RIGHT:
    return "right";
  default:
    return NULL;
  }
}

static const char *
nav_resolution_string(nexting_device_nav_resolution_t resolution) {
  switch (resolution) {
  case NEXTING_DEVICE_NAV_RESOLUTION_SELECTED:
    return "selected";
  case NEXTING_DEVICE_NAV_RESOLUTION_CANCELLED:
    return "cancelled";
  case NEXTING_DEVICE_NAV_RESOLUTION_EXPIRED:
    return "expired";
  case NEXTING_DEVICE_NAV_RESOLUTION_REPLACED:
    return "replaced";
  default:
    return NULL;
  }
}

static const char *gesture_string(nexting_device_gesture_t gesture) {
  switch (gesture) {
  case NEXTING_DEVICE_GESTURE_PRESS:
    return "press";
  case NEXTING_DEVICE_GESTURE_RELEASE:
    return "release";
  case NEXTING_DEVICE_GESTURE_HOLD:
    return "hold";
  case NEXTING_DEVICE_GESTURE_DOUBLE:
    return "double";
  default:
    return NULL;
  }
}

static const char *light_string(nexting_device_light_t light) {
  switch (light) {
  case NEXTING_DEVICE_LIGHT_OFF:
    return "off";
  case NEXTING_DEVICE_LIGHT_DIM:
    return "dim";
  case NEXTING_DEVICE_LIGHT_SOLID:
    return "solid";
  case NEXTING_DEVICE_LIGHT_PULSE:
    return "pulse";
  default:
    return NULL;
  }
}

static const char *
voice_event_string(nexting_device_voice_event_t voice_event) {
  switch (voice_event) {
  case NEXTING_DEVICE_VOICE_EVENT_START:
    return "start";
  case NEXTING_DEVICE_VOICE_EVENT_STOP:
    return "stop";
  case NEXTING_DEVICE_VOICE_EVENT_CANCEL:
    return "cancel";
  default:
    return NULL;
  }
}

static const char *
voice_state_string(nexting_device_voice_state_t voice_state) {
  switch (voice_state) {
  case NEXTING_DEVICE_VOICE_IDLE:
    return "idle";
  case NEXTING_DEVICE_VOICE_LISTENING:
    return "listening";
  case NEXTING_DEVICE_VOICE_TRANSCRIBING:
    return "transcribing";
  case NEXTING_DEVICE_VOICE_SUBMITTED:
    return "submitted";
  case NEXTING_DEVICE_VOICE_ERROR:
    return "error";
  default:
    return NULL;
  }
}

static const char *
config_status_string(nexting_device_config_status_t status) {
  if (status == NEXTING_DEVICE_CONFIG_APPLIED)
    return "applied";
  if (status == NEXTING_DEVICE_CONFIG_REJECTED)
    return "rejected";
  return NULL;
}

static const char *
config_error_string(nexting_device_config_error_t error) {
  switch (error) {
  case NEXTING_DEVICE_CONFIG_UNKNOWN_KEY:
    return "unknown_key";
  case NEXTING_DEVICE_CONFIG_INVALID_VALUE:
    return "invalid_value";
  case NEXTING_DEVICE_CONFIG_STORAGE_ERROR:
    return "storage_error";
  case NEXTING_DEVICE_CONFIG_UNSUPPORTED:
    return "unsupported";
  default:
    return NULL;
  }
}

static bool valid_message_for_encode(const nexting_device_message_t *message) {
  if (message == NULL)
    return false;
  switch (message->type) {
  case NEXTING_DEVICE_MESSAGE_PRESENT:
    return valid_request_id(message->request_id) &&
           valid_utf8_cstring(message->summary,
                              NEXTING_DEVICE_SUMMARY_CAPACITY) &&
           message->ttl_ms >= 1 &&
           message->ttl_ms <= NEXTING_DEVICE_MAX_TTL_MS;
  case NEXTING_DEVICE_MESSAGE_ANSWER:
    return valid_request_id(message->request_id) &&
           choice_string(message->choice) != NULL;
  case NEXTING_DEVICE_MESSAGE_RESOLVED:
    return valid_request_id(message->request_id) &&
           resolution_string(message->resolution) != NULL;
  case NEXTING_DEVICE_MESSAGE_ERROR:
    return (!message->has_request_id ||
            valid_request_id(message->request_id)) &&
           error_string(message->error_code) != NULL;
  case NEXTING_DEVICE_MESSAGE_STATUS:
    return valid_status_agents(message);
  case NEXTING_DEVICE_MESSAGE_NAV_PRESENT:
    if (!valid_request_id(message->request_id) ||
        message->interaction.navigation.item_count < 2U ||
        message->interaction.navigation.item_count >
            NEXTING_DEVICE_NAV_MAX_ITEMS ||
        message->interaction.navigation.cursor >=
            message->interaction.navigation.item_count ||
        message->ttl_ms < 1U ||
        message->ttl_ms > NEXTING_DEVICE_MAX_TTL_MS)
      return false;
    for (size_t index = 0;
         index < message->interaction.navigation.item_count; ++index) {
      if (!valid_profile_text(message->interaction.navigation.items[index],
                              sizeof message->interaction.navigation.items[index],
                              false, false))
        return false;
    }
    return true;
  case NEXTING_DEVICE_MESSAGE_NAV_MOVE:
    return valid_request_id(message->request_id) &&
           direction_string(message->interaction.navigation.direction) != NULL;
  case NEXTING_DEVICE_MESSAGE_NAV_SELECT:
    return valid_request_id(message->request_id) &&
           message->interaction.navigation.index <= 7U;
  case NEXTING_DEVICE_MESSAGE_NAV_RESOLVED:
    return valid_request_id(message->request_id) &&
           nav_resolution_string(
               message->interaction.navigation.resolution) != NULL;
  case NEXTING_DEVICE_MESSAGE_KEYMAP:
    if (message->interaction.keymap.key_count > NEXTING_DEVICE_KEYS_MAX)
      return false;
    for (size_t index = 0; index < message->interaction.keymap.key_count;
         ++index) {
      const nexting_device_key_presentation_t *key =
          &message->interaction.keymap.keys[index];
      if (key->slot > 63U ||
          !valid_profile_text(key->label, sizeof key->label, false, false) ||
          light_string(key->light) == NULL)
        return false;
      for (size_t previous = 0; previous < index; ++previous) {
        if (message->interaction.keymap.keys[previous].slot == key->slot)
          return false;
      }
    }
    return true;
  case NEXTING_DEVICE_MESSAGE_KEY_EVENT:
    return message->interaction.slot <= 63U &&
           gesture_string(message->interaction.gesture) != NULL;
  case NEXTING_DEVICE_MESSAGE_ROTARY_MAP:
    if (message->interaction.rotary_map.control_count >
        NEXTING_DEVICE_ROTARY_MAX)
      return false;
    for (size_t index = 0;
         index < message->interaction.rotary_map.control_count; ++index) {
      const nexting_device_rotary_control_t *control =
          &message->interaction.rotary_map.controls[index];
      if (control->slot > 15U ||
          !valid_profile_text(control->label, sizeof control->label, false,
                              false) ||
          control->minimum < -1000000 || control->maximum > 1000000 ||
          control->minimum > control->value ||
          control->value > control->maximum)
        return false;
      for (size_t previous = 0; previous < index; ++previous) {
        if (message->interaction.rotary_map.controls[previous].slot ==
            control->slot)
          return false;
      }
    }
    return true;
  case NEXTING_DEVICE_MESSAGE_ROTARY_EVENT:
    return message->interaction.slot <= 15U &&
           message->interaction.delta >= -127 &&
           message->interaction.delta <= 127 &&
           message->interaction.delta != 0;
  case NEXTING_DEVICE_MESSAGE_ROTARY_PRESS:
    return message->interaction.slot <= 15U &&
           gesture_string(message->interaction.gesture) != NULL;
  case NEXTING_DEVICE_MESSAGE_VOICE_EVENT:
    return voice_event_string(message->interaction.voice_event) != NULL;
  case NEXTING_DEVICE_MESSAGE_VOICE_STATE:
    return voice_state_string(message->interaction.voice_state) != NULL &&
           (!message->interaction.has_label ||
            valid_profile_text(message->interaction.label,
                               sizeof message->interaction.label, false,
                               false));
  case NEXTING_DEVICE_MESSAGE_TEXT:
    return message->interaction.channel <= 7U &&
           (!message->interaction.text.has_title ||
            valid_profile_text(message->interaction.text.title,
                               sizeof message->interaction.text.title, false,
                               false)) &&
           valid_profile_text(message->interaction.text.content,
                              sizeof message->interaction.text.content, true,
                              true);
  case NEXTING_DEVICE_MESSAGE_USAGE:
    return valid_profile_text(message->interaction.usage.model,
                              sizeof message->interaction.usage.model, false,
                              false) &&
           message->interaction.usage.input_tokens <=
               UINT64_C(9007199254740991) &&
           message->interaction.usage.output_tokens <=
               UINT64_C(9007199254740991) &&
           (!message->interaction.usage.has_cached_tokens ||
            message->interaction.usage.cached_tokens <=
                UINT64_C(9007199254740991)) &&
           (!message->interaction.usage.has_context ||
            (message->interaction.usage.context_used <=
                 message->interaction.usage.context_limit &&
             message->interaction.usage.context_limit <=
                 UINT64_C(9007199254740991)));
  case NEXTING_DEVICE_MESSAGE_USAGE_CLEAR:
    return true;
  case NEXTING_DEVICE_MESSAGE_CONFIG:
    if (message->interaction.config.entry_count >
        NEXTING_DEVICE_CONFIG_MAX_ENTRIES)
      return false;
    for (size_t index = 0; index < message->interaction.config.entry_count;
         ++index) {
      const nexting_device_config_entry_t *entry =
          &message->interaction.config.entries[index];
      if (!valid_config_key(entry->key) ||
          (entry->type == NEXTING_DEVICE_CONFIG_INTEGER &&
           (entry->integer_value < -1000000 ||
            entry->integer_value > 1000000)) ||
          (entry->type == NEXTING_DEVICE_CONFIG_STRING &&
           !valid_profile_text(entry->string_value,
                               sizeof entry->string_value, false, true)) ||
          (entry->type != NEXTING_DEVICE_CONFIG_BOOLEAN &&
           entry->type != NEXTING_DEVICE_CONFIG_INTEGER &&
           entry->type != NEXTING_DEVICE_CONFIG_STRING))
        return false;
      for (size_t previous = 0; previous < index; ++previous) {
        if (strcmp(message->interaction.config.entries[previous].key,
                   entry->key) == 0)
          return false;
      }
    }
    return true;
  case NEXTING_DEVICE_MESSAGE_CONFIG_RESULT:
    return config_status_string(message->interaction.config_status) != NULL &&
           ((message->interaction.config_status ==
                 NEXTING_DEVICE_CONFIG_APPLIED &&
             message->interaction.config_error ==
                 NEXTING_DEVICE_CONFIG_ERROR_NONE) ||
            (message->interaction.config_status ==
                 NEXTING_DEVICE_CONFIG_REJECTED &&
             config_error_string(message->interaction.config_error) != NULL));
  default:
    return false;
  }
}

nexting_device_result_t
nexting_device_encode(const nexting_device_message_t *message, char *output,
                      size_t output_capacity, size_t *output_length) {
  writer_t writer = {output, output_capacity, 0, false};
  char number[32];
  if (output == NULL || output_length == NULL ||
      !valid_message_for_encode(message))
    return NEXTING_DEVICE_BAD_MESSAGE;
  switch (message->type) {
  case NEXTING_DEVICE_MESSAGE_PRESENT:
    write_text(&writer, "{\"v\":1,\"t\":\"present\",\"id\":");
    write_json_string(&writer, message->request_id);
    write_text(&writer, ",\"sum\":");
    write_json_string(&writer, message->summary);
    write_text(&writer, ",\"opt\":[\"allow\",\"deny\"],\"ttl\":");
    (void)snprintf(number, sizeof number, "%u", message->ttl_ms);
    write_text(&writer, number);
    break;
  case NEXTING_DEVICE_MESSAGE_ANSWER:
    write_text(&writer, "{\"v\":1,\"t\":\"answer\",\"id\":");
    write_json_string(&writer, message->request_id);
    write_text(&writer, ",\"ch\":");
    write_json_string(&writer, choice_string(message->choice));
    break;
  case NEXTING_DEVICE_MESSAGE_RESOLVED:
    write_text(&writer, "{\"v\":1,\"t\":\"resolved\",\"id\":");
    write_json_string(&writer, message->request_id);
    write_text(&writer, ",\"r\":");
    write_json_string(&writer, resolution_string(message->resolution));
    break;
  case NEXTING_DEVICE_MESSAGE_ERROR:
    write_text(&writer, "{\"v\":1,\"t\":\"error\"");
    if (message->has_request_id) {
      write_text(&writer, ",\"id\":");
      write_json_string(&writer, message->request_id);
    }
    write_text(&writer, ",\"code\":");
    write_json_string(&writer, error_string(message->error_code));
    break;
  case NEXTING_DEVICE_MESSAGE_STATUS:
    write_text(&writer, "{\"v\":1,\"t\":\"status\",\"agents\":[");
    for (size_t i = 0; i < message->agent_count; ++i) {
      const nexting_device_agent_status_t *agent = &message->agents[i];
      if (i != 0)
        write_byte(&writer, ',');
      write_text(&writer, "{\"slot\":");
      (void)snprintf(number, sizeof number, "%u", (unsigned)agent->slot);
      write_text(&writer, number);
      write_text(&writer, ",\"state\":");
      write_json_string(&writer, agent_state_string(agent->state));
      if (agent->has_label) {
        write_text(&writer, ",\"label\":");
        write_json_string(&writer, agent->label);
      }
      write_byte(&writer, '}');
    }
    write_byte(&writer, ']');
    break;
  case NEXTING_DEVICE_MESSAGE_NAV_PRESENT:
    write_text(&writer, "{\"v\":1,\"t\":\"nav_present\",\"id\":");
    write_json_string(&writer, message->request_id);
    write_text(&writer, ",\"items\":[");
    for (size_t index = 0;
         index < message->interaction.navigation.item_count; ++index) {
      if (index != 0U)
        write_byte(&writer, ',');
      write_json_string(&writer, message->interaction.navigation.items[index]);
    }
    write_text(&writer, "],\"cursor\":");
    (void)snprintf(number, sizeof number, "%u",
                   (unsigned)message->interaction.navigation.cursor);
    write_text(&writer, number);
    write_text(&writer, ",\"ttl\":");
    (void)snprintf(number, sizeof number, "%u", message->ttl_ms);
    write_text(&writer, number);
    break;
  case NEXTING_DEVICE_MESSAGE_NAV_MOVE:
    write_text(&writer, "{\"v\":1,\"t\":\"nav_move\",\"id\":");
    write_json_string(&writer, message->request_id);
    write_text(&writer, ",\"dir\":");
    write_json_string(
        &writer, direction_string(message->interaction.navigation.direction));
    write_text(&writer, ",\"seq\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.sequence);
    write_text(&writer, number);
    break;
  case NEXTING_DEVICE_MESSAGE_NAV_SELECT:
    write_text(&writer, "{\"v\":1,\"t\":\"nav_select\",\"id\":");
    write_json_string(&writer, message->request_id);
    write_text(&writer, ",\"index\":");
    (void)snprintf(number, sizeof number, "%u",
                   (unsigned)message->interaction.navigation.index);
    write_text(&writer, number);
    write_text(&writer, ",\"seq\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.sequence);
    write_text(&writer, number);
    break;
  case NEXTING_DEVICE_MESSAGE_NAV_RESOLVED:
    write_text(&writer, "{\"v\":1,\"t\":\"nav_resolved\",\"id\":");
    write_json_string(&writer, message->request_id);
    write_text(&writer, ",\"r\":");
    write_json_string(
        &writer,
        nav_resolution_string(message->interaction.navigation.resolution));
    break;
  case NEXTING_DEVICE_MESSAGE_KEYMAP:
    write_text(&writer, "{\"v\":1,\"t\":\"keymap\",\"rev\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.revision);
    write_text(&writer, number);
    write_text(&writer, ",\"keys\":[");
    for (size_t index = 0; index < message->interaction.keymap.key_count;
         ++index) {
      const nexting_device_key_presentation_t *key =
          &message->interaction.keymap.keys[index];
      if (index != 0U)
        write_byte(&writer, ',');
      write_text(&writer, "{\"slot\":");
      (void)snprintf(number, sizeof number, "%u", (unsigned)key->slot);
      write_text(&writer, number);
      write_text(&writer, ",\"label\":");
      write_json_string(&writer, key->label);
      write_text(&writer, ",\"enabled\":");
      write_text(&writer, key->enabled ? "true" : "false");
      write_text(&writer, ",\"light\":");
      write_json_string(&writer, light_string(key->light));
      if (key->has_rgb) {
        write_text(&writer, ",\"rgb\":[");
        for (size_t component = 0; component < 3U; ++component) {
          if (component != 0U)
            write_byte(&writer, ',');
          (void)snprintf(number, sizeof number, "%u",
                         (unsigned)key->rgb[component]);
          write_text(&writer, number);
        }
        write_byte(&writer, ']');
      }
      write_byte(&writer, '}');
    }
    write_byte(&writer, ']');
    break;
  case NEXTING_DEVICE_MESSAGE_KEY_EVENT:
    write_text(&writer, "{\"v\":1,\"t\":\"key_event\",\"slot\":");
    (void)snprintf(number, sizeof number, "%u",
                   (unsigned)message->interaction.slot);
    write_text(&writer, number);
    write_text(&writer, ",\"event\":");
    write_json_string(&writer, gesture_string(message->interaction.gesture));
    write_text(&writer, ",\"seq\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.sequence);
    write_text(&writer, number);
    break;
  case NEXTING_DEVICE_MESSAGE_ROTARY_MAP:
    write_text(&writer, "{\"v\":1,\"t\":\"rotary_map\",\"rev\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.revision);
    write_text(&writer, number);
    write_text(&writer, ",\"controls\":[");
    for (size_t index = 0;
         index < message->interaction.rotary_map.control_count; ++index) {
      const nexting_device_rotary_control_t *control =
          &message->interaction.rotary_map.controls[index];
      if (index != 0U)
        write_byte(&writer, ',');
      write_text(&writer, "{\"slot\":");
      (void)snprintf(number, sizeof number, "%u", (unsigned)control->slot);
      write_text(&writer, number);
      write_text(&writer, ",\"label\":");
      write_json_string(&writer, control->label);
      write_text(&writer, ",\"value\":");
      (void)snprintf(number, sizeof number, "%" PRId32, control->value);
      write_text(&writer, number);
      write_text(&writer, ",\"min\":");
      (void)snprintf(number, sizeof number, "%" PRId32, control->minimum);
      write_text(&writer, number);
      write_text(&writer, ",\"max\":");
      (void)snprintf(number, sizeof number, "%" PRId32, control->maximum);
      write_text(&writer, number);
      write_text(&writer, ",\"wrap\":");
      write_text(&writer, control->wrap ? "true" : "false");
      write_byte(&writer, '}');
    }
    write_byte(&writer, ']');
    break;
  case NEXTING_DEVICE_MESSAGE_ROTARY_EVENT:
    write_text(&writer, "{\"v\":1,\"t\":\"rotary_event\",\"slot\":");
    (void)snprintf(number, sizeof number, "%u",
                   (unsigned)message->interaction.slot);
    write_text(&writer, number);
    write_text(&writer, ",\"delta\":");
    (void)snprintf(number, sizeof number, "%d",
                   (int)message->interaction.delta);
    write_text(&writer, number);
    write_text(&writer, ",\"seq\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.sequence);
    write_text(&writer, number);
    break;
  case NEXTING_DEVICE_MESSAGE_ROTARY_PRESS:
    write_text(&writer, "{\"v\":1,\"t\":\"rotary_press\",\"slot\":");
    (void)snprintf(number, sizeof number, "%u",
                   (unsigned)message->interaction.slot);
    write_text(&writer, number);
    write_text(&writer, ",\"event\":");
    write_json_string(&writer, gesture_string(message->interaction.gesture));
    write_text(&writer, ",\"seq\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.sequence);
    write_text(&writer, number);
    break;
  case NEXTING_DEVICE_MESSAGE_VOICE_EVENT:
    write_text(&writer, "{\"v\":1,\"t\":\"voice_event\",\"event\":");
    write_json_string(
        &writer, voice_event_string(message->interaction.voice_event));
    write_text(&writer, ",\"seq\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.sequence);
    write_text(&writer, number);
    break;
  case NEXTING_DEVICE_MESSAGE_VOICE_STATE:
    write_text(&writer, "{\"v\":1,\"t\":\"voice_state\",\"state\":");
    write_json_string(
        &writer, voice_state_string(message->interaction.voice_state));
    if (message->interaction.has_label) {
      write_text(&writer, ",\"label\":");
      write_json_string(&writer, message->interaction.label);
    }
    break;
  case NEXTING_DEVICE_MESSAGE_TEXT:
    write_text(&writer, "{\"v\":1,\"t\":\"text\",\"channel\":");
    (void)snprintf(number, sizeof number, "%u",
                   (unsigned)message->interaction.channel);
    write_text(&writer, number);
    if (message->interaction.text.has_title) {
      write_text(&writer, ",\"title\":");
      write_json_string(&writer, message->interaction.text.title);
    }
    write_text(&writer, ",\"content\":");
    write_json_string(&writer, message->interaction.text.content);
    break;
  case NEXTING_DEVICE_MESSAGE_USAGE:
    write_text(&writer, "{\"v\":1,\"t\":\"usage\",\"model\":");
    write_json_string(&writer, message->interaction.usage.model);
    write_text(&writer, ",\"input_tokens\":");
    (void)snprintf(number, sizeof number, "%" PRIu64,
                   message->interaction.usage.input_tokens);
    write_text(&writer, number);
    write_text(&writer, ",\"output_tokens\":");
    (void)snprintf(number, sizeof number, "%" PRIu64,
                   message->interaction.usage.output_tokens);
    write_text(&writer, number);
    if (message->interaction.usage.has_cached_tokens) {
      write_text(&writer, ",\"cached_tokens\":");
      (void)snprintf(number, sizeof number, "%" PRIu64,
                     message->interaction.usage.cached_tokens);
      write_text(&writer, number);
    }
    if (message->interaction.usage.has_context) {
      write_text(&writer, ",\"context_used\":");
      (void)snprintf(number, sizeof number, "%" PRIu64,
                     message->interaction.usage.context_used);
      write_text(&writer, number);
      write_text(&writer, ",\"context_limit\":");
      (void)snprintf(number, sizeof number, "%" PRIu64,
                     message->interaction.usage.context_limit);
      write_text(&writer, number);
    }
    break;
  case NEXTING_DEVICE_MESSAGE_USAGE_CLEAR:
    write_text(&writer, "{\"v\":1,\"t\":\"usage_clear\"");
    break;
  case NEXTING_DEVICE_MESSAGE_CONFIG:
    write_text(&writer, "{\"v\":1,\"t\":\"config\",\"rev\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.revision);
    write_text(&writer, number);
    write_text(&writer, ",\"entries\":[");
    for (size_t index = 0; index < message->interaction.config.entry_count;
         ++index) {
      const nexting_device_config_entry_t *entry =
          &message->interaction.config.entries[index];
      if (index != 0U)
        write_byte(&writer, ',');
      write_text(&writer, "{\"key\":");
      write_json_string(&writer, entry->key);
      write_text(&writer, ",\"value\":");
      if (entry->type == NEXTING_DEVICE_CONFIG_BOOLEAN) {
        write_text(&writer, entry->boolean_value ? "true" : "false");
      } else if (entry->type == NEXTING_DEVICE_CONFIG_INTEGER) {
        (void)snprintf(number, sizeof number, "%" PRId32,
                       entry->integer_value);
        write_text(&writer, number);
      } else {
        write_json_string(&writer, entry->string_value);
      }
      write_byte(&writer, '}');
    }
    write_byte(&writer, ']');
    break;
  case NEXTING_DEVICE_MESSAGE_CONFIG_RESULT:
    write_text(&writer, "{\"v\":1,\"t\":\"config_result\",\"rev\":");
    (void)snprintf(number, sizeof number, "%u",
                   message->interaction.revision);
    write_text(&writer, number);
    write_text(&writer, ",\"status\":");
    write_json_string(
        &writer,
        config_status_string(message->interaction.config_status));
    if (message->interaction.config_status == NEXTING_DEVICE_CONFIG_REJECTED) {
      write_text(&writer, ",\"code\":");
      write_json_string(
          &writer, config_error_string(message->interaction.config_error));
    }
    break;
  default:
    return NEXTING_DEVICE_BAD_MESSAGE;
  }
  write_text(&writer, "}\n");
  if (writer.length < writer.capacity)
    writer.bytes[writer.length] = '\0';
  else
    writer.overflow = true;
  *output_length = writer.length;
  return writer.overflow ? NEXTING_DEVICE_BUFFER_TOO_SMALL : NEXTING_DEVICE_OK;
}

void nexting_device_stream_init(nexting_device_stream_t *stream,
                                uint8_t *storage, size_t capacity) {
  if (stream == NULL)
    return;
  stream->storage = storage;
  stream->capacity = capacity;
  stream->length = 0;
  stream->discarding_oversize = false;
}

void nexting_device_stream_reset(nexting_device_stream_t *stream) {
  if (stream == NULL)
    return;
  stream->length = 0;
  stream->discarding_oversize = false;
}

nexting_device_result_t nexting_device_stream_push(
    nexting_device_stream_t *stream, const uint8_t *bytes, size_t length,
    nexting_device_message_handler_t handler, void *context) {
  nexting_device_result_t result = NEXTING_DEVICE_OK;
  if (stream == NULL || stream->storage == NULL || stream->capacity == 0 ||
      (bytes == NULL && length != 0))
    return NEXTING_DEVICE_BAD_MESSAGE;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t byte = bytes[i];
    if (stream->discarding_oversize) {
      result = NEXTING_DEVICE_MESSAGE_TOO_LARGE;
      if (byte == '\n')
        stream->discarding_oversize = false;
      continue;
    }
    if (byte == '\n') {
      nexting_device_message_t message = {0};
      nexting_device_result_t decoded = nexting_device_decode(
          (const char *)stream->storage, stream->length, &message);
      stream->length = 0;
      if (decoded == NEXTING_DEVICE_OK) {
        if (handler != NULL)
          handler(&message, context);
      } else if (result == NEXTING_DEVICE_OK) {
        result = decoded;
      }
      continue;
    }
    if (stream->length >= stream->capacity - 1U) {
      stream->length = 0;
      stream->discarding_oversize = true;
      result = NEXTING_DEVICE_MESSAGE_TOO_LARGE;
      continue;
    }
    stream->storage[stream->length++] = byte;
  }
  return result;
}

static uint64_t add_saturated(uint64_t value, uint64_t increment) {
  return value > UINT64_MAX - increment ? UINT64_MAX : value + increment;
}

void nexting_device_state_init(nexting_device_state_t *state) {
  if (state != NULL)
    memset(state, 0, sizeof *state);
}

nexting_device_result_t
nexting_device_state_on_present(nexting_device_state_t *state,
                                const nexting_device_message_t *message,
                                uint64_t now_ms) {
  if (state == NULL || message == NULL ||
      message->type != NEXTING_DEVICE_MESSAGE_PRESENT ||
      !valid_message_for_encode(message))
    return NEXTING_DEVICE_BAD_MESSAGE;
  state->phase = NEXTING_DEVICE_PHASE_PENDING;
  state->request = *message;
  state->deadline_ms = add_saturated(now_ms, message->ttl_ms);
  state->next_retry_ms = 0;
  state->chosen = NEXTING_DEVICE_CHOICE_NONE;
  return NEXTING_DEVICE_OK;
}

static void build_answer(const nexting_device_state_t *state,
                         nexting_device_message_t *answer) {
  memset(answer, 0, sizeof *answer);
  answer->type = NEXTING_DEVICE_MESSAGE_ANSWER;
  answer->has_request_id = true;
  (void)strcpy(answer->request_id, state->request.request_id);
  answer->choice = state->chosen;
}

nexting_device_result_t
nexting_device_state_choose(nexting_device_state_t *state,
                            nexting_device_choice_t choice, uint64_t now_ms,
                            nexting_device_message_t *answer) {
  if (state == NULL || answer == NULL ||
      state->phase != NEXTING_DEVICE_PHASE_PENDING ||
      choice_string(choice) == NULL)
    return NEXTING_DEVICE_BAD_MESSAGE;
  if (now_ms >= state->deadline_ms) {
    nexting_device_state_init(state);
    return NEXTING_DEVICE_BAD_MESSAGE;
  }
  state->phase = NEXTING_DEVICE_PHASE_WAITING_RESOLUTION;
  state->chosen = choice;
  state->next_retry_ms = add_saturated(now_ms, NEXTING_DEVICE_ANSWER_RETRY_MS);
  build_answer(state, answer);
  return NEXTING_DEVICE_OK;
}

bool nexting_device_state_retry_answer(nexting_device_state_t *state,
                                       uint64_t now_ms,
                                       nexting_device_message_t *answer) {
  if (state == NULL || answer == NULL ||
      state->phase != NEXTING_DEVICE_PHASE_WAITING_RESOLUTION)
    return false;
  if (now_ms >= state->deadline_ms) {
    nexting_device_state_init(state);
    return false;
  }
  if (now_ms < state->next_retry_ms)
    return false;
  build_answer(state, answer);
  state->next_retry_ms = add_saturated(now_ms, NEXTING_DEVICE_ANSWER_RETRY_MS);
  return true;
}

nexting_device_result_t
nexting_device_state_on_resolved(nexting_device_state_t *state,
                                 const nexting_device_message_t *message) {
  if (state == NULL || message == NULL ||
      state->phase == NEXTING_DEVICE_PHASE_IDLE ||
      message->type != NEXTING_DEVICE_MESSAGE_RESOLVED ||
      resolution_string(message->resolution) == NULL ||
      strcmp(state->request.request_id, message->request_id) != 0)
    return NEXTING_DEVICE_BAD_MESSAGE;
  nexting_device_state_init(state);
  return NEXTING_DEVICE_OK;
}

bool nexting_device_state_tick(nexting_device_state_t *state, uint64_t now_ms) {
  if (state == NULL || state->phase == NEXTING_DEVICE_PHASE_IDLE ||
      now_ms < state->deadline_ms)
    return false;
  nexting_device_state_init(state);
  return true;
}

void nexting_device_state_disconnect(nexting_device_state_t *state) {
  nexting_device_state_init(state);
}

void nexting_device_status_init(nexting_device_status_state_t *state) {
  if (state != NULL)
    memset(state, 0, sizeof *state);
}

nexting_device_result_t
nexting_device_status_on_message(nexting_device_status_state_t *state,
                                 const nexting_device_message_t *message) {
  if (state == NULL || message == NULL ||
      message->type != NEXTING_DEVICE_MESSAGE_STATUS ||
      !valid_status_agents(message))
    return NEXTING_DEVICE_BAD_MESSAGE;
  nexting_device_status_init(state);
  for (size_t i = 0; i < message->agent_count; ++i) {
    const nexting_device_agent_status_t *agent = &message->agents[i];
    state->occupied[agent->slot] = true;
    state->slots[agent->slot] = *agent;
  }
  return NEXTING_DEVICE_OK;
}

void nexting_device_status_disconnect(nexting_device_status_state_t *state) {
  nexting_device_status_init(state);
}

void nexting_device_sequence_init(nexting_device_sequence_state_t *state) {
  if (state != NULL)
    memset(state, 0, sizeof *state);
}

bool nexting_device_sequence_accept(nexting_device_sequence_state_t *state,
                                    uint32_t sequence) {
  if (state == NULL)
    return false;
  if (state->has_value && sequence <= state->latest)
    return false;
  state->has_value = true;
  state->latest = sequence;
  return true;
}

void nexting_device_sequence_disconnect(nexting_device_sequence_state_t *state) {
  nexting_device_sequence_init(state);
}
