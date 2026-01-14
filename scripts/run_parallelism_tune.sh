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
EXEC_TGS_STR="${METALFPGA_EXEC_READY_TGS:-"64 128"}"
READY_TGS_STR="${METALFPGA_READY_TGS:-"256 384"}"
ALIAS_MODES_STR="${METALFPGA_ALIAS_MODES:-"off on auto"}"
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

now_s() {
  if command -v perl >/dev/null 2>&1; then
    perl -MTime::HiRes -e 'printf "%.6f", Time::HiRes::time()'
  else
    date +%s
  fi
}

calc_seconds() {
  local start="$1"
  local end="$2"
  awk -v s="$start" -v e="$end" 'BEGIN { printf "%.3f", (e - s) }'
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

last_status_line() {
  local input="$1"
  local line=""
  if [[ "$FILTER_CMD" == "rg" ]]; then
    line="$(rg -n "^sched status:" "$input" | tail -n 1 || true)"
  else
    line="$(grep -n "^sched status:" "$input" | tail -n 1 || true)"
  fi
  printf '%s' "${line#*:}"
}

extract_iter() {
  local status_line="$1"
  if [[ "$status_line" != *"iter="* ]]; then
    printf ''
    return
  fi
  printf '%s' "$status_line" | sed -E 's/.*iter=([0-9]+).*/\1/'
}

BASE_ENV=(
  METALFPGA_PIPELINE_PRECOMPILE=1
  METALFPGA_SCHED_READY_EVERY=1
  METALFPGA_SCHED_READY=1
  METALFPGA_SCHED_EXEC_READY=1
  METALFPGA_SCHED_WAIT_EVAL=1
  METALFPGA_SCHED_BATCH=1
  METALFPGA_SCHED_READY_BATCH=0
  METALFPGA_SCHED_SERVICE_DRAIN_EVERY=1
  METALFPGA_BATCH_BARRIERS_DISABLE=0
)

read -r -a COUNTS <<< "$COUNTS_STR"
read -r -a EXEC_TGS <<< "$EXEC_TGS_STR"
read -r -a READY_TGS <<< "$READY_TGS_STR"
read -r -a ALIAS_MODES <<< "$ALIAS_MODES_STR"

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
    out_root="$(mktemp -d "$TMP_BASE/metalfpga_parallelism.tune.count${count}.XXXXXX")"
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
  : > "$summary"

  for exec_tg in "${EXEC_TGS[@]}"; do
    for ready_tg in "${READY_TGS[@]}"; do
      for alias_mode in "${ALIAS_MODES[@]}"; do
        local alias_env=()
        case "$alias_mode" in
          off)
            alias_env=(METALFPGA_BATCH_BARRIER_ALIAS=0
                       METALFPGA_BATCH_BARRIER_ALIAS_AUTO=0)
            ;;
          on)
            alias_env=(METALFPGA_BATCH_BARRIER_ALIAS=1
                       METALFPGA_BATCH_BARRIER_ALIAS_AUTO=0)
            ;;
          auto)
            alias_env=(METALFPGA_BATCH_BARRIER_ALIAS=0
                       METALFPGA_BATCH_BARRIER_ALIAS_AUTO=1)
            ;;
          *)
            echo "Unknown alias mode: $alias_mode (expected off/on/auto)" >&2
            exit 1
            ;;
        esac

        local tg_env=(
          "METALFPGA_SCHED_EXEC_READY_TG=$exec_tg"
          "METALFPGA_SCHED_READY_RESET_TG=$ready_tg"
          "METALFPGA_SCHED_WAIT_EVAL_TG=$ready_tg"
          "METALFPGA_SCHED_READY_FLAGS_TG=$ready_tg"
          "METALFPGA_SCHED_READY_COMPACT_TG=$ready_tg"
        )

        local name="tg${exec_tg}_ready${ready_tg}_alias_${alias_mode}"
        local log="$out_root/${name}.log"
        local env_file="$out_root/${name}.env"
        local cmd_file="$out_root/${name}.cmd"

        printf '%s\n' "${BASE_ENV[@]}" "${tg_env[@]}" "${alias_env[@]}" > "$env_file"
        printf '%q ' "$CLI" "${run_args[@]}" > "$cmd_file"
        printf '\n' >> "$cmd_file"

        echo "Running $name..."
        local start
        local end
        start="$(now_s)"
        set +e
        run_with_timeout env "${BASE_ENV[@]}" "${tg_env[@]}" "${alias_env[@]}" \
          "$CLI" "${run_args[@]}" &> "$log"
        local exit_code=$?
        set -e
        end="$(now_s)"
        local duration
        duration="$(calc_seconds "$start" "$end")"
        local timed_out=0
        case "$exit_code" in
          124|137|142|143) timed_out=1 ;;
        esac

        local status_line
        status_line="$(last_status_line "$log")"
        local iter_val
        iter_val="$(extract_iter "$status_line")"
        local iters_per_sec="n/a"
        if [[ -n "$iter_val" && "$duration" != "0.000" ]]; then
          iters_per_sec="$(awk -v i="$iter_val" -v d="$duration" \
            'BEGIN { if (d > 0) { printf "%.2f", (i / d) } else { print "n/a" } }')"
        fi

        {
          echo "count=$count"
          echo "exec_ready_tg=$exec_tg"
          echo "ready_tg=$ready_tg"
          echo "alias_mode=$alias_mode"
          echo "exit_code=$exit_code"
          echo "timed_out=$timed_out"
          echo "duration_secs=$duration"
          echo "iter_last=${iter_val}"
          echo "iters_per_sec=$iters_per_sec"
          echo "status_line=${status_line}"
          echo "log=$log"
          echo "env=$env_file"
          echo "cmd=$cmd_file"
          echo "---"
        } >> "$summary"
      done
    done
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
