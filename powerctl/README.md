# powerctl

Battery charging and idle power rules for the M2 Air (Asahi Fedora).

```sh
./install.sh                 # idempotent; re-run after editing anything here
powerctl cap 80              # rules start inactive — turn them on explicitly
powerctl list
./uninstall.sh               # restores 100/100 first; --purge also drops config
```

Design notes, hardware findings, and open questions live in
[`../docs/powerctl.md`](../docs/powerctl.md). Read that before changing the
health policy or the swayidle handover — both have non-obvious constraints.

## Layout

| Path | |
|---|---|
| `bin/powerctl` | the entire CLI |
| `bin/powerctl-idle-action` | swayidle handler; no-ops unless the timeout matches the live power source |
| `etc/rules.conf` | charging rules → `/etc/powerctl/` (root) |
| `config/idle.conf` | idle rules → `~/.config/powerctl/` (user) |
| `systemd/system/` | apply-on-boot, reapply-on-resume, weekly health refresh |
| `systemd/user/swayidle.service` | swayidle, moved out of `~/.config/dwl/startup.sh` |
| `polkit/` | lets wheel toggle the units without a password |

Root-owned files are **copied**, not symlinked into `~/.dotfiles` — a
user-writable path backing a polkit decision is bad practice. Only session files
are symlinked, so edits there are live.

## The one manual step

`install.sh` will not touch `~/.config/dwl/startup.sh`. If it still launches
swayidle directly, the installer prints the offending line and asks you to
comment it out — two swayidle instances would otherwise race over the same
timeouts. That file is what gets you a session and isn't tracked here, so a bad
automated edit is expensive.
