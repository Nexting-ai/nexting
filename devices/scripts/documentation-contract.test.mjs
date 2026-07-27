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
  assert.match(interfaces, /no public TCP, UDP, HTTP, MQTT, or WebSocket endpoint/);
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
    assert.ok(tracks.includes(marker), "missing implementation marker: " + marker);
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
    assert.ok(conformance.includes(marker), "missing conformance marker: " + marker);
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
  assert.doesNotMatch(development, /Espressif targets still require an upstream/);
  assert.doesNotMatch(development, /ESP32 builds.*pending/i);
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
  const [agents, contributing, swift, c, firmware, porting] = await Promise.all([
    read("AGENTS.md"),
    read("CONTRIBUTING.md"),
    read("sdk/swift/README.md"),
    read("sdk/c/README.md"),
    read("firmware/zephyr/README.md"),
    read("docs/porting-guide.md"),
  ]);

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

test("public Quickstart is self-serve and honest about the public App gate", async () => {
  const [overview, index, quickstart, troubleshooting, firstApproval] =
    await Promise.all([
      read("README.md"),
      read("docs/README.md"),
      read("QUICKSTART.md"),
      read("docs/troubleshooting.md"),
      read("docs/first-approval.md"),
    ]);

  for (const source of [overview, index]) {
    assert.match(source, /QUICKSTART\.md/);
    assert.match(source, /troubleshooting\.md/);
  }
  for (const marker of [
    "devices-v0.2.0-experimental.1",
    "XIAO nRF52840",
    "D0",
    "D1",
    "bootstrap-zephyr.sh",
    "nexting-device-host-smoke",
    "PASS answer=",
    "App Store 2.4",
  ]) {
    assert.ok(quickstart.includes(marker), "Quickstart missing marker: " + marker);
  }
  assert.doesNotMatch(quickstart, /private Nexting App|Debug build/);
  assert.doesNotMatch(firstApproval, /current private Nexting App|Debug build/);
  assert.match(troubleshooting, /west: unknown command "build"/);
  assert.match(troubleshooting, /Bluetooth/);
  assert.match(troubleshooting, /Device Info/);
});

test("Swift package exposes the documented public Host smoke executable", async () => {
  const [manifest, source, readme] = await Promise.all([
    read("sdk/swift/Package.swift"),
    read("sdk/swift/Sources/NextingDeviceHostSmoke/main.swift"),
    read("sdk/swift/README.md"),
  ]);

  for (const marker of ["nexting-device-host-smoke", "NextingDeviceHostSmoke"]) {
    assert.ok(manifest.includes(marker), `Swift manifest missing ${marker}`);
  }
  for (const marker of [
    "onDiscovered",
    "connectedDeviceInfo",
    "Device Info",
    "PASS answer=",
    "Bluetooth",
  ]) {
    assert.ok(source.includes(marker), `Host smoke source missing ${marker}`);
  }
  assert.match(readme, /nexting-device-host-smoke/);
});

test("public firmware workflow publishes pinned self-describing tag assets", async () => {
  const workflow = await read(
    "scripts/public-workflows/nexting-devices-firmware.yml",
  );
  for (const marker of [
    "devices-v*",
    "artifact-manifest.json",
    "SHA256SUMS",
    '"zephyr": "4.3.0"',
    '"zephyrSdk": "0.17.4"',
    '"west": "1.5.0"',
    '"flash": "%s"',
    '"evidence": "Build verified"',
    '"boardVerified": false',
    "gh release create",
  ]) {
    assert.ok(workflow.includes(marker), `firmware release workflow missing ${marker}`);
  }
});

test("public workflows use current Node 24 action majors", async () => {
  const [ci, firmware] = await Promise.all([
    read("scripts/public-workflows/nexting-devices-ci.yml"),
    read("scripts/public-workflows/nexting-devices-firmware.yml"),
  ]);
  const workflows = `${ci}\n${firmware}`;
  for (const marker of [
    "actions/checkout@v7",
    "actions/setup-node@v7",
    "actions/setup-python@v7",
    "actions/setup-java@v5",
    "gradle/actions/setup-gradle@v6",
    "actions/upload-artifact@v7",
    "actions/download-artifact@v8",
  ]) {
    assert.ok(workflows.includes(marker), `public workflows missing ${marker}`);
  }
  assert.doesNotMatch(
    workflows,
    /actions\/(?:checkout|setup-node|setup-python|setup-java)@v4/,
  );
});

test("Kotlin has a checksum-pinned one-command launcher", async () => {
  const [launcher, readme, workflow] = await Promise.all([
    read("sdk/kotlin/gradlew"),
    read("sdk/kotlin/README.md"),
    read("scripts/public-workflows/nexting-devices-ci.yml"),
  ]);
  for (const marker of [
    'gradle_version="9.0.0"',
    "8fad3d78296ca518113f3d29016617c7f9367dc005f932bd9d93bf45ba46072b",
    "services.gradle.org/distributions",
    "checksum mismatch",
  ]) {
    assert.ok(launcher.includes(marker), `Kotlin launcher missing ${marker}`);
  }
  assert.match(readme, /\.\/gradlew test/);
  assert.match(workflow, /\.\/gradlew test/);
});
