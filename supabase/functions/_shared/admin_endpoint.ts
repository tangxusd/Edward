import { adminHeaders, validateAdminRequest } from "./admin_guard.ts";

export function serveAdminResource(name: string, handler: (request: Request) => Promise<unknown> = async () => ({ resource: name, data: [] })) {
  Deno.serve(async (request) => {
    const headers = adminHeaders();
    if (request.method === "OPTIONS") return new Response(null, { headers });
    const error = validateAdminRequest({
      authorization: request.headers.get("Authorization") ?? "",
      origin: request.headers.get("Origin") ?? "",
      csrf: request.headers.get("X-CSRF-Token") ?? "",
      requestId: request.headers.get("X-Request-ID") ?? "",
    }, Deno.env.get("ORBIT_ADMIN_ORIGIN") ?? "https://admin.edward.uno");
    if (error) return new Response(JSON.stringify({ error }), { status: 403, headers: { ...Object.fromEntries(headers), "Content-Type": "application/json" } });
    try {
      const projectUrl = Deno.env.get("SUPABASE_URL");
      const serviceRole = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY");
      if (!projectUrl || !serviceRole) return new Response(JSON.stringify({ error: "admin_server_not_configured" }), { status: 503, headers: { ...Object.fromEntries(headers), "Content-Type": "application/json" } });
      const userResponse = await fetch(`${projectUrl}/auth/v1/user`, { headers: { apikey: serviceRole, Authorization: request.headers.get("Authorization")! } });
      if (!userResponse.ok) return new Response(JSON.stringify({ error: "invalid_session" }), { status: 401, headers: { ...Object.fromEntries(headers), "Content-Type": "application/json" } });
      const user = await userResponse.json();
      const roleResponse = await fetch(`${projectUrl}/rest/v1/admin_roles?user_id=eq.${encodeURIComponent(user.id)}&revoked_at=is.null&select=user_id`, { headers: { apikey: serviceRole, Authorization: `Bearer ${serviceRole}` } });
      if (!roleResponse.ok || (await roleResponse.json()).length === 0) return new Response(JSON.stringify({ error: "admin_role_required" }), { status: 403, headers: { ...Object.fromEntries(headers), "Content-Type": "application/json" } });
      return new Response(JSON.stringify(await handler(request)), { headers: { ...Object.fromEntries(headers), "Content-Type": "application/json" } });
    }
    catch { return new Response(JSON.stringify({ error: "admin_request_failed" }), { status: 500, headers: { ...Object.fromEntries(headers), "Content-Type": "application/json" } }); }
  });
}
