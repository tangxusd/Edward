create table if not exists public.redeem_code_batches (
  id uuid primary key default gen_random_uuid(),
  batch_key text not null unique,
  currency text not null default 'CNY',
  entitlement_type text not null check (entitlement_type in ('subscription','credit')),
  duration_months integer check (duration_months is null or duration_months in (1,3,12)),
  credit_amount bigint check (credit_amount is null or credit_amount >= 0),
  rule_version integer not null,
  effective_at timestamptz not null default now(),
  expires_at timestamptz not null,
  created_by uuid references auth.users(id),
  created_at timestamptz not null default now(),
  check ((entitlement_type = 'subscription' and duration_months is not null and credit_amount is null)
      or (entitlement_type = 'credit' and credit_amount is not null and duration_months is null))
);

create table if not exists public.redeem_codes (
  id uuid primary key default gen_random_uuid(),
  batch_id uuid not null references public.redeem_code_batches(id) on delete cascade,
  code_digest text not null unique check (code_digest ~ '^[0-9a-f]{64}$'),
  redeemed_by uuid references auth.users(id),
  redeemed_at timestamptz,
  status text not null default 'active' check (status in ('active','redeemed','revoked','expired')),
  created_at timestamptz not null default now()
);

create table if not exists public.redeem_code_redemptions (
  id uuid primary key default gen_random_uuid(),
  request_id text not null unique,
  code_id uuid not null unique references public.redeem_codes(id),
  user_id uuid not null references auth.users(id),
  entitlement_snapshot jsonb not null,
  created_at timestamptz not null default now()
);

create table if not exists public.referral_clicks (
  id uuid primary key default gen_random_uuid(),
  referral_code text not null,
  token_digest text not null,
  ip_digest text,
  user_agent_digest text,
  clicked_at timestamptz not null default now(),
  unique (token_digest, ip_digest, clicked_at)
);

create table if not exists public.admin_roles (
  user_id uuid primary key references auth.users(id) on delete cascade,
  role text not null check (role in ('owner','admin','analyst')),
  granted_by uuid references auth.users(id),
  granted_at timestamptz not null default now(),
  revoked_at timestamptz
);

create table if not exists public.admin_audit_events (
  id uuid primary key default gen_random_uuid(),
  request_id text not null,
  actor_user_id uuid references auth.users(id) on delete set null,
  action text not null,
  target_type text not null,
  target_id text,
  result text not null check (result in ('success','failure','rejected')),
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now()
);

create index if not exists redeem_codes_status_idx on public.redeem_codes(status);
create index if not exists redeem_batches_expiry_idx on public.redeem_code_batches(expires_at);
create index if not exists admin_audit_created_idx on public.admin_audit_events(created_at desc);

alter table public.redeem_code_batches enable row level security;
alter table public.redeem_codes enable row level security;
alter table public.redeem_code_redemptions enable row level security;
alter table public.referral_clicks enable row level security;
alter table public.admin_roles enable row level security;
alter table public.admin_audit_events enable row level security;

drop policy if exists redeem_redemptions_read_own on public.redeem_code_redemptions;
create policy redeem_redemptions_read_own on public.redeem_code_redemptions
  for select to authenticated using (user_id = (select auth.uid()));
drop policy if exists admin_roles_read_own on public.admin_roles;
create policy admin_roles_read_own on public.admin_roles
  for select to authenticated using (user_id = (select auth.uid()) and revoked_at is null);
drop policy if exists admin_audit_read_admin on public.admin_audit_events;
create policy admin_audit_read_admin on public.admin_audit_events
  for select to authenticated using (exists (
    select 1 from public.admin_roles r where r.user_id = (select auth.uid()) and r.revoked_at is null and r.role in ('owner','admin')
  ));

revoke all on public.redeem_code_batches, public.redeem_codes, public.redeem_code_redemptions,
  public.referral_clicks, public.admin_roles, public.admin_audit_events from anon;
grant select on public.redeem_code_redemptions, public.admin_roles, public.admin_audit_events to authenticated;

create or replace function public.redeem_code_once(p_request_id text, p_code_digest text, p_user_id uuid)
returns jsonb
language plpgsql
security definer
set search_path = public
as $$
declare
  existing jsonb;
  code_row public.redeem_codes%rowtype;
  batch_row public.redeem_code_batches%rowtype;
  redemption_id uuid;
  snapshot jsonb;
begin
  if p_request_id is null or p_request_id = '' or p_code_digest !~ '^[0-9a-f]{64}$' then
    raise exception using errcode = '22023', message = 'invalid_redemption_request';
  end if;
  select jsonb_build_object('redemptionId', id, 'entitlement', entitlement_snapshot)
    into existing from public.redeem_code_redemptions where request_id = p_request_id;
  if existing is not null then return existing; end if;
  select * into code_row from public.redeem_codes where code_digest = p_code_digest for update;
  if not found then raise exception using errcode = 'P0002', message = 'redeem_code_not_found'; end if;
  select * into batch_row from public.redeem_code_batches where id = code_row.batch_id;
  if code_row.status <> 'active' or now() < batch_row.effective_at or now() >= batch_row.expires_at then
    raise exception using errcode = 'P0001', message = 'redeem_code_expired_or_used';
  end if;
  snapshot := jsonb_build_object('currency', batch_row.currency, 'entitlementType', batch_row.entitlement_type,
    'durationMonths', batch_row.duration_months, 'creditAmount', batch_row.credit_amount, 'ruleVersion', batch_row.rule_version,
    'effectiveAt', batch_row.effective_at, 'expiresAt', batch_row.expires_at);
  update public.redeem_codes set status = 'redeemed', redeemed_by = p_user_id, redeemed_at = now() where id = code_row.id;
  insert into public.redeem_code_redemptions(request_id, code_id, user_id, entitlement_snapshot)
    values (p_request_id, code_row.id, p_user_id, snapshot) returning id into redemption_id;
  return jsonb_build_object('redemptionId', redemption_id, 'entitlement', snapshot);
exception when unique_violation then
  select jsonb_build_object('redemptionId', id, 'entitlement', entitlement_snapshot) into existing
    from public.redeem_code_redemptions where request_id = p_request_id;
  if existing is not null then return existing; end if;
  raise;
end;
$$;

revoke all on function public.redeem_code_once(text, text, uuid) from public, anon, authenticated;
grant execute on function public.redeem_code_once(text, text, uuid) to service_role;
