#!/usr/bin/bash

  VAULT="/home/bouncyelectron/Documents/Obsidian Vault"

  cd "$VAULT" || exit 1

  git add -A

  if ! git diff --cached --quiet; then
      git commit -m "auto-sync: $(date '+%Y-%m-%d %H:%M')"
  fi

  # Pull remote changes first (rebase, auto-stashing anything uncommitted) so
  # the push below can't be rejected as non-fast-forward.
  git pull --rebase --autostash || exit 1

  git push
