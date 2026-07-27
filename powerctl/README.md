# powerctl

Battery charging and idle power rules for the M2 Air (Asahi Fedora).

```sh
make install                 # as your normal user, NOT sudo. Idempotent.
powerctl cap 80              # rules install inactive — turn them on explicitly
powerctl list                # check it took
make uninstall               # restores 100/100 first;  make purge  drops config too
```

## Usage

### Charging

```sh
powerctl cap 80              # stop at 80%, resume charging below 75%
powerctl health              # let SoH pick the target instead
powerctl health --explain    # show the inputs and the arithmetic
powerctl off                 # no rule; charge to 100% as normal
powerctl list                # rules + live sysfs values + drift warnings
```

`cap N` sets the resume point to `N-5` automatically — equal start and stop
would make the SMC nibble at the boundary. Edit `/etc/powerctl/rules.conf` if
you want a different spread.

**Two switches, not one.** `cap`/`health`/`off` choose *which rule*;
`powerctl toggle` (the waybar click) pauses and resumes *enforcement* without
forgetting the rule. So `off` clears it, `toggle` suspends it.

### Idle timeouts

```sh
powerctl idle                        # current timeouts, and which source is live
powerctl idle set bat screen 3m      # screen off after 3 min on battery
powerctl idle set ac sleep never     # never auto-suspend while plugged in
```

Takes `45s`, `5m`, `1h`, or `never`. Changes apply immediately — swayidle is
restarted, but only on a config change, never on plug/unplug.

`dwl-suspend-toggle` still works and still wins: it is a *temporary* "not right
now" override, distinct from configuring `never`.

### Where things live

| | |
|---|---|
| charging rules | `/etc/powerctl/rules.conf` (root) |
| idle rules | `~/.config/powerctl/idle.conf` (yours, no sudo) |
| what the bar shows | `powerctl status --waybar` |

Only `cap`, `health`, `off` and `apply` need root; they re-exec under sudo
themselves. Everything else runs unprivileged so waybar never sees a prompt.

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

`make install` will not touch `~/.config/dwl/startup.sh`. If it still launches
swayidle directly, the installer prints the offending line and asks you to
comment it out — two swayidle instances would otherwise race over the same
timeouts. That file is what gets you a session and isn't tracked here, so a bad
automated edit is expensive.
