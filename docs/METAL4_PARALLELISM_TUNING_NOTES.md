# Metal4 Parallelism: Tuning & Perf Notes

This note captures the recent tuning runs and the conclusions so far, plus
what to do next to move performance beyond hertz-level progress on picorv32.

## Current State (Plan Status)

- Parallelism plan phases 0-5 are implemented (ready list, indirect dispatch,
  batching, service drain). Remaining work is Phase 6 tuning + verification.
- Exec-ready indirect dispatch now auto-fills grid size from `ready_count[0]`
  and supports `--count > 1` in a single global dispatch.

## Tuning Results (Count=1, 20s runs)

Sweep: `exec_ready_tg` in {64,128}, `ready_tg` in {256,384}, `alias` in
{off,on,auto}. All runs used exec-ready + batch with 20s timeout.

Best result (iters/sec):
- `exec_ready_tg=64`, `ready_tg=256`, `alias=off` ≈ 202 it/s

Other observations:
- `ready_tg=384` is slower across the board.
- `alias=on/auto` is slightly slower than `alias=off` for this workload.
- `exec_ready_tg=128` fails immediately with:
  `required threadgroup size (128) exceeds max (64) for kernel ...`

Implication:
- `exec_ready_tg` must be <= 64 for this kernel pipeline. This is a kernel
  resource limit (`maxTotalThreadsPerThreadgroup`), not a hardware-wide limit.

Note:
- Count=2 did not run in that sweep because `METALFPGA_COuNTS` was set
  (case-sensitive typo). Use `METALFPGA_COUNTS="1 2"` for both.

## Why GPU% Looks Low

- Activity Monitor GPU % is a busy-time metric (0-100), not a "core count"
  metric like CPU%. It will not exceed 100% when more GPU cores are used.
- The scheduler is usually in `ready:0 blocked:25 done:20` state, so the
  exec-ready grid is tiny and GPU occupancy stays low.
- In other words: the design is not small, but the *parallel work per
  iteration* is small because most processes are blocked on time/edge waits.

## Why We Are Still at Hertz-Level Progress

The main bottleneck is not threadgroup sizing; it is the algorithmic cadence:
- The scheduler advances time with many iterations where no real work happens.
- Ready lists are small, so GPU parallelism is underutilized each iteration.
- This leads to hertz-level progress on picorv32 even with GPU execution.

## Metal System Trace Findings (Count=2, 30s, exec-ready+batch)

Capture: `tmp/metal_system_trace.20260110_174124.trace` via `TRACE_MODE=all`.
The app-level Metal tables did not include `metalfpga_cli`, so the data below
comes from `metal-gpu-intervals` rows labeled `metalfpga_cli` (Compute channel).

- 9,236 GPU compute intervals over 30s (~308 commands/sec).
- GPU duration per command: avg 2.63 ms, median 2.01 ms, min 0.086 ms,
  max 6.48 ms.
- CPU→GPU submit latency: avg 0.187 ms, median 0.179 ms, max 6.70 ms.
- Start-to-start gap: avg 3.32 ms, median 2.56 ms, max 20.97 ms.
- Idle gap (end→next start): avg 0.69 ms, median 0.49 ms, max 16.26 ms.

Interpretation:
- The GPU is busy most of the time (~79% duty over this window), but each
  dispatch is small. The bottleneck is per-iteration work volume, not queueing.

## Next Steps That Matter (Beyond TG Tuning)

Tuning TGs helps only marginally. For simulator-level speed, focus on:

1) **Time jump / next-event advance**
   - Compute the minimum next wake time across blocked procs and jump
     `sched_time` directly.
   - This removes huge numbers of idle iterations.
   - Concrete: reduce `sched_wait_time` per-sim on GPU (min reduction), then
     advance `sched_time[sim]` to that minimum when no ready procs exist.
   - Guard for event/edge waits: if any non-time waits are pending, do not jump
     past them; only advance when time wait is the next unblock reason.

2) **Multiple iterations per dispatch**
   - Move more of the scheduler loop onto the GPU so each dispatch runs many
     iterations before a CPU sync.
   - Reduces CPU-GPU round trips and improves throughput.
   - Concrete: add a bounded GPU loop that executes N scheduler ticks per
     dispatch (or until finished/stopped), with a short-circuit when service
     drain needs CPU processing.

3) **Multi-sim throughput**
   - Running `count > 1` independent sims can lift overall utilization and
     total sims/sec even if single-sim remains slow.
   - Concrete: validate `count=2/4/8` scaling with exec-ready + batch and
     measure sims/sec rather than single-sim Hz.

## Immediate Action Items

- Keep `exec_ready_tg=64`, `ready_tg=256`, `alias=off` as the best known
  tuning baseline for now.
- Rerun the tuning sweep with `METALFPGA_COUNTS="2"` to confirm multi-sim
  behavior, then lock in defaults.
- Start work on time-jump (next-event advance) to cut idle iterations.
