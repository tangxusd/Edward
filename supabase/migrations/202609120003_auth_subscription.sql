create table if not exists public.profiles (
  user_id uuid primary key references auth.users(id) on delete cascade,
  username_normalized text not null unique check (char_length(username_normalized) between 3 and 32),
  username_display text not null,
  referral_code text not null unique,
  referred_by_user_id uuid references public.profiles(user_id),
  risk_status text not null default 'normal' check (risk_status in ('normal','review','blocked')),
  created_at timestamptz not null default now(), updated_at timestamptz not null default now()
);
create table if not exists public.subscription_plans (
  id uuid primary key default gen_random_uuid(), plan_key text not null unique, name text not null,
  currency text not null, unit_amount bigint not null check (unit_amount >= 0), billing_interval text not null check (billing_interval in ('day','month','year')),
  is_recurring boolean not null default false, trial_days integer not null default 0 check (trial_days >= 0), discount_config jsonb not null default '{}'::jsonb,
  required_entitlement text, status text not null default 'draft' check (status in ('draft','active','archived')),
  effective_from timestamptz not null default now(), effective_to timestamptz, created_at timestamptz not null default now(), updated_at timestamptz not null default now()
);
create table if not exists public.subscription_rules (
  id uuid primary key default gen_random_uuid(), version integer not null unique, default_trial_days integer not null default 3 check (default_trial_days >= 0),
  referral_reward_amount bigint not null default 0 check (referral_reward_amount >= 0), referral_reward_valid_days integer not null default 365 check (referral_reward_valid_days >= 0),
  max_credit_per_order bigint, offline_grace_days integer not null default 0 check (offline_grace_days >= 0), config jsonb not null default '{}'::jsonb,
  effective_from timestamptz not null default now(), effective_to timestamptz, created_at timestamptz not null default now()
);
create table if not exists public.subscriptions (
  id uuid primary key default gen_random_uuid(), user_id uuid not null references auth.users(id) on delete cascade, plan_id uuid not null references public.subscription_plans(id),
  status text not null check (status in ('trialing','active','past_due','canceled','expired','revoked')), current_period_start timestamptz not null, current_period_end timestamptz not null,
  cancel_at_period_end boolean not null default false, provider text not null default 'manual', provider_subscription_id text, created_at timestamptz not null default now(), updated_at timestamptz not null default now()
);
create table if not exists public.orders (
  id uuid primary key default gen_random_uuid(), user_id uuid not null references auth.users(id) on delete cascade, plan_id uuid not null references public.subscription_plans(id),
  idempotency_key text not null unique, currency text not null, original_amount bigint not null check (original_amount >= 0), discount_amount bigint not null default 0 check (discount_amount >= 0),
  credit_amount bigint not null default 0 check (credit_amount >= 0), paid_amount bigint not null check (paid_amount >= 0), rule_version integer not null, status text not null default 'pending', provider text not null default 'manual', created_at timestamptz not null default now(), updated_at timestamptz not null default now()
);
create table if not exists public.referrals (
  id uuid primary key default gen_random_uuid(), referrer_user_id uuid not null references auth.users(id) on delete cascade, referred_user_id uuid not null unique references auth.users(id) on delete cascade,
  email_verified_at timestamptz, first_paid_at timestamptz, reward_status text not null default 'pending' check (reward_status in ('pending','granted','frozen','revoked')), created_at timestamptz not null default now(), updated_at timestamptz not null default now(), check (referrer_user_id <> referred_user_id)
);
create table if not exists public.credit_ledger (
  id uuid primary key default gen_random_uuid(), user_id uuid not null references auth.users(id) on delete cascade, referral_id uuid references public.referrals(id), order_id uuid references public.orders(id), amount bigint not null, state text not null check (state in ('available','consumed','frozen','revoked','expired')), expires_at timestamptz, created_at timestamptz not null default now()
);
create table if not exists public.trial_grants (
  user_id uuid primary key references auth.users(id) on delete cascade, days integer not null check (days >= 0), starts_at timestamptz, ends_at timestamptz, rule_version integer not null, status text not null check (status in ('pending','active','expired','revoked')), created_at timestamptz not null default now()
);
create table if not exists public.risk_events (
  id uuid primary key default gen_random_uuid(), user_id uuid references auth.users(id) on delete set null, event_type text not null, identifier_hash text not null, occurred_at timestamptz not null default now(), metadata jsonb not null default '{}'::jsonb
);
create index if not exists subscriptions_user_status_idx on public.subscriptions(user_id,status,current_period_end);
create index if not exists orders_user_created_idx on public.orders(user_id,created_at desc);
create index if not exists risk_events_identifier_idx on public.risk_events(identifier_hash,occurred_at desc);
alter table public.profiles enable row level security; alter table public.subscription_plans enable row level security; alter table public.subscription_rules enable row level security; alter table public.subscriptions enable row level security; alter table public.orders enable row level security; alter table public.referrals enable row level security; alter table public.credit_ledger enable row level security; alter table public.trial_grants enable row level security; alter table public.risk_events enable row level security;
create policy profiles_read_own on public.profiles for select to authenticated using (user_id = (select auth.uid()));
create policy plans_read_active on public.subscription_plans for select to authenticated using (status='active' and now() >= effective_from and (effective_to is null or now() < effective_to));
create policy subscriptions_read_own on public.subscriptions for select to authenticated using (user_id = (select auth.uid()));
create policy orders_read_own on public.orders for select to authenticated using (user_id = (select auth.uid()));
create policy referrals_read_own on public.referrals for select to authenticated using (referrer_user_id = (select auth.uid()) or referred_user_id = (select auth.uid()));
create policy credits_read_own on public.credit_ledger for select to authenticated using (user_id = (select auth.uid()));
create policy trials_read_own on public.trial_grants for select to authenticated using (user_id = (select auth.uid()));
revoke all on public.profiles,public.subscription_plans,public.subscription_rules,public.subscriptions,public.orders,public.referrals,public.credit_ledger,public.trial_grants,public.risk_events from anon;
grant select on public.profiles,public.subscription_plans,public.subscriptions,public.orders,public.referrals,public.credit_ledger,public.trial_grants to authenticated;
