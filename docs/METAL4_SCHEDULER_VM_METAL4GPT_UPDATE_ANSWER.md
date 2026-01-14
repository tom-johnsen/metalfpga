Here’s my review of the **time‑jump plan**, **multi‑iteration per dispatch**, and **Metal 4 visibility/profiling**, with concrete “do this next” suggestions and a couple of correctness pitfalls that are very likely to bite once exec‑ready is truly parallel.

---

## 1) Time‑jump / next‑event advance

### Is “only jump when no ready procs and no delta waits” sufficient?

It’s **necessary**, but I’d tighten it into a rule set that matches how discrete‑event (HDL‑style) simulators avoid skipping observable behavior.

A safe time‑jump condition (per sim) is:

1. **No runnable work** at the current time:

   * `local_ready_count == 0` (or “no procs with `READY && wait_kind == NONE`”)
2. **No pending delta-cycle work**:

   * no proc is in `WAIT_DELTA` (or whatever your “delta” representation is)
3. **There exists a future time event** you can jump to:

   * at least one proc is in `WAIT_TIME` with `wait_time > sched_time[sim]`
4. **No host-injected/async event is pending** that could change readiness without time advance (optional but recommended depending on your architecture):

   * e.g. “service ring contains CPU→GPU injections” or some other external stimulus queue

If (1) and (2) are true but (3) is false (only edge/cond waits remain), you are in **deadlock / “waiting for external stimulus”** territory. You should not spin. Choose a policy:

* mark sim **STOPPED** (or a new status like **BLOCKED/IDLE**), or
* keep it running but return control to host (so host can inject stimuli), or
* treat it as a finish condition if that matches your tool’s semantics.

### Do you also need to guard on edge/cond wait presence?

You generally **do not need to prevent time‑jump just because edge/cond waits exist**, as long as you’re jumping to the **earliest future time event**.

Reasoning:

* Edge/cond waits only become true when signals/conditions change.
* With **no runnable procs** and **no delta work**, nothing will change until some timed wakeup occurs (or an external input arrives).
* Jumping directly to the minimum `WAIT_TIME` wake time does not skip any earlier work, because (by definition) there are no earlier timed wakeups.

So the important guard is not “no edge waits exist”, but “we are jumping to the **minimum** future time-wake (and we’re quiescent w.r.t. delta).”

### Corner case to explicitly handle

If your reduction finds `min_wait_time == sched_time[sim]` (or `<`), that indicates either:

* a bug (wait_eval should have unblocked), or
* your wait semantics are inclusive and you need a second wait_eval pass.

In time‑jump, I’d only consider **strictly greater** times:

* `candidate_time = wait_time` only if `wait_time > sched_time[sim]`

This avoids getting stuck “jumping to the same time.”

---

## 1.1) GPU reduction pattern for `min_wait_time` per sim

### Best practical pattern (no 64‑bit atomics required)

Use **one threadgroup per sim**, with threads striding over procs, then do a threadgroup reduction:

* **Grid**: `threadgroupsPerGrid = (count, 1, 1)`
* **Threads per group**: e.g. 128/256 (whatever is comfortable)
* Each thread scans `pid = tid; pid < PROC_COUNT; pid += TG`
* Reduce to a single `min_time` and also flags like `has_delta`, `has_time`

This avoids needing `atomic_min` on `ulong` (which is where portability/availability can get messy).

Pseudo‑MSL sketch:

```metal
kernel void sched_next_time(
    device const uint*  sched_wait_kind,
    device const ulong* sched_wait_time,
    device const uint*  sched_state,
    device const ulong* sched_time,
    device ulong*       out_next_time,   // per sim
    device uint*        out_can_jump,    // per sim (0/1)
    constant Sched&     sched,
    uint tid [[thread_index_in_threadgroup]],
    uint sim [[threadgroup_position_in_grid]],
    uint tgSize [[threads_per_threadgroup]]
) {
  if (sim >= sched.count) return;

  ulong cur = sched_time[sim];
  ulong local_min = ULONG_MAX;
  bool local_has_time = false;
  bool local_has_delta = false;

  for (uint pid = tid; pid < GPGA_SCHED_PROC_COUNT; pid += tgSize) {
    uint idx = sim * GPGA_SCHED_PROC_COUNT + pid;

    // ignore DONE, etc. as appropriate
    uint wk = sched_wait_kind[idx];

    if (wk == GPGA_SCHED_WAIT_DELTA) {
      local_has_delta = true;
    } else if (wk == GPGA_SCHED_WAIT_TIME) {
      ulong t = sched_wait_time[idx];
      if (t > cur && t < local_min) local_min = t;
      local_has_time = (t > cur) ? true : local_has_time;
    }
  }

  // Reduce local_min + flags across threadgroup (use threadgroup memory)
  // ...

  if (tid == 0) {
    bool can = (!has_delta && has_time && min_time != ULONG_MAX);
    out_can_jump[sim] = can ? 1u : 0u;
    out_next_time[sim] = can ? min_time : cur; // or keep old
  }
}
```

Then a second tiny kernel can apply the jump:

```metal
kernel void sched_apply_time_jump(
  device const uint*  can_jump,
  device const ulong* next_time,
  device ulong*       sched_time,
  constant Sched&     sched,
  uint sim [[thread_position_in_grid]]
) {
  if (sim >= sched.count) return;
  if (can_jump[sim]) sched_time[sim] = next_time[sim];
}
```

### Where to integrate it

You’ll get the most value if time‑jump happens **immediately after you discover no ready work**, without returning to CPU.

Given your current pipeline, that means:

* after `ready_compact` (you know `ready_count`)
* if `ready_count == 0` (or per‑sim none ready), run:

  * `sched_next_time`
  * `sched_apply_time_jump`
  * then **re-run wait_eval** (at the new time) and proceed

Even if this adds 1–2 extra dispatches in the “idle” case, it should *massively* reduce the number of idle iterations overall.

### Global vs per‑sim time‑jump

Right now your trigger is global (`ready_count==0` across all sims). For `count>1` throughput, you’ll eventually want **per‑sim** time‑jump:

* compute **per‑sim ready count** (or a “sim_has_ready” bit) during compaction
* jump only sims that are quiescent
* let other sims continue executing

This is a big win for multi‑sim because one “busy” sim won’t prevent all other sims from skipping dead time.

---

## 2) Multi‑iteration per dispatch (really: per command buffer submit)

### What’s feasible in Metal

A true “loop inside one kernel that runs the whole scheduler N ticks” runs into the classic problem: **no grid‑wide barrier** across threadgroups. Your current design relies on multi‑kernel phases + barriers; collapsing that into one kernel is hard unless you restructure to “one threadgroup per sim” for *everything* (which likely won’t scale for proc_count > TG size).

So the pragmatic win is:

* **Encode N scheduler iterations worth of dispatches into one command buffer/encoder** using your `DispatchBatch` machinery.

This reduces:

* command buffer submission rate (your ~308 commands/sec number)
* CPU overhead and CPU→GPU scheduling gaps
* time spent in per-command-buffer bookkeeping

### How to stop early (finished/stopped/service pressure)

Because the CPU cannot intervene mid‑command‑buffer, the usual pattern is:

* add a shared **“should_yield” flag per sim** (or global)
* kernels set it when:

  * sim finished/stopped
  * service ring is near full
  * error detected
* every kernel begins with:

  * `if (should_yield[sim]) return;`
  * (for per-proc kernels, compute sim and early-out)

This way, once a yield condition occurs, the remaining encoded iterations do **almost no work**, so the command buffer completes quickly and the CPU regains control.

### Service drain coordination: watermark + yield beats polling

Between “poll ring watermark in-device” vs “force periodic exit every N iterations”:

* You can’t rely on CPU draining during a single command buffer anyway.
* So the key is preventing ring overflow and keeping latency reasonable.

What I recommend:

1. **Give the ring a clear capacity margin**

* Decide a “safe watermark” below full, e.g. `ring_size - 256` records.

2. **Before writing each service record**, check capacity:

* If tail is about to collide with head (or hit watermark), do:

  * set `need_drain = 1`
  * stop emitting further records (or drop them safely)
  * optionally cause the current iteration to stop scheduling more work

3. In the multi-iteration mega-batch:

* at the end of each iteration (or in a tiny “iteration footer” kernel), if `need_drain` is set, set `should_yield` so subsequent iterations early-out.

This gives you **dynamic early exit**, but still stays within what Metal can do.

If you also want a simple bound, you can combine both:

* run up to `N` iterations per command buffer,
* but also yield early if `need_drain` trips.

### Tie-in with time-jump

Time-jump and multi-iteration are complementary, but time-jump should come first in priority when the system is idle.

Within your N-iteration mega-batch, the “iteration body” can do:

1. wait_eval
2. ready_flags + compact
3. if ready_count>0 → exec_ready
4. else if no delta + has future time → time-jump + (optionally) immediate wait_eval again
5. service bookkeeping / yield checks

That pattern collapses “idle time” very aggressively.

---

## 3) Indirect dispatch + fixed TG size pattern

### Your approach is reasonable, with two cautions

**Pattern:** “indirect grid size is GPU-written; threadgroup size is fixed by pipeline; host validates tg fields”

That’s reasonable, especially if you want:

* GPU-driven grid
* strict enforcement that TG size matches what the kernel/pipeline expects

**Caution A: confirm the indirect args struct size**
In your runtime you check:

```objc
sizeof(MTLDispatchThreadsIndirectArguments)
```

…but you also read 6 u32 (including `threadsPerThreadgroup`).

If Metal 4 truly defines that struct as 6 u32 for the GPUAddress-based API, you’re fine. If it’s still 3 u32 on your SDK, you’ve got an OOB read that could “work” by luck.

I’d add a compile-time guard:

```cpp
static_assert(sizeof(MTLDispatchThreadsIndirectArguments) == 6 * sizeof(uint32_t),
              "Unexpected MTLDispatchThreadsIndirectArguments layout");
```

(or adjust to whatever layout the SDK actually provides).

**Caution B: your CPU validation currently requires `contents()`**
In `DispatchBatch`, if `indirect_buffer->contents()` is null, you keep tg_x/tg_y/tg_z as 0 and then error out.

That will block you from using a **private** indirect args buffer later (which is often where you’ll want to end up for performance).

Two clean options:

* **Option 1 (keep validation only when readable):**

  * If `contents()==nullptr`, skip the tg read/validation and just dispatch. Let the driver/pipeline enforce requiredThreadsPerThreadgroup.

* **Option 2 (make tg fields GPU-written too):**

  * Have `sched_ready_dispatch` write `[3..5]` as well (from a constant like `sched.exec_ready_tg`).
  * Then the host doesn’t need to prefill anything and `sched_ready` can be private.

### Should you “stick strictly to 3 u32” indirect args?

If Metal 4’s API you’re using truly expects 6 u32 (threadsPerGrid + threadsPerThreadgroup), then you can’t stick to 3. If you *can* use the older form that takes `threadsPerThreadgroup` as a separate parameter, then yes, using 3 u32 is simpler and avoids storing TG in memory.

But given you’re already leveraging `requiredThreadsPerThreadgroup`, your current scheme is coherent—just make the struct/layout explicit and robust.

### For tuning 32 vs 64 vs 128: multiple pipelines or one?

* If you want to keep `requiredThreadsPerThreadgroup` (compile-time fixed TG), then **yes**, you need **one pipeline per TG size**.
* Practically, you don’t need to build all of them at once—building the single one selected by env at startup is fine for tuning runs.
* Runtime switching (within a single run) is only worth it if you have strong evidence that TG size should vary based on `ready_count` distribution. Given your bottleneck is “tiny work per iteration”, it’s unlikely to be the main win.

Also: your `exec_ready_tg=128` failure is a *pipeline* resource limit (`maxTotalThreadsPerThreadgroup` == 64). If you really want larger TGs, you’ll have to **reduce per-thread resource pressure** in that kernel (e.g., reduce register usage by simplifying/splitting work). But I agree with your conclusion: it’s not the key lever right now.

---

## 4) Profiling visibility in Instruments

Given what you observed, I’d treat **`metal-gpu-intervals` as the reliable ground truth** for your CLI right now.

Why the “application command buffer submissions” table might be empty for a CLI:

* it may be oriented around “app” identity (bundle-id/UI lifecycle), or
* it may only populate for the explicitly targeted process in the trace, or
* it may require additional debug metadata (labels, debug info)

Concrete steps that often improve visibility and attribution:

1. **Ensure you target the process explicitly when recording**

* If you’re using `xctrace`, prefer launching the CLI as the target from the trace command (not “system-wide only”), so Instruments treats it as the “app under test”.

2. **Label everything**

* `commandQueue.label`
* `commandBuffer.label`
* `computeEncoder.label`
* pipeline state labels (if you have them)

This won’t necessarily make the table appear, but it makes the GPU interval rows much easier to interpret.

3. **Add signposts around “iteration submit”**

* Put an `os_signpost` begin/end around each command buffer commit + wait.
* Then you can correlate CPU cadence with GPU intervals even if the “app table” is missing.

4. **Use `MTLCaptureScope` for a bounded capture**

* Programmatically create a capture scope around a few iterations (or use your script automation to do it).
* Even when Instruments tables are sparse, capture scopes often improve what shows up in Metal-related UI.

If after doing (1) you still don’t see submissions in the “app table” but do see them in `metal-gpu-intervals`, I’d stop fighting it and treat `metal-gpu-intervals` as the correct source for queue/occupancy analysis—because it clearly is capturing your workload.

---

## Extra: two correctness pitfalls to prioritize (because exec-ready is now parallel)

You called out correctness divergence; two specific places in your `sched_exec_ready` excerpt are high-risk in parallel:

### A) Join/unblock update is non-atomic

This block is racy when multiple children finish in the same exec-ready wave:

```metal
if (sched_join_count[pidx] > 0u) {
  sched_join_count[pidx] -= 1u;
}
if (sched_join_count[pidx] == 0u) {
  sched_wait_kind[pidx] = NONE;
  sched_state[pidx] = READY;
}
```

If two children run concurrently:

* both can read `join_count==1` and both decrement → underflow / missed “last child” logic / double unblock, etc.

Fix pattern:

* make `sched_join_count` an atomic (or use an atomic view)
* do `old = atomic_fetch_sub(...)`
* if `old == 1`, this thread was the last → it performs the unblock

### B) Any shared “halt/status” writes can race

If multiple procs can trigger finish/stop, you’ll want a deterministic policy.
Even if you don’t fully solve determinism yet, use atomics or a “finish beats stop” rule implemented with compare/exchange so you don’t end up with random final status.

---

## What I would do next (tight, high impact)

1. Implement **time-jump** (global first, per-sim later), using **one threadgroup per sim reduction** (no 64-bit atomics).
2. Add a small **deadlock/idle detection**: no ready, no delta, no time waits → stop/idle.
3. Implement **multi-iteration per command buffer** with:

   * `N` iteration unroll
   * `need_drain` + `should_yield` early-outs
4. Fix **join_count atomicity** before trusting correctness comparisons.
5. Make indirect dispatch layout bulletproof with a **static_assert** on the indirect argument struct size, and loosen CPU validation so private buffers remain an option.

If you want, paste (or point me at) how you represent `WAIT_DELTA`, how time waits are stored (`sched_wait_time` semantics), and how signals/edges are evaluated in `sched_wait_eval`, and I can suggest an even tighter “no-delta” gate and whether you can fold the min-time reduction into an existing kernel without another dispatch.
