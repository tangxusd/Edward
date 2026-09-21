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
  assert.match(runtime, /function applyTransitionMask/);
  assert.match(runtime, /applyTransitionMask\(entry\.host, clip\.props\)/);
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

test("official Edward runtime manifests use distinct native preview and render entries", () => {
  const ids = ["annotation.rect.react", "annotation.rect.gsap", "annotation.rect.html-css", "annotation.rect.svg"];
  for (const id of ids) {
    const manifest = JSON.parse(fs.readFileSync(path.join(root, "components", id, "edward-runtime.json"), "utf8"));
    assert.equal(manifest.protocol, "edward.web-runtime.v1");
    assert.equal(manifest.previewEntry, "component.js");
    assert.equal(manifest.renderEntry, "component.js");
    assert.ok(manifest.propsSchema.properties.color);
    assert.equal(manifest.propsSchema.additionalProperties, false);
    assert.ok(manifest.editableProperties.includes("color"));
    assert.ok(manifest.editableTracks.includes("color"));
    assert.deepEqual(manifest.capabilities, { preview: true, export: true, editable: true, audio: false, transparent: true });
  }
  const runtime = fs.readFileSync(path.join(root, "component-runtime.js"), "utf8");
  const server = fs.readFileSync(path.join(root, "server.js"), "utf8");
  assert.match(runtime, /function runtimeEntry\(manifest, mode\)/);
  assert.match(runtime, /manifest\.previewEntry/);
  assert.match(runtime, /manifest\.renderEntry/);
  assert.match(runtime, /entry\.mode !== mode/);
  assert.match(server, /edward-runtime\.json/);
  assert.match(fs.readFileSync(path.join(root, "app.js"), "utf8"), /spec\.format === "color"/);
});

test("native rectangles share the one second cubic ease and draw a single closed path", () => {
  const ids = ["annotation.rect.react", "annotation.rect.gsap", "annotation.rect.html-css", "annotation.rect.svg"];
  for (const id of ids) {
    const manifest = JSON.parse(fs.readFileSync(path.join(root, "components", id, "manifest.json"), "utf8"));
    assert.equal(manifest.props.duration.default, 3);
    const source = fs.readFileSync(path.join(root, "components", id, "component.js"), "utf8");
    assert.match(source, /(?:0\.5|1)/);
    assert.match(source, /cubic-bezier|ease|progress/);
    assert.match(source, /<rect|createElement\("rect"|createElementNS\(/);
    assert.match(source, /drawRect/);
    assert.match(source, /stroke(?:-dash(?:array|offset)|Dash(?:array|offset))/);
    assert.match(source, /dashLength = Math\.max\([^,]+, 0\.0001\)/);
    assert.match(source, /stroke(?:-dasharray|Dasharray).*\$\{dashLength\} 1/);
    assert.doesNotMatch(source, /baseRect|stroke(?:-opacity|Opacity)/);
  }
});

test("native component editing uses normalized centered geometry", () => {
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  assert.match(app, /annotation\.rect\./);
  assert.match(app, /isNativeAnnotation/);
  assert.match(app, /Number\(p\.width/);
  assert.match(app, /Number\(p\.height/);
  assert.match(app, /W \/ 2 \+ \(native \? Number\(p\.x \?\? 0\)/);
  assert.match(app, /H \/ 2 - \(native \? Number\(p\.y \?\? 0\)/);
  assert.match(app, /canvasDrag\.native/);
  assert.match(app, /drawFrame\(state\.time\)/);
  assert.match(app, /function worldToCanvas\(x, y, W, H\)/);
  assert.match(app, /function canvasToWorld\(x, y, W, H\)/);
  assert.match(app, /row\("Position X"/);
  assert.match(app, /row\("Position Y"/);
  assert.match(app, /canvasToWorld\(point\.x, point\.y/);
});

test("native components use direct SVG export and automatic upper-track fallback", () => {
  const runtime = fs.readFileSync(path.join(root, "component-runtime.js"), "utf8");
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  assert.match(runtime, /Native annotation components are SVG documents/);
  assert.match(runtime, /directSvg/);
  assert.match(runtime, /svgNode\.getBoundingClientRect\(\)/);
  assert.match(runtime, /outputSpec\.width/);
  assert.match(runtime, /outputSpec\.height/);
  assert.match(app, /resolveComponentTrack/);
  assert.match(app, /addTimelineTrack\("video"\)/);
  assert.match(app, /setDragImage/);
});

test("stale asynchronous component mounts remove their host", () => {
  const runtime = fs.readFileSync(path.join(root, "component-runtime.js"), "utf8");
  assert.match(runtime, /entry\.instance\.destroy\?\.\(\);\s*entry\.host\.remove\(\);/);
});

test("native and legacy component hosts are composited together", () => {
  const runtime = fs.readFileSync(path.join(root, "component-runtime.js"), "utf8");
  assert.match(runtime, /if \(nativeSvgNodes\.length > 0\)/);
  assert.match(runtime, /const legacyHosts = visibleHosts\.filter/);
  assert.match(runtime, /legacyIds/);
});

test("monitor rulers are attached to the monitor frame and use ten-pixel ticks", () => {
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
  assert.match(html, /monitor-ruler-top/);
  assert.match(html, /monitor-ruler-left/);
  assert.match(app, /const dpr = window\.devicePixelRatio \|\| 1/);
  assert.match(app, /const topH = 17, leftW = 17/);
  assert.match(app, /chooseStep/);
  assert.match(app, /project\.width \/ 2/);
  assert.match(app, /top\.style\.left = `\$\{leftW\}px`/);
  assert.match(app, /left\.style\.left = "0px"/);
});
