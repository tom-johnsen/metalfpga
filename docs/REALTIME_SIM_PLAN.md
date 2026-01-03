# Real-Time Simulation Plan (Run vs Sim)

Goal: keep the current fast, correct event engine (`--run`), and add a
deadline-aware real-time mode (`--sim`) suitable for demos like an NES core.

## Current behavior (baseline)
- `--run` executes as fast as possible, preserving event semantics.
- Scheduler path advances based on `sched.max_steps` and `sched.max_proc_steps`
  per dispatch. Host drains service records between dispatches.
- Host waits for every dispatch; there is no real-time pacing.

This is correct and fast, but it is not time-aware.

## Proposed modes
### `--run` (default)
- "Correct and fast." No real-time pacing.
- Deterministic results from event engine and scheduler.
- Best for verification and golden outputs.

### `--sim`
- "Correct and time-aware." Maintain a real-time wall-clock target.
- If the sim can run faster than real-time, it throttles to match deadlines.
- If the sim falls behind, it keeps correctness and reports drift.

## CLI shape (proposal)
Common flags (existing):
- `--max-steps`, `--max-proc-steps`, `--service-capacity`, `--profile`

New flags (sim mode):
- `--sim`: enable real-time pacing.
- `--sim-rate-hz <Hz>`: target clock rate for 1:1 pacing (NES ~1.789773 MHz).
- `--sim-speed <float>`: 1.0 = real-time, >1 = faster-than-real-time (cap).
- `--sim-max-speed <float>`: upper bound for catch-up (default 2.0–4.0).
- `--sim-headroom-ms <ms>`: extra budget to hide jitter (default 2–5ms).
- `--sim-service-interval-ms <ms>`: service drain cadence (default 1–5ms).
- `--sim-warn-ms <ms>`: drift threshold to log warnings.

If a design has no scheduler (no `sched_time`), `--sim` warns and
falls back to `--run` unless explicitly forced.

## Required plumbing
### 1) Timebase and sim-time mapping
- Use `sched_time` as the authoritative simulation time for scheduler designs.
- Map `sched_time` to seconds using the design timescale.
- Expose a helper to convert `sched_time` deltas to wall-clock durations.

Notes:
- `sched_time` is already a buffer (per-instance).
- Timescale parsing may live in frontend; if unavailable, allow explicit
  `--sim-timescale` (e.g. `1ns`) or default to `1ns`.

### 2) Batch control for scheduler
- Drive `sched.max_steps` dynamically each dispatch based on budget:
  - `target_sim_time = start_sim_time + (wall_elapsed * speed) + headroom`
  - `backlog = target_sim_time - current_sim_time`
  - `steps = clamp(backlog_to_steps(backlog), min=1, max=cap)`
- Keep `max_proc_steps` as a per-process safety limit.

This turns the existing scheduler into a "run N steps per dispatch"
engine without changing event semantics.

### 3) Batch control for non-scheduler kernels
For designs without the scheduler:
- Add a `gpga_<top>_tick_loop` kernel that runs N ticks per dispatch
  (or an in-kernel loop around tick/comb).
- Host selects loop kernel in `--sim` to amortize dispatch overhead.

If this is too invasive initially, restrict `--sim` to scheduler-only designs.

### 4) Real-time pacing loop (host)
- Introduce a `SimController` that owns:
  - `start_wall_time`, `start_sim_time`
  - target rate, speed cap, headroom, drift thresholds
  - dispatch batch sizing policy
- After each dispatch, read `sched_time` and compute:
  - `drift = current_sim_time - target_sim_time`
  - `sleep` if ahead, `increase batch` if behind.
- Log drift stats periodically.

### 5) Service record decimation
- Drain service records on a cadence (`--sim-service-interval-ms`),
  not every dispatch.
- Optionally disable heavy services by default in `--sim`:
  - VCD
  - verbose `$display` spam
- Keep a safe path for `$finish`/`$stop` (always drain those).

### 6) Real-time I/O buffering
For demo workloads (NES):
- Introduce ring buffers for audio/video (host side).
- Surface inputs at fixed intervals (e.g. 60 Hz video, audio buffer size).
- Optional frame-skipping if the sim falls behind. (must be configurable)

This avoids CPU stalls from synchronous I/O.

## Implementation steps
1) Add `--sim` flag and parse sim options in `src/main.mm` and
   `src/codegen/host_codegen.mm`.
2) Add `SimController` in runtime (or host helper) with:
   - timebase mapping
   - batch sizing
   - pacing/sleep policy
3) Wire scheduler loop to dynamic `max_steps` and service drain cadence.
4) Add `sched_time` readback plumbing (already present in buffers).
5) Optional: introduce a tick-loop kernel for non-scheduler designs.
6) Add logging of drift + effective sim speed.

## Validation checklist
- `--run` produces identical outputs to pre-change baseline.
- `--sim` with speed=1.0 holds real-time for lightweight tests.
- Drift warnings appear when the sim cannot keep up.
- GPU dispatch count drops (batches are larger).
- NES demo: stable 60 Hz video and glitch-free audio buffering.

## Notes on correctness
`--sim` must never skip events or reorder scheduling. If the sim is too slow,
it should fall behind real-time rather than violate event semantics. Frame
skipping is acceptable for rendering, but not for core logic.
