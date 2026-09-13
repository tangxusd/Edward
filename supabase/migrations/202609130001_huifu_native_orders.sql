alter table public.orders add column if not exists provider_order_id text;
alter table public.orders add column if not exists provider_request_id text;
alter table public.orders add column if not exists provider_qr_code text;
alter table public.orders add column if not exists provider_response jsonb;
create unique index if not exists orders_provider_order_id_idx on public.orders(provider_order_id) where provider_order_id is not null;
