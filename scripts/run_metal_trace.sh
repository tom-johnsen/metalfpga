#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="${METALFPGA_CLI:-"$ROOT/build/metalfpga_cli"}"
RTL="${METALFPGA_RTL:-"$ROOT/goldentests/yosys-tests/bigsim/picorv32/rtl/picorv32.v"}"
TB="${METALFPGA_TB:-"$ROOT/goldentests/yosys-tests/bigsim/picorv32/sim/testbench.v"}"
TOP="${METALFPGA_TOP:-testbench}"
FIRMWARE="${METALFPGA_FIRMWARE:-"$ROOT/goldentests/yosys-tests/bigsim/picorv32/sim/firmware.hex"}"
COUNT="${METALFPGA_COUNT:-1}"
MAX_PROC_STEPS="${METALFPGA_MAX_PROC_STEPS:-0}"

TRACE_TEMPLATE="${TRACE_TEMPLATE:-Metal System Trace}"
TRACE_TIME_LIMIT="${TRACE_TIME_LIMIT:-20s}"
TRACE_RUN_NAME="${TRACE_RUN_NAME:-metalfpga_cli}"
TRACE_OUTPUT="${TRACE_OUTPUT:-"$ROOT/tmp/metal_system_trace.$(date +%Y%m%d_%H%M%S).trace"}"
TRACE_MODE="${TRACE_MODE:-attach}" # launch|attach|all
TRACE_ATTACH_DELAY="${TRACE_ATTACH_DELAY:-0.5}"

if [[ ! -x "$CLI" ]]; then
  echo "metalfpga_cli not found/executable: $CLI" >&2
  exit 1
fi

mkdir -p "$(dirname "$TRACE_OUTPUT")"

SCHED_READY="${METALFPGA_SCHED_READY:-1}"
SCHED_EXEC_READY="${METALFPGA_SCHED_EXEC_READY:-1}"
SCHED_WAIT_EVAL="${METALFPGA_SCHED_WAIT_EVAL:-1}"
SCHED_BATCH="${METALFPGA_SCHED_BATCH:-1}"
SCHED_READY_BATCH="${METALFPGA_SCHED_READY_BATCH:-0}"
SCHED_READY_EVERY="${METALFPGA_SCHED_READY_EVERY:-1}"
SCHED_SERVICE_DRAIN_EVERY="${METALFPGA_SCHED_SERVICE_DRAIN_EVERY:-1}"
PIPELINE_PRECOMPILE="${METALFPGA_PIPELINE_PRECOMPILE:-1}"
BARRIER_DISABLE="${METALFPGA_BATCH_BARRIERS_DISABLE:-0}"
BARRIER_ALIAS="${METALFPGA_BATCH_BARRIER_ALIAS:-0}"
BARRIER_ALIAS_AUTO="${METALFPGA_BATCH_BARRIER_ALIAS_AUTO:-0}"

EXEC_READY_TG="${METALFPGA_SCHED_EXEC_READY_TG:-64}"
READY_TG="${METALFPGA_SCHED_READY_TG:-256}"
READY_RESET_TG="${METALFPGA_SCHED_READY_RESET_TG:-$READY_TG}"
WAIT_EVAL_TG="${METALFPGA_SCHED_WAIT_EVAL_TG:-$READY_TG}"
READY_FLAGS_TG="${METALFPGA_SCHED_READY_FLAGS_TG:-$READY_TG}"
READY_COMPACT_TG="${METALFPGA_SCHED_READY_COMPACT_TG:-$READY_TG}"

ENV_VARS=(
  "METALFPGA_PIPELINE_PRECOMPILE=$PIPELINE_PRECOMPILE"
  "METALFPGA_SCHED_READY_EVERY=$SCHED_READY_EVERY"
  "METALFPGA_SCHED_READY=$SCHED_READY"
  "METALFPGA_SCHED_EXEC_READY=$SCHED_EXEC_READY"
  "METALFPGA_SCHED_WAIT_EVAL=$SCHED_WAIT_EVAL"
  "METALFPGA_SCHED_BATCH=$SCHED_BATCH"
  "METALFPGA_SCHED_READY_BATCH=$SCHED_READY_BATCH"
  "METALFPGA_SCHED_SERVICE_DRAIN_EVERY=$SCHED_SERVICE_DRAIN_EVERY"
  "METALFPGA_BATCH_BARRIERS_DISABLE=$BARRIER_DISABLE"
  "METALFPGA_BATCH_BARRIER_ALIAS=$BARRIER_ALIAS"
  "METALFPGA_BATCH_BARRIER_ALIAS_AUTO=$BARRIER_ALIAS_AUTO"
  "METALFPGA_SCHED_EXEC_READY_TG=$EXEC_READY_TG"
  "METALFPGA_SCHED_READY_RESET_TG=$READY_RESET_TG"
  "METALFPGA_SCHED_WAIT_EVAL_TG=$WAIT_EVAL_TG"
  "METALFPGA_SCHED_READY_FLAGS_TG=$READY_FLAGS_TG"
  "METALFPGA_SCHED_READY_COMPACT_TG=$READY_COMPACT_TG"
)

CLI_ARGS=(
  "$RTL"
  "$TB"
  --top "$TOP"
  --4state
  --sched-vm
  --run
  --count "$COUNT"
  --max-proc-steps "$MAX_PROC_STEPS"
  "+firmware=$FIRMWARE"
)

if [[ "$#" -gt 0 ]]; then
  CLI_ARGS+=("$@")
fi

echo "Trace template: $TRACE_TEMPLATE"
echo "Trace output:   $TRACE_OUTPUT"
echo "Time limit:     $TRACE_TIME_LIMIT"
echo "Run name:       $TRACE_RUN_NAME"
echo "Trace mode:     $TRACE_MODE"
echo "CLI:            $CLI"

TRACE_ARGS=(
  xcrun xctrace record
  --template "$TRACE_TEMPLATE"
  --time-limit "$TRACE_TIME_LIMIT"
  --run-name "$TRACE_RUN_NAME"
  --output "$TRACE_OUTPUT"
)

case "$TRACE_MODE" in
  attach)
    echo "Launching metalfpga_cli for attach..."
    env "${ENV_VARS[@]}" "$CLI" "${CLI_ARGS[@]}" &
    CLI_PID=$!
    trap 'kill "$CLI_PID" 2>/dev/null || true' EXIT
    sleep "$TRACE_ATTACH_DELAY"
    if ! kill -0 "$CLI_PID" 2>/dev/null; then
      echo "metalfpga_cli exited before attach (pid $CLI_PID)" >&2
      exit 1
    fi
    TRACE_ARGS+=(--attach "$CLI_PID")
    "${TRACE_ARGS[@]}"
    if kill -0 "$CLI_PID" 2>/dev/null; then
      kill "$CLI_PID" 2>/dev/null || true
      wait "$CLI_PID" 2>/dev/null || true
    fi
    ;;
  all)
    echo "Launching metalfpga_cli for all-process trace..."
    env "${ENV_VARS[@]}" "$CLI" "${CLI_ARGS[@]}" &
    CLI_PID=$!
    trap 'kill "$CLI_PID" 2>/dev/null || true' EXIT
    sleep "$TRACE_ATTACH_DELAY"
    if ! kill -0 "$CLI_PID" 2>/dev/null; then
      echo "metalfpga_cli exited before trace (pid $CLI_PID)" >&2
      exit 1
    fi
    TRACE_ARGS+=(--all-processes)
    "${TRACE_ARGS[@]}"
    if kill -0 "$CLI_PID" 2>/dev/null; then
      kill "$CLI_PID" 2>/dev/null || true
      wait "$CLI_PID" 2>/dev/null || true
    fi
    ;;
  launch)
    for env_kv in "${ENV_VARS[@]}"; do
      TRACE_ARGS+=(--env "$env_kv")
    done
    TRACE_ARGS+=(--launch -- "$CLI" "${CLI_ARGS[@]}")
    "${TRACE_ARGS[@]}"
    ;;
  *)
    echo "Unknown TRACE_MODE: $TRACE_MODE (use attach, launch, or all)" >&2
    exit 1
    ;;
esac
