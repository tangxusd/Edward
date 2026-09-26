-- PostgreSQL regular-expression strings do not need an escaped dot here.
-- A character class keeps the semantic-version separator literal regardless
-- of string escape settings.
alter table public.resource_versions
  drop constraint if exists resource_versions_version_check;

alter table public.resource_versions
  add constraint resource_versions_version_check
  check (version ~ '^[0-9]+[.][0-9]+[.][0-9]+$');
