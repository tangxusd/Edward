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
  plan_row public.subscription_plans%rowtype;
  current_end timestamptz;
  start_at timestamptz;
  end_at timestamptz;
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
  if batch_row.entitlement_type = 'subscription' then
    select * into plan_row from public.subscription_plans
      where status = 'active' and ((batch_row.duration_months = 12 and billing_interval = 'year' and billing_interval_count = 1)
        or (batch_row.duration_months in (1, 3) and billing_interval = 'month' and billing_interval_count = batch_row.duration_months))
      order by unit_amount asc limit 1;
    if not found then raise exception 'subscription_plan_not_found'; end if;
    select current_period_end into current_end from public.subscriptions
      where user_id = p_user_id and plan_id = plan_row.id and status in ('trialing','active','past_due')
      order by current_period_end desc limit 1 for update;
    start_at := greatest(now(), coalesce(current_end, now()));
    end_at := case plan_row.billing_interval when 'month' then start_at + make_interval(months => plan_row.billing_interval_count)
      when 'year' then start_at + make_interval(years => plan_row.billing_interval_count) end;
    if current_end is null then
      insert into public.subscriptions(user_id, plan_id, status, current_period_start, current_period_end, provider)
        values (p_user_id, plan_row.id, 'active', start_at, end_at, 'redeem_code');
    else
      update public.subscriptions set status = 'active', current_period_start = start_at, current_period_end = end_at,
        cancel_at_period_end = false, provider = 'redeem_code', updated_at = now()
        where user_id = p_user_id and plan_id = plan_row.id and current_period_end = current_end;
    end if;
  elsif batch_row.entitlement_type = 'credit' then
    insert into public.credit_ledger(user_id, amount, state, expires_at)
      values (p_user_id, batch_row.credit_amount, 'available', batch_row.expires_at);
  end if;
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
