#!/bin/bash
set -euo pipefail
umask 077
base="$(cd -- "$(dirname -- "$0")" && pwd)"
work_config=""
cleanup() { if [ -n "$work_config" ] && [ -f "$work_config" ]; then rm -f -- "$work_config"; fi; }
trap cleanup EXIT
fail() { printf '\n%s\n' "$1" >&2; exit 1; }
[ "$(uname -s)" = Darwin ] || fail 'Use the Windows launcher on Windows.'
info="$base/setup-info.json"
[ -f "$info" ] || fail 'Extract the entire ZIP before starting setup.'
get_value() { /usr/bin/plutil -extract "$1" raw -o - "$info"; }
[ "$(get_value client_type)" = desktop_public ] || fail 'Invalid setup information. Ask the sender for a fresh ZIP.'
folder_id="$(get_value root_folder_id)"
client_id="$(get_value client_id)"
client_secret="$(get_value client_secret)"
[[ "$folder_id" =~ ^[A-Za-z0-9_-]{1,159}$ ]] || fail 'Invalid shared folder ID.'
[[ "$client_id" = *.apps.googleusercontent.com ]] && [ -n "$client_secret" ] || fail 'Invalid Desktop client definition.'
arch="$(uname -m)"
case "$arch" in
  arm64) arch=arm64; zip_hash=e72dc90b70f4947a7b72b61bde4a4fbba5777b2874fb448951ffdde67ebd0ab7 ;;
  x86_64) arch=amd64; zip_hash=4f5835733d52b818340f97b1fc0348a70461534d123363a183ca2b660219595a ;;
  *) fail 'Unsupported Mac CPU. Use an Intel or Apple Silicon Mac.' ;;
esac
tools="$base/tools/macos-$arch"
mkdir -p -- "$tools"
archive="$tools/rclone.zip"
if [ ! -f "$archive" ] || [ "$(/usr/bin/shasum -a 256 "$archive" | /usr/bin/awk '{print $1}')" != "$zip_hash" ]; then
  printf 'Downloading official rclone 1.76.0 for this Mac...\n'
  /usr/bin/curl --fail --location --retry 2 --connect-timeout 15 --max-time 300 \
    "https://downloads.rclone.org/v1.76.0/rclone-v1.76.0-osx-$arch.zip" -o "$archive.download" || fail 'Download failed. Check the internet connection and retry.'
  [ "$(/usr/bin/shasum -a 256 "$archive.download" | /usr/bin/awk '{print $1}')" = "$zip_hash" ] || fail 'Download integrity check failed. Nothing was executed.'
  mv -- "$archive.download" "$archive"
fi
printf '%s  %s\n' "$zip_hash" "$archive" | /usr/bin/shasum -a 256 -c - >/dev/null || fail 'Invalid rclone archive.'
/usr/bin/unzip -oq "$archive" -d "$tools"
rclone="$tools/rclone-v1.76.0-osx-$arch/rclone"
chmod u+x "$rclone"
printf '\nSelect the SD card ROOT, containing the App folder.\n'
sd="$(/usr/bin/osascript -e 'POSIX path of (choose folder with prompt "Select Miyoo SD card root (contains App folder)" default location (POSIX file "/Volumes/"))')" || fail 'SD selection cancelled.'
sd="${sd%/}"
[ "$sd" != / ] && [ -f "$sd/App/FriendsLibrary/config.json" ] || fail 'Select the SD card root after copying App to it.'
data_dir="$sd/.config/friends-library"
[ ! -L "$sd/.config" ] || fail 'Linked configuration directory is not supported.'
[ ! -L "$data_dir" ] || fail 'Linked configuration directory is not supported.'
mkdir -p -- "$data_dir"
credential="$data_dir/rclone.conf"
if [ -f "$credential" ]; then
  printf 'Account already exists. Reconnect? Type YES, or Enter to keep it: '
  IFS= read -r answer
  [ "$answer" = YES ] || { printf 'Existing account preserved.\n'; exit 0; }
fi
work_config="$(mktemp "$data_dir/.setup-XXXXXXXX")"
printf '\nOpening Google sign-in. Choose YOUR invited account.\nWait for Success in the browser, then return here.\n'
if ! "$rclone" config create friends drive client_id "$client_id" client_secret "$client_secret" \
    scope drive.readonly root_folder_id "$folder_id" config_is_local true config_change_team_drive false \
    --config "$work_config" --no-obscure --no-output --log-level ERROR >/dev/null 2>&1; then
  fail 'Google connection failed. Check your account, app access and internet connection; see the PDF.'
fi
/usr/bin/grep -Eq '^scope[[:space:]]*=[[:space:]]*drive\.readonly[[:space:]]*$' "$work_config" && \
  /usr/bin/grep -Eq '"refresh_token"[[:space:]]*:[[:space:]]*"[^"[:space:]]+"' "$work_config" || fail 'Google did not provide offline authorization. Reconnect and approve access.'
settings_temp="$(mktemp "$data_dir/.settings-XXXXXXXX")"
printf '{"root_folder_id":"%s","remote":"friends","cache_ttl_seconds":900,"timeout_seconds":30}\n' "$folder_id" > "$settings_temp"
mv -f -- "$work_config" "$credential"
work_config=""
mv -f -- "$settings_temp" "$data_dir/settings.json"
printf '\nREADY. Eject the SD card safely. Apps > Friends Library on your Miyoo.\n'
