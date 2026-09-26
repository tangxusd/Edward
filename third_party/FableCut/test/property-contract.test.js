import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import vm from "node:vm";

function contract() {
  const context = { window: {} };
  vm.createContext(context);
  vm.runInContext(fs.readFileSync(new URL("../property-contract.js", import.meta.url), "utf8"), context);
  return context.window.fablecutPropertyContract;
}

test("属性合同提供中文标签、循环默认值和关键帧资格", () => {
  const api = contract();
  assert.equal(api.label("opacity"), "不透明度");
  assert.equal(api.option("gentle-bounce"), "轻微跳跃");
  assert.equal(api.defaultValue("loopAnimation"), "none");
  assert.equal(api.defaultValue("loopFrequency"), 0.5);
  assert.equal(api.defaultValue("loopAmplitude"), 2);
  assert.equal(api.label("duration"), "展示时长（秒）");
  assert.equal(api.label("transitionIn"), "入场");
  assert.equal(api.label("duration", "en"), "Display duration (s)");
  assert.equal(api.option("gentle-bounce", "en"), "Gentle bounce");
  assert.equal(api.resolveLocale("zh-TW"), "zh-CN");
  assert.equal(api.resolveLocale("fr-FR"), "en");
  assert.equal(api.animatable.has("loopAmplitude"), true);
  assert.equal(api.animatable.has("borderWidth"), true);
  assert.equal(api.animatable.has("fit"), false);
});

test("原生闭合矩形以单一完整路径描边绘制，不用底线掩盖缺口", () => {
  for (const id of ["annotation.rect.react", "annotation.rect.gsap", "annotation.rect.html-css", "annotation.rect.svg"]) {
    const source = fs.readFileSync(new URL(`../components/${id}/component.js`, import.meta.url), "utf8");
    assert.match(source, /drawRect/);
    assert.match(source, /stroke(?:-dash(?:array|offset)|Dash(?:array|offset))/);
    assert.match(source, /dashLength = Math\.max\([^,]+, 0\.0001\)/);
    assert.match(source, /stroke(?:-dasharray|Dasharray).*\$\{dashLength\} 1/);
    assert.doesNotMatch(source, /baseRect|stroke(?:-opacity|Opacity)/);
    assert.match(source, /<rect|createElement\("rect"|createElementNS\(/);
  }
});

test("属性求值按关键帧、循环和转场顺序执行，并将结果交给原生组件", () => {
  const source = fs.readFileSync(new URL("../app.js", import.meta.url), "utf8");
  assert.match(source, /"cubic-out"/);
  assert.match(source, /applyLoopAnimation\(p, local\)/);
  assert.match(source, /easing: "cubic-out"/);
  assert.match(source, /props: evalProps\(clip, t\)/);
  assert.match(source, /data-kf-easing/);
  assert.doesNotMatch(source, /const KF_GRAPH_LABEL/);
  assert.match(source, /PROPERTY_CONTRACT\.label\(k\)/);
});
