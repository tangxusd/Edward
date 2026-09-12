do $$
begin
  if to_regclass('public.profiles') is null then raise exception 'profiles table missing'; end if;
  if to_regclass('public.subscription_plans') is null then raise exception 'subscription_plans table missing'; end if;
  if exists (select 1 from pg_policies where schemaname='public' and tablename='profiles' and roles @> array['anon']) then raise exception 'anon profile policy must not exist'; end if;
  if exists (select 1 from pg_policies where schemaname='public' and tablename='credit_ledger' and cmd='INSERT' and roles @> array['authenticated']) then raise exception 'client credit insert policy must not exist'; end if;
end $$;
