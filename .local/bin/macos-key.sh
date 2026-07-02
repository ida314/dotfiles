#!/bin/bash
# macOS-style key translation for Hyprland
# Uses hyprctl sendshortcut to inject keys directly into the active window,
# avoiding modifier conflicts with wtype.
#
# Terminals need Ctrl+Shift+C/V for copy/paste (Ctrl+C = SIGINT)
# Regular apps use Ctrl+C/V/X
#
# Usage: macos-key.sh copy|paste|cut

action="$1"
class=$(hyprctl activewindow -j | jq -r '.class')

is_terminal() {
    case "$class" in
        com.mitchellh.ghostty|kitty|Alacritty|foot|org.wezfurlong.wezterm)
            return 0 ;;
        *)
            return 1 ;;
    esac
}

case "$action" in
    copy)
        if is_terminal; then
            hyprctl dispatch sendshortcut "CTRL SHIFT, C, activewindow"
        else
            hyprctl dispatch sendshortcut "CTRL, C, activewindow"
        fi
        ;;
    paste)
        if is_terminal; then
            hyprctl dispatch sendshortcut "CTRL SHIFT, V, activewindow"
        else
            hyprctl dispatch sendshortcut "CTRL, V, activewindow"
        fi
        ;;
    cut)
        if is_terminal; then
            hyprctl dispatch sendshortcut "CTRL SHIFT, X, activewindow"
        else
            hyprctl dispatch sendshortcut "CTRL, X, activewindow"
        fi
        ;;
    *)
        echo "Usage: $0 copy|paste|cut" >&2
        exit 1
        ;;
esac
