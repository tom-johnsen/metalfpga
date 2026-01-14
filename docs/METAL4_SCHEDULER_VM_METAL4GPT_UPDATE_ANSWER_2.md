I read through it end-to-end. The structure is good (correctness → time-jump → batching → multi-sim → tuning), and you’ve already captured most of the “Metal gotchas” that tend to bite (allocator thread-safety, residency, indirect TG sizing, etc.). Below are the main things I’d tighten up / add, plus concrete implementation patterns for the open questions.

---

## The biggest “missing lever”: reduce dispatch count per iteration

You already identified that **queue latency isn’t the limiter** and the GPU is spending time on **lots of tiny dispatches**. Phase C (multi-iteration per command buffer) reduces *CPU submission overhead*, but it **does not reduce GPU per-dispatch overhead** — you still pay the fixed cost for each dispatch and each barrier, just inside one command buffer.

Given your finding (“iteration count + tiny per-iteration work is the limiter”), the highest-leverage perf knob after time-jump is usually:

### Add a Phase between B and C: “dispatch fusion for ready-building”

Try to collapse steps 1–5 into **one** dispatch:

Current:

1. reset
2. wait_eval
3. ready_flags
4. compact
5. write indirect args
6. exec_ready

Proposed:
A) **build_ready** (reset + wait_eval + compact + args-write)
B) **exec_ready**

This cuts you from ~6 dispatches/iter to ~2 dispatches/iter.

#### How to do it safely without a grid-wide barrier

The key trick: “last threadgroup writes the args”.

* Use block-level compaction inside each TG:

  * each TG builds a small local list of ready proc indices in `threadgroup` memory
  * a single atomic reserves a contiguous range in the global ready list
  * TG writes its chunk contiguously → far less contention than “1 atomic per ready proc”
* Also have an atomic `tg_done_count`:

  * at the end of the kernel, one lane does `done = atomic_fetch_add(tg_done_count, 1)`
  * if `done == numThreadgroups-1`, this TG is “last” → it writes the indirect dispatch args using the final `ready_count`

That eliminates “ready_dispatch” as a separate pass and also removes the ready_flags pass entirely.

Even if you don’t do this immediately, I’d add it to the plan explicitly because it matches your observed bottleneck much better than TG sweeps.

---

## Phase A: correctness hardening tweaks (things I’d add/clarify)

### A1) Join correctness is more than “atomic decrement”

Your bullet is correct, but I’d explicitly add two invariants and one guard:

* **Invariant:** only the transition *WAIT_JOIN → READY* should occur exactly once.
* **Guard:** detect underflow (it’s a canary for double-finish / double-decrement).

Pattern:

* `old = atomic_fetch_sub(join_count, 1)`
* if `old == 1`: you are the last child → unblock parent
* if `old == 0`: underflow → set sim error / diagnostic flag (don’t silently wrap)

Also: make sure the parent doesn’t re-enter WAIT_JOIN without resetting join_count (or you can get “late child finish” decrementing a recycled join counter).

### A2) Deterministic arbitration: make it *order-independent*

If you implement arbitration with CAS loops, it can still be deterministic, but it’s easy to accidentally depend on “who wins the race.” Prefer a commutative update rule so the final state is independent of execution order.

A really solid rule is:

* define a total order over outcomes (priority + tie-breaker)
* always atomically update to the “max” (or “min”) under that order

I give a concrete encoding below in the “status arbitration” section.

### A3) Indirect args: add **MSL layout safety** (this is the one people miss)

Your plan has host-side `static_assert(sizeof(...))`. I’d add one more explicit note:

* In MSL, `uint3`/`int3`/`float3` are **often 16-byte sized/aligned**, not 12.
* If your shader writes a `uint3` into a buffer thinking it’s “3 u32”, you can accidentally change the stride/layout when you pack multiple structs or when you reinterpret memory.

So: **write indirect args using `uint32_t[3]` / `uint[3]` (or `packed_uint3`)**, not `uint3`.

This is *exactly* the kind of “3 u32 vs something else” mismatch that shows up as “indirect dispatch reads garbage” or “OOB access”.

### A4) Service ring: reservation pattern matters

Watermark checks are good, but you want an unambiguous “reserve then write” flow. If you do “check then write” without a reservation, multiple writers can pass the watermark check and still overflow.

Safer approach:

* `slot = atomic_fetch_add(tail, 1)`
* if `slot - head >= capacity`: overflow → set should_yield and *do not write* (or write into a known “dropped” slot)
* else: write record at `slot % capacity`

You can use a “soft watermark” to set should_yield *before* it becomes hard-full, but still keep the “hard check” correct.

### A5) Explicitly document memory-visibility assumptions

You already say “barriers enabled during bring-up.” I’d add:

* Whether you’re relying on **dispatch boundary** vs **encoder memoryBarrierWithScope** for visibility.
* Whether CPU reads ring counters only after command buffer completion (simplifies everything).
* Storage mode constraints (`shared` vs `managed`) for anything CPU reads mid-frame.

---

## Phase B: time-jump correctness rules (delta/edge/cond)

Your time-jump phase is exactly what I’d do next, and your gating condition is almost there. The thing to make explicit is the simulator semantics you are preserving, especially around **delta cycles** and **event/edge waits**.

### A robust time-jump rule of thumb

Time can advance from `t` to `t'` (the min future time) only if:

1. There is **no runnable work** at time `t`

   * `ready_count == 0` (per sim, once you’re multi-sim)

2. There is **no “same-time” pending work** that could become runnable without advancing time
   Examples:

   * delta-cycle waits
   * “edge/cond” waits that can be triggered by **already-scheduled** updates at time `t` (NBA/commit queues, etc.)
   * internal event queues scheduled at time `t`

3. The next timed event exists and is strictly in the future

   * `min_time > current_time`

If (1) holds but (2) doesn’t, you’re in “delta resolution” territory: you should keep iterating at the same time until quiescence, *not* jump time.

If (1) holds and (2) holds but there is no time wait (no min_time), you’re idle/deadlocked from the model’s perspective:

* either mark sim STOP/IDLE
* or yield to host if external stimuli may arrive

### Host input is the silent footgun

If the simulation consumes host-driven inputs that are scheduled in time, the safe version is:

* represent “next host input time” in GPU state (per sim)
* include it in `min_time`

If you don’t, your GPU can legally time-jump past a host input that *should* have occurred sooner.

### Implementation detail that helps later

When you build your “min wait time reduction”, also compute:

* `has_delta_or_event_wait`
* `has_timed_wait`
* optionally `min_time_is_host` / `min_time_source` (debugging)

That makes it easier to diagnose “why did we not jump?” vs “why did we jump?”.

---

## Phase C: multi-iteration batching reality check

Your approach (encode N iterations, use should_yield for early-outs) is the correct pattern in Metal because you can’t “stop encoding execution” once it’s on the GPU.

Two gotchas worth adding explicitly:

1. **Early-out still pays dispatch overhead.**
   If should_yield gets set in iteration 1 and you encoded 8 iterations, you’ll still run the remaining dispatches (they’ll just exit quickly). That’s fine for correctness and responsiveness, but you’ll want N small until you know the yield rate is low.

2. **Make should_yield monotonic for the batch.**
   Once set, never clear it until CPU drains/handles whatever caused the yield. Otherwise later kernels might run and write into a ring you intended to protect.

---

## Phase D: multi-sim independence (one thing to watch)

The plan says “track per-sim ready counts during compaction” and “allow time-jump per sim; do not stall busy sims.” That’s correct, but it implies a specific ready-list representation:

* **Global ready list** of work items: entries are `(sim_id, proc_id)`

  * exec-ready is a single dispatch over the global list (good: no per-sim dispatch overhead)
  * per-sim ready_count exists only for time-jump decisions / metrics

If instead you keep per-sim ready lists and try to exec them with indirect dispatch per sim, you’ll reintroduce the per-sim encoder/dispatch overhead you’re trying to avoid.

So I’d add one sentence making that architectural intent explicit.

---

## Phase E: tuning additions (what I’d add before TG sweeps)

### E0) “Don’t clear giant arrays” optimization (epoch/stamp)

If `sched_ready_reset` is clearing an array of flags every iteration, that can be a significant fixed cost when iterations are frequent.

Common trick:

* keep `uint ready_epoch[proc]`
* keep `uint global_epoch` (per sim)
* “proc is ready this iteration” iff `ready_epoch[proc] == global_epoch`
* reset is just `global_epoch++` (with wrap handling)

This can remove an O(proc_count) clear pass.

### E1) Exec-ready TG=64 limit: check if it’s self-inflicted

If the pipeline’s `maxTotalThreadsPerThreadgroup` is 64, that can happen either because:

* the kernel genuinely uses too many registers / threadgroup memory, **or**
* it was compiled/annotated with a limit (explicit or implicit)

It’s worth a one-time audit:

* look for `[[max_total_threads_per_threadgroup(... )]]`
* look at threadgroup memory usage and large local arrays
* check if you’re forcing extra resources via function constants / specializations

If it’s resource-driven, TG sweeps won’t fix it, but reducing per-thread state sometimes raises the max and can improve occupancy.

---

## Validation: a couple of high-value tests to add

You already have deterministic small tests + scripts. I’d add two “race-catchers”:

1. **Join storm test**
   Many children finishing in the same iteration and multiple parents waiting.

   * verifies join_count atomic/unblock exactly once
   * verifies no underflow and no double-ready

2. **Status collision test**
   Two procs attempt STOP and FINISH in same timestep.

   * verifies arbitration priority and tie-breaker is deterministic

Also add a log/metric that’s incredibly informative for this project:

* **dispatches per simulated timestep**
* **idle-iteration percentage**
* **ready_count histogram** (lots of zeros vs small bursts)

Those will tell you immediately if time-jump and fusion are working.

---

## Answers to your “Questions for Metal4GPT”

### 1) Time-jump correctness: delta/edge/cond interactions and idle semantics

A safe policy is:

* Define three wait classes per proc:

  1. **runnable now** (READY)
  2. **same-time resolvable** (DELTA / event/edge waits that might trigger at `t` due to already-scheduled updates at `t`)
  3. **future-time** (timed waits with target time > t)

Then:

* If class (1) non-empty → run exec_ready as normal.
* Else if class (2) non-empty → **do not time-jump**; continue delta iterations at the same `t` until (2) becomes empty or (1) appears.
* Else if class (3) non-empty → set `t = min(target_times)` and immediately re-run wait_eval (at the new time).
* Else → no internal work remains: mark sim IDLE/STOP (or yield to host if external events can arrive).

This preserves the key semantics: you never skip events that occur at the current time or current delta.

### 2) Indirect args struct layout (3 u32 vs 6 u32) in the current SDK

Rather than betting on 3 vs 6, the most robust approach is:

* On the **host**, compile-time check whatever type you’re using:

  * `static_assert(sizeof(MTLDispatchThreadsIndirectArguments) == expected)`
  * also check `alignof(...)`

* On the **GPU side**, avoid `uint3` and write using explicit 32-bit scalars/arrays to match the C layout:

  * `uint threadsPerGrid[3];`
  * or `packed_uint3` if you prefer vector syntax

* If you ever pack multiple indirect-arg structs in one buffer, *also* assert the stride you use matches what the CPU expects.

If you want an extra belt-and-suspenders check during bring-up:

* have the GPU also write a “debug copy” of the args into a `shared` buffer (or a small `shared` struct), so you can validate even when the real indirect buffer is `private`.

The important correctness point is not just “3 vs 6”, it’s “don’t let vector alignment/padding change what the GPU writes”.

### 3) Best policy for status arbitration in multi-proc finish scenarios

Best practice: encode a deterministic total order and use a single atomic update that is **order-independent**.

Example policy you stated:

* FINISH beats STOP
* ties broken deterministically (e.g., lowest proc_id wins)

You can implement this with one atomic word:

* Assign priorities: e.g. `STOP=1`, `FINISH=2` (higher wins)
* Pack a “key” so that:

  * higher priority → larger key
  * within same priority, smaller proc_id → larger key (so it wins under atomicMax)

One encoding:

* `key = (priority << 24) | (0xFFFFFF - proc_id)`

Then:

* `atomic_fetch_max(sim_status_key, key)`

At the end, decode:

* `priority = key >> 24`
* `winner_proc = 0xFFFFFF - (key & 0xFFFFFF)`

This yields deterministic results regardless of which thread “wins” the race because the operation is commutative and converges to the same max.

If you also need to store an exit code/value, you can either:

* store it in a separate atomic updated only by the “winner” (CAS loop after reading the final key), or
* pack more bits if your proc_id range allows it.

---

## If you only change three things next

1. **Time-jump (Phase B)** — you’re right it’s the highest leverage for picorv32 if ready_count is often zero.
2. Add a “dispatch fusion” phase to cut per-iteration dispatch count (even if it’s only: remove ready_flags + merge ready_dispatch).
3. Make arbitration and join_count updates order-independent and self-checking (underflow/overflow guards + deterministic atomic scheme).

That combination directly targets the two biggest risk axes you identified: correctness under parallelism and “too many tiny iterations”.

If you want, I can also sketch a concrete fused-kernel design for `build_ready` (including “last TG writes args”) in the style you’re using now (DispatchBatch + optional barriers), but the above should be enough to update the plan and unblock implementation.
