create table if not exists public.model_provider_presets (
  id text primary key,
  name text not null,
  endpoint text not null check (endpoint ~ '^https://'),
  default_model text not null,
  is_active boolean not null default true,
  sort_order integer not null default 0
);

alter table public.model_provider_presets enable row level security;
create policy "active provider presets are readable" on public.model_provider_presets
  for select using (is_active = true);

create table if not exists public.desktop_feedback (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  kind text not null check (kind in ('error', 'feedback')),
  message text not null check (char_length(message) between 1 and 4000),
  diagnostics jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now()
);

alter table public.desktop_feedback enable row level security;
create policy "users read own desktop feedback" on public.desktop_feedback
  for select using (auth.uid() = user_id);
create policy "users insert own desktop feedback" on public.desktop_feedback
  for insert with check (auth.uid() = user_id);
