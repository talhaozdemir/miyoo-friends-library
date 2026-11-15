#!/bin/sh
# OnionOS owns ROM lists, emulators, saves, BIOS and game launching.
APP_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd) || exit 1
umask 077
export HOME=/mnt/SDCARD
export SSL_CERT_FILE="$APP_DIR/certs/cacert.pem"
export RCLONE_CA_CERT="$SSL_CERT_FILE"
export LD_LIBRARY_PATH="$APP_DIR/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$APP_DIR" || exit 1
# Retire only app-owned files from the old SSH login package. Account data lives
# separately in .config/friends-library and is never touched by this migration.
if [ ! -L "$APP_DIR/docs" ] && [ -f "$APP_DIR/docs/PHONE_SETUP.html" ] && [ ! -L "$APP_DIR/docs/PHONE_SETUP.html" ]; then
    rm -f "$APP_DIR/docs/PHONE_SETUP.html"
fi
if [ ! -L "$APP_DIR/config" ] && [ -f "$APP_DIR/config/phone-setup.json" ] && [ ! -L "$APP_DIR/config/phone-setup.json" ]; then
    rm -f "$APP_DIR/config/phone-setup.json"
fi
exec "$APP_DIR/friends-library" --sd-root /mnt/SDCARD --app-dir "$APP_DIR"
