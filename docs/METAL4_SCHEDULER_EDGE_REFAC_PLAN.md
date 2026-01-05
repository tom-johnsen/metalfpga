# Metal4 Scheduler Edge-Wait Refactor Plan

## Goal
Reduce Metal compiler blowups by replacing giant `switch (sched_wait_id)` blocks
with a data-driven edge-wait evaluator (1:n). This shrinks the scheduler kernel
CFG while preserving exact semantics.

## Why This Exists
The current scheduler emits multiple large `switch (sched_wait_id)` blocks
inside `gpga_*_sched_step`, with many cases that differ only by:
- which edge index/offset is used
- which edge kind (any/posedge/negedge) is applied
- which signals participate in edge-star groups

This creates a huge CFG and triggers LLVM PHI/CloneBasicBlock stress in
`MTLCompilerService` (as seen in the samples).

## High-Level Strategy (1:n)
Replace per-wait-case branching with a single loop:
- Store metadata arrays for each edge wait (offset + count).
- Store per-item metadata (edge kind + masks) for each edge signal entry.
- Evaluate edges in a generic helper that loops over metadata.

This makes edge evaluation scale with data size, not with duplicated code.

## Step-by-Step Plan

### Step 1: Inventory current edge-wait emission
Find the four `switch (sched_wait_id)` emission sites in:
`src/codegen/msl_codegen.cc` (active/NBA loops).
Capture the data currently embedded per case:
- `item_offset`
- `items.size()`
- per-item edge kind (any/posedge/negedge)
- star offsets and star signal lists

### Step 2: Define metadata arrays (MSL constants)
Emit constant arrays into the generated MSL:
- `sched_edge_wait_item_offset[]`
- `sched_edge_wait_item_count[]`
- `sched_edge_item_kind[]` (enum per item)
- `sched_edge_wait_star_offset[]`
- `sched_edge_wait_star_count[]`

Optional (if useful): precompute masks per item to avoid re-emitting the
bitmask constant in each loop.
Prefer compact types (`ushort` for counts/offsets, `uchar` for kinds) and
pack flags to keep constant data size small. If tables get large, use a hybrid:
small per-wait directories as constants and the bulk per-item data in a
read-only buffer (avoid `setBytes` beyond ~4 KB).

### Step 3: Emit a generic edge-wait helper
Generate a helper function inside the MSL kernel file, e.g.:
`bool gpga_eval_edge_wait(...)`
Inputs:
- `gid`, `edge_kind`, `wait_id`
- `sched_edge_prev_val/xz`, `sched_edge_star_prev_val/xz`
- value/xz sources for edge expressions
Behavior:
- iterate over item range for `wait_id`
- compute current val/xz for each item
- update `sched_edge_prev_*`
- compute readiness based on edge_kind
- do the same for star lists

Add loop hints to discourage unrolling/vectorization, e.g.:
`#pragma clang loop unroll(disable)` (and optionally `vectorize(disable)`)
on the per-item loops. Treat these as hints, not guarantees.
Also keep trip counts runtime-derived (from metadata arrays), not compile-time
literals, so the compiler is less likely to unroll.

### Step 4: Replace switch blocks with helper calls
In both the ACTIVE and NBA scheduling loops:
- remove `switch (sched_wait_id[idx])` blocks
- call the helper once per blocked process
- handle `ready` exactly as before
Make sure the helper is shared by ACTIVE and NBA phases (no duplicated logic).

### Step 5: Validation
1) Regenerate MSL for picorv32:
   - confirm `switch (sched_wait_id)` is gone or reduced to a tiny stub
2) Run regression tests:
   - `deprecated/verilog/pass/test_system_monitor.v`
   - `test_clock_big_vcd` (diff VCD)
3) Re-run picorv32 compile and observe:
   - progress/trace logs should move past `gpga_*_sched_step`
   - compiler memory footprint should stabilize
4) Compare compile time and metallib size before/after the change (offline
   compile if possible) to catch any regressions early.

## Rollout and Safety
- Keep old behavior behind a build-time or flag if needed for bisecting.
- If the helper changes semantics for any edge type, revert to
  edge-kind-specific logic only inside the helper (not a switch per wait id).

## Risks / Watch-outs
- Make sure edge-star (`@*`) waits preserve per-signal prev tracking.
- Ensure `sched_edge_prev_*` updates happen in the same phase as today.
- Avoid generating helper code that reintroduces large switch logic.
- Beware of unintended loop unrolling bringing back CFG bloat.
- If compilation remains pathological after the edge refactor, the next likely
  target is the largest `switch(pc)` blocks.

## Success Criteria
- `gpga_*_sched_step` compiles without long stalls in MTLCompilerService.
- All existing regression VCDs match pre-refactor output.
- Runtime performance is not regressed (preferably improved).
