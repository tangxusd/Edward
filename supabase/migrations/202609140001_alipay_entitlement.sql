alter table public.subscriptions add column if not exists entitlement_order_id uuid;
create unique index if not exists subscriptions_entitlement_order_idx on public.subscriptions(entitlement_order_id) where entitlement_order_id is not null;

create or replace function public.apply_paid_order(p_order_id uuid)
returns jsonb
language plpgsql
security definer
set search_path = public
as $$
declare
  v_order public.orders%rowtype;
  v_plan public.subscription_plans%rowtype;
  v_start timestamptz;
  v_end timestamptz;
  v_subscription_id uuid;
begin
  select * into v_order from public.orders where id = p_order_id for update;
  if not found then raise exception 'order_not_found'; end if;
  if v_order.status <> 'paid' then raise exception 'order_not_paid'; end if;
  if exists (select 1 from public.subscriptions where entitlement_order_id = p_order_id) then
    return jsonb_build_object('already_applied', true);
  end if;
  select * into v_plan from public.subscription_plans where id = v_order.plan_id;
  if not found then raise exception 'plan_not_found'; end if;
  select id, current_period_end into v_subscription_id, v_end
    from public.subscriptions
    where user_id = v_order.user_id and plan_id = v_order.plan_id
      and status in ('trialing','active','past_due')
    order by current_period_end desc limit 1 for update;
  v_start := greatest(now(), coalesce(v_end, now()));
  if v_plan.billing_interval not in ('day', 'month', 'year') then raise exception 'unsupported_billing_interval'; end if;
  v_end := case v_plan.billing_interval
    when 'day' then v_start + make_interval(days => 1)
    when 'month' then v_start + make_interval(months => 1)
    when 'year' then v_start + make_interval(years => 1)
  end;
  if v_subscription_id is null then
    insert into public.subscriptions(user_id, plan_id, status, current_period_start, current_period_end, provider, entitlement_order_id)
      values (v_order.user_id, v_order.plan_id, 'active', v_start, v_end, v_order.provider, p_order_id)
      returning id into v_subscription_id;
  else
    update public.subscriptions set status = 'active', current_period_start = v_start,
      current_period_end = v_end, cancel_at_period_end = false, provider = v_order.provider, entitlement_order_id = p_order_id, updated_at = now()
      where id = v_subscription_id;
  end if;
  return jsonb_build_object('subscription_id', v_subscription_id, 'period_end', v_end);
end;
$$;

revoke all on function public.apply_paid_order(uuid) from public, anon, authenticated;
grant execute on function public.apply_paid_order(uuid) to service_role;
