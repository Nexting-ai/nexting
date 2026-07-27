import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const packageJSON = JSON.parse(
  await readFile(new URL("../package.json", import.meta.url), "utf8"),
);
const releasePackageJSON = JSON.parse(
  await readFile(new URL("../../../package.json", import.meta.url), "utf8"),
);
const changelog = await readFile(
  new URL("../../../CHANGELOG.md", import.meta.url),
  "utf8",
);
const readme = await readFile(
  new URL("../../../README.md", import.meta.url),
  "utf8",
);

test("reference package stays private and exposes explicit entry points", async () => {
  assert.equal(packageJSON.name, "@nexting-ai/device-reference");
  assert.equal(packageJSON.private, true);
  assert.deepEqual(packageJSON.exports, {
    "./protocol": "./src/protocol.mjs",
    "./device-info": "./src/device-info.mjs",
    "./framing": "./src/framing.mjs",
    "./relay": "./src/relay.mjs",
  });

  const protocol = await import("@nexting-ai/device-reference/protocol");
  const deviceInfo = await import("@nexting-ai/device-reference/device-info");
  const framing = await import("@nexting-ai/device-reference/framing");
  const relay = await import("@nexting-ai/device-reference/relay");
  assert.equal(typeof protocol.decode, "function");
  assert.equal(typeof deviceInfo.decodeDeviceInfo, "function");
  assert.equal(typeof framing.createLineDecoder, "function");
  assert.equal(typeof relay.createApprovalRelay, "function");
});

test("public release identity is Experimental 0.2", () => {
  assert.equal(releasePackageJSON.version, "0.2.0-experimental.1");
  assert.equal(packageJSON.version, "0.2.0-experimental.1");
  assert.match(changelog, /## 0\.2\.0-experimental\.1/);
  assert.match(readme, /## Experimental 0\.2/);
});
