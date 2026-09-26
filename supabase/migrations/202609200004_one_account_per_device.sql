create or replace function public.reserve_registration_attempt(
  p_email_normalized text,
  p_device_fingerprint text
) returns table(action text, registration_id uuid, user_id uuid)
language plpgsql
security definer
set search_path = public, auth
as $$
declare
  normalized_email text := lower(trim(p_email_normalized));
  existing public.auth_registration_states%rowtype;
begin
  perform pg_advisory_xact_lock(hashtextextended(p_device_fingerprint, 0));
  perform pg_advisory_xact_lock(hashtextextended(normalized_email, 0));

  select * into existing
  from public.auth_registration_states
  where email_normalized = normalized_email
  for update;

  if found then
    if existing.state = 'active' then
      return query select 'active'::text, existing.id, existing.user_id;
      return;
    elsif existing.state = 'registration_pending' and existing.expires_at > now() then
      return query select 'pending'::text, existing.id, existing.user_id;
      return;
    end if;

    update public.auth_registration_states
    set state = 'registration_expired', updated_at = now()
    where id = existing.id and state = 'registration_pending';
    return query select 'expired_cleanup'::text, existing.id, existing.user_id;
    return;
  end if;

  select * into existing
  from public.auth_registration_states
  where device_fingerprint = p_device_fingerprint
    and (state = 'active' or (state = 'registration_pending' and expires_at > now()))
  order by created_at
  limit 1
  for update;

  if found then
    return query select 'device_already_registered'::text, existing.id, existing.user_id;
    return;
  end if;

  insert into public.auth_registration_states (email_normalized, device_fingerprint, state, expires_at)
  values (normalized_email, p_device_fingerprint, 'registration_pending', now() + interval '10 minutes')
  returning * into existing;
  return query select 'create'::text, existing.id, existing.user_id;
end;
$$;
