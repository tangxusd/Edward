create table if not exists public.payment_audit_events (
  id uuid primary key default gen_random_uuid(),
  order_id uuid references public.orders(id) on delete set null,
  provider text not null,
  phase text not null check (phase in ('create_request','create_response','notify','query')),
  http_status integer,
  provider_code text,
  provider_status text,
  payload jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now()
);
create index if not exists payment_audit_events_order_idx on public.payment_audit_events(order_id, created_at desc);
alter table public.payment_audit_events enable row level security;
revoke all on public.payment_audit_events from anon;
grant select on public.payment_audit_events to authenticated;
create policy payment_audit_events_read_own on public.payment_audit_events for select to authenticated using (exists (select 1 from public.orders o where o.id = order_id and o.user_id = (select auth.uid())));
