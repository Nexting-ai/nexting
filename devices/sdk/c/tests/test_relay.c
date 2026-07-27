#include "nexting_device.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static nexting_device_message_t present(const char *id, uint32_t ttl_ms) {
  nexting_device_message_t message = {0};
  message.type = NEXTING_DEVICE_MESSAGE_PRESENT;
  strcpy(message.request_id, id);
  strcpy(message.summary, "Allow action?");
  message.ttl_ms = ttl_ms;
  return message;
}

static nexting_device_message_t
resolved(const char *id, nexting_device_resolution_t resolution) {
  nexting_device_message_t message = {0};
  message.type = NEXTING_DEVICE_MESSAGE_RESOLVED;
  strcpy(message.request_id, id);
  message.resolution = resolution;
  return message;
}

static void chooses_once_and_waits_for_resolution(void) {
  nexting_device_state_t state;
  nexting_device_message_t output = {0};
  nexting_device_message_t request = present("r1", 3000);
  nexting_device_message_t done =
      resolved("r1", NEXTING_DEVICE_RESOLUTION_ANSWERED);
  nexting_device_state_init(&state);
  assert(nexting_device_state_on_present(&state, &request, 100) ==
         NEXTING_DEVICE_OK);
  assert(state.phase == NEXTING_DEVICE_PHASE_PENDING);
  assert(nexting_device_state_choose(&state, NEXTING_DEVICE_CHOICE_ALLOW, 200,
                                     &output) == NEXTING_DEVICE_OK);
  assert(output.type == NEXTING_DEVICE_MESSAGE_ANSWER);
  assert(output.choice == NEXTING_DEVICE_CHOICE_ALLOW);
  assert(state.phase == NEXTING_DEVICE_PHASE_WAITING_RESOLUTION);
  assert(nexting_device_state_choose(&state, NEXTING_DEVICE_CHOICE_DENY, 300,
                                     &output) == NEXTING_DEVICE_BAD_MESSAGE);
  assert(!nexting_device_state_retry_answer(&state, 1199, &output));
  assert(nexting_device_state_retry_answer(&state, 1200, &output));
  assert(output.choice == NEXTING_DEVICE_CHOICE_ALLOW);
  assert(nexting_device_state_on_resolved(&state, &done) == NEXTING_DEVICE_OK);
  assert(state.phase == NEXTING_DEVICE_PHASE_IDLE);
}

static void replaces_expires_and_disconnects(void) {
  nexting_device_state_t state;
  nexting_device_message_t first = present("r1", 100);
  nexting_device_message_t second = present("r2", 50);
  nexting_device_state_init(&state);
  assert(nexting_device_state_on_present(&state, &first, 1000) ==
         NEXTING_DEVICE_OK);
  assert(nexting_device_state_on_present(&state, &second, 1010) ==
         NEXTING_DEVICE_OK);
  assert(strcmp(state.request.request_id, "r2") == 0);
  assert(!nexting_device_state_tick(&state, 1059));
  assert(nexting_device_state_tick(&state, 1060));
  assert(state.phase == NEXTING_DEVICE_PHASE_IDLE);
  assert(nexting_device_state_on_present(&state, &first, 2000) ==
         NEXTING_DEVICE_OK);
  nexting_device_state_disconnect(&state);
  assert(state.phase == NEXTING_DEVICE_PHASE_IDLE);
  assert(state.request.request_id[0] == '\0');
}

static void rejects_stale_resolution_and_saturates_deadlines(void) {
  nexting_device_state_t state;
  nexting_device_message_t request = present("r1", 100);
  nexting_device_message_t stale =
      resolved("other", NEXTING_DEVICE_RESOLUTION_ANSWERED);
  nexting_device_message_t answer = {0};

  nexting_device_state_init(&state);
  assert(nexting_device_state_on_present(&state, &request, UINT64_MAX - 10) ==
         NEXTING_DEVICE_OK);
  assert(state.deadline_ms == UINT64_MAX);
  assert(nexting_device_state_on_resolved(&state, &stale) ==
         NEXTING_DEVICE_BAD_MESSAGE);
  assert(state.phase == NEXTING_DEVICE_PHASE_PENDING);
  assert(nexting_device_state_choose(&state, NEXTING_DEVICE_CHOICE_DENY,
                                     UINT64_MAX - 9, &answer) ==
         NEXTING_DEVICE_OK);
  assert(state.next_retry_ms == UINT64_MAX);
  assert(!nexting_device_state_retry_answer(&state, UINT64_MAX - 1, &answer));
  assert(nexting_device_state_tick(&state, UINT64_MAX));
  assert(state.phase == NEXTING_DEVICE_PHASE_IDLE);
}

int main(void) {
  chooses_once_and_waits_for_resolution();
  replaces_expires_and_disconnects();
  rejects_stale_resolution_and_saturates_deadlines();
  puts("relay tests passed");
  return 0;
}
