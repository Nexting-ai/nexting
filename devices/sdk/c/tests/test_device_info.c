#include "generated_device_info_vectors.h"
#include "nexting_device.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void matches_shared_vectors(void) {
  for (size_t i = 0; i < nexting_device_valid_vectors_count; ++i) {
    nexting_device_info_t info = {0};
    const char *wire = nexting_device_valid_vectors[i];
    assert(nexting_device_info_decode(wire, strlen(wire), &info) ==
           NEXTING_DEVICE_OK);
    assert(strcmp(info.protocol_name, "nexting-device") == 0);
    assert(info.supports_approval_v1);
  }
  for (size_t i = 0; i < nexting_device_invalid_vectors_count; ++i) {
    nexting_device_info_t info = {0};
    const char *wire = nexting_device_invalid_vectors[i];
    assert(nexting_device_info_decode(wire, strlen(wire), &info) !=
           NEXTING_DEVICE_OK);
  }
}

static void decodes_extensible_macropad(void) {
  nexting_device_info_t info = {0};
  const char *wire = nexting_device_valid_vectors[1];
  assert(nexting_device_info_decode(wire, strlen(wire), &info) ==
         NEXTING_DEVICE_OK);
  assert(strcmp(info.model, "Multi Pad") == 0);
  assert(strcmp(info.device_id, "5cc0a66e-a204-4c33-a3ef-b2b352a35489") == 0);
  assert(info.has_button_count);
  assert(info.button_count == 12U);
  assert(info.status_slots == 3U);
  assert(info.has_battery_service);
  assert(info.has_vendor);
  assert(strcmp(info.vendor_namespace, "com.ilx.multipad") == 0);
  assert(info.vendor_fact_count == 2U);
  assert(strcmp(info.vendor_facts[0].key, "layers") == 0);
  assert(strcmp(info.vendor_facts[0].value, "4") == 0);
}

static void drops_invalid_vendor_only(void) {
  static const char wire[] =
      "{\"protocol\":\"nexting-device\",\"spec\":\"0.2.0\",\"wire\":[1],"
      "\"profiles\":[\"approval/1\"],\"model\":\"x\",\"fw\":\"1\","
      "\"max_message_bytes\":512,\"max_summary_bytes\":1,"
      "\"vendor\":{\"namespace\":\"bad\",\"facts\":[{\"key\":\"x\","
      "\"label\":\"X\",\"value\":\"https://example.com\"}]}}";
  nexting_device_info_t info = {0};
  assert(nexting_device_info_decode(wire, strlen(wire), &info) ==
         NEXTING_DEVICE_OK);
  assert(!info.has_vendor);
  assert(info.vendor_fact_count == 0U);
}

int main(void) {
  matches_shared_vectors();
  decodes_extensible_macropad();
  drops_invalid_vendor_only();
  puts("device info tests passed");
  return 0;
}
