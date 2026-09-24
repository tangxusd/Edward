import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import vm from "node:vm";

test("preference client is a safe no-op outside Orbit", async () => {
  const context = { window: {}, document: { createElement: () => ({}), head: { appendChild() {} } } };
  vm.createContext(context);
  vm.runInContext(fs.readFileSync(new URL("../preference-client.js", import.meta.url), "utf8"), context);
  assert.equal(await context.window.edwardPreferences.getCreationPreferences({}), null);
  assert.equal(await context.window.edwardPreferences.recordConfirmedPropertyChange({}), null);
});

test("FableCut keeps preference data out of project serialization", () => {
  const source = fs.readFileSync(new URL("../app.js", import.meta.url), "utf8");
  assert.match(source, /function applyCreationPreferences/);
  assert.match(source, /creationSessionId/);
  assert.match(source, /recordConfirmedPropertyChange/);
  assert.doesNotMatch(source, /clipOut\.creationSessionId/);
  assert.doesNotMatch(source, /clipOut\.__preferenceManifest/);
});
