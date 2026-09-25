export function createAuthProvider({ apiBase, getSession, fetchImpl = fetch }) {
  return {
    async login() { const session = await getSession(); return session ? { success: true } : { success: false, error: new Error("未登录") }; },
    async logout() { return { success: true, redirectTo: "/login" }; },
    async check() {
      const session = await getSession();
      if (!session?.access_token) return { authenticated: false, redirectTo: "/login" };
      const response = await fetchImpl(`${apiBase}/api/admin/stats`, { headers: { Authorization: `Bearer ${session.access_token}`, "X-Request-ID": crypto.randomUUID(), "X-CSRF-Token": session.csrf ?? "" } });
      return response.ok ? { authenticated: true } : { authenticated: false, redirectTo: "/login" };
    },
    getIdentity: async () => ({ id: "current", name: "管理员" }),
    getPermissions: async () => ["admin"],
  };
}
