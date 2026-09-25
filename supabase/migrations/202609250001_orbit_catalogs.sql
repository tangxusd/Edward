create table if not exists public.color_catalog_revisions (
  revision text primary key,
  parent_revision text references public.color_catalog_revisions(revision),
  full_hash text not null check (full_hash ~ '^[0-9a-f]{64}$'),
  status text not null check (status in ('draft','validated','staged','published','deprecated','rolled_back')),
  change_summary text not null default '',
  published_at timestamptz,
  created_at timestamptz not null default now(),
  unique (full_hash)
);

create table if not exists public.color_catalog_items (
  revision text not null references public.color_catalog_revisions(revision) on delete cascade,
  semantic_path text not null check (char_length(semantic_path) between 1 and 256),
  value_type text not null check (value_type in ('rgba','gradient','shadow','svg')),
  value jsonb not null,
  content_hash text not null check (content_hash ~ '^[0-9a-f]{64}$'),
  created_at timestamptz not null default now(),
  primary key (revision, semantic_path)
);

create table if not exists public.font_catalog_revisions (
  revision text primary key,
  parent_revision text references public.font_catalog_revisions(revision),
  full_hash text not null check (full_hash ~ '^[0-9a-f]{64}$'),
  status text not null check (status in ('draft','validated','staged','published','deprecated','rolled_back')),
  change_summary text not null default '',
  published_at timestamptz,
  created_at timestamptz not null default now(),
  unique (full_hash)
);

create table if not exists public.font_catalog_items (
  revision text not null references public.font_catalog_revisions(revision) on delete cascade,
  font_id text not null check (font_id ~ '^[a-z0-9][a-z0-9._-]{1,127}$'),
  display_name text not null,
  family_name text not null,
  style_name text not null default 'Regular',
  weight integer not null default 400 check (weight between 1 and 1000),
  preview_object_key text not null,
  install_object_key text not null,
  content_hash text not null check (content_hash ~ '^[0-9a-f]{64}$'),
  file_size bigint not null check (file_size > 0 and file_size <= 52428800),
  mime_type text not null check (mime_type in ('font/ttf','font/otf','font/woff','font/woff2')),
  license_id text not null,
  created_at timestamptz not null default now(),
  primary key (revision, font_id)
);

create index if not exists color_catalog_revisions_status_idx
  on public.color_catalog_revisions(status, published_at desc);
create index if not exists font_catalog_revisions_status_idx
  on public.font_catalog_revisions(status, published_at desc);
create index if not exists font_catalog_items_font_id_idx
  on public.font_catalog_items(font_id, revision);

alter table public.color_catalog_revisions enable row level security;
alter table public.color_catalog_items enable row level security;
alter table public.font_catalog_revisions enable row level security;
alter table public.font_catalog_items enable row level security;

drop policy if exists color_catalog_published_read on public.color_catalog_revisions;
create policy color_catalog_published_read on public.color_catalog_revisions
  for select to authenticated using (status = 'published');
drop policy if exists color_catalog_items_published_read on public.color_catalog_items;
create policy color_catalog_items_published_read on public.color_catalog_items
  for select to authenticated using (
    exists (select 1 from public.color_catalog_revisions r
      where r.revision = color_catalog_items.revision and r.status = 'published')
  );
drop policy if exists font_catalog_published_read on public.font_catalog_revisions;
create policy font_catalog_published_read on public.font_catalog_revisions
  for select to authenticated using (status = 'published');
drop policy if exists font_catalog_items_published_read on public.font_catalog_items;
create policy font_catalog_items_published_read on public.font_catalog_items
  for select to authenticated using (
    exists (select 1 from public.font_catalog_revisions r
      where r.revision = font_catalog_items.revision and r.status = 'published')
  );

revoke all on public.color_catalog_revisions, public.color_catalog_items,
  public.font_catalog_revisions, public.font_catalog_items from anon;
grant select on public.color_catalog_revisions, public.color_catalog_items,
  public.font_catalog_revisions, public.font_catalog_items to authenticated;

create or replace function public.published_catalog_manifest(catalog_type text, known_revision text default null)
returns jsonb
language plpgsql
security invoker
set search_path = public
as $$
declare
  latest_revision text;
  latest_hash text;
  parent_revision text;
begin
  if catalog_type not in ('color','font') then
    raise exception using errcode = '22023', message = 'unsupported_catalog_type';
  end if;
  if catalog_type = 'color' then
    select revision, full_hash, parent_revision into latest_revision, latest_hash, parent_revision
      from public.color_catalog_revisions where status = 'published'
      order by published_at desc nulls last, created_at desc limit 1;
  else
    select revision, full_hash, parent_revision into latest_revision, latest_hash, parent_revision
      from public.font_catalog_revisions where status = 'published'
      order by published_at desc nulls last, created_at desc limit 1;
  end if;
  if latest_revision is null then
    return jsonb_build_object('revision', null, 'fullHash', null, 'delta', jsonb_build_object('items', '[]'::jsonb, 'removed', '[]'::jsonb));
  end if;
  if known_revision is not null and known_revision = latest_revision then
    return jsonb_build_object('revision', latest_revision, 'fullHash', latest_hash,
      'delta', jsonb_build_object('items', '[]'::jsonb, 'removed', '[]'::jsonb));
  end if;
  if catalog_type = 'color' then
    return jsonb_build_object('revision', latest_revision, 'fullHash', latest_hash,
      'parentRevision', parent_revision,
      'delta', jsonb_build_object('items', coalesce((select jsonb_agg(to_jsonb(i)) from public.color_catalog_items i where i.revision = latest_revision), '[]'::jsonb), 'removed', '[]'::jsonb));
  end if;
  return jsonb_build_object('revision', latest_revision, 'fullHash', latest_hash,
    'parentRevision', parent_revision,
    'delta', jsonb_build_object('items', coalesce((select jsonb_agg(to_jsonb(i)) from public.font_catalog_items i where i.revision = latest_revision), '[]'::jsonb), 'removed', '[]'::jsonb));
end;
$$;

revoke all on function public.published_catalog_manifest(text, text) from public;
grant execute on function public.published_catalog_manifest(text, text) to authenticated;
