#!/bin/sh

# restore wallpaper
feh --bg-fill ~/wallpaper.jpg 2>/dev/null &

# notifications
dunst &

# compositor (only if installed)
command -v picom >/dev/null 2>&1 && picom --daemon

# status bar
slstatus &
redshift &

exec dwm
