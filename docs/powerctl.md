# powerctl — power rules

**Status:** implemented, not installed. See "Install / verify".

A small CLI for power rules on the M2 Air (Mac14,2, Asahi Fedora):

- **cap** — settable max charge
- **health** — derive a target percentage from the battery's measured state and
  hold there
- **idle** — screen-off and system-sleep timeouts, set per power source
- `powerctl list` — view rules and which one is in effect

## Shape: standalone project, not a single script

Line count is not what decides this — **install topology** is. The pieces land
in four destinations across two privilege domains:

| | |
|---|---|
| `/etc/powerctl/`, `/etc/systemd/system/`, `/etc/polkit-1/rules.d/` | root |
| `/usr/local/bin/` | root, must be root-owned to be exec'd by a system unit |
| `~/.config/powerctl/`, `~/.config/systemd/user/` | user session |

Installing also requires `daemon-reload`, unit enablement, and — critically — an
**uninstall path**, because this replaces a working swayidle invocation in
`~/.config/dwl/startup.sh`. Anything that edits a file you depend on to get a
session needs a way back. That is a project with an installer, not a script.

It stays *close* to one script: all the logic is in `bin/powerctl`; everything
else is declarative units and config. Repo layout follows the `dwm`/`slock`
precedent — its own top-level dir with a manual install step.

### Root-owned files are copied, not symlinked

`/etc/keyd/default.conf` is a symlink into `~/.dotfiles`, but polkit rules and
systemd units **must not be** — a user-writable path backing a privilege
decision is bad practice, and SELinux is liable to object. The installer copies
those and symlinks only the user-session files.

## What the hardware actually gives us

`macsmc-battery` exposes real charge thresholds, enforced by SMC firmware — so
**no daemon and no polling loop**, unlike TLP-style setups that babysit capacity
from userspace:

```
/sys/class/power_supply/macsmc-battery/charge_control_start_threshold
/sys/class/power_supply/macsmc-battery/charge_control_end_threshold
/sys/class/power_supply/macsmc-battery/charge_behaviour   # auto | inhibit-charge | force-discharge
```

Readable state for the health rule (values sampled 2026-07-26):

| Attribute | Value | Use |
|---|---|---|
| `charge_full` / `charge_full_design` | 3885000 / 4563000 | **SoH 85.1%** |
| `cycle_count` | 359 | wear proxy |
| `manufacture_{year,month,day}` | 2023-06-13 | calendar age ~3.1y |
| `temp` | 260 (26.0 °C) | charge-temperature guard |
| `health` | `Good` | driver's own coarse verdict |

### Two constraints that shape the design

**1. Battery chemistry is NOT readable.** There is no `technology` attribute in
this driver, and UPower reports `Technology = 0` (unknown). `model_name`
(`bq40z651`) is the TI fuel-gauge IC, not the cell. So the "read battery type
from the computer" half of the health rule **cannot be satisfied as written**.
Specced instead as a stated constant — `CHEMISTRY=li-ion`, correct for every
Apple laptop — so the assumption is visible rather than buried.

**2. UPower is a competing writer.** UPower 1.91.2 has its own charge-limit
feature:

```
EnableChargeThreshold(b)             method, polkit: org.freedesktop.UPower.enable-charging-limit
ChargeStartThreshold  = 75           readonly
ChargeEndThreshold    = 80           readonly
ChargeThresholdEnabled = false       currently off
```

Its thresholds are **readonly and hardcoded at 75/80**, so it can't back a
settable cap. But if it is ever enabled it will fight us over the same sysfs
files. `powerctl` must call `EnableChargeThreshold(false)` before applying and
refuse to run if it can't.

> **Worth knowing:** if 75/80 were the only setting you ever wanted, UPower
> alone would do the whole job — `busctl call ... EnableChargeThreshold b true`,
> no new code at all. The entire tool below exists to buy settable thresholds
> and the health rule.

## Design

One applier, one config file, one CLI.

Both charging rules drive the same two sysfs knobs, so **exactly one is in
effect at a time** — that keeps the state model honest and `list` unambiguous.

**Config is split by privilege domain, not lumped into one file.** Charging
needs root; idle is pure session policy and needs none. Keeping them together
would have forced every `idle set` through sudo for no reason:

```sh
# /etc/powerctl/rules.conf                    (root)
ACTIVE=none             # cap | health | none
CAP_START=75            # charging resumes below this
CAP_STOP=80             # charging stops here
HEALTH_FLOOR=60         # health rule never targets below this
HEALTH_CEILING=90       # ...nor above this
CHEMISTRY=li-ion        # not readable from hardware; see above

# ~/.config/powerctl/idle.conf                (user)
IDLE_AC_SCREEN=600
IDLE_AC_SLEEP=0
IDLE_BAT_SCREEN=180     # the values previously hardcoded in startup.sh
IDLE_BAT_SLEEP=600
```

Both are shell-sourceable, so the applier stays trivial. Charging and idle rules
are independent — `ACTIVE` selects between `cap` and `health` only; idle rules
apply regardless.

`HEALTH_CEILING` is 90, not 85: at 85 it would clamp away the `SoH < 70% -> 90`
branch of the policy table below, silently disabling the rule's own worn-battery
case.

`systemctl enable/disable powerctl.service` remains the boot default, and the
unit's active state remains the on/off state — no flag file to drift out of
sync (contrast `dwl-suspend-toggle`, which needs one because swayidle has no
equivalent handle).

### The health rule is a policy heuristic, not a computation

Nothing in the hardware yields an "optimal percentage" — the battery reports its
condition, not what to do about it. Any number comes from published Li-ion
degradation behaviour, so the rule must be **transparent and tunable**, and
`powerctl health --explain` should print its inputs and arithmetic.

Proposed policy, from two well-established effects — calendar aging rises with
resting state-of-charge, and usable capacity is what you actually feel:

```
SoH = charge_full / charge_full_design

SoH >= 90%   ->  target 75    healthy: spend margin on longevity
SoH >= 80%   ->  target 80    the usual compromise
SoH >= 70%   ->  target 85    degraded: capping now costs more than it saves
SoH <  70%   ->  target 90    worn: prioritise usable runtime
```

Clamped to `[HEALTH_FLOOR, HEALTH_CEILING]`; `start = target - 5` for
hysteresis. At today's SoH 85.1% this yields **target 80, start 75**.

The inversion at low SoH is the deliberate part: once a pack has lost real
capacity, further restricting the top slice hurts utility more than the marginal
aging benefit is worth. Disagreeing with that curve is fine — it is four lines
in one function, which is the point of specifying it this way.

Recompute on a **weekly timer**, not a daemon: SoH moves over months.

## Files

| Path | What |
|---|---|
Source of truth is `~/.dotfiles/powerctl/`; `make install` places them.

| Installed path | From | What |
|---|---|---|
| `/usr/local/bin/powerctl` | `bin/powerctl` | the whole CLI — all logic lives here |
| `/usr/local/bin/powerctl-idle-action` | `bin/powerctl-idle-action` | swayidle handler; gates on live power source |
| `/etc/powerctl/rules.conf` | `etc/rules.conf` | charging rules (root) |
| `/etc/systemd/system/powerctl.service` | `systemd/system/` | oneshot + `RemainAfterExit`; applies on start, restores 100 on stop |
| `/etc/systemd/system/powerctl-resume.service` | ″ | `try-restart` after suspend |
| `/etc/systemd/system/powerctl-refresh.{service,timer}` | ″ | weekly recompute for the health rule |
| `/etc/polkit-1/rules.d/49-powerctl.rules` | `polkit/` | wheel manages these units, no password |
| `~/.config/powerctl/idle.conf` | `config/idle.conf` | idle rules (user) |
| `~/.config/systemd/user/swayidle.service` | `systemd/user/` | symlinked; swayidle, moved out of `startup.sh` |
| `~/.local/state/powerctl/swayidle-cmd.sh` | *generated* | swayidle command line, not tracked |

There is no separate waybar script — the module calls the CLI directly for both
halves: `exec` is `powerctl status --waybar`, `on-click` is `powerctl toggle`.

**`toggle` pauses enforcement via systemd rather than rewriting `rules.conf`.**
That is a privilege decision, not a style one: the polkit rule makes
`systemctl start/stop powerctl.service` passwordless, whereas `powerctl cap`
needs sudo and would hang forever waiting for a password with no tty attached.
So the click pauses and resumes; *changing* which rule applies stays a
deliberate CLI action. `status --waybar` reports three states — no rule set,
configured-but-paused, and enforced.

The generated swayidle invocation is a **script, not an argument list**: swayidle
clauses nest quotes (`timeout 180 'powerctl-idle-action screen 180'`), and
round-tripping that through a systemd `ExecStart=` is a quoting minefield. A
generated `#!/bin/sh` file with a plain `exec` sidesteps it entirely.

Per repo convention, scripts live in `.dotfiles/.local/bin/` symlinked into
`~/.local/bin/`; `/etc` files stage under `.dotfiles/etc/`. POSIX sh + `awk`
throughout, matching `powerprofile-*` — no new dependencies.

### CLI surface

```
powerctl list                 # everything: rules, live sysfs values, drift warnings
powerctl status               # one line;  --waybar emits JSON for the bar
powerctl toggle               # pause/resume enforcement (the waybar on-click)

powerctl cap 80               # set + activate the cap rule          [sudo]
powerctl health               # activate the SoH-derived rule        [sudo]
powerctl health --explain     # show SoH, cycles, chosen target, why  (no sudo)
powerctl off                  # restore 100/100                      [sudo]
powerctl apply [--restore]    # re-apply current rule (used by unit/timer)

powerctl idle                 # effective screen/sleep timeouts per source
powerctl idle set bat screen 3m
powerctl idle set ac sleep never
powerctl idle apply           # regenerate swayidle command and restart it
```

Commands marked `[sudo]` re-exec themselves under sudo; everything else runs
unprivileged, which is what keeps waybar's polling and clicking prompt-free.

**`cap` sets `CAP_START` to `stop - 5` automatically.** Equal start and stop
would let the SMC nibble at the boundary; the 5-point gap is hysteresis. Edit
`/etc/powerctl/rules.conf` directly if you want a different spread.

`list` shows **both** the configured rule and the actual sysfs values — they
diverge when the driver clamps, and silent divergence is the main failure mode
here. It prints a `WARNING` line when they disagree.

### Two independent switches

Worth being explicit, because it is the one genuinely confusing part of the
model:

| | Set by | Means |
|---|---|---|
| **which rule** | `cap` / `health` / `off` | what the thresholds *would* be |
| **enforcement** | `toggle`, or `systemctl start/stop powerctl.service` | whether they are applied right now |

So `powerctl off` is not the same as `powerctl toggle`. `off` forgets the rule
(`ACTIVE=none`); `toggle` keeps it and stops applying it, restoring 100/100
until you toggle back. The bar shows a distinct "paused" tooltip for that state.

### The unit

```ini
[Unit]
Description=Battery charging rules
ConditionPathExists=/sys/class/power_supply/macsmc-battery/charge_control_end_threshold

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/local/lib/powerctl/apply
ExecStop=/usr/local/lib/powerctl/apply --restore

[Install]
WantedBy=multi-user.target
```

**Write ordering is deliberate** and belongs in `apply`: drivers that enforce
`START <= STOP` reject a transient inversion, so lower `START` first going down
and raise `STOP` first coming back up. Unverified whether *this* driver enforces
it; the ordering is correct either way.

### Resume + timer

`powerctl-resume.service` runs `systemctl try-restart powerctl.service` with
`After=`/`WantedBy=` on `suspend.target hibernate.target hybrid-sleep.target
suspend-then-hibernate.target`. `powerctl-refresh.timer` runs the same weekly.
`try-restart` no-ops when the rule is off, so the user's choice survives both.
Whether resume re-application is needed at all is **unverified** — keep it
unless testing shows the driver already persists across suspend.

### polkit

```js
polkit.addRule(function (action, subject) {
    if (action.id == "org.freedesktop.systemd1.manage-units" &&
        /^powerctl(-resume)?\.(service|timer)$/.test(action.lookup("unit")) &&
        /^(start|stop|restart|try-restart)$/.test(action.lookup("verb")) &&
        subject.isInGroup("wheel")) {
        return polkit.Result.YES;
    }
});
```

Editing `rules.conf` still needs root. Simplest is `powerctl` re-execing
itself under `sudo` for write subcommands (`cap`, `health`, `off`) while `list`
and `status` stay unprivileged — so waybar polling never triggers an auth
prompt.

### waybar

New `custom/chargecap` module before `battery`, mirroring `custom/suspend`:
`return-type: json`, `interval: "once"`, `signal: 9` (8 is taken by
`custom/suspend`), `on-click` cycling `cap -> health -> off`. The CLI sends
`pkill -RTMIN+9 -x waybar` after any change. Style: `#custom-chargecap` in the
shared padding rule, `.on` in green `#98c379`.

## Idle rules (screen off + system sleep)

**This half does not inherit the elegance of the charging half.** Charge
thresholds are firmware-enforced — write and walk away. Idle timeouts are
enforced by `swayidle`, a userspace daemon whose timeouts are **fixed at launch
and cannot be changed at runtime**. There is no sysfs equivalent. So this is a
config-generation problem, not a state-write problem.

### What exists today

`~/.config/dwl/startup.sh:69` launches swayidle with hardcoded values:

```sh
swayidle -w \
    timeout 180 "wlopm --off '*'; $HOME/.local/bin/dwl-lock" \
      resume "wlopm --on '*'" \
    timeout 600 "$HOME/.local/bin/dwl-idle-suspend" \
    before-sleep "$HOME/.local/bin/dwl-lock" \
    after-resume "wlopm --on '*'"
```

`dwl-idle-suspend` already gates on `~/.local/state/dwl/no-suspend`, toggled by
`dwl-suspend-toggle` — an existing, working on/off switch that **overlaps with a
sleep rule**. These must be reconciled, not stacked; see open questions.

Note `~/.config/dwl/startup.sh` is a **real file, not tracked in `~/.dotfiles`**.
Moving swayidle into a user unit therefore also brings it under version control.

### Mechanism: register every timeout, gate in the handler

The obvious approach — regenerate swayidle's args and restart it on every
AC plug/unplug — has a real wart: **restarting swayidle resets the idle
counter**, so plugging in silently gives you a fresh timer.

Avoid it. swayidle accepts multiple `timeout` clauses, so register the union of
all four configured values and let each handler decide whether it applies:

```sh
# powerctl-idle-action <screen|sleep> <the timeout that fired>
#   no-op unless this timeout is the one configured for the *current* power
#   source, read live from /sys/class/power_supply/macsmc-ac/online
```

Then AC transitions need **no restart at all** — the same running daemon does
the right thing on either power source, and the idle counter is never disturbed.
swayidle only restarts when the *config* changes, which is rare and explicit.

`0` means never: simply don't register that timeout.

### Wiring

Move the invocation out of `startup.sh` into a **user unit**, `swayidle.service`,
with `ExecStart` reading generated args from
`~/.local/state/powerctl/swayidle.args`. `powerctl idle set ...` rewrites
that file and runs `systemctl --user restart swayidle`. This gets a supervised
daemon, a restart handle, and no root anywhere in the idle path — it is all
per-user session state.

**The old invocation in `startup.sh` must be deleted in the same change**, or
two swayidle instances race over the same timeouts.

### CLI

```
powerctl idle                       # show effective timeouts + which source is live
powerctl idle set bat screen 3m
powerctl idle set ac sleep never
```

Accept `30s`/`5m`/`1h`/`never`, store seconds.

### Caveat specific to this machine

**Keyboard input cannot wake s2idle on this Asahi kernel** — waking needs the
lid or power button. A short `IDLE_BAT_SLEEP` is therefore more disruptive here
than on a typical laptop. Keep the sleep timeout comfortably longer than the
screen-off timeout, and treat `IDLE_BAT_SLEEP=0` as a legitimate everyday
setting rather than a degenerate one.

Also note `logind.conf` is entirely at defaults (`IdleAction=ignore`), so it is
not currently a competing writer — but `HandleLidSwitch` and `IdleAction` are a
third potential source of sleep policy if ever set.

## Worthwhile extensions

Roughly in order of value:

1. **`powerctl full` — one-shot charge to 100% for travel.** The obvious
   companion to a cap; without it you disable the rule by hand and forget to
   re-enable it. Should auto-revert on the next unplug or after N hours.
2. **Use `charge_behaviour` to *reach* the target.** Thresholds only prevent
   charging — sitting at 84% with an 80% cap, the pack stays at 84% until you
   drain it. `force-discharge` actively brings it down; `inhibit-charge` holds
   it. Makes the cap take effect immediately instead of eventually.
3. **Storage mode.** `powerctl storage` → force-discharge to ~60% and hold,
   for a laptop being shelved for weeks. Falls out nearly free from (2).
4. **SoH logging.** Weekly timer appends `date,SoH,cycle_count,temp` to a CSV.
   Costs nothing, and after a year the health rule can be driven by *this
   machine's* measured curve instead of a generic table — the single biggest
   upgrade available to the health rule.
5. **Drift detection.** `apply` compares intended vs actual sysfs after writing
   and warns on mismatch. Catches both driver clamping and UPower re-enabling
   itself behind our back.
6. **Charge-temperature guard.** `temp` is exposed; charging hot is a large
   aging factor. Lower the target when sustained temp is high.
7. **Recalibration reminder.** Gauge accuracy drifts when a battery never sees a
   full cycle — which is exactly what a permanent cap causes, so this rule
   degrades its own inputs over time. Note `ExpectBatteryRecalibration` in
   `/etc/UPower/UPower.conf`. Prompt for a full cycle every few months.

## Install / verify

```sh
cd ~/.dotfiles/powerctl
make check       # syntax-check scripts and units, change nothing
make install     # idempotent; re-run after edits
make uninstall   # restores 100/100 first;  make purge  also drops config
```

**Run as your normal user, not `sudo make install`** — the recipes escalate for
the system half themselves, and running the whole thing as root would leave
root-owned files under `~/.config`. The Makefile refuses if `id -u` is 0.

The installer leaves charging rules **inactive** (`ACTIVE=none`) — installing
should not silently change how your battery charges. Turn them on explicitly:

```sh
powerctl cap 80          # or: powerctl health
powerctl list
```

`make uninstall` restores 100/100 *before* removing anything, so a cap can't
outlive the tool that explains it.

### Verified so far

- `list`, `status`, `status --waybar`, `health --explain`, `idle`, `idle set`,
  `idle apply` all exercised against real hardware readings (SoH 86%, 359
  cycles) — read-only paths only.
- Generated swayidle command checked with `sh -n` and by dumping its actual
  argv; the union-of-timeouts dedupe works, and all-timeouts-`never` degrades to
  a valid file with just the sleep hooks.
- Handler gating tested with actions stubbed: on battery, the AC screen timeout
  no-ops and the battery one fires.
- Threshold write ordering brute-forced over all 441 valid transitions — zero
  invalid intermediate states.

### Not yet verified — needs an install

- Any **write** to sysfs. Whether the driver enforces `start <= stop`, whether
  it silently clamps, and whether SELinux permits the write from a system unit
  are all still open (`ausearch -m avc -ts recent`).
- Whether thresholds survive suspend — i.e. whether `powerctl-resume.service` is
  load-bearing or redundant.
- swayidle running under a user unit, and the `startup.sh` handover.

Check the driver took what it was given — `list` warns on drift, but confirm
directly at least once:

```sh
grep . /sys/class/power_supply/macsmc-battery/charge_control_{start,end}_threshold
busctl get-property org.freedesktop.UPower \
  /org/freedesktop/UPower/devices/battery_macsmc_battery \
  org.freedesktop.UPower.Device ChargeThresholdEnabled    # must stay false
```

Verify the driver accepted both thresholds — it may silently clamp rather than
error:

```sh
grep . /sys/class/power_supply/macsmc-battery/charge_control_{start,end}_threshold
busctl get-property org.freedesktop.UPower \
  /org/freedesktop/UPower/devices/battery_macsmc_battery \
  org.freedesktop.UPower.Device ChargeThresholdEnabled    # must stay false
```

Near or above the cap, `status` reads `Not charging` — that is the cap working,
not a fault. Watch for SELinux denials on the sysfs write
(`ausearch -m avc -ts recent`) — untested.

## Open questions

- **`CAP_STOP` semantics.** The original request was "only enable charging when
  I go under 80%", read literally as `START=80`. `STOP` was never specified —
  this spec assumes `STOP=80` (park at 80). `STOP=100` (charge to full, resume
  below 80) is an equally valid reading.
- **Is the health rule's curve right?** It is a defensible starting policy, not
  a derived optimum. Extension (4) is how it stops being a guess.
- **What happens to `dwl-suspend-toggle`?** It is a working on/off switch for
  idle suspend, backed by a flag file, and it directly overlaps
  `IDLE_*_SLEEP`. Three options: (a) `powerctl` subsumes it and the flag file
  and `dwl-suspend-{toggle,status}` are deleted; (b) it survives as a temporary
  override on top of the configured rule — "off until I say otherwise" is a
  genuinely different intent from "never sleep on battery", and arguably worth
  keeping; (c) leave it alone and accept two mechanisms. **(b) is the
  recommendation** — but it is a real decision, not a detail, and it determines
  whether `custom/suspend` stays in waybar alongside `custom/chargecap`.
- **Is `startup.sh` the right thing to disturb?** Moving swayidle into a user
  unit is the largest behavioural change in this spec and touches a file that
  currently works. It is worth it — supervision, a restart handle, and version
  control that `startup.sh` doesn't have — but it is the step most likely to
  break the session if it goes wrong. Do it on its own, verify, then build idle
  rules on top.
