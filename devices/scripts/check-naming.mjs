import { lstat, readdir, readFile } from "node:fs/promises";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const ignoredDirectories = new Set([
  ".build",
  ".git",
  "build",
  "build-sanitize",
]);
const forbiddenNames = [
  ["O", "DP"].join(""),
  ["Open Device", " Protocol"].join(""),
  ["N", "DP"].join(""),
  ["od", "p-"].join(""),
  ["Nexting-", "O", "DP"].join(""),
];

async function collect(directory) {
  const files = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    if (ignoredDirectories.has(entry.name)) continue;
    const path = join(directory, entry.name);
    const metadata = await lstat(path);
    if (metadata.isDirectory()) files.push(...(await collect(path)));
    else if (metadata.isFile()) files.push(path);
  }
  return files;
}

const failures = [];
for (const path of await collect(root)) {
  const name = relative(root, path);
  const content = await readFile(path, "utf8");
  for (const forbidden of forbiddenNames) {
    if (name.includes(forbidden) || content.includes(forbidden)) {
      failures.push(
        `${name}: contains retired or ambiguous public name ${JSON.stringify(forbidden)}`,
      );
    }
  }
}

if (failures.length > 0) {
  console.error(failures.join("\n"));
  process.exit(1);
}
console.log("public naming check passed");
