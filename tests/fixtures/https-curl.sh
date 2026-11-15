#!/bin/sh
# Mock transport only: no network, real account or device files.
set -eu
[ "$#" = 2 ] && [ "$1" = --config ]
conf=$2
grep -q '^proto = "=https"$' "$conf"
grep -q '^cacert = ' "$conf"
grep -q '^max-time = ' "$conf"
body=$(sed -n 's/^data-binary = "@\(.*\)"$/\1/p' "$conf")
[ -s "$body" ]
mode=${FL_HTTPS_TEST_MODE:-ok}
operation=$(sed -n 's|^url = "https://test.workers.dev/v1/pair/\([a-z]*\)"$|\1|p' "$conf")
status=200
case "$operation" in
create)
  grep -q 'device_secret' "$body"
  case "$mode" in offline) exit 6;; slow) sleep 20;; esac
  url='https://test.workers.dev/pair/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
  [ "$mode" != unsafe ] || url='https://evil.test/pair/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
  printf '{"pairing_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","verification_uri":"%s","display_code":"123456"}' "$url";;
status)
  case "$mode" in expired) status=404;printf '{"error":"expired"}';; denied) printf '{"status":"error","error":"shared_folder_denied"}';; pending) printf '{"status":"pending"}';; rate) status=429;printf '{"error":"slow_down"}';; *) printf '{"status":"ready"}';; esac;;
claim)
  client=mock.apps.googleusercontent.com
  [ "$mode" != wrongclient ] || client=evil.apps.googleusercontent.com
  token_url=https://test.workers.dev/v1/token
  [ "$mode" != wrongurl ] || token_url=https://evil.test/token
  printf '{"access_token":"MOCK_ACCESS","refresh_token":"MOCK_REFRESH","client_id":"%s","root_folder_id":"mock_root","token_url":"%s","expires_in":3600,"token_type":"Bearer"}' "$client" "$token_url";;
ack|cancel) [ "$mode" != ackfail ] || exit 7;printf '{"status":"closed"}';;
*) exit 9;;
esac
printf '\nMFL_STATUS:%s\n' "$status"
