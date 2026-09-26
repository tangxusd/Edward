import { assert, assertMatch } from "https://deno.land/std@0.224.0/assert/mod.ts";

const read = (path: string) => Deno.readTextFile(new URL(path, import.meta.url));

Deno.test("settings cloud migration keeps provider presets public and feedback owned", async () => {
  const sql = await read("../migrations/202609170002_settings_cloud.sql");
  assertMatch(sql, /alter table public\.model_provider_presets enable row level security/i);
  assertMatch(sql, /for select using \(is_active = true\)/i);
  const presetSection = sql.split("create table if not exists public.desktop_feedback")[0];
  assert(!/for insert/i.test(presetSection));
  assertMatch(sql, /users read own desktop feedback/i);
  assertMatch(sql, /users insert own desktop feedback/i);
});

Deno.test("settings functions require authentication and reject sensitive diagnostics", async () => {
  const presets = await read("../functions/model-provider-presets/index.ts");
  const feedback = await read("../functions/desktop-feedback/index.ts");
  assertMatch(presets, /select\("id,name,endpoint,default_model"\)/);
  assert(!/select\([^)]*key/i.test(presets));
  assertMatch(feedback, /authentication_required/);
  assertMatch(feedback, /forbidden/);
  assertMatch(feedback, /project\[_ -\]\?\(path\|name\|content\)/i);
});
