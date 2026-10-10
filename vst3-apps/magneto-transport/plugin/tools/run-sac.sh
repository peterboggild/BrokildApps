#!/usr/bin/env bash
# Run a freshly linked test exe past Smart App Control: copy it, append a few
# random bytes (overlay data the loader ignores, so the program is unchanged
# but the hash is new), and retry until one copy is allowed to start.
#   tools/run-sac.sh <exe> [args...]
exe="$1"; shift
dir=$(dirname "$exe"); base=$(basename "$exe" .exe)
for a in 1 2 3 4 5 6 7 8; do
  c="$dir/${base}_sac$a.exe"
  cp "$exe" "$c"; head -c $((RANDOM % 8 + 1)) /dev/urandom >> "$c"
  out=$("$c" "$@" 2>&1); rc=$?
  if ! echo "$out" | grep -q "Application Control\|Permission denied"; then
    echo "$out"; rm -f "$dir/${base}_sac"*.exe 2>/dev/null; exit $rc
  fi
done
echo "Smart App Control blocked every copy"; exit 99
