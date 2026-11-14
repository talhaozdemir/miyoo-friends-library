#!/bin/sh
# Controlled fake rclone for integration tests; no network or real credentials.
test "$1" = backend && test "$2" = copyid || exit 3
id=$4
target=$5
if test -n "$FL_TEST_PID_FILE"; then printf '%s\n' "$$" > "$FL_TEST_PID_FILE"; fi
case "$id" in
  copy_ok) cp "$FL_TEST_SOURCE" "$target" ;;
  copy_slow)
    printf 'partial' > "$target"
    exec sleep 30
    ;;
  copy_crash)
    printf 'partial' > "$target"
    kill -KILL "$PPID"
    exec sleep 30
    ;;
  copy_badsize) printf 'bad' > "$target" ;;
  copy_auth) printf 'invalid_grant: authorization revoked\n' >&2; exit 1 ;;
  copy_offline) printf 'dial tcp: no such host\n' >&2; exit 1 ;;
  copy_permission) printf '403 forbidden\n' >&2; exit 1 ;;
  copy_race)
    cp "$FL_TEST_SOURCE" "$target"
    printf 'concurrent ROM' > "$FL_TEST_RACE_TARGET"
    ;;
  *) exit 3 ;;
esac
