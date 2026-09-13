alter table public.resources
  add column if not exists target text not null default 'resolve.fusion';

alter table public.resources
  drop constraint if exists resources_target_format;

alter table public.resources
  add constraint resources_target_format
  check (target ~ '^[a-z][a-z0-9._:-]{1,127}$');

create index if not exists resources_target_idx on public.resources(target);
