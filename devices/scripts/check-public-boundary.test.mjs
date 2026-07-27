import assert from "node:assert/strict";
import test from "node:test";

import { forbiddenPatterns } from "./check-public-boundary.mjs";

test("legacy private authorization key is rejected", () => {
  const legacyKey = [
    "pinclaw",
    "odp",
    "authorized",
    "peripheral",
    "uuids",
  ].join("_");
  assert.ok(forbiddenPatterns.some((pattern) => pattern.test(legacyKey)));
});
