#!/bin/bash
# Part 122: same-binary ENV arms on the crowd route, alternated, N runs an arm, the CPU
# clock printed before every run. Each arm is "tag" or "tag:VAR=val,VAR=val".
# Usage: tools/part122_campaign_env.sh <bin> <N> <arm> [arm ...]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$1"; N="$2"; shift 2
export OUT="${OUT:-$HOME/DR2CZ-troubleshooting/part122}"
export SOAK="${SOAK:-70}" RES="${RES:-1920x1080}" FPSLOG=10
clk() { awk '{s+=$1;n++} END {printf "%.0f MHz", s/n/1000}' /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq; }
for i in $(seq 1 "$N"); do
    for arm in "$@"; do
        tag="${arm%%:*}"; envs=()
        [[ "$arm" == *:* ]] && IFS=',' read -ra envs <<< "${arm#*:}"
        echo "clock before ${tag}$i: $(clk)"
        BIN_SRC="$BIN" "$ROOT/tools/part80_crowdroute.sh" "${tag}$i" "${envs[@]}"
    done
done
echo CAMPAIGN DONE
