alter table public.resources
  add column if not exists target text not null default 'web.runtime';

alter table public.resources
  alter column target set default 'web.runtime';

update public.resources
  set target = 'web.runtime'
  where target <> 'web.runtime';

alter table public.resources
  drop constraint if exists resources_target_format;

alter table public.resources
  add constraint resources_target_format
  check (target = 'web.runtime');

create index if not exists resources_target_idx on public.resources(target);
