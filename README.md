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
| `.local/bin/` | custom scripts: `dwm-session`, `dwl-lock`, `macos-key.sh`, `obsidian-sync-cron.sh` |

## Manual steps files can't capture (new machine)

- **suckless** (`dwm`/`slstatus`/`slock`): drop each `config.h` into its source tree
  and `sudo make clean install` — the binaries aren't tracked here.
- **T2 Mac kernel/hardware, services, group memberships**: see `~/README.md`.
- **tmux plugins**: `git clone https://github.com/tmux-plugins/tpm ~/.config/tmux/plugins/tpm`
  then `prefix + I`.
- **autorandr** profile is EDID-specific; re-run `autorandr --save docked` if displays differ.
