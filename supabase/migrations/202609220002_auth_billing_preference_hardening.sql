-- 强制恢复密码期限，并在恢复开始时撤销已有会话。
create or replace function public.revoke_user_sessions(p_user_id uuid)
returns void
language plpgsql
security definer
set search_path = public, auth
as $$
begin
  delete from auth.sessions where user_id = p_user_id;
end;
$$;

create or replace function public.enforce_recovery_password_deadline()
returns trigger
language plpgsql
security definer
set search_path = public, auth
as $$
begin
  if old.encrypted_password is distinct from new.encrypted_password and exists (
    select 1 from public.auth_recovery_locks
    where user_id = new.id and state = 'recovery_locked' and expires_at <= now()
  ) then
    raise exception 'password_recovery_expired';
  end if;
  return new;
end;
$$;

drop trigger if exists auth_recovery_password_deadline on auth.users;
create trigger auth_recovery_password_deadline
before update of encrypted_password on auth.users
for each row execute function public.enforce_recovery_password_deadline();

create or replace function public.begin_password_recovery(p_user_id uuid)
returns boolean
language plpgsql
security definer
set search_path = public, auth
as $$
begin
  perform public.revoke_user_sessions(p_user_id);
  insert into public.auth_recovery_locks (user_id, state, expires_at)
  values (p_user_id, 'recovery_locked', now() + interval '10 minutes')
  on conflict (user_id) do update
    set state = 'recovery_locked', expires_at = now() + interval '10 minutes',
        requested_at = now(), completed_at = null;
  return true;
end;
$$;

revoke all on function public.revoke_user_sessions(uuid), public.enforce_recovery_password_deadline() from public, anon, authenticated;
grant execute on function public.revoke_user_sessions(uuid), public.enforce_recovery_password_deadline() to service_role;

-- 本地偏好事实按账号隔离；旧数据归入匿名设备范围，不会自动上传到账号。
alter table public.preference_facts add column if not exists account_scope text not null default 'anonymous';
alter table public.preference_facts drop constraint if exists preference_facts_pkey;
alter table public.preference_facts add primary key (user_id, account_scope, event_id);

-- 订单折扣积分必须先冻结，支付成功后才转为 consumed。
create or replace function public.reserve_order_credits(p_order_id uuid)
returns boolean
language plpgsql
security definer
set search_path = public
as $$
declare
  o public.orders%rowtype;
  row_item record;
  remaining bigint;
  take_amount bigint;
begin
  select * into o from public.orders where id = p_order_id for update;
  if not found or o.credit_amount <= 0 then return true; end if;
  remaining := o.credit_amount;
  for row_item in
    select * from public.credit_ledger
    where user_id = o.user_id and state = 'available'
      and (expires_at is null or expires_at > now()) and amount > 0
    order by expires_at nulls last, created_at, id for update
  loop
    exit when remaining <= 0;
    take_amount := least(remaining, row_item.amount);
    if take_amount = row_item.amount then
      update public.credit_ledger set state = 'frozen', order_id = p_order_id where id = row_item.id;
    else
      update public.credit_ledger set amount = amount - take_amount where id = row_item.id;
      insert into public.credit_ledger(user_id, referral_id, order_id, amount, state, expires_at)
        values(row_item.user_id, row_item.referral_id, p_order_id, take_amount, 'frozen', row_item.expires_at);
    end if;
    remaining := remaining - take_amount;
  end loop;
  if remaining > 0 then raise exception 'insufficient_credit'; end if;
  return true;
end;
$$;

create or replace function public.finalize_order_credits(p_order_id uuid)
returns boolean
language sql
security definer
set search_path = public
as $$
  update public.credit_ledger set state = 'consumed' where order_id = p_order_id and state = 'frozen';
  select true;
$$;

create or replace function public.release_order_credits(p_order_id uuid)
returns boolean
language sql
security definer
set search_path = public
as $$
  update public.credit_ledger set state = 'available', order_id = null where order_id = p_order_id and state = 'frozen';
  select true;
$$;

-- 统一支付确认入口：确认成功后将本单冻结额度消费，重复确认保持幂等。
create or replace function public.confirm_paid_order(
  p_order_id uuid,
  p_provider_order_id text,
  p_paid_amount bigint,
  p_provider_response jsonb
)
returns jsonb
language plpgsql
security definer
set search_path = public
as $$
declare
  o public.orders%rowtype;
  applied jsonb;
begin
  select * into o from public.orders where id = p_order_id for update;
  if not found then raise exception 'order_not_found'; end if;
  if p_paid_amount <> o.paid_amount then raise exception 'payment_amount_mismatch'; end if;
  if o.status = 'paid' then
    perform public.finalize_order_credits(p_order_id);
    return jsonb_build_object('status', 'paid', 'already_paid', true);
  end if;
  if o.status <> 'pending' then raise exception 'order_not_pending'; end if;

  update public.orders
    set status = 'paid',
        provider_order_id = coalesce(nullif(p_provider_order_id, ''), provider_order_id),
        provider_response = p_provider_response,
        updated_at = now()
    where id = p_order_id;
  applied := public.apply_paid_order(p_order_id);
  perform public.finalize_order_credits(p_order_id);
  return coalesce(applied, '{}'::jsonb) || jsonb_build_object('status', 'paid', 'already_paid', false);
end;
$$;

revoke all on function public.reserve_order_credits(uuid), public.finalize_order_credits(uuid), public.release_order_credits(uuid) from public, anon, authenticated;
grant execute on function public.reserve_order_credits(uuid), public.finalize_order_credits(uuid), public.release_order_credits(uuid) to service_role;
revoke all on function public.confirm_paid_order(uuid, text, bigint, jsonb) from public, anon, authenticated;
grant execute on function public.confirm_paid_order(uuid, text, bigint, jsonb) to service_role;
