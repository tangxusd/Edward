alter table public.subscription_plans add column if not exists billing_interval_count integer not null default 1;
alter table public.subscription_plans drop constraint if exists subscription_plans_billing_interval_count_check;
alter table public.subscription_plans add constraint subscription_plans_billing_interval_count_check check (billing_interval_count > 0 and billing_interval_count <= 120);

insert into public.subscription_plans (plan_key, name, currency, unit_amount, billing_interval, billing_interval_count, is_recurring, status)
values
  ('edward_test_monthly_001', 'Edward 联调月付', 'CNY', 1, 'month', 1, false, 'active'),
  ('edward_test_quarterly_001', 'Edward 联调季付', 'CNY', 1, 'month', 3, false, 'active'),
  ('edward_test_yearly_001', 'Edward 联调年付', 'CNY', 1, 'year', 1, false, 'active')
on conflict (plan_key) do update set name = excluded.name, unit_amount = excluded.unit_amount,
  billing_interval = excluded.billing_interval, billing_interval_count = excluded.billing_interval_count,
  is_recurring = excluded.is_recurring, status = excluded.status, effective_from = now(), effective_to = null, updated_at = now();

create or replace function public.apply_paid_order(p_order_id uuid)
returns jsonb language plpgsql security definer set search_path = public as $$
declare v_order public.orders%rowtype; v_plan public.subscription_plans%rowtype; v_start timestamptz; v_end timestamptz; v_subscription_id uuid;
begin
  select * into v_order from public.orders where id = p_order_id for update;
  if not found then raise exception 'order_not_found'; end if;
  if v_order.status <> 'paid' then raise exception 'order_not_paid'; end if;
  if exists (select 1 from public.subscriptions where entitlement_order_id = p_order_id) then return jsonb_build_object('already_applied', true); end if;
  select * into v_plan from public.subscription_plans where id = v_order.plan_id;
  if not found then raise exception 'plan_not_found'; end if;
  select id, current_period_end into v_subscription_id, v_end from public.subscriptions where user_id = v_order.user_id and plan_id = v_order.plan_id and status in ('trialing','active','past_due') order by current_period_end desc limit 1 for update;
  v_start := greatest(now(), coalesce(v_end, now()));
  if v_plan.billing_interval not in ('day','month','year') then raise exception 'unsupported_billing_interval'; end if;
  v_end := case v_plan.billing_interval when 'day' then v_start + make_interval(days => v_plan.billing_interval_count) when 'month' then v_start + make_interval(months => v_plan.billing_interval_count) when 'year' then v_start + make_interval(years => v_plan.billing_interval_count) end;
  if v_subscription_id is null then insert into public.subscriptions(user_id, plan_id, status, current_period_start, current_period_end, provider, entitlement_order_id) values (v_order.user_id, v_order.plan_id, 'active', v_start, v_end, v_order.provider, p_order_id) returning id into v_subscription_id;
  else update public.subscriptions set status='active', current_period_start=v_start, current_period_end=v_end, cancel_at_period_end=false, provider=v_order.provider, entitlement_order_id=p_order_id, updated_at=now() where id=v_subscription_id; end if;
  return jsonb_build_object('subscription_id', v_subscription_id, 'period_end', v_end);
end; $$;
