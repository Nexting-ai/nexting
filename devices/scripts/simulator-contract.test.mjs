import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const read = (path) => readFile(new URL(path, import.meta.url), "utf8");

async function readPublicCIWorkflow() {
  try {
    return await read("../.github/workflows/ci.yml");
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
    return read("../../.github/workflows/nexting-devices-ci.yml");
  }
}

function between(source, start, end) {
  const startIndex = source.indexOf(start);
  const endIndex = source.indexOf(end, startIndex + start.length);
  assert.notEqual(startIndex, -1, `missing start marker: ${start}`);
  assert.notEqual(endIndex, -1, `missing end marker: ${end}`);
  return source.slice(startIndex, endIndex);
}

test("public simulator keeps the protocol and BLE transport contracts", async () => {
  const [simulator, workflow] = await Promise.all([
    read("../examples/macos-device-simulator/main.swift"),
    readPublicCIWorkflow(),
  ]);
  for (const suffix of ["0001", "0002", "0003", "0004"]) {
    assert.ok(simulator.includes(`6EADC0DE-${suffix}-4A21-9C5E-1B7F3D9E42A0`));
  }
  assert.match(simulator, /NextingDeviceCodec\.decode/);
  assert.match(simulator, /NextingDeviceCodec\.encode/);
  assert.match(simulator, /0\.2\.0-experimental\.2/);
  assert.match(simulator, /button_count/);
  assert.match(simulator, /approval_button_count/);
  assert.match(simulator, /systemUptime/);
  assert.match(simulator, /deadlineMs/);
  assert.ok(!simulator.includes("JSONSerialization"));
  assert.match(simulator, /notifyEncryptionRequired/);
  assert.match(simulator, /writeEncryptionRequired/);
  assert.match(simulator, /subscribedCentral\.identifier == request\.central\.identifier/);
  assert.match(simulator, /insufficientAuthorization/);
  assert.match(simulator, /maximumUpdateValueLength/);
  assert.match(simulator, /outboundFrameOffset/);
  assert.match(simulator, /peripheralManagerIsReady/);
  assert.match(simulator, /lockedChoice/);
  const completedOversize = between(
    simulator,
    "if buffer.count + fragment.count >= maximumMessageBytes {",
    "} else {",
  );
  assert.match(completedOversize, /buffer\.removeAll/);
  const flush = between(
    simulator,
    "private func flushOutboundFrame()",
    "func peripheralManagerIsReady(toUpdateSubscribers",
  );
  assert.match(flush, /approval\.deadlineMs > monotonicNowMs\(\)/);
  assert.match(workflow, /NextingDeviceKit\/Protocol\.swift/);
});
