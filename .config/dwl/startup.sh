#!/bin/sh
# dwl startup — launched via: dwl -s ~/.config/dwl/startup.sh
# Tell portals/apps which desktop this is
export XDG_CURRENT_DESKTOP=wlroots

# Push the session env into the systemd user manager and dbus. dwl itself never
# does this, so anything *they* launch (portals, user units, dbus-activated
# services) starts with an empty environment -- no WAYLAND_DISPLAY. A Qt GUI app
# in that state falls back to the xcb platform plugin, finds no DISPLAY either,
# and qFatal()s in QGuiApplication's constructor. That is what crash-looped
# drkonqi on 2026-07-25: its own SIGABRT dumped a core, which instantiated a
# fresh drkonqi-coredump-processor@, which relaunched it -- 1814 cores in 83 min.
command -v systemctl >/dev/null && \
	systemctl --user import-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE
command -v dbus-update-activation-environment >/dev/null && \
	dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE

# terminal server. Super+Shift+Return spawns `footclient`, which opens a window
# on this process rather than starting a terminal from scratch -- one font-set
# load and one config parse for the whole session. Started here rather than via
# the shipped foot-server.socket so it inherits dwl's full environment; the unit
# is left disabled to avoid two servers racing for %t/foot.sock.
# Caveat of server mode: if this process dies, every terminal dies with it.
command -v foot >/dev/null && foot --server &

# wallpaper (solid color fallback if swaybg missing)
command -v swaybg >/dev/null && swaybg -c '#222222' &

# status bar
command -v waybar >/dev/null && waybar &

# tray applets (NetworkManager wifi menu + Bluetooth) — same backends KDE used
command -v nm-applet >/dev/null && nm-applet --indicator &
command -v blueman-applet >/dev/null && blueman-applet &

# notifications
command -v mako >/dev/null && mako &

# input method for Chinese + Japanese (ibus). The alphabetic layouts
# (us/es/pt/fr/ru, Alt+Shift) are pure xkb in dwl's config.h and need nothing
# here -- ibus is only for CJK, which needs a candidate popup.
#
# NOTE: this dwl build has no input-method-v2/text-input-v3 support (the
# upstream branch is from 2022 and does not port to wlroots 0.20), AND it is
# built without XWayland (XWAYLAND is empty in ~/git/dwl/config.mk). So ibus can
# only reach apps through the GTK/Qt IM modules: CJK works in GTK and Qt apps,
# and nowhere else. There is no X fallback -- do not bother with
# MOZ_ENABLE_WAYLAND=0 for Firefox, there is no XWayland to fall back to.
#
# Do NOT add -x/--xim: that starts the XIM server, which needs an X display that
# cannot exist here. It drove ibus-ui-gtk3 into a segfault loop in
# binding_common_get_xdisplay (5 coredumps per login) until one instance
# happened to survive. -R is kept deliberately: it respawns the panel, which
# provides the CJK candidate window, if it dies.
# Toggle CJK with Super+Space; configure engines with `ibus-setup`.
command -v ibus-daemon >/dev/null && ibus-daemon -drR --desktop=dwl &

# night light: NOT auto-started; toggle it with Alt+n (Alt+Shift+n warmer,
# Alt+Ctrl+n cooler). `dwl-nightlight` auto-picks the backend by renderer:
#   - "dwl (Vulkan)" session -> TRUE gammastep gamma (real redshift, blacks stay
#     black); the Vulkan renderer supports output color transforms.
#   - plain "dwl" (GLES2) session -> amber overlay fallback (GLES2 drops gamma).
# This same startup.sh serves both sessions. See ~/README.md section 4.

# Two-stage idle:
#   3 min  -> lock + screen off (any key/mouse press re-lights it)
#  10 min  -> system suspend (s2idle) via dwl-idle-suspend, which honors the
#             no-suspend flag from `dwl-suspend-toggle`. Waking from suspend
#             needs the power button or lid: the dockchannel-hid driver has no
#             wakeup support on this Asahi kernel, so keys can't wake s2idle.
# The lock is hung off before-sleep as well, so lid-close and manual
# `systemctl suspend` also lock; `-w` makes swayidle hold logind's sleep
# inhibitor until dwl-lock confirms the lock is up. after-resume re-lights
# the panel after wake in case the waking keypress isn't seen as activity.
# Apps holding an idle inhibitor (dwl supports idle-inhibit-v1, e.g. video
# players) suppress the timeouts.
command -v swayidle >/dev/null && swayidle -w \
	timeout 180 "wlopm --off '*'; $HOME/.local/bin/dwl-lock" \
	resume "wlopm --on '*'" \
	timeout 600 "$HOME/.local/bin/dwl-idle-suspend" \
	before-sleep "$HOME/.local/bin/dwl-lock" \
	after-resume "wlopm --on '*'" &
