#include "generated_interaction_vectors.h"
#include "nexting_device.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static nexting_device_message_t decode_ok(const char *wire) {
  nexting_device_message_t message = {0};
  char encoded[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES + 1];
  size_t encoded_length = 0;
  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_encode(&message, encoded, sizeof encoded,
                               &encoded_length) == NEXTING_DEVICE_OK);
  assert(encoded_length == strlen(wire));
  assert(memcmp(encoded, wire, encoded_length) == 0);
  return message;
}

static void decodes_navigation_and_physical_events(void) {
  nexting_device_message_t message = decode_ok(
      "{\"v\":1,\"t\":\"nav_present\",\"id\":\"q7\",\"items\":[\"Fix "
      "it\",\"Explain\"],\"cursor\":0,\"ttl\":30000}\n");
  assert(message.type == NEXTING_DEVICE_MESSAGE_NAV_PRESENT);
  assert(message.interaction.navigation.item_count == 2U);
  assert(strcmp(message.interaction.navigation.items[1], "Explain") == 0);

  message = decode_ok(
      "{\"v\":1,\"t\":\"key_event\",\"slot\":0,\"event\":\"press\","
      "\"seq\":41}\n");
  assert(message.type == NEXTING_DEVICE_MESSAGE_KEY_EVENT);
  assert(message.interaction.slot == 0U);
  assert(message.interaction.sequence == 41U);
  assert(message.interaction.gesture == NEXTING_DEVICE_GESTURE_PRESS);

  message = decode_ok(
      "{\"v\":1,\"t\":\"rotary_event\",\"slot\":0,\"delta\":-2,"
      "\"seq\":52}\n");
  assert(message.type == NEXTING_DEVICE_MESSAGE_ROTARY_EVENT);
  assert(message.interaction.delta == -2);
}

static void decodes_display_and_configuration_state(void) {
  nexting_device_message_t message = decode_ok(
      "{\"v\":1,\"t\":\"voice_state\",\"state\":\"listening\","
      "\"label\":\"Release to send\"}\n");
  assert(message.type == NEXTING_DEVICE_MESSAGE_VOICE_STATE);
  assert(message.interaction.voice_state == NEXTING_DEVICE_VOICE_LISTENING);

  message = decode_ok(
      "{\"v\":1,\"t\":\"text\",\"channel\":0,\"title\":\"Current task\","
      "\"content\":\"Waiting for approval\"}\n");
  assert(message.type == NEXTING_DEVICE_MESSAGE_TEXT);
  assert(strcmp(message.interaction.text.content, "Waiting for approval") == 0);

  message = decode_ok(
      "{\"v\":1,\"t\":\"config\",\"rev\":7,\"entries\":["
      "{\"key\":\"display.brightness\",\"value\":70},"
      "{\"key\":\"haptics.enabled\",\"value\":true}]}\n");
  assert(message.type == NEXTING_DEVICE_MESSAGE_CONFIG);
  assert(message.interaction.revision == 7U);
  assert(message.interaction.config.entry_count == 2U);
  assert(message.interaction.config.entries[0].type ==
         NEXTING_DEVICE_CONFIG_INTEGER);
  assert(message.interaction.config.entries[1].type ==
         NEXTING_DEVICE_CONFIG_BOOLEAN);
}

static void rejects_hostile_interaction_frames(void) {
  const char *invalid[] = {
      "{\"v\":1,\"t\":\"nav_present\",\"id\":\"q\",\"items\":[\"A\"],"
      "\"cursor\":0,\"ttl\":1}\n",
      "{\"v\":1,\"t\":\"key_event\",\"slot\":64,\"event\":\"press\","
      "\"seq\":1}\n",
      "{\"v\":1,\"t\":\"rotary_event\",\"slot\":0,\"delta\":0,\"seq\":1}\n",
      "{\"v\":1,\"t\":\"voice_event\",\"event\":\"audio\",\"seq\":1}\n",
      "{\"v\":1,\"t\":\"usage\",\"model\":\"GPT\",\"input_tokens\":-1,"
      "\"output_tokens\":0}\n",
      "{\"v\":1,\"t\":\"config\",\"rev\":1,\"entries\":["
      "{\"key\":\"a\",\"value\":1},{\"key\":\"a\",\"value\":2}]}\n",
  };
  for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index) {
    nexting_device_message_t message = {0};
    assert(nexting_device_decode(invalid[index], strlen(invalid[index]),
                                 &message) == NEXTING_DEVICE_BAD_MESSAGE);
  }
}

static void sequence_gate_is_connection_scoped(void) {
  nexting_device_sequence_state_t state;
  nexting_device_sequence_init(&state);
  assert(nexting_device_sequence_accept(&state, 41U));
  assert(!nexting_device_sequence_accept(&state, 41U));
  assert(!nexting_device_sequence_accept(&state, 40U));
  assert(nexting_device_sequence_accept(&state, 42U));
  nexting_device_sequence_disconnect(&state);
  assert(nexting_device_sequence_accept(&state, 1U));
}

static void matches_every_shared_interaction_vector(void) {
  char encoded[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES + 1];
  for (size_t index = 0;
       index < nexting_device_interaction_valid_vectors_count; ++index) {
    nexting_device_message_t message = {0};
    size_t encoded_length = 0;
    const char *wire = nexting_device_interaction_valid_vectors[index];
    assert(nexting_device_decode(wire, strlen(wire), &message) ==
           NEXTING_DEVICE_OK);
    assert(nexting_device_encode(&message, encoded, sizeof encoded,
                                 &encoded_length) == NEXTING_DEVICE_OK);
    assert(encoded_length == strlen(wire));
    assert(memcmp(encoded, wire, encoded_length) == 0);
  }
  for (size_t index = 0;
       index < nexting_device_interaction_invalid_vectors_count; ++index) {
    nexting_device_message_t message = {0};
    const char *wire = nexting_device_interaction_invalid_vectors[index];
    assert(nexting_device_decode(wire, strlen(wire), &message) !=
           NEXTING_DEVICE_OK);
  }
}

int main(void) {
  decodes_navigation_and_physical_events();
  decodes_display_and_configuration_state();
  rejects_hostile_interaction_frames();
  sequence_gate_is_connection_scoped();
  matches_every_shared_interaction_vector();
  puts("interaction tests passed");
  return 0;
}
