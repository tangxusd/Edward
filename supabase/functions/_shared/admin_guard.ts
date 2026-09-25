export type AdminRequest = { authorization: string; origin: string; csrf: string; requestId: string };

export function validateAdminRequest(request: AdminRequest, expectedOrigin: string): string | null {
  if (request.origin !== expectedOrigin) return "origin_not_allowed";
  if (!/^Bearer\s+\S+$/.test(request.authorization)) return "missing_session";
  if (!request.csrf.trim()) return "missing_csrf";
  if (!/^[A-Za-z0-9_-]{8,128}$/.test(request.requestId)) return "invalid_request_id";
  return null;
}

export function adminHeaders(origin = "https://admin.edward.uno"): Headers {
  return new Headers({
    "Access-Control-Allow-Origin": origin,
    "Access-Control-Allow-Headers": "authorization, apikey, content-type, x-request-id, x-csrf-token",
    "Access-Control-Allow-Methods": "GET,POST,PUT,DELETE,OPTIONS",
    "Vary": "Origin",
  });
}
