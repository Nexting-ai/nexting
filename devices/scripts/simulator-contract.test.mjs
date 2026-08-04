import assert from "node:assert/strict";
import { access, readFile } from "node:fs/promises";
import test from "node:test";

const read = (path) => readFile(new URL(path, import.meta.url), "utf8");

test("public package keeps the custom Host simulator private", async () => {
  await assert.rejects(access(new URL("../examples/macos-device-simulator/", import.meta.url)), { code: "ENOENT" });
  const workflow = await read("../scripts/public-workflows/nexting-devices-ci.yml");
  assert.doesNotMatch(workflow, /NextingDeviceKit|macos-device-simulator|swift test/);
  assert.match(workflow, /npm run check/);
});
