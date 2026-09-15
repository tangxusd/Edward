/* Edward host preference bridge. Standalone FableCut keeps working without Qt. */
(function () {
  "use strict";
  let channelPromise = null;
  function connect() {
    if (channelPromise) return channelPromise;
    if (!window.qt || !window.qt.webChannelTransport) return Promise.resolve(null);
    channelPromise = new Promise((resolve) => {
      const start = () => {
        if (!window.QWebChannel) return resolve(null);
        new QWebChannel(window.qt.webChannelTransport, (channel) => resolve(channel.objects.preferenceStore || null));
      };
      if (window.QWebChannel) return start();
      const script = document.createElement("script");
      script.src = "qrc:///qtwebchannel/qwebchannel.js";
      script.onload = start;
      script.onerror = () => resolve(null);
      document.head.appendChild(script);
    });
    return channelPromise;
  }
  async function call(method, ...args) {
    const bridge = await connect();
    if (!bridge || typeof bridge[method] !== "function") return null;
    return new Promise((resolve) => {
      try { bridge[method](...args, (result) => resolve(result)); }
      catch { resolve(null); }
    });
  }
  window.edwardPreferences = {
    available: () => !!window.qt,
    getCreationPreferences: (identity, rank = 0) => call("creationPreferences", identity, rank),
    recordConfirmedPropertyChange: (observation) => call("recordConfirmedPropertyChange", observation),
    flushPendingPreferences: () => call("flushPendingPreferences"),
    compilePreferences: () => call("compilePreferences"),
  };
})();
