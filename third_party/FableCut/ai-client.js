/* Edward AI bridge. Standalone FableCut intentionally stays local-only. */
(function () {
  "use strict";
  let bridgePromise = null;
  function connect() {
    if (bridgePromise) return bridgePromise;
    if (window.__edwardWorkbenchRuntime) return Promise.resolve(window.__edwardWorkbenchRuntime);
    bridgePromise = new Promise((resolve) => {
      const deadline = Date.now() + 5000;
      const start = () => {
        if (!window.qt || !window.qt.webChannelTransport) {
          if (Date.now() < deadline) return setTimeout(start, 25);
          return resolve(null);
        }
        if (!window.QWebChannel) return Date.now() < deadline ? setTimeout(start, 25) : resolve(null);
        new window.QWebChannel(window.qt.webChannelTransport, (channel) => resolve(channel.objects.workbenchRuntime || null));
      };
      start();
    });
    return bridgePromise;
  }
  function call(method, ...args) {
    return connect().then((bridge) => new Promise((resolve) => {
      if (!bridge || typeof bridge[method] !== "function") return resolve(false);
      try { bridge[method](...args, (result) => resolve(result)); } catch { resolve(false); }
    }));
  }
  window.edwardAi = {
    available: () => !!window.qt,
    connect,
    send: (snapshot, prompt) => {
      const safeSnapshot = snapshot && typeof snapshot === "object" ? snapshot : {};
      const promptText = String(prompt || "");
      window.edwardSettings?.recordDiagnostic?.(`ai_request promptLength=${promptText.length} snapshotBytes=${JSON.stringify(safeSnapshot).length}`);
      return call("requestAiFablecutPlan", JSON.stringify(safeSnapshot), promptText).then((result) => {
        window.edwardSettings?.recordDiagnostic?.(`ai_response success=${result !== false}`);
        return result;
      });
    },
    pendingPlan: () => call("currentPendingAiActionPlan"),
  };
})();
