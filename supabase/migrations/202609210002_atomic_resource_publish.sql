-- Storage is uploaded by the publisher only after local package validation. This
-- RPC makes the database half of publishing one transaction: a catalog resource
-- cannot become visible without a matching immutable published version.
create or replace function public.publish_resource_version(
  p_component_id text,
  p_target text,
  p_tab_key text,
  p_category_id uuid,
  p_name text,
  p_summary text,
  p_detail_markdown text,
  p_version text,
  p_content_hash text,
  p_manifest_path text,
  p_package_path text,
  p_preview_video_path text,
  p_file_size bigint,
  p_mime_type text,
  p_compatibility jsonb default '{}'::jsonb
)
returns uuid
language plpgsql
security definer
set search_path = public
as $$
declare
  v_resource_id uuid;
  v_existing_hash text;
begin
  insert into public.resources (
    component_id, target, tab_key, category_id, name, summary, detail_markdown,
    status, visibility, published_at
  ) values (
    p_component_id, p_target, p_tab_key, p_category_id, p_name, p_summary,
    p_detail_markdown, 'published', 'public', now()
  )
  on conflict (component_id) do update set
    target = excluded.target,
    tab_key = excluded.tab_key,
    category_id = excluded.category_id,
    name = excluded.name,
    summary = excluded.summary,
    detail_markdown = excluded.detail_markdown,
    status = 'published',
    visibility = 'public',
    published_at = coalesce(resources.published_at, excluded.published_at),
    updated_at = now()
  returning id into v_resource_id;

  insert into public.resource_versions (
    resource_id, version, content_hash, manifest_path, package_path,
    preview_image_path, preview_video_path, file_size, mime_type,
    compatibility, published_at
  ) values (
    v_resource_id, p_version, p_content_hash, p_manifest_path, p_package_path,
    null, p_preview_video_path, p_file_size, p_mime_type, p_compatibility, now()
  ) on conflict (resource_id, version) do nothing;

  select content_hash into v_existing_hash
  from public.resource_versions
  where resource_id = v_resource_id and version = p_version;

  if v_existing_hash is distinct from p_content_hash then
    raise exception 'resource_version_conflict';
  end if;
  return v_resource_id;
end;
$$;

revoke all on function public.publish_resource_version(
  text, text, text, uuid, text, text, text, text, text, text, text, text,
  bigint, text, jsonb
) from public;
grant execute on function public.publish_resource_version(
  text, text, text, uuid, text, text, text, text, text, text, text, text,
  bigint, text, jsonb
) to service_role;
