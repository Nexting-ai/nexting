import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import { readFile } from "node:fs/promises";
import test from "node:test";

const root = new URL("../", import.meta.url);
const read = (path) => readFile(new URL(path, root), "utf8");

test("foundation guide maps device behavior to public source files", async () => {
  const foundation = await read("docs/foundation-development.md");
  for (const marker of [
    "The product boundary",
    "One approval lifecycle",
    "Dependency direction",
    "nexting_device.h",
    "firmware/zephyr/src/main.c",
    "Official App boundary",
    "What to change",
    "capability declaration",
    "capability set",
    "agent anywhere",
    "needs-input/error states for LEDs or screens, full replacement, volatile",
  ]) {
    assert.match(foundation, new RegExp(marker.replaceAll(".", "\\.")));
  }
  assert.match(foundation, /does not receive Agent credentials/);
  assert.match(foundation, /does not define a public cloud API/);
  assert.doesNotMatch(foundation, /NextingDeviceKit|Protocol\.swift|Coordinator\.swift/);
});

test("interface catalog exposes only the device-side public surfaces", async () => {
  const interfaces = await read("docs/interfaces.md");
  for (const suffix of ["0001", "0002", "0003", "0004"]) {
    assert.match(interfaces, new RegExp("6EADC0DE-" + suffix));
  }
  for (const marker of [
    "present", "answer", "resolved", "error", "status/1", "statusSlots",
    "nexting_device_status_on_message", "status-v1.json", "navigation/1",
    "keys/1", "rotary/1", "voice/1", "text/1", "usage/1", "config/1",
    "navigation-v1.json", "key_event", "rotary_event", "voice_event",
    "config_result", "nexting_device_decode", "nexting_device_stream_push",
    "nexting_device_state_choose", "reference/js/src/protocol.mjs",
    "sdk/c/src/nexting_device.c", "npm run test:reference", "ctest --test-dir",
  ]) {
    assert.match(interfaces, new RegExp(marker));
  }
  assert.match(interfaces, /official (?:Nexting )?App/);
  assert.match(interfaces, /does not expose a Nexting cloud API/);
  assert.doesNotMatch(interfaces, /NextingDeviceKit|NextingDeviceCentral|sdk\/swift|sdk\/kotlin/);
});

test("the public package keeps Agent adapters and custom App SDKs private", async () => {
  const [overview, index, interfaces, manifest, availability] = await Promise.all([
    read("README.md"), read("docs/README.md"), read("docs/interfaces.md"),
    read("scripts/export-manifest.json"), read("docs/availability.json").then(JSON.parse),
  ]);
  for (const source of [overview, index, interfaces]) {
    assert.match(source, /official (?:Nexting )?App/i);
    assert.doesNotMatch(source, /sdk\/swift|sdk\/kotlin|codex-app-server\.md/);
  }
  assert.doesNotMatch(manifest, /sdk\/swift|sdk\/kotlin|codex-app-server|macos-device-simulator/);
  assert.equal(availability.fallback, "firmware-tests");
});

test("implementation tracks cover board, MCU, and protocol-tooling routes", async () => {
  const tracks = await read("docs/implementation-tracks.md");
  for (const marker of [
    "Track 1: Run a reference board", "Track 2: Port a new MCU or RTOS",
    "Track 3: Maintain protocol tooling", "nrf52840dk/nrf52840",
    "xiao_ble/nrf52840/sense", "xiao_esp32c3/esp32c3", "xiao_esp32s3/esp32s3/procpu",
    "nexting_device_state_disconnect", "approval-v1.json",
  ]) {
    assert.ok(tracks.includes(marker), "missing implementation marker: " + marker);
  }
  assert.doesNotMatch(tracks, /Host SDK|Track 4/);
});

test("conformance guide keeps evidence levels literal", async () => {
  const conformance = await read("docs/conformance.md");
  for (const marker of [
    "Protocol conformant", "Core tested", "Build verified", "Board verified",
    "Nexting Compatible", "npm run test:reference", "NEXTING_DEVICE_SANITIZE=ON",
    "npm run test:firmware",
  ]) assert.ok(conformance.includes(marker), "missing conformance marker: " + marker);
  assert.doesNotMatch(conformance, /swift test --package-path sdk\/swift/);
  assert.match(conformance, /Build verified is not Board verified/);
});

test("reader entry points link the public documentation system", async () => {
  const [overview, index, development] = await Promise.all([
    read("README.md"), read("docs/README.md"), read("docs/development.md"),
  ]);
  for (const link of [
    "docs/foundation-development.md", "docs/interfaces.md", "docs/implementation-tracks.md",
    "docs/conformance.md", "docs/use-cases.md",
  ]) assert.ok(overview.includes(link), "README missing link: " + link);
  for (const link of [
    "foundation-development.md", "interfaces.md", "implementation-tracks.md",
    "conformance.md", "use-cases.md",
  ]) assert.ok(index.includes(link), "docs index missing link: " + link);
  assert.match(development, /All four reference targets are Build verified/);
  assert.doesNotMatch(development, /swift|kotlin|Host SDK/i);
});

test("0.2 experimental.2 documents every frozen interaction profile", async () => {
  const [overview, changelog, projectStatus, c, js] = await Promise.all([
    read("README.md"), read("CHANGELOG.md"), read("docs/project-status.md"),
    read("sdk/c/README.md"), read("reference/js/README.md"),
  ]);
  for (const source of [overview, changelog, projectStatus, c]) {
    assert.match(source, /0\.2\.0-experimental\.2/);
    assert.doesNotMatch(source, /Swift 6|Kotlin SDK|NextingDeviceKit/);
  }
  assert.match(js, /Experimental 0\.2/);
  for (const profile of ["navigation/1", "keys/1", "rotary/1", "voice/1", "text/1", "usage/1", "config/1"]) {
    assert.match(overview, new RegExp(profile.replace("/", "\\/")));
    assert.match(projectStatus, new RegExp(profile.replace("/", "\\/")));
  }
  assert.match(overview, /Host microphone/i);
  assert.match(changelog, /frozen interaction profiles/i);
});

test("Agent and maintainer guides use the same public sources", async () => {
  const [agents, contributing, c, firmware, porting] = await Promise.all([
    read("AGENTS.md"), read("CONTRIBUTING.md"), read("sdk/c/README.md"),
    read("firmware/zephyr/README.md"), read("docs/porting-guide.md"),
  ]);
  for (const marker of ["docs/foundation-development.md", "docs/interfaces.md", "docs/implementation-tracks.md", "docs/conformance.md"]) {
    assert.ok(agents.includes(marker), "AGENTS missing source: " + marker);
  }
  assert.ok(contributing.includes("docs/conformance.md"));
  for (const component of [c, firmware, porting]) {
    assert.match(component, /foundation-development\.md/);
    assert.match(component, /interfaces\.md|implementation-tracks\.md/);
  }
});

test("public Quickstart explains the product before optional reference hardware", async () => {
  const referenceControllerUrl = new URL("docs/reference-approval-controller.md", root);
  const availabilityUrl = new URL("docs/availability.json", root);
  assert.ok(existsSync(referenceControllerUrl));
  assert.ok(existsSync(availabilityUrl));
  const [overview, index, quickstart, referenceController, availability, packageJson, security, troubleshooting, firstApproval] = await Promise.all([
    read("README.md"), read("docs/README.md"), read("QUICKSTART.md"),
    read("docs/reference-approval-controller.md"), read("docs/availability.json").then(JSON.parse),
    read("package.json").then(JSON.parse), read("SECURITY.md"),
    read("docs/troubleshooting.md"), read("docs/first-approval.md"),
  ]);
  for (const source of [overview, index]) {
    assert.match(source, /QUICKSTART\.md/);
    assert.match(source, /troubleshooting\.md/);
  }
  for (const marker of ["Nexting device", "encrypted BLE", "trusted Host", "Agent integration", "Use a supported first-party Nexting product", "Build with the Nexting SDK", "First remote interaction", "local device proof", "third-party developer-device enrollment"]) {
    assert.ok(quickstart.includes(marker), "Quickstart missing marker: " + marker);
  }
  assert.doesNotMatch(quickstart, /Build your first Nexting device|two-button Nexting device|D0 to GND|D1 to GND/);
  for (const marker of ["Build the reference approval controller", "Developer Reference", "approval/1", "XIAO nRF52840", "D0", "D1", "bootstrap-zephyr.sh", "official Nexting App"]) {
    assert.ok(referenceController.includes(marker), "reference controller missing marker: " + marker);
  }
  assert.equal(availability.sdkVersion, packageJson.version);
  assert.equal(availability.wireMajor, 1);
  assert.equal(availability.fallback, "firmware-tests");
  for (const marker of ["BLE LE Secure Connections", "encrypted GATT", "opaque request ID", "single consumption", "duplicate", "replay", "disconnect", "Just Works", "authenticated application identity", "voice/1", "never carries audio bytes or transcripts"]) {
    assert.ok(security.includes(marker), "Security missing marker: " + marker);
  }
  assert.doesNotMatch(firstApproval, /swift|NextingDeviceKit|Host SDK/i);
  assert.match(troubleshooting, /west: unknown command "build"/);
  assert.match(troubleshooting, /Bluetooth/);
  assert.match(troubleshooting, /Device Info/);
});

test("public firmware workflow publishes pinned self-describing tag assets", async () => {
  const workflow = await read("scripts/public-workflows/nexting-devices-firmware.yml");
  for (const marker of ["devices-v*", "artifact-manifest.json", "SHA256SUMS", '"zephyr": "4.3.0"', '"zephyrSdk": "0.17.4"', '"west": "1.5.0"', '"flash": "%s"', '"evidence": "Build verified"', '"boardVerified": false', "gh release create"]) {
    assert.ok(workflow.includes(marker), `firmware release workflow missing ${marker}`);
  }
});

test("public workflows use current Node action majors and no Host SDK jobs", async () => {
  const [ci, firmware] = await Promise.all([
    read("scripts/public-workflows/nexting-devices-ci.yml"), read("scripts/public-workflows/nexting-devices-firmware.yml"),
  ]);
  const workflows = `${ci}\n${firmware}`;
  for (const marker of ["actions/checkout@v7", "actions/setup-node@v7", "actions/setup-python@v7", "actions/upload-artifact@v7"]) {
    assert.ok(workflows.includes(marker), `public workflows missing ${marker}`);
  }
  assert.doesNotMatch(workflows, /NextingDeviceKit|gradle|swift test|sdk\/kotlin/);
});
