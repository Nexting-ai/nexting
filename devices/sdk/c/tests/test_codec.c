#include "generated_vectors.h"
#include "nexting_device.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(NEXTING_DEVICE_ASAN_TEST)
#include <sanitizer/asan_interface.h>
#endif

static void decodes_allow_answer(void) {
  const char *wire =
      "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\"}\n";
  nexting_device_message_t out = {0};
  assert(nexting_device_decode(wire, strlen(wire), &out) == NEXTING_DEVICE_OK);
  assert(out.type == NEXTING_DEVICE_MESSAGE_ANSWER);
  assert(strcmp(out.request_id, "r1") == 0);
  assert(out.choice == NEXTING_DEVICE_CHOICE_ALLOW);
}

static void direct_codec_reserves_required_newline(void) {
  static const char prefix[] =
      "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\",\"future\":\"";
  static const char suffix[] = "\"}";
  char wire[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES + 1];
  nexting_device_message_t message = {0};
  const size_t payload_length = NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES - 1U;
  const size_t padding_length =
      payload_length - (sizeof prefix - 1U) - (sizeof suffix - 1U);

  memcpy(wire, prefix, sizeof prefix - 1U);
  memset(wire + sizeof prefix - 1U, 'x', padding_length);
  memcpy(wire + sizeof prefix - 1U + padding_length, suffix,
         sizeof suffix - 1U);
  wire[payload_length] = '\n';
  wire[payload_length + 1U] = '\0';
  assert(nexting_device_decode(wire, payload_length + 1U, &message) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_decode(wire, payload_length, &message) ==
         NEXTING_DEVICE_OK);

  memmove(wire + payload_length - (sizeof suffix - 1U) + 1U,
          wire + payload_length - (sizeof suffix - 1U), sizeof suffix - 1U);
  wire[payload_length - (sizeof suffix - 1U)] = 'x';
  wire[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES] = '\0';
  assert(nexting_device_decode(wire, NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES,
                               &message) ==
         NEXTING_DEVICE_MESSAGE_TOO_LARGE);
}

static void accepts_bounded_long_unknown_key(void) {
  char key[242];
  char wire[512];
  nexting_device_message_t message = {0};
  memset(key, 'k', sizeof key - 1U);
  key[sizeof key - 1U] = '\0';
  const int written = snprintf(
      wire, sizeof wire,
      "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\",\"%s\":true}",
      key);
  assert(written > 0 && (size_t)written < sizeof wire);
  assert(nexting_device_decode(wire, (size_t)written, &message) ==
         NEXTING_DEVICE_OK);
  assert(message.type == NEXTING_DEVICE_MESSAGE_ANSWER);
  assert(message.choice == NEXTING_DEVICE_CHOICE_ALLOW);
}

static void matches_shared_vectors(void) {
  char encoded[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES + 1];
  for (size_t i = 0; i < nexting_device_valid_vectors_count; ++i) {
    nexting_device_message_t message = {0};
    size_t encoded_length = 0;
    const char *wire = nexting_device_valid_vectors[i];
    assert(nexting_device_decode(wire, strlen(wire), &message) ==
           NEXTING_DEVICE_OK);
    assert(nexting_device_encode(&message, encoded, sizeof encoded,
                                 &encoded_length) == NEXTING_DEVICE_OK);
    assert(encoded_length == strlen(wire));
    assert(memcmp(encoded, wire, encoded_length) == 0);
  }
  for (size_t i = 0; i < nexting_device_invalid_vectors_count; ++i) {
    nexting_device_message_t message = {0};
    const char *wire = nexting_device_invalid_vectors[i];
    assert(nexting_device_decode(wire, strlen(wire), &message) !=
           NEXTING_DEVICE_OK);
  }
}

static void decodes_every_protocol_enum(void) {
  const struct {
    const char *wire;
    nexting_device_resolution_t expected;
  } resolutions[] = {
      {"{\"v\":1,\"t\":\"resolved\",\"id\":\"r\",\"r\":\"answered\"}\n",
       NEXTING_DEVICE_RESOLUTION_ANSWERED},
      {"{\"v\":1,\"t\":\"resolved\",\"id\":\"r\",\"r\":\"expired\"}\n",
       NEXTING_DEVICE_RESOLUTION_EXPIRED},
      {"{\"v\":1,\"t\":\"resolved\",\"id\":\"r\",\"r\":\"cancelled\"}\n",
       NEXTING_DEVICE_RESOLUTION_CANCELLED},
      {"{\"v\":1,\"t\":\"resolved\",\"id\":\"r\",\"r\":\"replaced\"}\n",
       NEXTING_DEVICE_RESOLUTION_REPLACED},
  };
  const struct {
    const char *wire;
    nexting_device_error_code_t expected;
  } errors[] = {
      {"{\"v\":1,\"t\":\"error\",\"code\":\"bad_message\"}\n",
       NEXTING_DEVICE_ERROR_BAD_MESSAGE},
      {"{\"v\":1,\"t\":\"error\",\"code\":\"message_too_large\"}\n",
       NEXTING_DEVICE_ERROR_MESSAGE_TOO_LARGE},
      {"{\"v\":1,\"t\":\"error\",\"code\":\"unsupported_version\"}\n",
       NEXTING_DEVICE_ERROR_UNSUPPORTED_VERSION},
      {"{\"v\":1,\"t\":\"error\",\"code\":\"unsupported_profile\"}\n",
       NEXTING_DEVICE_ERROR_UNSUPPORTED_PROFILE},
      {"{\"v\":1,\"t\":\"error\",\"code\":\"unknown_request\"}\n",
       NEXTING_DEVICE_ERROR_UNKNOWN_REQUEST},
      {"{\"v\":1,\"t\":\"error\",\"code\":\"not_authorized\"}\n",
       NEXTING_DEVICE_ERROR_NOT_AUTHORIZED},
      {"{\"v\":1,\"t\":\"error\",\"code\":\"busy\"}\n",
       NEXTING_DEVICE_ERROR_BUSY},
  };
  for (size_t i = 0; i < sizeof resolutions / sizeof resolutions[0]; ++i) {
    nexting_device_message_t message = {0};
    assert(nexting_device_decode(resolutions[i].wire,
                                 strlen(resolutions[i].wire), &message) ==
           NEXTING_DEVICE_OK);
    assert(message.type == NEXTING_DEVICE_MESSAGE_RESOLVED);
    assert(message.resolution == resolutions[i].expected);
  }
  for (size_t i = 0; i < sizeof errors / sizeof errors[0]; ++i) {
    nexting_device_message_t message = {0};
    assert(nexting_device_decode(errors[i].wire, strlen(errors[i].wire),
                                 &message) == NEXTING_DEVICE_OK);
    assert(message.type == NEXTING_DEVICE_MESSAGE_ERROR);
    assert(message.error_code == errors[i].expected);
  }
}

static void handles_escapes_and_utf8_bounds(void) {
  const char *wire = "{\"v\":1,\"t\":\"present\",\"id\":\"utf8\",\"sum\":"
                     "\"允许\\npush \\\"main\\\"\","
                     "\"opt\":[\"allow\",\"deny\"],\"ttl\":1}\n";
  nexting_device_message_t message = {0};
  char encoded[512];
  size_t encoded_length = 0;
  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(strcmp(message.summary, "允许\npush \"main\"") == 0);
  assert(nexting_device_encode(&message, encoded, sizeof encoded,
                               &encoded_length) == NEXTING_DEVICE_OK);
  assert(encoded_length == strlen(wire));
  assert(memcmp(encoded, wire, encoded_length) == 0);
}

static void rejects_duplicates_and_bad_shapes(void) {
  const char *cases[] = {
      "{\"v\":1,\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\"}\n",
      "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"id\":\"r2\",\"ch\":\"allow\"}"
      "\n",
      "{\"v\":1,\"t\":\"error\",\"id\":null,\"code\":\"busy\"}\n",
      "{\"v\":1,\"t\":\"present\",\"id\":\"r1\",\"sum\":[],\"opt\":[\"allow\","
      "\"deny\"],\"ttl\":1}\n",
      "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\"}\nextra",
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
    nexting_device_message_t message = {0};
    assert(nexting_device_decode(cases[i], strlen(cases[i]), &message) ==
           NEXTING_DEVICE_BAD_MESSAGE);
  }
}

static void ignores_one_unknown_optional_field(void) {
  const char *wire =
      "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"deny\","
      "\"future\":{\"note\":\"comma, brace } and quote \\\" remain data\"}}\n";
  nexting_device_message_t message = {0};
  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(message.type == NEXTING_DEVICE_MESSAGE_ANSWER);
  assert(message.choice == NEXTING_DEVICE_CHOICE_DENY);
}

static void encode_rejects_unterminated_fields(void) {
  nexting_device_message_t message = {0};
  char output[256];
  size_t output_length = 0;
  message.type = NEXTING_DEVICE_MESSAGE_ANSWER;
  message.has_request_id = true;
  memset(message.request_id, 'x', sizeof message.request_id);
  message.choice = NEXTING_DEVICE_CHOICE_ALLOW;
#if defined(NEXTING_DEVICE_ASAN_TEST)
  __asan_poison_memory_region(message.summary, sizeof message.summary);
#endif
  const nexting_device_result_t result = nexting_device_encode(
      &message, output, sizeof output, &output_length);
#if defined(NEXTING_DEVICE_ASAN_TEST)
  __asan_unpoison_memory_region(message.summary, sizeof message.summary);
#endif
  assert(result == NEXTING_DEVICE_BAD_MESSAGE);
}

static void rejects_null_arguments_and_small_output_buffers(void) {
  const char *wire =
      "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\"}\n";
  nexting_device_message_t message = {0};
  char output[8] = {0};
  size_t output_length = 0;

  assert(nexting_device_decode(NULL, 0, &message) ==
         NEXTING_DEVICE_BAD_MESSAGE);
  assert(nexting_device_decode(wire, strlen(wire), NULL) ==
         NEXTING_DEVICE_BAD_MESSAGE);
  assert(nexting_device_encode(NULL, output, sizeof output, &output_length) ==
         NEXTING_DEVICE_BAD_MESSAGE);
  assert(nexting_device_encode(&message, NULL, 0, &output_length) ==
         NEXTING_DEVICE_BAD_MESSAGE);

  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_encode(&message, output, sizeof output,
                               &output_length) ==
         NEXTING_DEVICE_BUFFER_TOO_SMALL);
  assert(output_length == strlen(wire));
}

int main(void) {
  decodes_allow_answer();
  direct_codec_reserves_required_newline();
  accepts_bounded_long_unknown_key();
  encode_rejects_unterminated_fields();
  rejects_null_arguments_and_small_output_buffers();
  matches_shared_vectors();
  decodes_every_protocol_enum();
  handles_escapes_and_utf8_bounds();
  rejects_duplicates_and_bad_shapes();
  ignores_one_unknown_optional_field();
  puts("codec tests passed");
  return 0;
}
