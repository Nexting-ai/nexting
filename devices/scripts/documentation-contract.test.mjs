import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const root = new URL("../", import.meta.url);
const read = (path) => readFile(new URL(path, root), "utf8");

test("foundation guide maps product behavior to real source files", async () => {
  const foundation = await read("docs/foundation-development.md");

  for (const marker of [
    "The product boundary",
    "One approval lifecycle",
    "Dependency direction",
    "Protocol.swift",
    "Coordinator.swift",
    "nexting_device.h",
    "firmware/zephyr/src/main.c",
    "Fail closed",
    "What to change",
    "capability declaration",
    "capability roadmap",
    "agent anywhere",
    "needs-input/error states for LEDs or screens, full replacement, volatile",
  ]) {
    assert.match(foundation, new RegExp(marker.replaceAll(".", "\\.")));
  }

  assert.match(foundation, /does not receive Agent credentials/);
  assert.match(foundation, /does not define a public cloud API/);
});

test("interface catalog exposes only the supported public surfaces", async () => {
  const interfaces = await read("docs/interfaces.md");

  for (const suffix of ["0001", "0002", "0003", "0004"]) {
    assert.match(interfaces, new RegExp("6EADC0DE-" + suffix));
  }
  for (const marker of [
    "present",
    "answer",
    "resolved",
    "error",
    "status/1",
    "statusSlots",
    "publishStatus",
    "nexting_device_status_on_message",
    "status-v1.json",
    "NextingDeviceRelayCoordinator",
    "NextingDeviceCentral",
    "nexting_device_decode",
    "nexting_device_stream_push",
    "nexting_device_state_choose",
    "reference/js/src/protocol.mjs",
    "sdk/swift/Sources/NextingDeviceKit/Central.swift",
    "sdk/c/src/nexting_device.c",
    "npm run test:reference",
    "ctest --test-dir",
  ]) {
    assert.match(interfaces, new RegExp(marker));
  }

  assert.match(interfaces, /does not expose a Nexting cloud API/);
  assert.match(
    interfaces,
    /no public TCP, UDP, HTTP, MQTT, or WebSocket endpoint/,
  );
});

test("implementation tracks cover every supported developer route", async () => {
  const tracks = await read("docs/implementation-tracks.md");

  for (const marker of [
    "Track 1: Run a reference board",
    "Track 2: Integrate a Host or App",
    "Track 3: Port a new MCU or RTOS",
    "Track 4: Maintain a new language SDK",
    "nrf52840dk/nrf52840",
    "xiao_ble/nrf52840/sense",
    "xiao_esp32c3/esp32c3",
    "xiao_esp32s3/esp32s3/procpu",
    "answerSucceeded",
    "answerFailed",
    "nexting_device_state_disconnect",
    "approval-v1.json",
  ]) {
    assert.ok(
      tracks.includes(marker),
      "missing implementation marker: " + marker,
    );
  }
});

test("conformance guide keeps evidence levels literal", async () => {
  const conformance = await read("docs/conformance.md");

  for (const marker of [
    "Protocol conformant",
    "Core tested",
    "Build verified",
    "Board verified",
    "Nexting Compatible",
    "npm run test:reference",
    "swift test --package-path sdk/swift",
    "NEXTING_DEVICE_SANITIZE=ON",
    "npm run test:firmware",
  ]) {
    assert.ok(
      conformance.includes(marker),
      "missing conformance marker: " + marker,
    );
  }

  assert.match(conformance, /Build verified is not Board verified/);
  assert.match(conformance, /not available during Experimental 0\.2/);
});

test("reader entry points link the public documentation system", async () => {
  const [overview, index, development] = await Promise.all([
    read("README.md"),
    read("docs/README.md"),
    read("docs/development.md"),
  ]);

  for (const link of [
    "docs/foundation-development.md",
    "docs/interfaces.md",
    "docs/implementation-tracks.md",
    "docs/conformance.md",
    "docs/use-cases.md",
  ]) {
    assert.ok(overview.includes(link), "README missing link: " + link);
  }

  for (const link of [
    "foundation-development.md",
    "interfaces.md",
    "implementation-tracks.md",
    "conformance.md",
    "use-cases.md",
  ]) {
    assert.ok(index.includes(link), "docs index missing link: " + link);
  }

  assert.match(development, /All four reference targets are Build verified/);
  assert.doesNotMatch(
    development,
    /Espressif targets still require an upstream/,
  );
  assert.doesNotMatch(development, /ESP32 builds.*pending/i);
});

test("the first MultiPad case pins its source and keeps flash claims honest", async () => {
  const [caseGuide, multipad] = await Promise.all([
    read("docs/cases/multipad-first-case.md"),
    read("docs/multipad-usb.md"),
  ]);

  for (const marker of [
    "iLx11/multi-pad",
    "78c1ee533a7f513e9f390741c4f5eed1e0aa91b3",
    "PA13 = SWDIO",
    "PA14 = SWCLK",
    "nexting_multipad_adapter.c/.h",
    "AA BB xx",
    "present → answer → resolved",
    "Module PCB",
    "FPC PCB",
    "原版固件",
    "Nexting HEX",
  ]) {
    assert.ok(
      caseGuide.includes(marker),
      "case guide missing marker: " + marker,
    );
  }

  assert.match(caseGuide, /不能把原固件误当作 Nexting 固件/);
  assert.match(caseGuide, /不能.*直接 Type-C 烧录|Type-C.*bootloader/);
  assert.match(multipad, /cases\/multipad-first-case\.md/);
});

test("use-case guide maps scenarios to real profiles and limits", async () => {
  const useCases = await read("docs/use-cases.md");

  for (const marker of [
    "approval/1",
    "status/1",
    "statusSlots",
    "conformance.md",
    "implementation-tracks.md",
  ]) {
    assert.ok(useCases.includes(marker), "use cases missing marker: " + marker);
  }
  assert.match(useCases, /roadmap/i);
});

test("Agent and maintainer guides use the same sources and claims", async () => {
  const [agents, contributing, swift, c, firmware, porting] = await Promise.all(
    [
      read("AGENTS.md"),
      read("CONTRIBUTING.md"),
      read("sdk/swift/README.md"),
      read("sdk/c/README.md"),
      read("firmware/zephyr/README.md"),
      read("docs/porting-guide.md"),
    ],
  );

  for (const marker of [
    "docs/foundation-development.md",
    "docs/interfaces.md",
    "docs/implementation-tracks.md",
    "docs/conformance.md",
  ]) {
    assert.ok(agents.includes(marker), "AGENTS missing source: " + marker);
  }

  assert.ok(contributing.includes("docs/conformance.md"));
  for (const component of [swift, c, firmware, porting]) {
    assert.match(component, /foundation-development\.md/);
    assert.match(component, /interfaces\.md|implementation-tracks\.md/);
  }
});
