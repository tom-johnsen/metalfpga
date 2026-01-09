#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="${METALFPGA_CLI:-"$ROOT/build/metalfpga_cli"}"
RTL="${METALFPGA_RTL:-"$ROOT/goldentests/yosys-tests/bigsim/picorv32/rtl/picorv32.v"}"
TB="${METALFPGA_TB:-"$ROOT/goldentests/yosys-tests/bigsim/picorv32/sim/testbench.v"}"
TOP="${METALFPGA_TOP:-testbench}"
FIRMWARE="${METALFPGA_FIRMWARE:-"$ROOT/goldentests/yosys-tests/bigsim/picorv32/sim/firmware.hex"}"
COUNT_DEFAULT="${METALFPGA_COUNT:-1}"
COUNTS_STR="${METALFPGA_COUNTS:-$COUNT_DEFAULT}"
TIMEOUT_SECS="${METALFPGA_TIMEOUT_SECS:-20}"
MAX_PROC_STEPS="${METALFPGA_MAX_PROC_STEPS:-0}"
OUT_ROOT_OVERRIDE="${OUT_ROOT:-}"

TIMEOUT_MODE="none"
TIMEOUT_BIN=""
if [[ "$TIMEOUT_SECS" -gt 0 ]]; then
  if command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_BIN="gtimeout"
    TIMEOUT_MODE="gtimeout"
  elif command -v timeout >/dev/null 2>&1; then
    TIMEOUT_BIN="timeout"
    TIMEOUT_MODE="timeout"
  elif command -v perl >/dev/null 2>&1; then
    TIMEOUT_MODE="perl"
  fi
fi

run_with_timeout() {
  if [[ "$TIMEOUT_SECS" -le 0 ]]; then
    "$@"
    return $?
  fi
  if [[ -n "$TIMEOUT_BIN" ]]; then
    "$TIMEOUT_BIN" "${TIMEOUT_SECS}s" "$@"
    return $?
  fi
  if [[ "$TIMEOUT_MODE" == "perl" ]]; then
    perl -e 'alarm shift; exec @ARGV' "$TIMEOUT_SECS" "$@"
    return $?
  fi
  "$@"
}

if [[ ! -x "$CLI" ]]; then
  echo "metalfpga_cli not found/executable: $CLI" >&2
  exit 1
fi

TMP_BASE="${OUT_ROOT_BASE:-"$ROOT/tmp"}"
mkdir -p "$TMP_BASE"

if command -v rg >/dev/null 2>&1; then
  FILTER_CMD="rg"
else
  FILTER_CMD="grep"
fi

filter_log() {
  local input="$1"
  local output="$2"
  local pattern="^(warning:|error:|fatal:|sched stop:|sched status:|sched ready|sched-vm: watch|warning: service record overflow)"
  if [[ "$FILTER_CMD" == "rg" ]]; then
    rg -n -e "$pattern" "$input" > "$output" || true
  else
    grep -E "$pattern" "$input" > "$output" || true
  fi
}

count_matches() {
  local pattern="$1"
  local input="$2"
  local matches=""
  if [[ "$FILTER_CMD" == "rg" ]]; then
    matches="$(rg -i "$pattern" "$input" 2>/dev/null || true)"
  else
    matches="$(grep -E -i "$pattern" "$input" 2>/dev/null || true)"
  fi
  printf '%s' "$matches" | wc -l | tr -d ' '
}

BASE_ENV=(
  METALFPGA_PIPELINE_PRECOMPILE=1
  METALFPGA_SCHED_READY_EVERY=1
)

SCENARIOS=(
  "legacy_no_ready|METALFPGA_SCHED_READY=0 METALFPGA_SCHED_EXEC_READY=0 METALFPGA_SCHED_WAIT_EVAL=0 METALFPGA_SCHED_BATCH=0 METALFPGA_SCHED_READY_BATCH=0 METALFPGA_BATCH_BARRIERS_DISABLE=0 METALFPGA_BATCH_BARRIER_ALIAS=0 METALFPGA_BATCH_BARRIER_ALIAS_AUTO=0 METALFPGA_SCHED_SERVICE_DRAIN_EVERY=1"
  "exec_ready_no_batch|METALFPGA_SCHED_READY=1 METALFPGA_SCHED_EXEC_READY=1 METALFPGA_SCHED_WAIT_EVAL=1 METALFPGA_SCHED_BATCH=0 METALFPGA_SCHED_READY_BATCH=0 METALFPGA_BATCH_BARRIERS_DISABLE=0 METALFPGA_BATCH_BARRIER_ALIAS=0 METALFPGA_BATCH_BARRIER_ALIAS_AUTO=0 METALFPGA_SCHED_SERVICE_DRAIN_EVERY=1"
  "exec_ready_batch_alias_auto|METALFPGA_SCHED_READY=1 METALFPGA_SCHED_EXEC_READY=1 METALFPGA_SCHED_WAIT_EVAL=1 METALFPGA_SCHED_BATCH=1 METALFPGA_SCHED_READY_BATCH=0 METALFPGA_BATCH_BARRIERS_DISABLE=0 METALFPGA_BATCH_BARRIER_ALIAS=0 METALFPGA_BATCH_BARRIER_ALIAS_AUTO=1 METALFPGA_SCHED_SERVICE_DRAIN_EVERY=1"
)

read -r -a COUNTS <<< "$COUNTS_STR"
if [[ "${#COUNTS[@]}" -eq 0 ]]; then
  COUNTS=("$COUNT_DEFAULT")
fi

summaries=()

run_for_count() {
  local count="$1"
  local out_root=""

  if [[ -n "$OUT_ROOT_OVERRIDE" && "${#COUNTS[@]}" -eq 1 ]]; then
    out_root="$OUT_ROOT_OVERRIDE"
    mkdir -p "$out_root"
  else
    out_root="$(mktemp -d "$TMP_BASE/metalfpga_parallelism.count${count}.XXXXXX")"
  fi

  local run_args=(
    "$RTL"
    "$TB"
    --top "$TOP"
    --4state
    --sched-vm
    --run
    --run-verbose
    --count "$count"
    --max-proc-steps "$MAX_PROC_STEPS"
    "+firmware=$FIRMWARE"
  )

  echo "Output dir: $out_root"
  echo "CLI: $CLI"
  echo "COUNT: $count"
  echo "TIMEOUT: ${TIMEOUT_SECS}s ($TIMEOUT_MODE)"

  local summary="$out_root/summary.txt"
  local baseline_name=""
  local baseline_filtered=""

  for spec in "${SCENARIOS[@]}"; do
    local name="${spec%%|*}"
    local env_spec="${spec#*|}"
    local scenario_env=()
    if [[ "$env_spec" != "$spec" ]]; then
      read -r -a scenario_env <<< "$env_spec"
    fi

    local log="$out_root/${name}.log"
    local filtered="$out_root/${name}.filtered"
    local env_file="$out_root/${name}.env"
    local cmd_file="$out_root/${name}.cmd"

    printf '%s\n' "${BASE_ENV[@]}" "${scenario_env[@]}" > "$env_file"
    printf '%q ' "$CLI" "${run_args[@]}" > "$cmd_file"
    printf '\n' >> "$cmd_file"

    echo "Running $name..."
    set +e
    run_with_timeout env "${BASE_ENV[@]}" "${scenario_env[@]}" "$CLI" "${run_args[@]}" &> "$log"
    local exit_code=$?
    set -e
    local timed_out=0
    case "$exit_code" in
      124|137|142|143) timed_out=1 ;;
    esac

    filter_log "$log" "$filtered"

    if [[ -z "$baseline_name" ]]; then
      baseline_name="$name"
      baseline_filtered="$filtered"
    fi

    local diff_file="$out_root/${name}.diff"
    local diff_status="match"
    if [[ -n "$baseline_filtered" && "$name" != "$baseline_name" ]]; then
      if ! diff -u "$baseline_filtered" "$filtered" > "$diff_file"; then
        diff_status="diff"
      fi
    fi

    local warn_count
    local err_count
    warn_count="$(count_matches "warning:" "$log")"
    err_count="$(count_matches "error:|fatal" "$log")"
    local stop_line=""
    local status_line=""
    if [[ "$FILTER_CMD" == "rg" ]]; then
      stop_line="$(rg -n "sched stop:" "$log" | tail -n 1 || true)"
      status_line="$(rg -n "^sched status:" "$log" | tail -n 1 || true)"
    else
      stop_line="$(grep -E "sched stop:" "$log" | tail -n 1 || true)"
      status_line="$(grep -E "^sched status:" "$log" | tail -n 1 || true)"
    fi

    {
      echo "count=$count"
      echo "scenario=$name"
      echo "exit_code=$exit_code"
      echo "log=$log"
      echo "filtered=$filtered"
      echo "env=$env_file"
      echo "cmd=$cmd_file"
      echo "warnings=$warn_count errors=$err_count"
      echo "stop_line=${stop_line}"
      echo "status_line=${status_line}"
      echo "timeout_secs=$TIMEOUT_SECS"
      echo "timeout_mode=$TIMEOUT_MODE"
      echo "timed_out=$timed_out"
      if [[ "$name" == "$baseline_name" ]]; then
        echo "baseline=1"
      else
        echo "baseline=0"
        echo "compare_to=$baseline_name"
        echo "compare_status=$diff_status"
        if [[ "$diff_status" == "diff" ]]; then
          echo "diff=$diff_file"
        fi
      fi
      echo "---"
    } >> "$summary"
  done

  cat "$summary"
  echo "Summary written to: $summary"
  summaries+=("$summary")
}

for count in "${COUNTS[@]}"; do
  run_for_count "$count"
done

if [[ "${#summaries[@]}" -gt 1 ]]; then
  echo "All summaries:"
  printf '  %s\n' "${summaries[@]}"
fi
