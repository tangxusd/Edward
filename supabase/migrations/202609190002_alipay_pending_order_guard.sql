-- 支付宝二维码有效期为 30 分钟。清理历史残留后，由数据库保证每个用户、每个方案最多一笔待支付订单。
update public.orders
set status = 'expired',
    updated_at = now()
where provider = 'alipay'
  and status = 'pending'
  and created_at < now() - interval '30 minutes';

create unique index if not exists orders_alipay_one_pending_plan_per_user_idx
  on public.orders(user_id, plan_id)
  where provider = 'alipay' and status = 'pending';
