-- 找回密码锁的过期与完成条件必须由数据库强制执行，不能依赖客户端清理。
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

revoke all on function public.complete_password_recovery(uuid), public.auth_login_gate(uuid) from public, anon, authenticated;
