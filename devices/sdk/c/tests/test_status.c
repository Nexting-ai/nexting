#include "generated_status_vectors.h"
#include "nexting_device.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void matches_shared_status_vectors(void) {
  char encoded[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES + 1];
  for (size_t i = 0; i < nexting_device_valid_vectors_count; ++i) {
    nexting_device_message_t message = {0};
    size_t encoded_length = 0;
    const char *wire = nexting_device_valid_vectors[i];
    assert(nexting_device_decode(wire, strlen(wire), &message) ==
           NEXTING_DEVICE_OK);
    assert(message.type == NEXTING_DEVICE_MESSAGE_STATUS);
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

static void decodes_multibyte_label_boundary(void) {
  const char *wire = "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":1,"
                     "\"state\":\"working\",\"label\":\"\xC3\xA9\xC3\xA9\"}]}\n";
  nexting_device_message_t message = {0};
  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(message.agent_count == 1);
  assert(message.agents[0].slot == 1);
  assert(message.agents[0].state == NEXTING_DEVICE_AGENT_STATE_WORKING);
  assert(message.agents[0].has_label);
  assert(strcmp(message.agents[0].label, "\xC3\xA9\xC3\xA9") == 0);
}

static void status_state_full_replacement_and_disconnect(void) {
  nexting_device_status_state_t status;
  nexting_device_message_t message = {0};
  nexting_device_status_init(&status);
  for (size_t i = 0; i < NEXTING_DEVICE_STATUS_MAX_AGENTS; ++i) {
    assert(!status.occupied[i]);
  }

  const char *wire = "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":0,"
                     "\"state\":\"thinking\",\"label\":\"fix\"},{\"slot\":3,"
                     "\"state\":\"complete\"}]}\n";
  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_status_on_message(&status, &message) ==
         NEXTING_DEVICE_OK);
  assert(status.occupied[0] && status.occupied[3]);
  assert(!status.occupied[1]);
  assert(status.slots[0].state == NEXTING_DEVICE_AGENT_STATE_THINKING);
  assert(strcmp(status.slots[0].label, "fix") == 0);
  assert(status.slots[3].state == NEXTING_DEVICE_AGENT_STATE_COMPLETE);
  assert(!status.slots[3].has_label);

  const char *clear = "{\"v\":1,\"t\":\"status\",\"agents\":[]}\n";
  assert(nexting_device_decode(clear, strlen(clear), &message) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_status_on_message(&status, &message) ==
         NEXTING_DEVICE_OK);
  for (size_t i = 0; i < NEXTING_DEVICE_STATUS_MAX_AGENTS; ++i) {
    assert(!status.occupied[i]);
  }

  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_status_on_message(&status, &message) ==
         NEXTING_DEVICE_OK);
  nexting_device_status_disconnect(&status);
  for (size_t i = 0; i < NEXTING_DEVICE_STATUS_MAX_AGENTS; ++i) {
    assert(!status.occupied[i]);
  }
}

static void status_state_rejects_non_status_messages(void) {
  nexting_device_status_state_t status;
  nexting_device_message_t message = {0};
  nexting_device_status_init(&status);
  const char *wire = "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"allow\"}\n";
  assert(nexting_device_decode(wire, strlen(wire), &message) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_status_on_message(&status, &message) ==
         NEXTING_DEVICE_BAD_MESSAGE);
  assert(nexting_device_status_on_message(NULL, &message) ==
         NEXTING_DEVICE_BAD_MESSAGE);
  assert(nexting_device_status_on_message(&status, NULL) ==
         NEXTING_DEVICE_BAD_MESSAGE);
}

static void status_encode_rejects_bad_shapes(void) {
  nexting_device_message_t message = {0};
  char output[256];
  size_t output_length = 0;
  message.type = NEXTING_DEVICE_MESSAGE_STATUS;
  message.agent_count = 1;
  message.agents[0].slot = 8;
  message.agents[0].state = NEXTING_DEVICE_AGENT_STATE_IDLE;
  assert(nexting_device_encode(&message, output, sizeof output,
                               &output_length) == NEXTING_DEVICE_BAD_MESSAGE);

  message.agents[0].slot = 0;
  message.agents[0].state = NEXTING_DEVICE_AGENT_STATE_NONE;
  assert(nexting_device_encode(&message, output, sizeof output,
                               &output_length) == NEXTING_DEVICE_BAD_MESSAGE);

  message.agents[0].state = NEXTING_DEVICE_AGENT_STATE_IDLE;
  message.agents[0].has_label = true;
  message.agents[0].label[0] = '\a';
  message.agents[0].label[1] = '\0';
  assert(nexting_device_encode(&message, output, sizeof output,
                               &output_length) == NEXTING_DEVICE_BAD_MESSAGE);

  message.agents[0].label[0] = 'o';
  message.agents[0].label[1] = 'k';
  message.agents[0].label[2] = '\0';
  message.agent_count = 2;
  message.agents[1].slot = 0;
  message.agents[1].state = NEXTING_DEVICE_AGENT_STATE_WORKING;
  assert(nexting_device_encode(&message, output, sizeof output,
                               &output_length) == NEXTING_DEVICE_BAD_MESSAGE);
}

static void status_does_not_touch_approval_state(void) {
  nexting_device_state_t approval;
  nexting_device_message_t present = {0};
  nexting_device_message_t status_message = {0};
  nexting_device_state_init(&approval);

  const char *present_wire = "{\"v\":1,\"t\":\"present\",\"id\":\"r1\","
                             "\"sum\":\"Allow?\",\"opt\":[\"allow\",\"deny\"],"
                             "\"ttl\":30000}\n";
  assert(nexting_device_decode(present_wire, strlen(present_wire), &present) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_state_on_present(&approval, &present, 1000) ==
         NEXTING_DEVICE_OK);

  const char *status_wire = "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":0,"
                            "\"state\":\"error\"}]}\n";
  assert(nexting_device_decode(status_wire, strlen(status_wire),
                               &status_message) == NEXTING_DEVICE_OK);
  assert(nexting_device_state_on_present(&approval, &status_message, 1000) ==
         NEXTING_DEVICE_BAD_MESSAGE);
  assert(approval.phase == NEXTING_DEVICE_PHASE_PENDING);
  assert(strcmp(approval.request.request_id, "r1") == 0);
}

int main(void) {
  matches_shared_status_vectors();
  decodes_multibyte_label_boundary();
  status_state_full_replacement_and_disconnect();
  status_state_rejects_non_status_messages();
  status_encode_rejects_bad_shapes();
  status_does_not_touch_approval_state();
  puts("status tests passed");
  return 0;
}
