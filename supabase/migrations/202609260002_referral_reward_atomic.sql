create or replace function public.apply_referral_reward_once(p_request_id text, p_order_id uuid)
returns jsonb language plpgsql security definer set search_path = public as $$
declare
  existing jsonb;
  order_row public.orders%rowtype;
  referral_row public.referrals%rowtype;
  rule_row public.subscription_rules%rowtype;
  reward_id uuid;
begin
  if p_request_id is null or p_request_id = '' or p_order_id is null then raise exception 'invalid_reward_request'; end if;
  select metadata into existing from public.admin_audit_events where request_id = p_request_id and action = 'referral_reward';
  if existing is not null then return existing; end if;
  select * into order_row from public.orders where id = p_order_id for update;
  if not found or order_row.status <> 'paid' then raise exception 'order_not_paid'; end if;
  select * into referral_row from public.referrals where referred_user_id = order_row.user_id for update;
  if not found or referral_row.reward_status <> 'pending' then return jsonb_build_object('granted', false); end if;
  select * into rule_row from public.subscription_rules order by effective_from desc limit 1;
  if coalesce(rule_row.referral_reward_amount, 0) <= 0 then return jsonb_build_object('granted', false); end if;
  insert into public.credit_ledger(user_id, referral_id, order_id, amount, state, expires_at)
    values (referral_row.referrer_user_id, referral_row.id, order_row.id, rule_row.referral_reward_amount, 'available', now() + make_interval(days => rule_row.referral_reward_valid_days))
    returning id into reward_id;
  update public.referrals set first_paid_at = now(), reward_status = 'granted', updated_at = now() where id = referral_row.id;
  insert into public.admin_audit_events(request_id, actor_user_id, action, target_type, target_id, result, metadata)
    values (p_request_id, referral_row.referrer_user_id, 'referral_reward', 'order', order_row.id::text, 'success', jsonb_build_object('creditLedgerId', reward_id));
  return jsonb_build_object('granted', true, 'creditLedgerId', reward_id);
exception when unique_violation then
  select metadata into existing from public.admin_audit_events where request_id = p_request_id and action = 'referral_reward';
  if existing is not null then return existing; end if;
  raise;
end; $$;
revoke all on function public.apply_referral_reward_once(text, uuid) from public, anon, authenticated;
grant execute on function public.apply_referral_reward_once(text, uuid) to service_role;
