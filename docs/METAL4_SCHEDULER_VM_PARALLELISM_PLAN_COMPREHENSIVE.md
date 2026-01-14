# Metal4 Scheduler VM Parallelism - Comprehensive Plan

This document consolidates the current state, findings, and next steps for the
Metal4 scheduler VM parallelism effort. It is intended to be the single
reference plan that ties together implementation status, correctness risks,
performance strategy, and validation.

## Scope and goals

- Turn the scheduler VM into a GPU-saturating, proc-parallel pipeline.
- Remove per-step CPU waits by batching phases in a single command buffer.
- Preserve correctness relative to legacy scheduler behavior.
- Provide clear profiling data that separates queue delay vs GPU execution.
- Enable multi-sim throughput without per-sim dispatch overhead.

## Current state (implemented)

- Ready-list pipeline: wait-eval -> ready flags -> ready compaction ->
  ready-count -> exec-ready.
- Indirect exec-ready dispatch driven by GPU-written args (grid size) with
  pipeline-fixed threadgroup size (requiredThreadsPerThreadgroup).
- Batched dispatches via DispatchBatch with optional intra-pass barriers and
  resource-alias visibility.
- Service ring buffer with head/tail counters and adjustable drain cadence.
- Commit feedback logging (queue_ms, gpu_exec_ms) and timestamp sampling.
- Instrumentation scripts for tuning and Metal System Trace capture.

## Key findings

- Exec-ready kernel max threads per TG is 64; higher TG sizes fail in pipeline
  creation (maxTotalThreadsPerThreadgroup limit).
- GPU is busy but each dispatch is small; overall throughput is limited by
  iteration count and tiny per-iteration work, not queue latency.
- Ready-count often zero on picorv32; time-advance is the highest leverage fix.
- After time-jump, reducing dispatch count per iteration is the next largest
  lever; batching alone does not remove GPU per-dispatch overhead.

## Metal 4 execution model notes (carry-over)

- Command allocators are not thread-safe; reset only after GPU completion.
- GPU-address bindings require explicit resource lifetime management.
- Apply residency sets for GPU-address bound buffers (per-cmd or queue-level).
- Queue supports add/remove residency sets and applies them to committed buffers.
- Keep producer -> consumer ordering explicit with barriers during bring-up.
- Barrier types exist for intra-pass (encoder stages) and queue-scoped
  producer/consumer ordering; prefer intra-pass barriers for phase chaining.
- Indirect dispatch requires a fixed TG size (requiredThreadsPerThreadgroup).
  If per-dispatch TG sizing is needed, use an API that takes threadsPerTG
  (dispatchThreadgroups(indirectBuffer:threadsPerThreadgroup:)).
- Commit feedback gpuStartTime/gpuEndTime are host-time seconds for the
  committed workload; use these for queue_ms vs gpu_exec_ms.

## Scheduler VM pipeline (current architecture)

Per iteration (simplified):

1) sched_ready_reset (clear flags/count)
2) sched_wait_eval (evaluate waits -> READY)
3) sched_ready_flags (mark ready procs)
4) sched_ready_compact (atomic append -> ready list/count)
5) sched_ready_dispatch (writes indirect args)
6) sched_exec_ready (exec ready procs via indirect dispatch)
7) service drain (CPU when needed)

All steps can be encoded in one command buffer (single compute encoder), with
barriers between phases when enabled.

## Correctness risks and invariants

- join_count updates must be atomic to avoid double-unblock races.
- join_count underflow should be detected (indicates double-decrement).
- halt/status updates must be deterministic when multiple procs finish/stop.
- ready_count must be clamped to list capacity and zero-ready must dispatch 1
  thread with early-out in kernel.
- indirect args layout must match SDK struct size (no OOB access).
- avoid MSL `uint3` layout ambiguity; prefer packed uint[3] for indirect args
  (packed_uint3 has well-defined size/alignment in the MSL spec).
- service ring must never overflow (enforce watermark + yield).
  Use reserve-then-write (atomic tail) rather than check-then-write.

## Plan forward (phased)

### Phase A - Correctness hardening (must-do)

1) Make sched_join_count atomic and unblock on last child only:
   - atomic_fetch_sub, if old == 1 -> transition to READY.
   - if old == 0 -> underflow (set error/diagnostic).
2) Deterministic halt/status arbitration:
   - prefer FINISH over STOP; use atomic compare/exchange or a single-writer
     policy for final status with a total order.
3) Indirect args layout guard:
   - static_assert on sizeof(MTLDispatchThreadsIndirectArguments).
   - skip TG validation when indirect buffer is not CPU-mapped (private).
   - ensure shader writes packed uint[3] (or packed_uint3), not uint3.
   - alternative: use dispatchThreadgroups(indirectBuffer:threadsPerThreadgroup:)
     to avoid storing threadsPerThreadgroup in the indirect args buffer.
4) Service ring safety:
   - reserve slot via atomic_fetch_add, then check head vs capacity.
   - if overflow, set should_yield and do not write the record.
   - optional soft watermark to trigger should_yield before hard-full.
5) Keep batch barriers enabled during correctness bring-up:
   - default `.device` visibility; only tune alias after correctness holds.
6) Document memory-visibility assumptions:
   - dispatch boundary vs explicit barrier usage.
   - CPU reads only after command buffer completion.
   - storage mode for buffers read by CPU mid-run.
   - prefer intra-pass barriers for phase chaining; use queue-level barriers
     only for cross-command-buffer ordering.

Exit criteria:
- Exec-ready produces stable outputs for small tests without data races.
- No crashes when using private indirect buffers.

### Phase B - Time-jump (highest leverage)

1) Add per-sim min wait time reduction:
   - one threadgroup per sim; threads stride procs.
   - compute min future WAIT_TIME and flags (has_time, has_delta).
2) Apply time jump when:
   - no ready work, no delta waits, and min_time > current_time.
3) Re-run wait_eval immediately after time jump.
4) Deadlock/idle policy:
   - if no time waits exist, mark sim stopped/idle or yield to host.
5) Include host-driven input time in min_time (if host can inject stimuli).

Exit criteria:
- Ready-count zero case advances time without CPU round-trip.
- idle iterations drop sharply on picorv32.

### Phase B.5 - Dispatch fusion for ready-building

Goal: reduce per-iteration dispatch count (current ~6 -> target ~2).

1) Replace reset + wait_eval + ready_flags + compact + args-write with one
   build_ready kernel:
   - threadgroup-local compaction per TG.
   - single atomic to reserve contiguous output range.
   - last TG writes indirect args once (tg_done_count).
2) Keep exec_ready as a separate dispatch (indirect).

Exit criteria:
- dispatches per iteration drop materially (log metric).
- ready list and args are correct across multiple TGs.

### Phase C - Multi-iteration per command buffer

1) Encode N iterations worth of dispatches in one DispatchBatch.
2) Add should_yield flag per sim (or global):
   - set on finish/stop/error or service watermark.
   - kernels early-out if should_yield is set.
   - should_yield is monotonic for the batch (never cleared by GPU).
3) Keep max iteration bound small at first (N=4 or N=8).
   - early-out still pays dispatch overhead for the remaining iterations.

Exit criteria:
- Fewer command buffer submissions per second.
- Service drain still responsive; no ring overflow.

### Phase D - Per-sim readiness and time-jump

1) Track per-sim ready counts during compaction.
2) Allow time-jump per sim; do not stall busy sims.
3) Enable count > 1 validation runs.
4) Keep a global ready list of (sim, proc) entries to avoid per-sim dispatches.

Exit criteria:
- sims with no ready work advance independently.
- aggregate sims/sec increases with count.

### Phase E - Performance tuning (after correctness)

1) Threadgroup sizing:
   - ready phases: test 128/256/384 (TEW-aligned).
   - exec-ready: 32/64 (pipeline-fixed variants only).
   - per-phase overrides:
     METALFPGA_SCHED_READY_RESET_TG, METALFPGA_SCHED_WAIT_EVAL_TG,
     METALFPGA_SCHED_READY_FLAGS_TG, METALFPGA_SCHED_READY_COMPACT_TG,
     METALFPGA_SCHED_READY_DISPATCH_TG, METALFPGA_SCHED_EXEC_READY_TG.
2) Barrier alias:
   - evaluate METALFPGA_BATCH_BARRIER_ALIAS(_AUTO).
3) Ready compaction optimization:
   - if atomic append is hot, add block-level compaction then prefix-sum.
4) Optional work-queue:
   - chunked atomic head per threadgroup to reduce contention.
5) Replace ready_reset with epoch/stamp if clear cost is material.
6) Audit exec-ready TG=64 limit:
   - check for max_total_threads_per_threadgroup annotations.
   - reduce per-thread state if resource pressure caps TG size.

Exit criteria:
- measurable iters/sec improvement on picorv32.
- no regressions in correctness.

### Phase F - Profiling + instrumentation

1) Commit feedback remains the primary queue vs exec signal.
2) Add labels for command queue, command buffer, encoder.
3) Optional os_signpost around iteration batches.
4) Use metal-gpu-intervals for CLI traces when app tables are empty.

Exit criteria:
- consistent capture of GPU intervals and dispatch cadence.

## Validation plan

- Use existing scripts:
  - scripts/run_parallelism_verify.sh (baseline scenarios)
  - scripts/run_parallelism_tune.sh (TG sweeps)
  - scripts/run_metal_trace.sh (Metal System Trace)
- Add a small correctness target with deterministic output:
  - low proc count, known wait/ready patterns.
- Compare outputs by filtering noisy diagnostics (ready/debug lines).
- Run baselines for count=1 and count>1.
- Check logs show grid >= proc_count for proc-parallel phases.
- Confirm commit feedback logs queue_ms vs gpu_exec_ms.
- Ensure no regressions in --run for known designs (VCD/testbench).
- Add race-catcher tests:
  - join storm (many children finish at once).
  - status collision (stop vs finish in same timestep).
- Track dispatches per simulated timestep.

## Deliverables checklist

- Code:
  - Atomic join_count updates and deterministic halt/status.
  - Time-jump kernels + integration into batch.
  - Multi-iteration batching + should_yield gating.
  - Indirect args struct guard + relaxed validation when unmapped.
- Docs:
  - Update tuning notes with new results.
  - Update metal4gpt update summary when milestones hit.
- Scripts:
  - Extend verify/tune scripts for new env flags (time-jump, iterations).
  - Document env overrides for per-phase TGs and batch barriers.

## Acceptance criteria

- Correctness: outputs match legacy behavior on small deterministic cases.
- Performance: picorv32 moves from hertz-level toward meaningful speedups.
- Scalability: count > 1 increases total sims/sec without correctness drift.
- Tooling: traces and timing logs are consistent and interpretable.

## Questions for Metal4GPT

- Time-jump correctness: delta/edge/cond interactions and idle semantics.
- Indirect args struct layout (3 u32 vs 6 u32) in current SDK.
- Best policy for status arbitration in multi-proc finish scenarios.
- Preferred pattern for dispatch fusion (last-TG writes args vs split pass).
