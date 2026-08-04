import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const read = (relativePath) => readFile(new URL(relativePath, import.meta.url), "utf8");

test("firmware reuses the fixed-buffer protocol core", async () => {
  const cmake = await read("../CMakeLists.txt");
  const main = await read("../src/main.c");
  assert.match(cmake, /sdk\/c\/src\/nexting_device\.c/);
  assert.match(cmake, /sdk\/c\/include/);
  for (const call of [
    "nexting_device_stream_push",
    "nexting_device_state_on_present",
    "nexting_device_state_choose",
    "nexting_device_state_retry_answer",
    "nexting_device_state_tick",
    "nexting_device_state_disconnect",
  ]) assert.ok(main.includes(call), `missing shared-core call ${call}`);
  assert.ok(!main.includes("strstr("), "firmware must not parse protocol with substring searches");
});

test("GATT service publishes the complete encrypted transport", async () => {
  const main = await read("../src/main.c");
  const manifest = await read("../../../west.yml");
  assert.match(main, /0x6eadc0de/i);
  for (const shortID of ["0x0001", "0x0002", "0x0003", "0x0004"]) {
    assert.ok(main.includes(shortID), `missing GATT UUID component ${shortID}`);
  }
  assert.match(main, /BT_GATT_PERM_WRITE_ENCRYPT/);
  assert.match(main, /BT_GATT_PERM_READ_ENCRYPT[\s\S]*BT_GATT_PERM_WRITE_ENCRYPT/);
  assert.match(main, /max_message_bytes/);
  assert.match(main, /max_summary_bytes/);
  assert.match(main, /0\.2\.0-experimental\.2/);
  assert.match(main, /button_count/);
  assert.match(main, /approval_button_count/);
  assert.match(main, /bt_conn_set_security\([\s\S]*BT_SECURITY_L2/);
  assert.match(manifest, /- cmsis_6/);
  assert.match(main, /__has_include\(<zephyr\/version\.h>\)/);
  assert.match(main, /#include <version\.h>/);
  assert.match(main, /ZEPHYR_VERSION_CODE[\s\S]*BT_LE_ADV_OPT_CONN/);
  assert.match(main, /BT_LE_ADV_PARAM\(/);
  assert.ok(!main.includes("bt_le_adv_start(BT_LE_ADV_CONN"));
});

test("bonding is persistent but approvals are volatile", async () => {
  const config = await read("../prj.conf");
  const main = await read("../src/main.c");
  for (const option of [
    "CONFIG_BT_SMP=y",
    "CONFIG_BT_SMP_SC_ONLY=y",
    "CONFIG_BT_BONDING_REQUIRED=y",
    "CONFIG_BT_SETTINGS=y",
    "CONFIG_SETTINGS=y",
  ]) assert.ok(config.includes(option), `missing ${option}`);
  assert.match(main, /settings_load\(\)/);
  assert.ok(!main.includes("settings_save_one"));
  assert.match(main, /clear_volatile_state_locked[\s\S]*nexting_device_state_disconnect/);
  assert.match(main, /static void disconnected[\s\S]*clear_volatile_state_locked\(\)/);
  assert.match(main, /bt_unpair\(BT_ID_DEFAULT, BT_ADDR_LE_ANY\)/);
  assert.match(main, /K_SECONDS\(3\)/);
  assert.match(main, /static void bond_reset_work_handler[\s\S]*bt_conn_disconnect/);
  assert.match(main, /static void bond_reset_work_handler[\s\S]*clear_volatile_state_locked/);
  assert.match(main, /static void bond_reset_work_handler[\s\S]*bt_unpair\(BT_ID_DEFAULT, BT_ADDR_LE_ANY\)/);
  assert.match(main, /both_pressed_since[\s\S]*K_SECONDS\(3\)/);
  const resetHandler = main.slice(
    main.indexOf("static void bond_reset_work_handler(struct k_work *work)\n{"),
    main.indexOf("static void connected(struct bt_conn *conn"),
  );
  assert.match(resetHandler, /if \(error != 0\)[\s\S]*return;/);
  assert.match(resetHandler, /bt_le_adv_stop\(\)/);
  assert.match(resetHandler, /bond_reset_unpair_succeeded\s*=\s*true/);
  assert.ok(!resetHandler.includes("bond_reset_in_progress = false"));
  assert.ok(!resetHandler.includes("set_pending_led(true)"));
  assert.ok(!resetHandler.includes("start_advertising()"));
  assert.match(
    main,
    /complete_bond_reset_locked[\s\S]*bond_reset_unpair_succeeded[\s\S]*current_connection\s*!=\s*NULL[\s\S]*return false;[\s\S]*bond_reset_in_progress\s*=\s*false[\s\S]*set_pending_led\(true\)/,
  );
  assert.match(main, /static void disconnected[\s\S]*bond_reset_in_progress/);
  assert.match(main, /static void disconnected[\s\S]*complete_bond_reset_locked\(\)/);
  assert.match(main, /write_downlink[\s\S]*bond_reset_in_progress[\s\S]*BT_ATT_ERR_AUTHORIZATION/);
  assert.match(main, /subscription_changed[\s\S]*!bond_reset_in_progress/);
  assert.match(main, /static void connected[\s\S]*bond_reset_in_progress[\s\S]*bt_conn_disconnect/);
  const connectedHandler = main.slice(
    main.indexOf("static void connected"),
    main.indexOf("static void disconnected"),
  );
  const disconnectedHandler = main.slice(
    main.indexOf("static void disconnected"),
    main.indexOf("static void security_changed"),
  );
  const securityHandler = main.slice(
    main.indexOf("static void security_changed"),
    main.indexOf("BT_CONN_CB_DEFINE"),
  );
  assert.match(connectedHandler, /bt_conn_set_security[\s\S]*bt_conn_disconnect/);
  assert.ok(!disconnectedHandler.includes("start_advertising()"));
  assert.match(disconnectedHandler, /schedule_advertising\(\)/);
  assert.match(securityHandler, /error\s*==\s*BT_SECURITY_ERR_SUCCESS[\s\S]*level\s*>=\s*BT_SECURITY_L2[\s\S]*return;[\s\S]*bt_conn_disconnect/);
  assert.match(main, /advertising_work_handler[\s\S]*start_advertising\(\)[\s\S]*k_work_reschedule/);
});

test("uplink uses one fixed callback-driven fragmented frame", async () => {
  const main = await read("../src/main.c");
  assert.match(main, /static char uplink_frame\[/);
  assert.match(main, /bt_gatt_get_mtu\(current_connection\)\s*-\s*3/);
  assert.match(main, /bt_gatt_notify_cb\(/);
  assert.match(main, /static void notification_sent[\s\S]*uplink_frame_offset\s*\+=/);
  assert.match(main, /uplink_frame_offset\s*>=\s*uplink_frame_length[\s\S]*next_retry_ms/);
  assert.match(main, /clear_uplink_locked[\s\S]*uplink_frame_length\s*=\s*0/);
  assert.match(main, /subscription_changed[\s\S]*clear_uplink_locked/);
  assert.match(main, /disconnected[\s\S]*clear_uplink_locked/);
  assert.match(main, /clear_uplink_locked[\s\S]*uplink_resync_required\s*=\s*true/);
  assert.match(main, /queue_uplink_locked[\s\S]*uplink_resync_required[\s\S]*'\\n'/);
  const queueHandler = main.slice(
    main.indexOf("static bool queue_uplink_locked"),
    main.indexOf("static void notification_sent"),
  );
  const notificationHandler = main.slice(
    main.indexOf("static void notification_sent"),
    main.indexOf("static void pump_uplink"),
  );
  assert.ok(
    !queueHandler.includes("uplink_resync_required = false"),
    "queueing a delimiter must not claim it was delivered",
  );
  assert.match(notificationHandler, /uplink_frame_has_resync_prefix[\s\S]*uplink_resync_required\s*=\s*false/);
  assert.match(main, /clear_uplink_locked[\s\S]*uplink_notification_in_flight[\s\S]*uplink_resync_required\s*=\s*true/);
  assert.ok(!main.includes("bt_gatt_notify("), "uplink must not bypass completion callbacks");
});

test("XIAO overlays map two explicit actions and a visible state", async () => {
  for (const board of ["xiao_ble", "xiao_esp32c3", "xiao_esp32s3"]) {
    const overlay = await read(`../boards/${board}.overlay`);
    assert.match(overlay, /nexting-allow/);
    assert.match(overlay, /nexting-deny/);
    assert.match(overlay, /nexting-led/);
    assert.match(overlay, /<&xiao_d 0/);
    assert.match(overlay, /<&xiao_d 1/);
  }
});

test("public board claims match the exact build matrix", async () => {
  const rootReadme = await read("../../../README.md");
  const hardwareSupport = await read("../../../docs/hardware-support.md");
  for (const supported of [
    "nRF52840 DK",
    "XIAO nRF52840",
    "XIAO ESP32C3",
    "XIAO ESP32S3",
  ]) {
    assert.ok(rootReadme.includes(supported), `README misses ${supported}`);
    assert.ok(hardwareSupport.includes(supported), `hardware support misses ${supported}`);
  }
  for (const unsupported of ["ESP32-C3-DevKitM", "ESP32-S3-DevKitC"]) {
    assert.ok(!rootReadme.includes(unsupported), `README overclaims ${unsupported}`);
    assert.ok(
      !hardwareSupport.includes(unsupported),
      `hardware support overclaims ${unsupported}`,
    );
  }
});

test("first choice stays locked through retry and expiry", async () => {
  const main = await read("../src/main.c");
  assert.match(main, /NEXTING_DEVICE_CHOICE_ALLOW/);
  assert.match(main, /NEXTING_DEVICE_CHOICE_DENY/);
  assert.match(main, /nexting_device_state_retry_answer/);
  assert.match(main, /nexting_device_state_tick/);
  assert.match(main, /nexting_device_stream_reset/);
});
