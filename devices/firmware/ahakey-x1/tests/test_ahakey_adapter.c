#include "nexting_ahakey_adapter.h"

#include <assert.h>
#include <string.h>

typedef struct {
  uint64_t now_ms;
  char tx[512];
  size_t tx_length;
  size_t approval_renders;
  size_t text_renders;
  nexting_device_phase_t phase;
  ahakey_state_t led;
  size_t led_renders;
  char text_title[NEXTING_DEVICE_TEXT_TITLE_CAPACITY];
  char text_content[64];
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

static void render_led(ahakey_state_t state, void *context) {
  test_context_t *test = (test_context_t *)context;
  test->led = state;
  test->led_renders += 1U;
}

static void render_text(const nexting_device_text_payload_t *text,
                        void *context) {
  test_context_t *test = (test_context_t *)context;
  test->text_renders += 1U;
  memset(test->text_title, 0, sizeof test->text_title);
  memset(test->text_content, 0, sizeof test->text_content);
  if (text->has_title)
    memcpy(test->text_title, text->title, sizeof test->text_title - 1U);
  memcpy(test->text_content, text->content, sizeof test->text_content - 1U);
}

static void receive(nexting_ahakey_t *adapter, const char *wire) {
  assert(nexting_ahakey_receive(adapter, (const uint8_t *)wire,
                                strlen(wire)) == NEXTING_DEVICE_OK);
}

int main(void) {
  test_context_t context = {0};
  nexting_ahakey_t adapter;
  nexting_ahakey_init(&adapter, now_ms, write_frame, render_approval,
                      render_led, render_text, &context);

  /* Idle hardware shows nothing; the LED starts dark. */
  assert(nexting_ahakey_resolve_led(&adapter) == AHAKEY_STATE_STOP);

  /* Approval present drives the permission-request LED over any status. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"present\",\"id\":\"ak1\",\"sum\":\"Allow push?\","
          "\"opt\":[\"allow\",\"deny\"],\"ttl\":30000}\n");
  assert(context.phase == NEXTING_DEVICE_PHASE_PENDING);
  assert(context.led == AHAKEY_STATE_PERMISSION_REQUEST);

  /* F18 answers allow with the same request id. */
  context.now_ms = 100;
  assert(nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_ALLOW, true));
  assert(strcmp(context.tx,
                "{\"v\":1,\"t\":\"answer\",\"id\":\"ak1\",\"ch\":\"allow\"}\n") ==
         0);
  assert(nexting_ahakey_resolve_led(&adapter) == AHAKEY_STATE_STOP);

  /* Resolved clears the request. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"resolved\",\"id\":\"ak1\",\"r\":\"answered\"}\n");
  assert(context.phase == NEXTING_DEVICE_PHASE_IDLE);

  /* Deny flows through the same state machine with F19. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"present\",\"id\":\"ak2\",\"sum\":\"Deny push?\","
          "\"opt\":[\"allow\",\"deny\"],\"ttl\":30000}\n");
  assert(nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_DENY, true));
  assert(strcmp(context.tx,
                "{\"v\":1,\"t\":\"answer\",\"id\":\"ak2\",\"ch\":\"deny\"}\n") ==
         0);

  /* Status maps agent states onto the LED strip. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":0,"
          "\"state\":\"working\",\"label\":\"build\"}]}\n");
  assert(context.led == AHAKEY_STATE_PRE_TOOL_USE);
  receive(&adapter,
          "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":0,"
          "\"state\":\"needs_input\",\"label\":\"build\"}]}\n");
  assert(context.led == AHAKEY_STATE_PERMISSION_REQUEST);
  receive(&adapter,
          "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":0,"
          "\"state\":\"complete\",\"label\":\"build\"}]}\n");
  assert(context.led == AHAKEY_STATE_TASK_COMPLETED);
  receive(&adapter,
          "{\"v\":1,\"t\":\"status\",\"agents\":[]}\n");
  assert(context.led == AHAKEY_STATE_STOP);

  /* The lowest-numbered occupied slot wins; higher slots are ignored. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":0,"
          "\"state\":\"working\",\"label\":\"a\"},{\"slot\":1,"
          "\"state\":\"error\",\"label\":\"b\"}]}\n");
  assert(context.led == AHAKEY_STATE_PRE_TOOL_USE);
  receive(&adapter,
          "{\"v\":1,\"t\":\"status\",\"agents\":[{\"slot\":1,"
          "\"state\":\"error\",\"label\":\"b\"}]}\n");
  assert(context.led == AHAKEY_STATE_NOTIFICATION);

  /* No key events before the Host keymap declares and enables slots. */
  assert(!nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_F13, true));
  assert(!nexting_ahakey_lever(&adapter, 0));
  assert(!nexting_ahakey_lever(&adapter, 1));

  /* Keymap enables slots 0, 3, and 7; everything else stays silent. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"keymap\",\"rev\":4,\"keys\":[{\"slot\":0,"
          "\"label\":\"One\",\"enabled\":true,\"light\":\"solid\"},{\"slot\":3,"
          "\"label\":\"Four\",\"enabled\":true,\"light\":\"solid\"},{\"slot\":7,"
          "\"label\":\"Lever\",\"enabled\":true,\"light\":\"solid\"}]}\n");
  assert(nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_F13, true));
  assert(strcmp(context.tx,
                "{\"v\":1,\"t\":\"key_event\",\"slot\":0,\"event\":\"press\","
                "\"seq\":1}\n") == 0);
  assert(nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_F16, true));
  assert(strcmp(context.tx,
                "{\"v\":1,\"t\":\"key_event\",\"slot\":3,\"event\":\"press\","
                "\"seq\":2}\n") == 0);
  /* Undeclared and disabled slots emit nothing; releases are ignored. */
  assert(!nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_F14, true));
  assert(!nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_F13, false));
  assert(!nexting_ahakey_hid_key(&adapter, 0x2CU, true));

  /* The lever reports on its declared slot only on change. */
  assert(!nexting_ahakey_lever(&adapter, 1)); /* steady state, silent */
  assert(nexting_ahakey_lever(&adapter, 0));
  assert(strcmp(context.tx,
                "{\"v\":1,\"t\":\"key_event\",\"slot\":7,"
                "\"event\":\"release\",\"seq\":3}\n") == 0);

  /* A replacement keymap fully replaces the enabled set. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"keymap\",\"rev\":5,\"keys\":[]}\n");
  assert(!nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_F13, true));
  receive(&adapter,
          "{\"v\":1,\"t\":\"keymap\",\"rev\":6,\"keys\":[{\"slot\":7,"
          "\"label\":\"Lever\",\"enabled\":true,\"light\":\"solid\"}]}\n");
  assert(nexting_ahakey_lever(&adapter, 1));
  assert(strcmp(context.tx,
                "{\"v\":1,\"t\":\"key_event\",\"slot\":7,\"event\":\"press\","
                "\"seq\":4}\n") == 0);

  /* TTL expiry clears the pending approval and drops the override LED. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"present\",\"id\":\"ak4\",\"sum\":\"Short\","
          "\"opt\":[\"allow\",\"deny\"],\"ttl\":500}\n");
  assert(context.led == AHAKEY_STATE_PERMISSION_REQUEST);
  context.now_ms = 1000;
  nexting_ahakey_tick(&adapter);
  assert(context.phase == NEXTING_DEVICE_PHASE_IDLE);
  assert(context.led == AHAKEY_STATE_NOTIFICATION); /* slot-1 error remains */

  /* The answer retry re-sends the same frame until resolved. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"present\",\"id\":\"ak5\",\"sum\":\"Retry\","
          "\"opt\":[\"allow\",\"deny\"],\"ttl\":30000}\n");
  context.now_ms = 2000;
  assert(nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_ALLOW, true));
  const char *want_answer =
      "{\"v\":1,\"t\":\"answer\",\"id\":\"ak5\",\"ch\":\"allow\"}\n";
  assert(strcmp(context.tx, want_answer) == 0);
  context.now_ms = 4000;
  nexting_ahakey_tick(&adapter);
  assert(strcmp(context.tx, want_answer) == 0);
  receive(&adapter,
          "{\"v\":1,\"t\":\"resolved\",\"id\":\"ak5\",\"r\":\"answered\"}\n");
  context.now_ms = 6000;
  context.tx[0] = '\0';
  nexting_ahakey_tick(&adapter);
  assert(context.tx[0] == '\0'); /* resolved stops the retry */

  /* text/1 reaches the display callback with title and content. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"text\",\"channel\":0,\"title\":\"Current task\","
          "\"content\":\"Waiting for approval\"}\n");
  assert(context.text_renders == 1U);
  assert(strcmp(context.text_title, "Current task") == 0);
  assert(strcmp(context.text_content, "Waiting for approval") == 0);

  /* Disconnect clears approval, status, and the lever baseline; LED dark. */
  receive(&adapter,
          "{\"v\":1,\"t\":\"present\",\"id\":\"ak3\",\"sum\":\"Later\","
          "\"opt\":[\"allow\",\"deny\"],\"ttl\":30000}\n");
  nexting_ahakey_disconnect(&adapter);
  assert(context.phase == NEXTING_DEVICE_PHASE_IDLE);
  assert(context.led == AHAKEY_STATE_STOP);
  assert(!nexting_ahakey_lever(&adapter, 0)); /* baseline re-established */
  /* The volatile keymap is gone too: F13 was enabled before disconnect. */
  assert(!nexting_ahakey_hid_key(&adapter, NEXTING_AHAKEY_HID_F13, true));

  return 0;
}
