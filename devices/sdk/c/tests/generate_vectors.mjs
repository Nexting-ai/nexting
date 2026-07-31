import { readFileSync, writeFileSync } from "node:fs";

const [, , inputPath, outputPath] = process.argv;
if (!inputPath || !outputPath) process.exit(2);

const vectors = JSON.parse(readFileSync(inputPath, "utf8"));
const invalidVectors =
  vectors.invalid ?? vectors.invalidCore.map(({ wire }) => wire);

function emitCString(value) {
  const fragments = [];
  let fragment = "";
  for (const character of value) {
    const candidate = fragment + character;
    if (fragment && JSON.stringify(candidate).length > 800) {
      fragments.push(fragment);
      fragment = character;
    } else {
      fragment = candidate;
    }
  }
  fragments.push(fragment);
  const literals = fragments.map((part) => JSON.stringify(part)).join("\n  ");
  return fragments.length > 1 ? `(${literals})` : literals;
}

function emitByteArray(name, value) {
  const bytes = [...Buffer.from(value, "utf8"), 0];
  const rows = [];
  for (let index = 0; index < bytes.length; index += 24) {
    rows.push(`  ${bytes.slice(index, index + 24).join(", ")},`);
  }
  return [`static const char ${name}[] = {`, ...rows, "};"].join("\n");
}

function emitArray(name, values) {
  const declarations = [];
  const entries = values.map((value, index) => {
    if (Buffer.byteLength(value, "utf8") <= 4_000) return emitCString(value);
    const storageName = `${name}_storage_${index}`;
    declarations.push(emitByteArray(storageName, value));
    return storageName;
  });
  return [
    ...declarations,
    `static const char *const ${name}[] = {`,
    ...entries.map((entry) => `  ${entry},`),
    "};",
    `static const size_t ${name}_count = sizeof(${name}) / sizeof(${name}[0]);`,
  ].join("\n");
}

writeFileSync(
  outputPath,
  [
    "#ifndef NEXTING_DEVICE_GENERATED_VECTORS_H",
    "#define NEXTING_DEVICE_GENERATED_VECTORS_H",
    "#include <stddef.h>",
    emitArray(
      "nexting_device_valid_vectors",
      vectors.valid.map(({ wire }) => wire),
    ),
    emitArray("nexting_device_invalid_vectors", invalidVectors),
    "#endif",
    "",
  ].join("\n"),
);
