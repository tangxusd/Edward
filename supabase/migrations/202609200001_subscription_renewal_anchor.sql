-- 续费规则：
-- 1. 未到期：从原 current_period_end 继续叠加，保留首次付费的 current_period_start。
-- 2. 已过期：从本次支付时间重新起算，并重置 current_period_start。
-- 3. 月付、季付、年付分别由 month×1、month×3、month×12（或 year×1）计算。
create or replace function public.apply_paid_order(p_order_id uuid)
returns jsonb
language plpgsql
security definer
set search_path = public
as $$
declare
  o public.orders%rowtype;
  p public.subscription_plans%rowtype;
  s public.subscriptions%rowtype;
  start_at timestamptz;
  end_at timestamptz;
  sid uuid;
  extending boolean;
begin
  select * into o from public.orders where id = p_order_id for update;
  if not found then raise exception 'order_not_found'; end if;
  if o.status <> 'paid' then raise exception 'order_not_paid'; end if;
  if exists(select 1 from public.subscription_periods where order_id = p_order_id) then
    return jsonb_build_object('already_applied', true);
  end if;

  select * into p from public.subscription_plans where id = o.plan_id;
  if not found then raise exception 'plan_not_found'; end if;
  if p.billing_interval not in ('day', 'month', 'year') then raise exception 'unsupported_billing_interval'; end if;

  -- 查找同一用户/方案的现有订阅，包括 expired，避免过期后无谓新建订阅记录。
  select * into s
    from public.subscriptions
    where user_id = o.user_id and plan_id = o.plan_id
    order by current_period_end desc nulls last, updated_at desc
    limit 1
    for update;
  sid := s.id;
  extending := s.id is not null and s.current_period_end is not null and s.current_period_end > now();
  start_at := case when extending then s.current_period_end else now() end;
  end_at := case p.billing_interval
    when 'day' then start_at + make_interval(days => p.billing_interval_count)
    when 'month' then start_at + make_interval(months => p.billing_interval_count)
    when 'year' then start_at + make_interval(years => p.billing_interval_count)
  end;

  if sid is null then
    insert into public.subscriptions(user_id, plan_id, status, current_period_start, current_period_end, provider, entitlement_order_id)
      values(o.user_id, o.plan_id, 'active', start_at, end_at, o.provider, p_order_id)
      returning id into sid;
  else
    update public.subscriptions
      set status = 'active',
          current_period_start = case when extending then s.current_period_start else start_at end,
          current_period_end = end_at,
          cancel_at_period_end = false,
          provider = o.provider,
          entitlement_order_id = p_order_id,
          updated_at = now()
      where id = sid;
  end if;

  insert into public.subscription_periods(subscription_id, order_id, user_id, period_start, period_end, amount, status)
    values(sid, p_order_id, o.user_id, start_at, end_at, o.paid_amount, case when start_at <= now() then 'active' else 'scheduled' end);
  return jsonb_build_object(
    'subscription_id', sid,
    'period_start', start_at,
    'period_end', end_at,
    'renewal_mode', case when extending then 'extend' else 'restart' end
  );
end;
$$;

revoke all on function public.apply_paid_order(uuid) from public, anon, authenticated;
grant execute on function public.apply_paid_order(uuid) to service_role;
