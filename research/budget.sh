#!/usr/bin/env bash
# Disk budget guard for the WPE automation browser project.
# Hard cap: 10 GB for /root/wk-project (source + build + dist + ccache + logs).
set -uo pipefail

CAP_GB=10
ROOT="${WK_PROJECT_ROOT:-/root/wk-project}"
CCACHE_DIR="${CCACHE_DIR:-/root/wk-project/build/ccache}"

# System packages installed on the OS's behalf, tracked for transparency.
SYSLOG="$ROOT/research/system-deps.txt"

fmt() { numfmt --to=iec --suffix=B "$1" 2>/dev/null || echo "${1}B"; }

used_kb=$(du -sk --apparent-size "$ROOT" 2>/dev/null | awk '{print $1}')
[ -z "${used_kb:-}" ] && used_kb=0
used_mb=$(( used_kb / 1024 ))
used_gb=$(awk -v m="$used_mb" 'BEGIN{printf "%.2f", m/1024}')
cap_mb=$(( CAP_GB * 1024 ))
pct=$(awk -v u="$used_mb" -v c="$cap_mb" 'BEGIN{printf "%.1f", (u/c)*100}')

fs_avail_gb=$(df -BG --output=avail / | tail -1 | tr -dc '0-9')

printf '=== WPE project disk budget ===\n'
printf 'root      : %s\n' "$ROOT"
printf 'used      : %s (%s%% of cap)\n' "$(fmt $((used_mb*1024*1024)))" "$pct"
printf 'cap       : %s GB\n' "$CAP_GB"
printf 'fs avail  : %s GB\n' "$fs_avail_gb"
printf '\n=== breakdown ===\n'
du -sh --apparent-size "$ROOT"/* 2>/dev/null | sort -rh

status=0
if [ "$used_mb" -gt "$cap_mb" ]; then
  printf '\nSTATUS: OVER BUDGET by %s\n' "$(fmt $(( (used_mb-cap_mb)*1024*1024 )))"
  status=2
elif [ "$used_mb" -gt $(( cap_mb * 85 / 100 )) ]; then
  printf '\nSTATUS: WARNING - %s%% of cap consumed. Prune before continuing.\n' "$pct"
  status=1
else
  printf '\nSTATUS: OK\n'
fi

printf '\n=== system packages installed on behalf of this project ===\n'
if [ -f "$SYSLOG" ]; then grep -v '^#' "$SYSLOG" | grep -v '^$' || echo "(none logged yet)"; else
  echo "(not yet tracked)"; fi

exit $status