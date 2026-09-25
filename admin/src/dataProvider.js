export function createDataProvider({ apiBase, getSession, fetchImpl = fetch }) {
  async function request(path, init = {}) {
    const session = await getSession();
    if (!session?.access_token) throw new Error("未登录");
    const response = await fetchImpl(`${apiBase}${path}`, { ...init, headers: { "Content-Type": "application/json", Authorization: `Bearer ${session.access_token}`, "X-Request-ID": crypto.randomUUID(), "X-CSRF-Token": session.csrf ?? "", ...(init.headers ?? {}) } });
    if (!response.ok) throw new Error(`管理接口失败：${response.status}`);
    return response.json();
  }
  return { getList: (resource, params = {}) => request(`/api/${resource}?${new URLSearchParams(params).toString()}`), getOne: (resource, id) => request(`/api/${resource}/${encodeURIComponent(id)}`), create: (resource, variables) => request(`/api/${resource}`, { method: "POST", body: JSON.stringify(variables) }), update: (resource, id, variables) => request(`/api/${resource}/${encodeURIComponent(id)}`, { method: "PUT", body: JSON.stringify(variables) }), deleteOne: (resource, id) => request(`/api/${resource}/${encodeURIComponent(id)}`, { method: "DELETE" }) };
}
