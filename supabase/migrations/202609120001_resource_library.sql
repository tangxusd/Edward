create extension if not exists pgcrypto;

insert into storage.buckets (id, name, public)
values ('resource-packages', 'resource-packages', false)
on conflict (id) do update set public = excluded.public;

create table if not exists public.resource_categories (
  id uuid primary key default gen_random_uuid(),
  tab_key text not null check (tab_key in ('media','text','audio','cards','chart','background','annotation','number')),
  parent_id uuid references public.resource_categories(id) on delete cascade,
  name text not null check (char_length(name) between 1 and 80),
  slug text not null check (slug ~ '^[a-z0-9][a-z0-9_-]{0,79}$'),
  sort_order integer not null default 0,
  status text not null default 'published' check (status in ('draft','published','archived')),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique (tab_key, parent_id, slug),
  unique (tab_key, id)
);

create table if not exists public.resources (
  id uuid primary key default gen_random_uuid(),
  component_id text not null unique check (component_id ~ '^[a-z0-9][a-z0-9._-]{2,127}$'),
  tab_key text not null check (tab_key in ('media','text','audio','cards','chart','background','annotation','number')),
  category_id uuid not null references public.resource_categories(id) on delete restrict,
  name text not null check (char_length(name) between 1 and 120),
  summary text not null default '',
  detail_markdown text not null default '',
  status text not null default 'draft' check (status in ('draft','published','archived')),
  visibility text not null default 'public' check (visibility in ('public','private','unlisted')),
  required_plan text not null default 'free',
  favorite_count bigint not null default 0 check (favorite_count >= 0),
  view_count bigint not null default 0 check (view_count >= 0),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  published_at timestamptz,
  constraint resources_category_tab_match foreign key (tab_key, category_id)
    references public.resource_categories(tab_key, id) deferrable initially immediate
);

create index if not exists resources_tab_category_idx on public.resources(tab_key, category_id, status);
create index if not exists resources_latest_idx on public.resources(tab_key, published_at desc nulls last, id desc);
create index if not exists resources_popular_idx on public.resources(tab_key, favorite_count desc, id desc);

create table if not exists public.resource_versions (
  id uuid primary key default gen_random_uuid(),
  resource_id uuid not null references public.resources(id) on delete cascade,
  version text not null check (version ~ '^[0-9]+\\.[0-9]+\\.[0-9]+$'),
  content_hash text not null check (content_hash ~ '^[a-f0-9]{64}$'),
  manifest_path text not null,
  package_path text not null,
  preview_image_path text,
  preview_video_path text,
  file_size bigint not null default 0 check (file_size >= 0),
  mime_type text not null default 'application/zip',
  min_app_version text,
  compatibility jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  published_at timestamptz,
  unique (resource_id, version),
  unique (resource_id, content_hash)
);
create index if not exists resource_versions_resource_idx on public.resource_versions(resource_id, published_at desc nulls last);

create table if not exists public.resource_favorites (
  user_id uuid not null references auth.users(id) on delete cascade,
  resource_id uuid not null references public.resources(id) on delete cascade,
  created_at timestamptz not null default now(),
  primary key (user_id, resource_id)
);

create table if not exists public.resource_views (
  user_id uuid references auth.users(id) on delete cascade,
  resource_id uuid not null references public.resources(id) on delete cascade,
  viewed_at timestamptz not null default now(),
  dedupe_key text not null,
  primary key (resource_id, dedupe_key)
);

create table if not exists public.entitlements (
  user_id uuid not null references auth.users(id) on delete cascade,
  plan_key text not null,
  status text not null check (status in ('active','grace','expired','revoked')),
  starts_at timestamptz not null,
  expires_at timestamptz,
  source text not null default 'manual',
  updated_at timestamptz not null default now(),
  primary key (user_id, plan_key)
);
create index if not exists entitlements_active_idx on public.entitlements(user_id, status, expires_at);

alter table public.resource_categories enable row level security;
alter table public.resources enable row level security;
alter table public.resource_versions enable row level security;
alter table public.resource_favorites enable row level security;
alter table public.resource_views enable row level security;
alter table public.entitlements enable row level security;

create policy resource_categories_read_published on public.resource_categories
  for select to authenticated using (status = 'published');
create policy resources_read_published on public.resources
  for select to authenticated using (status = 'published' and visibility in ('public','unlisted'));
create policy resource_versions_read_published on public.resource_versions
  for select to authenticated using (published_at is not null);
create policy favorites_read_own on public.resource_favorites
  for select to authenticated using (user_id = (select auth.uid()));
create policy favorites_insert_own on public.resource_favorites
  for insert to authenticated with check (user_id = (select auth.uid()));
create policy favorites_delete_own on public.resource_favorites
  for delete to authenticated using (user_id = (select auth.uid()));
create policy views_insert_own on public.resource_views
  for insert to authenticated with check (user_id = (select auth.uid()));
create policy entitlements_read_own on public.entitlements
  for select to authenticated using (user_id = (select auth.uid()));

revoke all on public.resource_categories, public.resources, public.resource_versions,
  public.resource_favorites, public.resource_views, public.entitlements from anon;
grant select on public.resource_categories, public.resources, public.resource_versions to authenticated;
grant select, insert, delete on public.resource_favorites to authenticated;
grant insert on public.resource_views to authenticated;
grant select on public.entitlements to authenticated;
