create table if not exists public.subscription_periods (
  id uuid primary key default gen_random_uuid(), subscription_id uuid not null references public.subscriptions(id) on delete cascade,
  order_id uuid not null unique references public.orders(id), user_id uuid not null references auth.users(id) on delete cascade,
  period_start timestamptz not null, period_end timestamptz not null, amount bigint not null check (amount >= 0),
  status text not null default 'scheduled' check (status in ('scheduled','active','refunded','expired')), created_at timestamptz not null default now(), refunded_at timestamptz
);
create index if not exists subscription_periods_user_idx on public.subscription_periods(user_id, period_start);
alter table public.subscription_periods enable row level security;
revoke all on public.subscription_periods from anon;
grant select on public.subscription_periods to authenticated;
create policy subscription_periods_read_own on public.subscription_periods for select to authenticated using (user_id = (select auth.uid()));
alter table public.payment_audit_events drop constraint if exists payment_audit_events_phase_check;
alter table public.payment_audit_events add constraint payment_audit_events_phase_check check (phase in ('create_request','create_response','notify','query','refund_request','refund_response'));

create or replace function public.apply_paid_order(p_order_id uuid)
returns jsonb language plpgsql security definer set search_path = public as $$
declare o public.orders%rowtype; p public.subscription_plans%rowtype; s public.subscriptions%rowtype; start_at timestamptz; end_at timestamptz; sid uuid;
begin
  select * into o from orders where id=p_order_id for update; if not found then raise exception 'order_not_found'; end if;
  if o.status <> 'paid' then raise exception 'order_not_paid'; end if;
  if exists(select 1 from subscription_periods where order_id=p_order_id) then return jsonb_build_object('already_applied',true); end if;
  select * into p from subscription_plans where id=o.plan_id; if not found then raise exception 'plan_not_found'; end if;
  select * into s from subscriptions where user_id=o.user_id and plan_id=o.plan_id and status in ('trialing','active','past_due') order by current_period_end desc limit 1 for update;
  sid := s.id; start_at := greatest(now(), coalesce(s.current_period_end, now()));
  if p.billing_interval not in ('day','month','year') then raise exception 'unsupported_billing_interval'; end if;
  end_at := case p.billing_interval when 'day' then start_at + make_interval(days=>p.billing_interval_count) when 'month' then start_at + make_interval(months=>p.billing_interval_count) when 'year' then start_at + make_interval(years=>p.billing_interval_count) end;
  if sid is null then insert into subscriptions(user_id,plan_id,status,current_period_start,current_period_end,provider,entitlement_order_id) values(o.user_id,o.plan_id,'active',start_at,end_at,o.provider,p_order_id) returning id into sid;
  else update subscriptions set status='active', current_period_end=end_at, cancel_at_period_end=false, provider=o.provider, entitlement_order_id=p_order_id, updated_at=now() where id=sid; end if;
  insert into subscription_periods(subscription_id,order_id,user_id,period_start,period_end,amount,status) values(sid,p_order_id,o.user_id,start_at,end_at,o.paid_amount,case when start_at <= now() then 'active' else 'scheduled' end);
  return jsonb_build_object('subscription_id',sid,'period_start',start_at,'period_end',end_at);
end; $$;

create or replace function public.refund_scheduled_period(p_order_id uuid)
returns jsonb language plpgsql security definer set search_path = public as $$
declare r subscription_periods%rowtype; max_end timestamptz;
begin
  select * into r from subscription_periods where order_id=p_order_id for update;
  if not found then raise exception 'period_not_found'; end if;
  if r.period_start <= now() or r.status = 'active' then raise exception 'current_period_not_refundable'; end if;
  if r.status = 'refunded' then return jsonb_build_object('already_refunded',true); end if;
  update subscription_periods set status='refunded', refunded_at=now() where id=r.id;
  select max(period_end) into max_end from subscription_periods where subscription_id=r.subscription_id and status in ('active','scheduled');
  update subscriptions set current_period_end=coalesce(max_end,now()), status=case when coalesce(max_end,now()) <= now() then 'expired' else 'active' end, updated_at=now() where id=r.subscription_id;
  return jsonb_build_object('subscription_id',r.subscription_id,'period_end',max_end);
end; $$;
revoke all on function public.apply_paid_order(uuid), public.refund_scheduled_period(uuid) from public,anon,authenticated;
grant execute on function public.apply_paid_order(uuid), public.refund_scheduled_period(uuid) to service_role;
