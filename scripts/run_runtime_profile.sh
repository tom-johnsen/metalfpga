#!/bin/zsh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

RUNS="${METALFPGA_PROFILE_RUNS:-10}"
WARMUP="${METALFPGA_PROFILE_WARMUP:-1}"
HOST_BIN="${METALFPGA_PROFILE_HOST:-artifacts/profile/test_clock_big_vcd_host}"
MSL="${METALFPGA_PROFILE_MSL:-artifacts/profile/test_clock_big_vcd.msl}"
RAW_LOG="${METALFPGA_PROFILE_LOG:-artifacts/profile/runtime_profile_runs.log}"
SUMMARY="${METALFPGA_PROFILE_SUMMARY:-artifacts/profile/runtime_profile_summary.txt}"
REBUILD="${METALFPGA_PROFILE_REBUILD:-0}"

mkdir -p artifacts/profile

if [[ "$REBUILD" == "1" || ! -x "$HOST_BIN" || ! -f "$MSL" ]]; then
  ./scripts/reemit_test_clock_big_vcd.sh
fi

: > "$RAW_LOG"
echo "runtime_profile_harness" >> "$RAW_LOG"
echo "host=${HOST_BIN}" >> "$RAW_LOG"
echo "msl=${MSL}" >> "$RAW_LOG"
echo "runs=${RUNS} warmup=${WARMUP}" >> "$RAW_LOG"
echo "" >> "$RAW_LOG"

run=1
while [[ "$run" -le "$RUNS" ]]; do
  echo "=== run ${run} ===" >> "$RAW_LOG"
  "$HOST_BIN" "$MSL" --profile >> "$RAW_LOG" 2>&1
  echo "" >> "$RAW_LOG"
  run=$((run + 1))
done

awk -v warmup="$WARMUP" '
  /^=== run [0-9]+ ===$/ {
    run = $3 + 0;
    next;
  }
  run <= warmup {
    next;
  }
  {
    if ($1 == "[profile]" && $2 == "sim_loop" && $3 ~ /^ms=/) {
      label = "sim_loop";
      val = $3;
      sub(/^ms=/, "", val);
      sub(/ms$/, "", val);
      val = val + 0.0;
    } else if ($1 == "[profile]" && $3 ~ /^step=/) {
      label = $2;
      val = $3;
      sub(/^step=/, "", val);
      sub(/ms$/, "", val);
      val = val + 0.0;
    } else if ($1 == "[gpu_profile]" && $3 ~ /^ms=/) {
      label = "gpu_" $2;
      val = $3;
      sub(/^ms=/, "", val);
      sub(/ms$/, "", val);
      val = val + 0.0;
    } else {
      next;
    }
    if (!(label in seen)) {
      seen[label] = 1;
      labels[++n] = label;
      min[label] = val;
      max[label] = val;
    }
    count[label] += 1;
    sum[label] += val;
    if (val < min[label]) min[label] = val;
    if (val > max[label]) max[label] = val;
  }
  END {
    print "runtime_profile_summary";
    print "columns: label count mean_ms min_ms max_ms";
    for (i = 1; i <= n; i++) {
      label = labels[i];
      if (count[label] == 0) {
        continue;
      }
      mean = sum[label] / count[label];
      printf "%s %d %.6f %.6f %.6f\n", label, count[label], mean, min[label], max[label];
    }
  }
' "$RAW_LOG" > "$SUMMARY"
