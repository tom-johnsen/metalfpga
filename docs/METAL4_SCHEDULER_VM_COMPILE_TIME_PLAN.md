# Scheduler VM Compile-Time Stabilization Plan

Goal: reduce Metal compiler time/memory for large sched-vm kernels (picorv32
and larger) by shrinking MSL size and control-flow complexity without
reintroducing fallbacks.

This plan assumes the opcode coverage in
`docs/METAL4_SCHEDULER_VM_NEXT_OPCODES.md` is already in place. The focus here
is **compile-time stability** (LLVM/GVN/alias churn) rather than new opcode
coverage.

## Baseline Signals (record before/after each phase)
- MSL size (bytes) for the sched-vm kernel (picorv32).
- Metal compile time + time spent waiting in the async pipeline.
- `sched_vm_fallback_diag.txt` (should stay at 0 fallbacks).
- Sample snapshots (optional) for MTLCompilerService CPU + memory.

Suggested capture commands:
```
./build/metalfpga_cli goldentests/yosys-tests/bigsim/picorv32/rtl/picorv32.v \
  goldentests/yosys-tests/bigsim/picorv32/sim/testbench.v \
  --top testbench --4state --sched-vm --fallback-diag \
  --emit-msl tmp/picorv32_sched_vm.msl

wc -c tmp/picorv32_sched_vm.msl
```

## Phase 1: Shrink Control-Flow Surface Area
**Objective:** reduce large `switch`/`if` ladders and duplicated helper bodies
that cause LLVM to explode in alias analysis.

1.1 Consolidate duplicated sched-vm helpers
- Merge 2-state and 4-state helper bodies where possible; keep thin wrappers
  that only differ in xz handling.
- Remove near-duplicate helper bodies for blocking/nonblocking paths by
  factoring a shared inner routine.
- Success = fewer large functions in the generated MSL, identical behavior.

1.2 Collapse remaining id-based ladders
- Re-run a targeted scan on the emitted MSL:
  - `rg -n "switch \(" tmp/picorv32_sched_vm.msl`
  - `rg -n "if \(" tmp/picorv32_sched_vm.msl` (spot huge chains)
- Table-drive any remaining large id dispatches (except the main opcode
  dispatch).
- Success = biggest `switch` ladders gone or reduced to small ranges.
- Current scan: only the opcode dispatch switches remain; no other large
  ladders found in picorv32.

## Phase 2: Reduce Addressing/Alias Pressure
**Objective:** cut down on pointer arithmetic and GEP-heavy code that drives
LLVM alias analysis (GVN/MemoryDependence) into pathological cases.

2.0 Remove unused packed signal setup in VM helpers
- Drop `emit_packed_signal_setup`/`emit_packed_nb_setup`/`emit_packed_force_setup`
  from `sched_vm_eval_expr`, `sched_vm_exec`, and `sched_vm_exec_service_call`
  for both 2-state and 4-state variants.
- Keep packed setup in `sched_vm_exec_service_ret_assign` only when fallback
  entries exist.
- Re-emit MSL and compare size/async wait.
  - Status: no packed setup emitted in `sched_vm_eval_expr`/`sched_vm_exec`/
    `sched_vm_exec_service_call`; only `sched_vm_exec_service_ret_assign` emits
    packed setup when fallback entries exist, so no change needed.

2.1 Centralize packed-slot accessors
- Emit small inline accessors for value/xz loads and stores that take
  slot+word index rather than repeating pointer arithmetic everywhere.
- Prefer short helper functions to raw pointer math in-line.
- Success = fewer unique pointer expressions in MSL.
  - Status: added `gpga_sched_vm_load_word`/`gpga_sched_vm_store_word` and
    switched apply_assign/apply_delay, eval_expr loads, and force/release/service-ret
    writes to use them.

2.2 Normalize array/range writes
- Ensure `kAssignPart` paths always go through a single helper for range/bit
  updates (no per-site math).
- If required, use a compact mask/shift helper to avoid large expression trees.
  - Status: added `gpga_sched_vm_apply_bit`/`gpga_sched_vm_apply_range` and
    routed range/bit updates through them in apply_assign/apply_delay (2/4-state).

## Phase 3: Expression VM Hotspots
**Objective:** reduce expression VM control flow and repeated patterns that
inflate the compiled kernel.

3.1 Compact unary/binary operation paths
- Convert unary/binary eval paths to small, table-driven helpers where
  semantics are identical (e.g., per-op function tables or packed metadata
  with a single dispatch path).
- Keep signedness and width handling centralized.
- Status: added `gpga_sched_vm_unary_fs64`/`gpga_sched_vm_binary_fs64` and
  `gpga_sched_vm_unary_u64`/`gpga_sched_vm_binary_u64`, routed unary/binary
  eval paths through them (2/4-state). Re-emit MSL to confirm size drop.

3.2 Limit inline call expansion
- Keep complex expression helpers as `static` functions to avoid re-inlining
  across call sites.
- Ensure call patterns are uniform to help the compiler avoid cloning.
- Status: marked wide helper utilities + `gpga_sched_vm_sign64` as `noinline`
  for 4-state; reverted 2-state wide helpers to `inline` after Metal compile
  errors; fixed missing brace in 2-state index loads by using
  `gpga_sched_vm_load_word` (needs re-test on 2-state VCD run).
- Status: disabled unrolling for all emitted `for (uint ...)` loops via
  `#pragma clang loop unroll(disable)`; see
  `docs/METAL4_SCHEDULER_VM_UNROLL_PLAN.md`.
- Sample note: MTLCompilerService spends heavy time in LLVM `PHINode` removal
  and `CloneBasicBlock` (plus large memmove) with ~5GB footprint, pointing to
  CFG/PHI explosion. Prioritize unroll disabling and no-inline barriers.

## Phase 4: Sanity + Regression Gates
**Objective:** guard against regressions while tracking compile-time wins.

4.1 Regression checklist
- `--fallback-diag` stays at 0 for picorv32.
- 2-state + 4-state VCD tests still match gold.
- MSL size trending down; async pipeline wait trending down.
  - Status: `deprecated/verilog/test_clock_big_vcd.v` 2-state + 4-state VCDs OK.

4.2 Stretch target
- Run a larger testbench (if available) and repeat the above metrics to
  confirm scaling improvements.
  - Status: emitted sched-vm MSL for `issue_00940/TopEntity` (larger netlist).
    MSL size 9,693,206 bytes; fallback diag shows `rhs_unencodable: 28` in
    `tmp/issue_00940_sched_vm_fallback_diag.txt`.

## Tracking Table (fill as we go)
| Phase | MSL Size | Async Wait | Fallbacks | Notes |
|------:|---------:|-----------:|----------:|-------|
| Base  | 9,913,924 bytes | not run | 0 | Emitted via `tmp/picorv32_sched_vm.msl`. |
| 1     |          |            |           |       |
| 2     | 8,904,637 bytes | not run | 0 | Added bit/range helpers and routed assign/delay updates through them. |
| 3     | 8,902,686 bytes | not run | 0 | Unary/binary helpers + noinline wide helpers/sign64; re-emitted picorv32 MSL. |
| 3.3   | 8,904,374 bytes | not run | 0 | Added `#pragma clang loop unroll(disable)` for all emitted loops. |

## Notes
- This plan is intentionally about **compiler stress**, not new opcode
  coverage.
- If a phase increases MSL size or compile time, roll back that sub-step and
  try a smaller, more surgical change.
