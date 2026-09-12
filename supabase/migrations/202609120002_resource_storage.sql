insert into storage.buckets (id, name, public)
values ('resource-packages', 'resource-packages', false)
on conflict (id) do update set public = excluded.public;

create policy resource_packages_read_authenticated
  on storage.objects for select to authenticated
  using (bucket_id = 'resource-packages');

create policy resource_packages_insert_service_role
  on storage.objects for insert to service_role
  with check (bucket_id = 'resource-packages');
