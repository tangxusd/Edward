create table if not exists public.subscription_provider_bindings (
  id uuid primary key default gen_random_uuid(),
  subscription_id uuid not null references public.subscriptions(id) on delete cascade,
  user_id uuid not null references auth.users(id) on delete cascade,
  provider text not null default 'huifu',
  provider_customer_id text,
  provider_token text not null,
  status text not null default 'pending' check (status in ('pending','active','revoked','expired')),
  authorized_at timestamptz,
  revoked_at timestamptz,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique (subscription_id),
  unique (provider, provider_token)
);

create table if not exists public.subscription_charges (
  id uuid primary key default gen_random_uuid(),
  subscription_id uuid not null references public.subscriptions(id) on delete cascade,
  user_id uuid not null references auth.users(id) on delete cascade,
  period_start timestamptz not null,
  period_end timestamptz not null,
  amount bigint not null check (amount >= 0),
  currency text not null,
  idempotency_key text not null unique,
  provider_request_id text not null unique,
  provider_order_id text,
  status text not null default 'pending' check (status in ('pending','paid','failed','expired')),
  attempt_count integer not null default 0 check (attempt_count >= 0),
  last_error text,
  provider_response jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique (subscription_id, period_start)
);

create index if not exists subscription_bindings_user_idx on public.subscription_provider_bindings(user_id,status);
create index if not exists subscription_charges_due_idx on public.subscription_charges(status,period_end);

alter table public.subscription_provider_bindings enable row level security;
alter table public.subscription_charges enable row level security;
create policy subscription_bindings_read_own on public.subscription_provider_bindings for select to authenticated using (user_id = (select auth.uid()));
create policy subscription_charges_read_own on public.subscription_charges for select to authenticated using (user_id = (select auth.uid()));
revoke all on public.subscription_provider_bindings, public.subscription_charges from anon;
grant select on public.subscription_provider_bindings, public.subscription_charges to authenticated;
