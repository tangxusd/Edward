const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "..");

test("native runtimes are local files and four component contracts are explicit", () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, "vendor/runtime-manifest.json"), "utf8"));
  for (const key of ["react", "react-dom", "gsap"]) {
    assert.match(manifest[key].path, /^vendor\//);
    assert.ok(manifest[key].version);
  }
});

test("component runtime keeps direct native mount contract", () => {
  const runtime = fs.readFileSync(path.join(root, "component-runtime.js"), "utf8");
  assert.match(runtime, /mount\(\{ host, props, time, mode(?:, viewport)? \}\)/);
  assert.match(runtime, /update\?\./);
  assert.match(runtime, /destroy\?\./);
  assert.match(runtime, /syncDirectComponents\(clips, time, viewport, "export"\)/);
  assert.match(runtime, /mode = "preview"/);
});

test("four native annotation resources are manifest-driven", () => {
  const ids = ["annotation.rect.react", "annotation.rect.gsap", "annotation.rect.html-css", "annotation.rect.svg"];
  for (const id of ids) {
    const manifest = JSON.parse(fs.readFileSync(path.join(root, "components", id, "manifest.json"), "utf8"));
    assert.equal(manifest.id, id);
    assert.equal(manifest.category, "annotation");
    assert.equal(manifest.entry, "component.js");
    assert.ok(manifest.props.progress);
    assert.ok(manifest.props.borderWidth);
  }
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  assert.match(app, /loadNativeAnnotationResources/);
  assert.match(app, /addNativeComponent/);
});

test("native rectangles share the 0.5 second cubic ease and three second hold", () => {
  const ids = ["annotation.rect.react", "annotation.rect.gsap", "annotation.rect.html-css", "annotation.rect.svg"];
  for (const id of ids) {
    const manifest = JSON.parse(fs.readFileSync(path.join(root, "components", id, "manifest.json"), "utf8"));
    assert.equal(manifest.props.duration.default, 3);
    const source = fs.readFileSync(path.join(root, "components", id, "component.js"), "utf8");
    assert.match(source, /0\.5/);
    assert.match(source, /cubic-bezier|ease|progress/);
  }
});

test("native component editing uses normalized centered geometry", () => {
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  assert.match(app, /annotation\.rect\./);
  assert.match(app, /isNativeAnnotation/);
  assert.match(app, /Number\(p\.width/);
  assert.match(app, /Number\(p\.height/);
  assert.match(app, /W \/ 2 \+ \(native \? Number\(p\.x \?\? 0\)/);
  assert.match(app, /H \/ 2 \+ \(native \? Number\(p\.y \?\? 0\)/);
  assert.match(app, /canvasDrag\.native/);
  assert.match(app, /drawFrame\(state\.time\)/);
});
