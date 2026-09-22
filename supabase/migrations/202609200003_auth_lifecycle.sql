create table if not exists public.auth_registration_states (
  id uuid primary key default gen_random_uuid(),
  user_id uuid unique references auth.users(id) on delete cascade,
  email_normalized text not null unique,
  device_fingerprint text,
  state text not null check (state in ('registration_pending', 'active', 'registration_expired')),
  expires_at timestamptz,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  confirmed_at timestamptz,
  check (
    (state = 'registration_pending' and expires_at is not null and confirmed_at is null)
    or (state = 'active' and confirmed_at is not null)
    or state = 'registration_expired'
  )
);

create table if not exists public.auth_recovery_locks (
  user_id uuid primary key references auth.users(id) on delete cascade,
  state text not null check (state in ('recovery_locked', 'completed')),
  expires_at timestamptz not null,
  requested_at timestamptz not null default now(),
  completed_at timestamptz
);

create table if not exists public.device_security_audits (
  id uuid primary key default gen_random_uuid(),
  registration_state_id uuid references public.auth_registration_states(id) on delete set null,
  user_id uuid references auth.users(id) on delete set null,
  device_fingerprint text not null,
  serial_ciphertext text not null,
  mac_ciphertext text not null,
  event_type text not null check (event_type in ('registration_requested', 'registration_confirmed', 'password_recovery_requested')),
  metadata jsonb not null default '{}'::jsonb,
  occurred_at timestamptz not null default now()
);

create index if not exists auth_registration_states_pending_expiry_idx
  on public.auth_registration_states(expires_at)
  where state = 'registration_pending';
create index if not exists device_security_audits_fingerprint_idx
  on public.device_security_audits(device_fingerprint, occurred_at desc);

alter table public.auth_registration_states enable row level security;
alter table public.auth_recovery_locks enable row level security;
alter table public.device_security_audits enable row level security;
revoke all on public.auth_registration_states, public.auth_recovery_locks, public.device_security_audits from anon, authenticated;

insert into public.auth_registration_states (
  user_id, email_normalized, state, expires_at, confirmed_at
)
select id, lower(email), 'active', null, email_confirmed_at
from auth.users
where email is not null and email_confirmed_at is not null
on conflict (email_normalized) do nothing;

create or replace function public.reserve_registration_attempt(
  p_email_normalized text,
  p_device_fingerprint text
) returns table(action text, registration_id uuid, user_id uuid)
language plpgsql
security definer
set search_path = public, auth
as $$
declare
  existing public.auth_registration_states%rowtype;
begin
  insert into public.auth_registration_states (email_normalized, device_fingerprint, state, expires_at)
  values (lower(trim(p_email_normalized)), p_device_fingerprint, 'registration_pending', now() + interval '10 minutes')
  on conflict (email_normalized) do nothing
  returning * into existing;

  if found then
    return query select 'create'::text, existing.id, existing.user_id;
    return;
  end if;

  select * into existing
  from public.auth_registration_states
  where email_normalized = lower(trim(p_email_normalized))
  for update;

  if existing.state = 'active' then
    return query select 'active'::text, existing.id, existing.user_id;
  elsif existing.state = 'registration_pending' and existing.expires_at > now() then
    return query select 'pending'::text, existing.id, existing.user_id;
  end if;

  update public.auth_registration_states
  set state = 'registration_expired', updated_at = now()
  where id = existing.id and state = 'registration_pending';
  return query select 'expired_cleanup'::text, existing.id, existing.user_id;
end;
$$;

create or replace function public.attach_registration_user(
  p_registration_id uuid,
  p_user_id uuid
) returns boolean
language plpgsql
security definer
set search_path = public, auth
as $$
begin
  update public.auth_registration_states
  set user_id = p_user_id, updated_at = now()
  where id = p_registration_id
    and state = 'registration_pending'
    and expires_at > now()
    and user_id is null;
  return found;
end;
$$;

create or replace function public.complete_registration_confirmation(
  p_user_id uuid,
  p_confirmed_at timestamptz
)
returns boolean
language plpgsql
security definer
set search_path = public, auth
as $$
begin
  update public.auth_registration_states
  set state = 'active', confirmed_at = now(), expires_at = null, updated_at = now()
  where user_id = p_user_id
    and state = 'registration_pending'
    and p_confirmed_at <= expires_at;
  return found;
end;
$$;

create or replace function public.begin_password_recovery(p_user_id uuid)
returns boolean
language plpgsql
security definer
set search_path = public, auth
as $$
begin
  insert into public.auth_recovery_locks (user_id, state, expires_at)
  values (p_user_id, 'recovery_locked', now() + interval '10 minutes')
  on conflict (user_id) do update
    set state = 'recovery_locked', expires_at = now() + interval '10 minutes',
        requested_at = now(), completed_at = null;
  return true;
end;
$$;

create or replace function public.complete_password_recovery(p_user_id uuid)
returns boolean
language plpgsql
security definer
set search_path = public, auth
as $$
declare
  lock_row public.auth_recovery_locks%rowtype;
  user_updated_at timestamptz;
begin
  select * into lock_row
  from public.auth_recovery_locks
  where user_id = p_user_id and state = 'recovery_locked'
  for update;
  if not found or lock_row.expires_at <= now() then
    return false;
  end if;
  select updated_at into user_updated_at from auth.users where id = p_user_id;
  if user_updated_at is null or user_updated_at <= lock_row.requested_at then
    return false;
  end if;
  update public.auth_recovery_locks
  set state = 'completed', completed_at = now()
  where user_id = p_user_id and state = 'recovery_locked' and expires_at > now();
  return found;
end;
$$;

create or replace function public.auth_login_gate(p_user_id uuid)
returns text
language sql
stable
security definer
set search_path = public, auth
as $$
  select case
    when exists (
      select 1 from public.auth_registration_states
      where user_id = p_user_id and state = 'registration_pending' and expires_at > now()
    ) then 'registration_pending'
    when exists (
      select 1 from public.auth_recovery_locks
      where user_id = p_user_id and state = 'recovery_locked' and expires_at > now()
    ) then 'recovery_locked'
    when exists (
      select 1 from auth.users where id = p_user_id and email_confirmed_at is not null
    ) then 'active'
    else 'unknown'
  end;
$$;

create or replace function public.cleanup_expired_auth_registrations()
returns integer
language plpgsql
security definer
set search_path = public, auth
as $$
declare
  deleted_count integer := 0;
begin
  update public.auth_registration_states state
  set state = 'active', confirmed_at = user_row.email_confirmed_at,
      expires_at = null, updated_at = now()
  from auth.users user_row
  where state.user_id = user_row.id
    and state.state = 'registration_pending'
    and state.expires_at <= now()
    and user_row.email_confirmed_at is not null
    and user_row.email_confirmed_at <= state.expires_at;

  delete from auth.users user_row
  using public.auth_registration_states state
  where state.user_id = user_row.id
    and state.state = 'registration_pending'
    and state.expires_at <= now()
    and (user_row.email_confirmed_at is null or user_row.email_confirmed_at > state.expires_at);
  get diagnostics deleted_count = row_count;

  delete from public.auth_registration_states
  where state = 'registration_pending'
    and user_id is null
    and expires_at <= now();

  return deleted_count;
end;
$$;

create extension if not exists pg_cron with schema extensions;
select cron.schedule(
  'edward-auth-registration-cleanup',
  '* * * * *',
  'select public.cleanup_expired_auth_registrations()'
);

revoke all on function public.reserve_registration_attempt(text, text) from public;
revoke all on function public.attach_registration_user(uuid, uuid) from public;
revoke all on function public.complete_registration_confirmation(uuid, timestamptz) from public;
revoke all on function public.begin_password_recovery(uuid) from public;
revoke all on function public.complete_password_recovery(uuid) from public;
revoke all on function public.auth_login_gate(uuid) from public;
revoke all on function public.cleanup_expired_auth_registrations() from public;
