#!/bin/sh
# Deterministic host-test fixture. No network and no real credentials.
case "$FL_MOCK_MODE" in
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
