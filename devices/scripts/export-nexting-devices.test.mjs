import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { cp, lstat, mkdir, mkdtemp, readFile, rm, symlink, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { promisify } from "node:util";

import { exportNextingDevices } from "./export-nexting-devices.mjs";

const source = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const execFileAsync = promisify(execFile);

async function fixture() {
  const root = await mkdtemp(join(tmpdir(), "nexting-export-test-"));
  const publicCheckout = join(root, "public");
  await mkdir(join(publicCheckout, ".git"), { recursive: true });
  await mkdir(join(publicCheckout, "unrelated"), { recursive: true });
  await writeFile(join(publicCheckout, "README.md"), "# Nexting\n\nExisting introduction.\n");
  await writeFile(join(publicCheckout, "LICENSE"), "existing public license\n");
  await writeFile(join(publicCheckout, "unrelated", "keep.txt"), "keep\n");
  return { root, publicCheckout };
}

async function copiedSource(root) {
  const destination = join(root, "source");
  await cp(source, destination, {
    recursive: true,
    filter: (path) => !/(?:^|\/)(?:\.build|\.gradle|\.kotlin|build|node_modules)(?:\/|$)/.test(path),
  });
  return destination;
}

test("exports a deterministic devices subtree without touching unrelated public files", async () => {
  const { root, publicCheckout } = await fixture();
  try {
    await exportNextingDevices({ source, publicCheckout, check: false });
    assert.match(await readFile(join(publicCheckout, "README.md"), "utf8"), /Build hardware for Nexting/);
    await assert.rejects(readFile(join(publicCheckout, "Package.swift")), { code: "ENOENT" });
    assert.match(await readFile(join(publicCheckout, "devices", "SHA256SUMS"), "utf8"), /SPEC\.md/);
    assert.match(
      await readFile(
        join(publicCheckout, "devices", "docs", "availability.json"),
        "utf8",
      ),
      /"fallback": "firmware-tests"/,
    );
    assert.match(
      await readFile(
        join(
          publicCheckout,
          "devices",
          "docs",
          "reference-approval-controller.md",
        ),
        "utf8",
      ),
      /Developer Reference/,
    );
    assert.match(
      await readFile(
        join(publicCheckout, ".github", "workflows", "nexting-devices-ci.yml"),
        "utf8",
      ),
      /npm run check/,
    );
    assert.equal(await readFile(join(publicCheckout, "LICENSE"), "utf8"), "existing public license\n");
    assert.equal(await readFile(join(publicCheckout, "unrelated", "keep.txt"), "utf8"), "keep\n");
    await exportNextingDevices({ source, publicCheckout, check: true });
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("CLI executes when invoked through a symlinked checkout path", async () => {
  const { root, publicCheckout } = await fixture();
  const entrypoint = fileURLToPath(new URL("./export-nexting-devices.mjs", import.meta.url));
  const linkedEntrypoint = join(root, "export-nexting-devices.mjs");
  try {
    await symlink(entrypoint, linkedEntrypoint);
    const { stdout } = await execFileAsync(process.execPath, [
      linkedEntrypoint,
      "--source",
      source,
      "--public-checkout",
      publicCheckout,
    ]);
    assert.match(stdout, /public export passed/);
    assert.match(
      await readFile(join(publicCheckout, "devices", "QUICKSTART.md"), "utf8"),
      /Nexting device ⇄ encrypted BLE ⇄ trusted Host ⇄ Agent integration/,
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("rejects unknown roots and sensitive paths", async () => {
  const { root, publicCheckout } = await fixture();
  try {
    const candidate = await copiedSource(root);
    await writeFile(join(candidate, "private-app.swift"), "private\n");
    await assert.rejects(
      exportNextingDevices({ source: candidate, publicCheckout, check: false }),
      /not present in the export allowlist/,
    );
    await rm(join(candidate, "private-app.swift"));
    await writeFile(join(candidate, "docs", ".env.production"), "TOKEN=not-a-real-token\n");
    await assert.rejects(
      exportNextingDevices({ source: candidate, publicCheckout, check: false }),
      /forbidden path/,
    );
    await rm(join(candidate, "docs", ".env.production"));
    await writeFile(join(candidate, "docs", "unexpected.md"), "looks public\n");
    await assert.rejects(
      exportNextingDevices({ source: candidate, publicCheckout, check: false }),
      /not present in the export allowlist/,
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("rejects private content and symlinks", async () => {
  const { root, publicCheckout } = await fixture();
  try {
    const candidate = await copiedSource(root);
    const privatePath = ["hardware", "internal/"].join("-");
    await writeFile(join(candidate, "docs", "bad.md"), `do not export ${privatePath}\n`);
    await assert.rejects(
      exportNextingDevices({ source: candidate, publicCheckout, check: false }),
      /private-boundary rule/,
    );
    await rm(join(candidate, "docs", "bad.md"));
    await symlink(join(candidate, "README.md"), join(candidate, "docs", "linked-readme.md"));
    await assert.rejects(
      exportNextingDevices({ source: candidate, publicCheckout, check: false }),
      /symlinks are forbidden/,
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("refuses to overwrite an unrelated root Swift package", async () => {
  const { root, publicCheckout } = await fixture();
  try {
    await writeFile(join(publicCheckout, "Package.swift"), "// unrelated package\n");
    await assert.rejects(
      exportNextingDevices({ source, publicCheckout, check: false }),
      /unrelated public Package\.swift/,
    );
    assert.equal(await lstat(join(publicCheckout, "unrelated", "keep.txt")).then((item) => item.isFile()), true);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("the exported devices tree can reproduce itself", async () => {
  const first = await fixture();
  const second = await fixture();
  try {
    await exportNextingDevices({
      source,
      publicCheckout: first.publicCheckout,
      check: false,
    });
    await exportNextingDevices({
      source: join(first.publicCheckout, "devices"),
      publicCheckout: second.publicCheckout,
      check: false,
    });
    assert.equal(
      await readFile(join(first.publicCheckout, "devices", "SHA256SUMS"), "utf8"),
      await readFile(join(second.publicCheckout, "devices", "SHA256SUMS"), "utf8"),
    );
  } finally {
    await rm(first.root, { recursive: true, force: true });
    await rm(second.root, { recursive: true, force: true });
  }
});
