# Scheduler VM Unroll Audit Plan (picorv32 MSL)

Goal: reduce Metal compile time by explicitly disabling loop unrolling in
hot/large loops that the compiler might otherwise try to expand. This is a
read-only audit + plan based on `tmp/picorv32_sched_vm.msl` (re-emitted with
`--sched-vm --fallback-diag --emit-msl`).

## Snapshot Summary
- Total `for` loops: 57
- Loops with `#pragma clang loop unroll(disable)`: 11
- Loops without pragma: 46
- Unpragmatized loop bounds observed:
  - `bit < 1u` (18 loops)
  - `bit < 32u` (6 loops)
  - `i < 36u` (3 loops)
  - `i < 16384u` (3 loops)
  - `pid < GPGA_SCHED_PROC_COUNT` (8 loops)
  - `e < GPGA_SCHED_EDGE_COUNT` (1 loop)
  - `c < GPGA_SCHED_VM_COND_COUNT` (1 loop)
  - `r < GPGA_SCHED_REPEAT_COUNT` (1 loop)
  - `w < (GPGA_SCHED_VM_CALL_FRAME_WORDS * GPGA_SCHED_VM_CALL_FRAME_DEPTH)` (1 loop)
  - `i < GPGA_SCHED_VM_EXPR_WIDE_WORDS` (3 loops)
  - `__gpga_entry < __gpga_header.entry_count` (1 loop)

## Existing Unroll-Disabled Loops (Keep)
These already have `#pragma clang loop unroll(disable)` and should remain:
- Edge/star scan loops in scheduler: `tmp/picorv32_sched_vm.msl:10603`,
  `tmp/picorv32_sched_vm.msl:12630`
- Wide word loops in cond eval: `tmp/picorv32_sched_vm.msl:13333`,
  `tmp/picorv32_sched_vm.msl:13515`,
  `tmp/picorv32_sched_vm.msl:15365`
- Service arg/word loops: `tmp/picorv32_sched_vm.msl:16210`,
  `tmp/picorv32_sched_vm.msl:16280`,
  `tmp/picorv32_sched_vm.msl:16337`,
  `tmp/picorv32_sched_vm.msl:16349`
- Fork/edge list loops in exec: `tmp/picorv32_sched_vm.msl:16841`,
  `tmp/picorv32_sched_vm.msl:17028`

## Candidates to Add `unroll(disable)`
### A) Driver-resolution loops (`bit < 1u` / `bit < 32u`)
These appear in both the combinational kernel and sched-step kernel:
- `tmp/picorv32_sched_vm.msl:2693`, `2754`, `2815`, `2876`, `2963`, `3014`,
  `3065`, `3116` (gpga_testbench)
- `tmp/picorv32_sched_vm.msl:9444`, `9505`, `9566`, `9627`, `9714`, `9765`,
  `9816`, `9867`, `11499`, `11560`, `11621`, `11682`, `11769`, `11820`,
  `11871`, `11922` (gpga_testbench_sched_step)

Rationale: these loops are small but occur many times; `unroll(disable)`
removes any risk of compiler expansion into a giant IR soup.

### B) Array init / NBA copy / NBA commit loops
Large fixed-size loops, likely the biggest compile-time stressors:
- Memory init: `tmp/picorv32_sched_vm.msl:8054` (`i < 16384u`)
- Regfile init: `tmp/picorv32_sched_vm.msl:8159` (`i < 36u`)
- NBA buffer init: `tmp/picorv32_sched_vm.msl:9065` + `9069`
- NBA commit: `tmp/picorv32_sched_vm.msl:11121` + `11125`

Rationale: 16K-element loops should never unroll.

### C) Scheduler state initialization loops
These are constant-count loops over scheduler tables:
- `tmp/picorv32_sched_vm.msl:8574` (`GPGA_SCHED_EDGE_COUNT`)
- `tmp/picorv32_sched_vm.msl:8579` (`GPGA_SCHED_PROC_COUNT`)
- `tmp/picorv32_sched_vm.msl:8596` (`GPGA_SCHED_VM_COND_COUNT`)
- `tmp/picorv32_sched_vm.msl:8601` + `8604` (call frame init)
- `tmp/picorv32_sched_vm.msl:8608` (`GPGA_SCHED_REPEAT_COUNT`)

Rationale: these look harmless, but they expand with large designs.

### D) Proc/loop scans in sched-step
Multiple `pid < GPGA_SCHED_PROC_COUNT` loops without unroll pragmas:
- `tmp/picorv32_sched_vm.msl:10552`, `10579`, `12608`,
  `12721`, `12735`, `12748`

Rationale: avoid unrolling by explicit pragma to keep IR compact.

### E) Wide-expression word loops (sched-step)
`GPGA_SCHED_VM_EXPR_WIDE_WORDS` loops without pragma:
- `tmp/picorv32_sched_vm.msl:12782`, `12834`, `12859`

Rationale: keep wide-word handling as real loops (no unroll).

### F) Case entry scans (cond eval)
Dynamic entry count loop without pragma:
- `tmp/picorv32_sched_vm.msl:15618`

Rationale: `entry_count` can be large; disable unroll to prevent runaway IR.

## Implementation Plan (Codegen)
1. Add a small helper in `src/codegen/msl_codegen.cc` to emit
   `#pragma clang loop unroll(disable)` consistently (if not already done).
2. Prepend the pragma to:
   - All driver-resolution loops (`for (uint bit = 0u; bit < …; ++bit)`)
     emitted in the net resolution helpers. Search anchors:
     `src/codegen/msl_codegen.cc:15540`, `15711`, `16734`, `16964`,
     `31596`, `31714`, `32282`, `32441`.
   - All large array init/copy loops emitted from net array setup /
     NBA copy / NBA commit. Search anchors:
     `src/codegen/msl_codegen.cc:17124`, `17597`, `18110`,
     `20758`, `21815`, `25421`, `32865`, `36805`, `40026`.
   - Scheduler init loops over `GPGA_SCHED_*` counts in the sched-step kernel.
     Search anchors:
     `src/codegen/msl_codegen.cc:20787`, `20837`, `20855`, `20862`,
     `20871`, `25227`, `25486`, `25647`, `25674`, `25687`.
   - Wide word loops in sched-step (GPGA_SCHED_VM_EXPR_WIDE_WORDS):
     `src/codegen/msl_codegen.cc:25755`, `25818`, `40343`, `40403`.
   - Case entry scans in `*_sched_vm_eval_cond` loops.
3. Re-emit picorv32 MSL and re-run:
   - `rg -n \"#pragma clang loop unroll\" tmp/picorv32_sched_vm.msl`
   - Loop-count scan to verify most loops are now guarded.
4. If compile time improves, keep; otherwise remove the least impactful
   pragmas (start with `bit < 1u` loops).

## Verification
- MSL size should remain roughly unchanged.
- VCD tests should stay correct (2-state + 4-state).
- Compiler samples: watch for reduced CPU time in `MTLCompilerService`.

## Status
- Implemented `#pragma clang loop unroll(disable)` for all emitted `for (uint …)`
  loops in `src/codegen/msl_codegen.cc` (scheduler init, wait loops, array
  copies, wide-word helpers, and case entry scans).
- Audit script shows 0 missing unroll pragmas for `out << "…for (uint …)"`
  loops in the codegen output.
