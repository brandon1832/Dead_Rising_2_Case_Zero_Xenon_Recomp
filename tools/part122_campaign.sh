#!/bin/bash
# Part 122: a same-route binary A/B (old vs new), alternated, N runs an arm, the CPU clock
# printed beside every run (part 118 §5: amd-pstate-epp drifted the clock ~6% inside one
# campaign, so a run's MHz is part of its reading). Read with tools/part116_guestcpu.py.
# Usage: tools/part122_campaign.sh <binA> <tagA> <binB> <tagB> [N]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
A="$1"; TA="$2"; B="$3"; TB="$4"; N="${5:-3}"
export OUT="${OUT:-$HOME/DR2CZ-troubleshooting/part122}"
export SOAK="${SOAK:-70}" RES="${RES:-1920x1080}" FPSLOG=10
clk() { awk '{s+=$1;n++} END {printf "%.0f MHz", s/n/1000}' /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq; }
for i in $(seq 1 "$N"); do
    echo "clock before ${TA}$i: $(clk)"; BIN_SRC="$A" "$ROOT/tools/part80_crowdroute.sh" "${TA}$i" "${@:6}"
    echo "clock before ${TB}$i: $(clk)"; BIN_SRC="$B" "$ROOT/tools/part80_crowdroute.sh" "${TB}$i" "${@:6}"
done
echo CAMPAIGN DONE
