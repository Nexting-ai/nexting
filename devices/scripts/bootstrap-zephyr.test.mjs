import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { readFile } from "node:fs/promises";
import { promisify } from "node:util";
import { fileURLToPath } from "node:url";
import test from "node:test";

const execFileAsync = promisify(execFile);
const script = fileURLToPath(new URL("./bootstrap-zephyr.sh", import.meta.url));

test("real setup enters the workspace before initializing or updating west", async () => {
  const source = await readFile(script, "utf8");
  const enterWorkspace = source.indexOf('cd "$workspace_dir"');
  const initialize = source.indexOf('"$west_bin" init');
  const update = source.indexOf('"$west_bin" update');
  assert.ok(enterWorkspace > 0);
  assert.ok(initialize > enterWorkspace);
  assert.ok(update > enterWorkspace);
});

test("dry run resolves the golden XIAO build without changing the workspace", async () => {
  const { stdout } = await execFileAsync(script, [
    "--board",
    "xiao-nrf52840-sense",
    "--build",
    "--dry-run",
  ]);

  for (const marker of [
    "West version: 1.5.0",
    "Zephyr revision: v4.3.0",
    "Board: xiao_ble/nrf52840/sense",
    "EXTRA_CONF_FILE=debug-test-device.conf",
    "zephyr.uf2",
  ]) {
    assert.ok(stdout.includes(marker), `dry run missing ${marker}`);
  }
});

test("unknown boards fail with the supported aliases", async () => {
  await assert.rejects(
    execFileAsync(script, ["--board", "mystery-board", "--dry-run"]),
    (error) => {
      assert.equal(error.code, 64);
      assert.match(error.stderr, /xiao-nrf52840-sense/);
      return true;
    },
  );
});
