#!/bin/sh
# Deterministic host-test fixture. No network and no real credentials.
config=
previous=
for argument in "$@"; do
  if [ "$previous" = --config ]; then config=$argument; break; fi
  previous=$argument
done
if [ -n "$FL_MOCK_CALLS" ]; then printf 'call\n' >> "$FL_MOCK_CALLS"; fi
case "$FL_MOCK_MODE" in
  renew|renew_revoke|renew_slow)
    if ! grep -q '"expiry":"1970-01-01T00:00:01Z"' "$config"; then
      printf '%s\n' 'googleapi: Error 401: Request had invalid authentication credentials. NEVER_SHOW_THIS_TOKEN' >&2
      exit 1
    fi
    grep -q 'fake-refresh' "$config" || exit 4
    if [ "$FL_MOCK_MODE" = renew_revoke ]; then printf '%s\n' 'invalid_grant: NEVER_SHOW_THIS_TOKEN' >&2; exit 1; fi
    if [ "$FL_MOCK_MODE" = renew_slow ]; then exec sleep 20; fi
    sed -e 's/"access_token":"fake-access"/"access_token":"fake-access-renewed"/' -e 's/1970-01-01T00:00:01Z/2099-01-01T00:00:00Z/' "$config" > "$config.mocktmp" || exit 4
    mv "$config.mocktmp" "$config" || exit 4
    ;;
  always401) printf '%s\n' 'googleapi: Error 401: Unauthorized' >&2; exit 1 ;;
  client) printf '%s\n' 'invalid_client: Unauthorized NEVER_SHOW_THIS_TOKEN' >&2; exit 1 ;;
  auth) printf '%s\n' 'invalid_grant: access_token=NEVER_SHOW_THIS_TOKEN' >&2; exit 1 ;;
  permission) printf '%s\n' '403 forbidden' >&2; exit 1 ;;
  offline) printf '%s\n' 'dial tcp: no such host' >&2; exit 1 ;;
  timeout) printf '%s\n' 'context deadline exceeded' >&2; exit 1 ;;
  rate) printf '%s\n' '403 userRateLimitExceeded' >&2; exit 1 ;;
  invalid) printf '%s\n' 'not JSON'; exit 0 ;;
  empty) printf '%s\n' '[]'; exit 0 ;;
  slow) sleep 20; exit 0 ;;
esac
if [ "$1" = config ]; then exit 0; fi
if [ "$1" != lsjson ]; then exit 2; fi
case "$2" in
  friends,root_folder_id=*:) ;;
  *) printf '%s\n' 'Unexpected unpinned root' >&2; exit 2 ;;
esac
cat "$FL_MOCK_LISTING"
