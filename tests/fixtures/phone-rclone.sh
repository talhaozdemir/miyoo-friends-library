#!/bin/sh
# Fake worker. Never connects to a network or reads a real account.
set -eu
action=$1
shift
cfg=
root=mock_root
while [ "$#" -gt 0 ]; do
    case "$1" in
        --config) cfg=$2; shift 2;;
        --drive-root-folder-id) root=$2; shift 2;;
        *) shift;;
    esac
done
[ -n "$cfg" ]
mode=${FL_PHONE_TEST_MODE:-ok}
if [ "$action" = config ]; then
    grep -q '^scope = drive.readonly$' "$cfg"
    grep -q '^config_auth_no_browser = true$' "$cfg"
    grep -q 'prompt=consent' "$cfg"
    case "$mode" in
        denied) echo 'invalid_grant DO_NOT_SHOW_TEST_TOKEN' >&2; exit 1;;
        unsafe) printf 'http://127.0.0.1:53682/auth?state=abcdefghijklmnop&evil=1\n' >&2; sleep 20; exit 1;;
    esac
    printf 'Please go to: http://127.0.0.1:53682/auth?state=abcdefghijklmnopqrstuv\n' >&2
    case "$mode" in wait) sleep 20;; *) sleep 0.1;; esac
    if [ "$mode" = missing ]; then
        printf 'token = {"access_token":"TEST_ONLY"}\n' >> "$cfg"
    else
        printf 'token = {"access_token":"TEST_ONLY","refresh_token":"TEST_ONLY_REFRESH","expiry":"2099-01-01T00:00:00Z"}\n' >> "$cfg"
    fi
    if [ "$mode" = scope ]; then sed -i 's/scope = drive.readonly/scope = drive/' "$cfg"; fi
elif [ "$action" = cat ]; then
    if [ "$mode" = permission ]; then echo '403 permission denied DO_NOT_SHOW_TEST_TOKEN' >&2; exit 1; fi
    if [ "$mode" = offline ]; then echo 'dial tcp: network is unreachable' >&2; exit 1; fi
    if [ "$mode" = wrongroot ]; then root=other_root; fi
    printf '{"id":"%s","mimeType":"application/vnd.google-apps.folder"}\n' "$root"
else exit 9
fi
