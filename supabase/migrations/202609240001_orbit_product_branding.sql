update public.subscription_plans
set name = replace(name, 'Edward', 'Orbit')
where name like '%Edward%';
