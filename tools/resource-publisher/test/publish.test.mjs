import test from "node:test";
import assert from "node:assert/strict";
import { validateManifest, sha256File } from "../publish.mjs";
import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";

test("rejects invalid manifest fields", () => {
  assert.deepEqual(validateManifest({}), ["component_id is invalid", "tab_key is invalid", "category_id must be a UUID", "name is required", "version is invalid"]);
});

test("hashes package bytes deterministically", async () => {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), "edward-resource-"));
  const file = path.join(dir, "package.zip");
  await fs.writeFile(file, "resource");
  assert.equal(await sha256File(file), "5de95319f17467ed6dc58e4e0b16c1193a13b35d60dc48bcf06bf6b7beebbe6c");
});
