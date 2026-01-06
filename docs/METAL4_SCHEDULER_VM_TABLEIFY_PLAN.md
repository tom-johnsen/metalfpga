# Scheduler VM Tableify Plan

Goal: shrink generated MSL and reduce compiler stress by replacing large
`switch` ladders with data-driven tables where feasible.

Scope (from `tmp/picorv32_sched_vm.msl`):
- Edge wait item switches: `switch (item_index)` / `switch (__gpga_edge_index)`
- Service fallback switch: `switch (service_id)`
- Delay id switch: `switch (delay_id)`
- Repeat count switch: `switch (__gpga_arg)` (repeat handling)
- Keep opcode dispatch (`switch (__gpga_op)`) as-is (VM interpreter)

## Step-by-step plan
1) Baseline capture
   - Re-emit MSL for picorv32 into `tmp/picorv32_sched_vm.msl`.
   - Record `rg -n "switch \\(" tmp/picorv32_sched_vm.msl` output for comparison.

2) Tableify edge wait items (highest ROI)
   - Add a compact table that maps edge item id -> {val_offset, xz_offset,
     width, mask, stride, flags} in scheduler VM tables.
   - Replace `switch (item_index)` and `switch (__gpga_edge_index)` in
     sched-vm edge wait evaluation with table lookups + generic load logic.
   - Ensure 2-state and 4-state paths both use the same table (XZ offsets
     may be zero/ignored for 2-state).
   - Validate: edge wait logic still uses the same item_kind and masks.

3) Tableify service fallbacks
   - Replace `switch (service_id)` fallback with a table of pre-baked service
     records (or a compact descriptor that can be expanded in a loop).
   - Keep the fast-path (non-fallback) behavior identical.
   - Validate: service ids match existing format/arg expectations.

4) Tableify delay ids
   - Replace `switch (delay_id)` with a delay table indexed by id.
   - Keep bounds checks and error handling identical.

5) Tableify repeat counts
   - Move `switch (__gpga_arg)` into a repeat-count table or encode count
     directly in bytecode.
   - Keep behavior for invalid ids (set error, stop) identical.

6) Re-emit and diff MSL
   - Regenerate `tmp/picorv32_sched_vm.msl`.
   - Confirm large `switch` ladders are gone or significantly smaller.
   - Track MSL size before/after each step.

7) Runtime validation
   - Run the small VCD tests (2-state and 4-state) to ensure correctness.
   - Optional: retry picorv32 run to confirm compiler stability.

## Notes
- Primary target is the edge wait item switches; they dominate the MSL size.
- Avoid changing bytecode semantics while tableifying (goal is same behavior,
  lower codegen size).
