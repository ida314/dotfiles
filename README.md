# dotfiles

Personal dotfiles. Files are stored at their `$HOME`-relative paths.
Managed as a normal repo (copy in/out); layout follows the bare-repo pattern:
<https://www.atlassian.com/git/tutorials/dotfiles>

## Layout

| Path | What |
|---|---|
| `.zshrc` | zsh: powerlevel10k + zinit + zoxide |
| `.vimrc` | active vim config (minimal: markdown-preview via vim-plug) |
| `.vimrc.old` | cold archive — previous vim-lsp + NERDTree + markdown config |
| `.config/tmux/tmux.conf` | tmux: C-Space prefix, vi copy, TPM + resurrect + continuum |
| `.config/nvim/` | neovim (packer) |
| `.config/alacritty/`, `htop/`, `weechat/`, `neofetch/` | app configs |
| `.xinitrc` | X session: feh + dunst + picom + slstatus + redshift, `exec dwm` |
| `.config/redshift.conf` | night-light temps |
| `.config/autorandr/docked/` | external-monitor profile (EDID-specific to current displays) |
| `dwm/config.h`, `slstatus/config.h`, `slock/config.h` | suckless build configs (source-compiled) |
| `powerctl/` | battery charging + idle rules; `cd powerctl && make install` |
| `docs/` | design notes — see `docs/powerctl.md` |
| `.local/bin/` | custom scripts — run **`scripts`** to list them (see below; don't hand-maintain) |
| `etc/` | files that live under `/etc` — `keyd/`, `ly/` (ly assets + `verify_scaler.py`) |

## Manual steps files can't capture (new machine)

- **suckless** (`dwm`/`slstatus`/`slock`): drop each `config.h` into its source tree
  and `sudo make clean install` — the binaries aren't tracked here.
- **powerctl**: `cd powerctl && make install` (as your normal user, not with sudo).
  Charging rules install inactive; enable with `powerctl cap 80`. powerctl owns
  swayidle via a user unit; the old invocation in `~/.config/dwl/startup.sh` is
  commented out and must stay that way — two instances race over the same
  timeouts. See `docs/powerctl.md`.
- **T2 Mac kernel/hardware, services, group memberships**: see `~/README.md`.
- **tmux plugins**: `git clone https://github.com/tmux-plugins/tpm ~/.config/tmux/plugins/tpm`
  then `prefix + I`.
- **autorandr** profile is EDID-specific; re-run `autorandr --save docked` if displays differ.

## Scripts (`.local/bin/`)

Generated — regenerate with `scripts --readme`. Each script's summary is read
from its own header, so this table can't drift out of sync the way a hand-kept
list does. Every script is a symlink from `~/.local/bin`, so tab-completion
finds them: type a few letters and press Tab.

| Script | What |
|---|---|
| `alarm` | Alarm clock: a fuzzel prompt in, a silent mako notification out. |
| `ansi2ly` | ansi2ly - turn ANSI art (.ANS) into a .dur background sized for ly. |
| `dwl-brightness` | Screen + keyboard backlight control for dwl with a lightweight mako |
| `dwl-clock-hide` | Hide/show the clock in both places at once: the waybar clock and gtklock. |
| `dwl-idle-suspend` | Called by swayidle at the idle-suspend timeout (see ~/.config/dwl/startup.sh). |
| `dwl-lock` | Lock screen replacement for dwl (replaces omarchy-system-lock / hyprlock) |
| `dwl-nightlight` | Night-light / "redshift" toggle for dwl, with a mako notification. |
| `dwl-record` | Screen recorder for dwl: a fuzzel menu to start, a red dot in the bar to stop. |
| `dwl-screenshot` | Screenshot tool for dwl with a lightweight mako notification. |
| `dwl-suspend-status` | waybar custom module: show whether idle auto-suspend is enabled |
| `dwl-suspend-toggle` | Toggle idle auto-suspend on/off. Writes the flag file read by |
| `dwl-volume` | Volume control for dwl with a lightweight mako notification. |
| `dwm-session` | — |
| `macos-key.sh` | macOS-style key translation for Hyprland |
| `noise` | Pink/brown/white noise for focus and masking, synthesised locally by ffmpeg. |
| `obsidian-sync-cron.sh` | — |
| `powerprofile-cycle` | waybar on-click: cycle power-saver -> balanced -> performance -> power-saver |
| `powerprofile-status` | waybar custom module: show current power profile (tuned-ppd via net.hadess.PowerProfiles) |
| `scripts` | List the custom scripts in ~/.local/bin with a one-line summary of each. |
| `tmux-autosave` | Save the tmux-resurrect state whenever panes, windows or sessions change. |
