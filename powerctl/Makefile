# powerctl -- battery charging and idle power rules.
#
# Run as your NORMAL USER: `make install`. The system half escalates with sudo
# from inside the recipes; running `sudo make install` yourself would create
# root-owned files under your home and break the session half.

SHELL := /bin/sh

PREFIX       ?= /usr/local
BINDIR       ?= $(PREFIX)/bin
SYSCONFDIR   ?= /etc
CONFDIR      ?= $(SYSCONFDIR)/powerctl
UNITDIR      ?= $(SYSCONFDIR)/systemd/system
POLKITDIR    ?= $(SYSCONFDIR)/polkit-1/rules.d
LOGINDDIR    ?= $(SYSCONFDIR)/systemd/logind.conf.d
LIDCONF      ?= $(LOGINDDIR)/50-powerctl-lid.conf
USERUNITDIR  ?= $(HOME)/.config/systemd/user
USERCONFDIR  ?= $(HOME)/.config/powerctl
USERSTATEDIR ?= $(HOME)/.local/state/powerctl
SUDO         ?= sudo

SRCDIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

SCRIPTS := powerctl powerctl-idle-action
UNITS   := powerctl.service powerctl-resume.service \
           powerctl-refresh.service powerctl-refresh.timer
ENABLE  := powerctl.service powerctl-resume.service powerctl-refresh.timer

.DEFAULT_GOAL := help

.PHONY: help check install install-system install-user enable \
        uninstall uninstall-system uninstall-user purge startup-check

help:
	@echo 'powerctl'
	@echo
	@echo '  make check        syntax-check scripts and units (no changes)'
	@echo '  make install      install everything, enable units'
	@echo '  make uninstall    restore 100/100, remove files, keep config'
	@echo '  make purge        uninstall and delete config too'
	@echo
	@echo '  Charging rules stay INACTIVE after install. Turn them on with:'
	@echo '    powerctl cap 80     or     powerctl health'

# ------------------------------------------------------------------ preflight

# Running the whole thing as root would chown session files to root.
check-not-root:
	@if [ "$$(id -u)" = 0 ]; then \
	  echo 'error: run as your normal user -- the system steps use sudo themselves'; \
	  exit 1; \
	fi

check:
	@rc=0; \
	for f in $(SCRIPTS:%=$(SRCDIR)/bin/%); do \
	  sh -n "$$f" && echo "ok    $${f##*/}" || { echo "FAIL  $${f##*/}"; rc=1; }; \
	done; \
	for u in $(UNITS:%=$(SRCDIR)/systemd/system/%) $(SRCDIR)/systemd/user/swayidle.service; do \
	  systemd-analyze verify "$$u" 2>&1 | grep -v 'is not executable' | grep . && rc=1 || echo "ok    $${u##*/}"; \
	done; \
	exit $$rc

# -------------------------------------------------------------------- install

install: check-not-root install-system install-user enable startup-check
	@echo
	@echo '==> done. Charging rules are installed but INACTIVE.'
	@echo '    Turn them on with:  powerctl cap 80   or   powerctl health'
	@echo '    Review with:        powerctl list'

install-system:
	@ls /sys/class/power_supply/*/charge_control_end_threshold >/dev/null 2>&1 || \
	  echo 'warning: no battery with charge-threshold support; idle rules will still work'
	@command -v swayidle >/dev/null || echo 'warning: swayidle not found; idle rules need it'
	@echo '==> installing system files'
	$(SUDO) install -Dm755 $(SCRIPTS:%=$(SRCDIR)/bin/%) -t $(BINDIR)
	$(SUDO) install -Dm644 $(UNITS:%=$(SRCDIR)/systemd/system/%) -t $(UNITDIR)
	$(SUDO) install -Dm644 $(SRCDIR)/polkit/49-powerctl.rules -t $(POLKITDIR)
	@# Never clobber rules.conf -- it holds the chosen cap.
	@if [ -e $(CONFDIR)/rules.conf ]; then \
	  echo "==> keeping existing $(CONFDIR)/rules.conf"; \
	else \
	  $(SUDO) install -Dm644 $(SRCDIR)/etc/rules.conf $(CONFDIR)/rules.conf; \
	fi
	$(SUDO) systemctl daemon-reload

install-user:
	@echo '==> installing session files'
	install -d $(USERCONFDIR) $(USERUNITDIR) $(USERSTATEDIR)
	@if [ -e $(USERCONFDIR)/idle.conf ]; then \
	  echo "==> keeping existing $(USERCONFDIR)/idle.conf"; \
	else \
	  install -m644 $(SRCDIR)/config/idle.conf $(USERCONFDIR)/idle.conf; \
	fi
	@# Symlinked, not copied: session files are yours to edit, and edits go
	@# straight back into the dotfiles repo.
	ln -sfn $(SRCDIR)/systemd/user/swayidle.service $(USERUNITDIR)/swayidle.service
	systemctl --user daemon-reload

enable:
	@echo '==> enabling units'
	$(SUDO) systemctl enable $(ENABLE)
	$(BINDIR)/powerctl idle apply || true
	systemctl --user enable swayidle.service

# The old invocation must go or two swayidle instances race. Not automated:
# startup.sh is what gets you a session and is not tracked in this repo.
startup-check:
	@s=$(HOME)/.config/dwl/startup.sh; \
	if [ -f "$$s" ] && grep -q '^[^#]*swayidle' "$$s"; then \
	  echo; \
	  echo '=== MANUAL STEP REQUIRED ==='; \
	  echo "$$s still launches swayidle directly:"; \
	  grep -n '^[^#]*swayidle' "$$s" | sed 's/^/    /'; \
	  echo 'Comment that out (through its trailing backslashes), then log out'; \
	  echo 'and back in. Until you do, two swayidle instances will run and idle'; \
	  echo 'rules will behave unpredictably.'; \
	  echo '==========================='; \
	fi

# ------------------------------------------------------------------ uninstall

# Restore thresholds FIRST: a cap installed by powerctl must not outlive the
# tool that explains why the battery stops at 80%.
uninstall: check-not-root uninstall-user uninstall-system
	@echo
	@echo '==> if you commented out the swayidle line in ~/.config/dwl/startup.sh,'
	@echo '    restore it, or you will have no idle timeouts after next login.'

uninstall-system:
	@echo '==> restoring charge thresholds to 100/100'
	-[ -x $(BINDIR)/powerctl ] && $(SUDO) $(BINDIR)/powerctl apply --restore
	@# Same reasoning: a lid policy set by powerctl must not outlive the tool
	@# that explains why closing the lid does nothing. Reload, don't restart --
	@# logind is Type=notify-reload and the session stays up.
	@if [ -e $(LIDCONF) ]; then \
	  echo '==> restoring default lid handling'; \
	  $(SUDO) rm -f $(LIDCONF); \
	  $(SUDO) systemctl reload systemd-logind || true; \
	fi
	@echo '==> disabling and removing system files'
	-$(SUDO) systemctl disable --now powerctl.service
	-$(SUDO) systemctl disable powerctl-resume.service powerctl-refresh.timer
	$(SUDO) rm -f $(SCRIPTS:%=$(BINDIR)/%) \
	              $(UNITS:%=$(UNITDIR)/%) \
	              $(POLKITDIR)/49-powerctl.rules
	$(SUDO) systemctl daemon-reload

uninstall-user:
	@echo '==> removing session files'
	-systemctl --user disable --now swayidle.service
	rm -f $(USERUNITDIR)/swayidle.service $(USERSTATEDIR)/swayidle-cmd.sh
	-systemctl --user daemon-reload

purge: uninstall
	@echo '==> purging config'
	$(SUDO) rm -rf $(CONFDIR)
	rm -rf $(USERCONFDIR) $(USERSTATEDIR)
