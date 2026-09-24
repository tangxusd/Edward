/* Edward desktop settings bridge. Standalone FableCut keeps settings unavailable. */
(function () {
  "use strict";
  let bridgePromise = null;
  const operationListeners = new Set();
  function connect() {
    if (bridgePromise) return bridgePromise;
    if (!window.qt || !window.qt.webChannelTransport) return Promise.resolve(null);
    bridgePromise = new Promise((resolve) => {
      const start = () => {
        if (!window.QWebChannel) return resolve(null);
        new QWebChannel(window.qt.webChannelTransport, (channel) => {
          const bridge = channel.objects.workbenchRuntime || null;
          window.__edwardWorkbenchRuntime = bridge;
          bridge?.settingsOperationCompleted?.connect((operation, success, message, values) => {
            for (const listener of operationListeners) listener({ operation, success, message, values: values || [] });
          });
          resolve(bridge);
        });
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
  async function call(method, ...args) {
    const bridge = await connect();
    if (!bridge || typeof bridge[method] !== "function") return null;
    return new Promise((resolve) => {
      try { bridge[method](...args, (result) => resolve(result)); }
      catch { resolve(null); }
    });
  }
  async function operation(method, operationName, ...args) {
    await connect();
    return new Promise(async (resolve) => {
      const listener = (result) => {
        if (result.operation !== operationName) return;
        operationListeners.delete(listener);
        resolve(result);
      };
      operationListeners.add(listener);
      const started = await call(method, ...args);
      if (!started) {
        operationListeners.delete(listener);
        resolve({ operation: operationName, success: false, message: "请求未启动。", values: [] });
      }
    });
  }
  window.edwardSettings = {
    available: () => !!window.qt,
    load: () => call("fablecutSettings"),
    authSession: () => call("fablecutAuthSession"),
    setAuthState: (authenticated) => call("setFablecutAuthState", authenticated),
    saveAuthSession: (session) => call("saveFablecutAuthSession", session),
    clearAuthSession: () => call("clearFablecutAuthSession"),
    registerAccount: (email, password, username) => operation("submitFablecutRegistration", "authRegistration", email, password, username),
    recoverAccount: (email) => operation("submitFablecutPasswordRecovery", "authRecovery", email),
    enrollDevice: (accessToken) => operation("submitFablecutDeviceEnrollment", "authDeviceEnrollment", accessToken),
    save: (providerId, provider, endpoint, apiKey, model, protocol, exportDirectory) => call("saveFablecutSettings", providerId, provider, endpoint, apiKey, model, protocol, exportDirectory),
    chooseExportDirectory: (currentDirectory) => call("chooseFablecutExportDirectory", currentDirectory),
    savePaths: (paths) => call("saveFablecutPathSettings", paths),
    migratePath: (key, destination) => call("migrateFablecutPath", key, destination),
    clearDerivedPath: (key) => call("clearDerivedFablecutPath", key),
    compilePreferences: () => call("compilePreferencesNow"),
    exportPreferences: (path) => call("exportFablecutPreferences", path),
    importPreferences: (path) => call("importFablecutPreferences", path),
    choosePreferencesExportPath: () => call("chooseFablecutPreferencesExportPath"),
    choosePreferencesImportPath: () => call("chooseFablecutPreferencesImportPath"),
    preferenceFacts: () => call("fablecutPreferenceFacts"),
    importPreferenceFacts: (facts) => call("importFablecutPreferenceFacts", facts),
    diagnostics: () => call("fablecutDiagnostics"),
    recordDiagnostic: (event) => call("recordFablecutDiagnostic", event),
    testProvider: (endpoint, apiKey, model, protocol) => operation("testFablecutAiProvider", "testProvider", endpoint, apiKey, model, protocol),
    fetchModels: (endpoint, apiKey, model, protocol) => operation("fetchFablecutAiModels", "fetchModels", endpoint, apiKey, model, protocol),
  };
})();
