-- Version metadata is readable only when its owning resource is discoverable.
-- Storage objects remain private and are exposed only by the entitlement-checked
-- detail function, but this closes the direct PostgREST metadata path as well.
drop policy if exists resource_versions_read_published on public.resource_versions;

create policy resource_versions_read_published on public.resource_versions
  for select to authenticated using (
    published_at is not null
    and exists (
      select 1
      from public.resources
      where resources.id = resource_versions.resource_id
        and resources.status = 'published'
        and resources.visibility in ('public', 'unlisted')
    )
  );
