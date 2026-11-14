#!/bin/sh
# Folder-ID routing simulates separate private Drive listings. Never network.
case "$1:$2" in
  lsjson:friends,root_folder_id=console_test:)
    if [ "$FL_FAV_MODE" = nofolder ]; then
      printf '%s\n' '[{"ID":"root_game","Name":"Root.nes","IsDir":false,"Size":3}]'
    else
      printf '%s\n' '[{"ID":"root_game","Name":"Root.nes","IsDir":false,"Size":3},{"ID":"favorites_test","Name":"Favorites","IsDir":true,"Size":-1}]'
    fi ;;
  lsjson:friends,root_folder_id=favorites_test:)
    case "$FL_FAV_MODE" in
      denied) printf '%s\n' '403 forbidden' >&2; exit 1 ;;
      offline) printf '%s\n' 'dial tcp: no such host' >&2; exit 1 ;;
      empty) printf '%s\n' '[]' ;;
      slow) sleep 20 ;;
      *) printf '%s\n' '[{"ID":"fav_game","Name":"Favorite.nes","IsDir":false,"Size":3}]' ;;
    esac ;;
  *) printf '%s\n' 'Unexpected root or command' >&2; exit 2 ;;
esac
