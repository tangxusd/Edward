-- Public resources use one lifecycle field. Published identities and versions are
-- immutable records: remove them from discovery by status instead of deleting them.
alter table public.resources
  drop constraint if exists resources_status_check;

alter table public.resources
  add constraint resources_status_check
  check (status in ('draft', 'published', 'archived', 'withdrawn'));

create index if not exists resources_popular_category_idx
  on public.resources (tab_key, category_id, favorite_count desc, published_at desc nulls last, id desc)
  where status = 'published' and visibility in ('public', 'unlisted');

create index if not exists resource_favorites_user_created_idx
  on public.resource_favorites (user_id, created_at desc, resource_id desc);

create or replace function public.prevent_published_resource_delete()
returns trigger
language plpgsql
set search_path = public
as $$
begin
  if old.status in ('published', 'archived', 'withdrawn') then
    raise exception 'published_resource_must_be_archived';
  end if;
  return old;
end;
$$;

drop trigger if exists resources_prevent_published_delete on public.resources;
create trigger resources_prevent_published_delete
  before delete on public.resources
  for each row execute function public.prevent_published_resource_delete();

create or replace function public.prevent_published_resource_version_delete()
returns trigger
language plpgsql
set search_path = public
as $$
begin
  if old.published_at is not null then
    raise exception 'published_resource_version_must_be_preserved';
  end if;
  return old;
end;
$$;

drop trigger if exists resource_versions_prevent_published_delete on public.resource_versions;
create trigger resource_versions_prevent_published_delete
  before delete on public.resource_versions
  for each row execute function public.prevent_published_resource_version_delete();

create or replace function public.apply_resource_favorite_count()
returns trigger
language plpgsql
security definer
set search_path = public
as $$
begin
  if tg_op = 'INSERT' then
    update public.resources
      set favorite_count = favorite_count + 1,
          updated_at = now()
      where id = new.resource_id;
    return new;
  end if;

  update public.resources
    set favorite_count = greatest(favorite_count - 1, 0),
        updated_at = now()
    where id = old.resource_id;
  return old;
end;
$$;

drop trigger if exists resource_favorites_favorite_count on public.resource_favorites;
create trigger resource_favorites_favorite_count
  after insert or delete on public.resource_favorites
  for each row execute function public.apply_resource_favorite_count();

create or replace function public.recount_resource_favorite_counts()
returns void
language sql
security definer
set search_path = public
as $$
  update public.resources as resource
    set favorite_count = counts.total,
        updated_at = now()
    from (
      select resource_id, count(*)::bigint as total
      from public.resource_favorites
      group by resource_id
    ) as counts
    where resource.id = counts.resource_id;

  update public.resources
    set favorite_count = 0,
        updated_at = now()
    where favorite_count <> 0
      and not exists (
        select 1 from public.resource_favorites
        where resource_favorites.resource_id = resources.id
      );
$$;

revoke all on function public.recount_resource_favorite_counts() from public;
grant execute on function public.recount_resource_favorite_counts() to service_role;

create or replace function public.list_favorite_resources(
  p_tab_key text,
  p_category_id uuid default null,
  p_limit integer default 24,
  p_offset integer default 0
)
returns setof public.resources
language sql
stable
security invoker
set search_path = public
as $$
  select resource.*
  from public.resources as resource
  inner join public.resource_favorites as favorite
    on favorite.resource_id = resource.id
  where favorite.user_id = (select auth.uid())
    and resource.tab_key = p_tab_key
    and resource.status = 'published'
    and resource.visibility in ('public', 'unlisted')
    and (p_category_id is null or resource.category_id = p_category_id)
  order by favorite.created_at desc, resource.id desc
  limit greatest(1, least(p_limit, 40))
  offset greatest(p_offset, 0);
$$;

revoke all on function public.list_favorite_resources(text, uuid, integer, integer) from public;
grant execute on function public.list_favorite_resources(text, uuid, integer, integer) to authenticated;
