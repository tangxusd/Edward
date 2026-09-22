import { authCors, cleanupExpiredRegistrations } from "../_shared/auth-lifecycle.ts";

Deno.serve(async (req) => {
  if (req.method !== "POST") return Response.json({ message: "请求方式不正确。" }, { status: 405, headers: authCors });
  const expected = Deno.env.get("AUTH_CLEANUP_CRON_SECRET");
  if (!expected || req.headers.get("x-auth-cleanup-secret") !== expected)
    return Response.json({ message: "无权执行该操作。" }, { status: 401, headers: authCors });
  try {
    const deleted = await cleanupExpiredRegistrations();
    return Response.json({ deleted }, { headers: authCors });
  } catch {
    return Response.json({ message: "清理任务暂时不可用。" }, { status: 503, headers: authCors });
  }
});
