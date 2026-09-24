const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const repositoryRoot = path.resolve(__dirname, "../../..");
const read = (relativePath) => fs.readFileSync(path.join(repositoryRoot, relativePath), "utf8");

test("desktop application and packaging use the Orbit product name", () => {
  const rootCmake = read("CMakeLists.txt");
  const desktopCmake = read("src/desktop/CMakeLists.txt");
  const workbench = read("src/desktop/qml/Workbench.qml");
  const packagingTests = read("tests/packaging/CMakeLists.txt");
  const packagingGate = read("tests/packaging/test_macos_package_gate.cmake");

  assert.match(rootCmake, /project\(Orbit VERSION 0\.7\.0/);
  assert.match(desktopCmake, /OUTPUT_NAME Orbit/);
  assert.match(desktopCmake, /MACOSX_BUNDLE_BUNDLE_NAME Orbit/);
  assert.match(workbench, /title: "Orbit"/);
  assert.match(packagingTests, /EXECUTABLE_NAME=Orbit/);
  assert.match(packagingTests, /macos-development-package\/Orbit\.app/);
  assert.match(packagingGate, /bin\/Orbit\.app/);
});

test("all maintained user-visible surfaces identify the product as Orbit", () => {
  const expectedOrbitFiles = [
    "src/ai/src/ai_orchestrator.cpp",
    "src/desktop/src/workbench_runtime.cpp",
    "src/desktop/qml/Workbench.qml",
    "third_party/FableCut/index.html",
    "third_party/FableCut/i18n.js",
    "third_party/FableCut/app.js",
    "third_party/FableCut/server.js",
    "supabase/config.toml",
    "supabase/templates/confirmation.html",
    "supabase/auth-recovery-site/public/index.html",
    "supabase/functions/auth-registration-confirm/index.ts",
    "supabase/functions/auth-recovery/index.ts",
    "supabase/functions/auth-recovery-complete/index.ts",
    "supabase/functions/huifu-subscription-charge/index.ts",
    "supabase/functions/huifu-native-create/index.ts",
  ];

  for (const relativePath of expectedOrbitFiles) {
    const source = read(relativePath);
    assert.match(source, /Orbit/, `${relativePath} must expose the Orbit product name`);
    assert.doesNotMatch(source, /\bEdward\b/, `${relativePath} still exposes the previous product name`);
  }

  const brandingMigration = read("supabase/migrations/202609240001_orbit_product_branding.sql");
  assert.match(brandingMigration, /replace\(name, 'Edward', 'Orbit'\)/);
});
