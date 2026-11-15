#!/bin/sh
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
temp=$(mktemp -d /tmp/fl-launcher-XXXXXX)
trap 'rm -rf "$temp"' EXIT HUP INT TERM
app="$temp/App/FriendsLibrary"
mkdir -p "$app/docs" "$app/config" "$temp/private"
cp "$root/package/App/FriendsLibrary/launch.sh" "$app/launch.sh"
printf '#!/bin/sh\nexit 0\n' > "$app/friends-library"
chmod +x "$app/friends-library"
printf 'old guide' > "$app/docs/PHONE_SETUP.html"
printf 'old public profile' > "$app/config/phone-setup.json"
printf 'keep guide' > "$app/docs/HTTPS_SETUP.html"
printf 'keep profile' > "$app/config/https-setup.json"
printf 'keep account' > "$temp/private/rclone.conf"
sh "$app/launch.sh"
test ! -e "$app/docs/PHONE_SETUP.html"
test ! -e "$app/config/phone-setup.json"
test "$(cat "$app/docs/HTTPS_SETUP.html")" = 'keep guide'
test "$(cat "$app/config/https-setup.json")" = 'keep profile'
test "$(cat "$temp/private/rclone.conf")" = 'keep account'
# Never traverse a symlinked parent or remove a symlinked legacy file.
mv "$app/docs" "$temp/external-docs"
printf 'external guide' > "$temp/external-docs/PHONE_SETUP.html"
ln -s "$temp/external-docs" "$app/docs"
ln -s "$temp/private/rclone.conf" "$app/config/phone-setup.json"
sh "$app/launch.sh"
test "$(cat "$temp/external-docs/PHONE_SETUP.html")" = 'external guide'
test -L "$app/config/phone-setup.json"
test "$(cat "$temp/private/rclone.conf")" = 'keep account'
echo 'Launcher migration passed: obsolete app files only; account, QR and symlink targets preserved.'
