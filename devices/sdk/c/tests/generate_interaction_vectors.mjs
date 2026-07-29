import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const [, , vectorDirectory, outputPath] = process.argv;
if (!vectorDirectory || !outputPath) process.exit(2);

const profiles = ["navigation", "keys", "rotary", "voice", "text", "usage", "config"];
const valid = [];
const invalid = [];
for (const profile of profiles) {
  const vectors = JSON.parse(
    readFileSync(join(vectorDirectory, `${profile}-v1.json`), "utf8"),
  );
  valid.push(...vectors.valid.map((vector) => vector.wire));
  invalid.push(...vectors.invalid.map((vector) => vector.wire));
}

function literal(value) {
  return JSON.stringify(value);
}

function array(name, values) {
  return [
    `static const char *const ${name}[] = {`,
    ...values.map((value) => `  ${literal(value)},`),
    "};",
    `static const size_t ${name}_count = sizeof(${name}) / sizeof(${name}[0]);`,
  ].join("\n");
}

writeFileSync(
  outputPath,
  [
    "#ifndef NEXTING_DEVICE_GENERATED_INTERACTION_VECTORS_H",
    "#define NEXTING_DEVICE_GENERATED_INTERACTION_VECTORS_H",
    "#include <stddef.h>",
    array("nexting_device_interaction_valid_vectors", valid),
    array("nexting_device_interaction_invalid_vectors", invalid),
    "#endif",
    "",
  ].join("\n"),
);
