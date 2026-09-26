-- 已支付订单也必须经过权益幂等补偿，避免历史上订单状态先落库、周期写入失败后永久缺失。
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
    applied := public.apply_paid_order(p_order_id);
    return coalesce(applied, '{}'::jsonb) || jsonb_build_object('status', 'paid', 'already_paid', true, 'entitlement_repaired', true);
  end if;
  if o.status <> 'pending' then raise exception 'order_not_pending'; end if;

  update public.orders
    set status = 'paid',
        provider_order_id = coalesce(nullif(p_provider_order_id, ''), provider_order_id),
        provider_response = p_provider_response,
        updated_at = now()
    where id = p_order_id;
  applied := public.apply_paid_order(p_order_id);
  return coalesce(applied, '{}'::jsonb) || jsonb_build_object('status', 'paid', 'already_paid', false, 'entitlement_repaired', false);
end;
$$;

revoke all on function public.confirm_paid_order(uuid, text, bigint, jsonb) from public, anon, authenticated;
grant execute on function public.confirm_paid_order(uuid, text, bigint, jsonb) to service_role;
