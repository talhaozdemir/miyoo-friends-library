#!/bin/sh
# OnionOS owns ROM lists, emulators, saves, BIOS and game launching.
APP_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd) || exit 1
umask 077
export HOME=/mnt/SDCARD
export SSL_CERT_FILE="$APP_DIR/certs/cacert.pem"
export RCLONE_CA_CERT="$SSL_CERT_FILE"
export LD_LIBRARY_PATH="$APP_DIR/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$APP_DIR" || exit 1
exec "$APP_DIR/friends-library" --sd-root /mnt/SDCARD --app-dir "$APP_DIR"
