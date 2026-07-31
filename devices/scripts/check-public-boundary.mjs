import { lstat, readdir, readFile } from "node:fs/promises";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const privateRepositoryRoot = resolve(root, "../..");
const ignoredDirectories = new Set([
  ".build",
  ".git",
  "build",
  "build-sanitize",
]);
export const forbiddenPatterns = [
  new RegExp(["hardware", "internal/"].join("-")),
  new RegExp(["CCSession", "Model"].join("")),
  new RegExp(["term", "Id|session", "Id|controlRequest", "Id"].join("")),
  new RegExp(["pinclaw", "odp", "authorized", "peripheral", "uuids"].join("_")),
  new RegExp(["BEGIN ", "(?:RSA |EC |OPENSSH )?", "PRIVATE KEY"].join("")),
  new RegExp(["supa", "base"].join(""), "i"),
];
const obsoletePrivateSources = [
  join(privateRepositoryRoot, "clients", ["od", "p-firmware-ref"].join("")),
  join(privateRepositoryRoot, "clients", ["od", "p-device-sim"].join("")),
  join(privateRepositoryRoot, "clients", ["od", "p-reference"].join("")),
  join(privateRepositoryRoot, "clients", ["od", "p-swift"].join("")),
];

async function collect(directory) {
  const files = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    if (ignoredDirectories.has(entry.name)) continue;
    const path = join(directory, entry.name);
    const metadata = await lstat(path);
    if (metadata.isSymbolicLink()) {
      throw new Error(
        `public subtree must not contain symlinks: ${relative(root, path)}`,
      );
    }
    if (metadata.isDirectory()) files.push(...(await collect(path)));
    else if (metadata.isFile()) files.push(path);
  }
  return files;
}

const failures = [];
for (const path of await collect(root)) {
  const name = relative(root, path);
  const content = await readFile(path, "utf8");
  for (const pattern of forbiddenPatterns) {
    if (pattern.test(name) || pattern.test(content)) {
      failures.push(`${name}: matches private-boundary rule ${pattern}`);
    }
  }
}
for (const path of obsoletePrivateSources) {
  try {
    await lstat(path);
    failures.push(
      `${relative(privateRepositoryRoot, path)}: obsolete prototype still exists`,
    );
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
}

if (failures.length > 0) {
  console.error(failures.join("\n"));
  process.exit(1);
}
console.log("public boundary check passed");
