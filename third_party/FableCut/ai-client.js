/* Edward AI bridge. Standalone FableCut intentionally stays local-only. */
(function () {
  "use strict";
  let bridgePromise = null;
  function connect() {
    if (bridgePromise) return bridgePromise;
    if (!window.qt || !window.qt.webChannelTransport) return Promise.resolve(null);
    bridgePromise = new Promise((resolve) => {
      const start = () => {
        if (!window.QWebChannel) return resolve(null);
        new QWebChannel(window.qt.webChannelTransport, (channel) => resolve(channel.objects.workbenchRuntime || null));
      };
      if (window.QWebChannel) return start();
      const script = document.createElement("script");
      script.src = "qrc:///qtwebchannel/qwebchannel.js";
      script.onload = start;
      script.onerror = () => resolve(null);
      document.head.appendChild(script);
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
  };
})();
