#!/bin/bash
function check_game_data () {
  if [[ "$1" == "" ]]; then
    zenity --error --ok-label "Quit" --width=400 --title "Could not find Quake game data" \
    --text "Please copy the Quake game data files (at least <tt>pak0.pak</tt>) to <tt><b>$XDG_DATA_HOME/id1/</b></tt>."
    exit 1
  fi
}

function check_exit_code () {
  if [[ "$1" != "0" ]]; then
    zenity --error --ok-label "Quit" --width=400 --title "fteqw exited with an error" \
        --text "For a detailed error message, please run fteqw from a terminal window using\n <tt><b>flatpak run $FLATPAK_ID</b></tt>."
    exit 1
  fi
}

/app/bin/fteqw -basedir $XDG_DATA_HOME "$@"
check_exit_code $?
