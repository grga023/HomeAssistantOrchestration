#!/bin/sh
set -eu
LOCK_FILE=".gh-pages.lock"
case "$1" in
  lock)
    if git ls-files --error-unmatch "$LOCK_FILE" >/dev/null 2>&1; then
      echo "Lock file exists in repo; aborting"
      git --no-pager log -n 3 -- "$LOCK_FILE" || true
      exit 1
    fi
    echo "Locked by $(whoami)@$(hostname) at $(date)" > "$LOCK_FILE"
    git add "$LOCK_FILE"
    git commit -m "gh-pages: acquire lock ($(whoami)@$(hostname))"
    git push
    ;;
  unlock)
    if [ ! -f "$LOCK_FILE" ]; then
      echo "No lock present"
      exit 0
    fi
    git rm -f "$LOCK_FILE"
    git commit -m "gh-pages: release lock ($(whoami)@$(hostname))" || true
    git push || true
    ;;
  status)
    if [ -f "$LOCK_FILE" ]; then
      cat "$LOCK_FILE"
    else
      echo "No lock"
    fi
    ;;
  *)
    echo "Usage: $0 {lock|unlock|status}"
    exit 1
    ;;
esac
