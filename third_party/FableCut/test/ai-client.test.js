const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const source = fs.readFileSync(path.join(__dirname, "..", "ai-client.js"), "utf8");

test("Edward AI client sends the compact project snapshot through the desktop WebChannel", async () => {
  let call = null;
  const bridge = {
    requestAiFablecutPlan(snapshot, prompt, callback) {
      call = { snapshot: JSON.parse(snapshot), prompt };
      callback(true);
    },
  };
  const QWebChannel = function QWebChannel(_transport, ready) { ready({ objects: { workbenchRuntime: bridge } }); };
  const window = {
    qt: { webChannelTransport: {} },
    QWebChannel,
  };
  vm.runInNewContext(source, { window, globalThis: window, QWebChannel, Promise, document: {} });
  const accepted = await window.edwardAi.send({ revision: 4, clips: [{ id: "c_1" }], resources: ["annotation.rect.svg"] }, "添加标注");
  assert.equal(accepted, true);
  assert.deepEqual(call, {
    snapshot: { revision: 4, clips: [{ id: "c_1" }], resources: ["annotation.rect.svg"] },
    prompt: "添加标注",
  });
});

test("Edward AI client is a safe no-op outside the desktop WebChannel", async () => {
  const window = {};
  vm.runInNewContext(source, { window, globalThis: window, Promise, document: {} });
  assert.equal(await window.edwardAi.connect(), null);
  assert.equal(await window.edwardAi.send({ revision: 1, clips: [], resources: [] }, "测试"), false);
});
