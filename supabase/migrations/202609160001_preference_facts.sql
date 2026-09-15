create table if not exists public.preference_facts (
  user_id uuid not null references auth.users(id) on delete cascade,
  event_id text not null,
  fact jsonb not null,
  created_at timestamptz not null default now(),
  primary key (user_id, event_id)
);

alter table public.preference_facts enable row level security;
create policy "preference facts owner read" on public.preference_facts for select using (auth.uid() = user_id);
create policy "preference facts owner insert" on public.preference_facts for insert with check (auth.uid() = user_id);
