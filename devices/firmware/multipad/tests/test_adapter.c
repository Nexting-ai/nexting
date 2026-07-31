#include "nexting_multipad_adapter.h"

#include <assert.h>
#include <string.h>

typedef struct {
  uint64_t now_ms;
  char tx[512];
  size_t tx_length;
  size_t approval_renders;
  size_t status_renders;
  nexting_device_phase_t phase;
  size_t status_count;
} test_context_t;

static uint64_t now_ms(void *context) {
  return ((test_context_t *)context)->now_ms;
}

static void write_frame(const uint8_t *bytes, size_t length, void *context) {
  test_context_t *test = (test_context_t *)context;
  assert(length < sizeof test->tx);
  memcpy(test->tx, bytes, length);
  test->tx[length] = '\0';
  test->tx_length = length;
}

static void render_approval(const nexting_device_state_t *state,
                            void *context) {
  test_context_t *test = (test_context_t *)context;
  test->approval_renders += 1U;
  test->phase = state->phase;
}

static void render_status(const nexting_device_status_state_t *state,
                          void *context) {
  test_context_t *test = (test_context_t *)context;
  test->status_renders += 1U;
  test->status_count = 0;
  for (size_t i = 0; i < NEXTING_DEVICE_STATUS_MAX_AGENTS; ++i)
    if (state->occupied[i])
      test->status_count += 1U;
}

int main(void) {
  test_context_t context = {0};
  nexting_multipad_t adapter;
  nexting_multipad_init(&adapter, now_ms, write_frame, render_approval,
                        render_status, &context);

  static const char present[] =
      "{\"v\":1,\"t\":\"present\",\"id\":\"mp1\",\"sum\":\"Allow\","
      "\"opt\":[\"allow\",\"deny\"],\"ttl\":30000}\n";
  assert(nexting_multipad_accepts(&adapter, (const uint8_t *)present, 4));
  assert(nexting_multipad_receive(&adapter, (const uint8_t *)present,
                                  sizeof present - 1U) == NEXTING_DEVICE_OK);
  assert(context.phase == NEXTING_DEVICE_PHASE_PENDING);
  assert(adapter.approval.request.has_request_id);
  assert(strcmp(adapter.approval.request.request_id, "mp1") == 0);

  context.now_ms = 100;
  assert(nexting_multipad_choose(&adapter, NEXTING_DEVICE_CHOICE_ALLOW) ==
         NEXTING_DEVICE_OK);
  assert(strcmp(context.tx,
                "{\"v\":1,\"t\":\"answer\",\"id\":\"mp1\",\"ch\":\"allow\"}\n") ==
         0);

  static const char status[] =
      "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":0,"
      "\"state\":\"working\",\"label\":\"build\"}]}\n";
  assert(nexting_multipad_receive(&adapter, (const uint8_t *)status,
                                  sizeof status - 1U) == NEXTING_DEVICE_OK);
  assert(context.status_count == 1U);

  static const char resolved[] =
      "{\"v\":1,\"t\":\"resolved\",\"id\":\"mp1\",\"r\":\"answered\"}\n";
  assert(nexting_multipad_receive(&adapter, (const uint8_t *)resolved,
                                  sizeof resolved - 1U) == NEXTING_DEVICE_OK);
  assert(context.phase == NEXTING_DEVICE_PHASE_IDLE);

  nexting_multipad_disconnect(&adapter);
  assert(adapter.approval.phase == NEXTING_DEVICE_PHASE_IDLE);
  assert(!adapter.status.occupied[0]);
  return 0;
}
