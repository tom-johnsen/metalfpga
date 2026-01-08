# Scheduler VM Ternary Reduction Plan

Goal: cut Metal compile time for `--sched-vm` by shrinking the worst `?:`
surface area without reintroducing fallbacks. This plan covers two options
and the extra diagnostics/guardrails that come along for the ride.

Context snapshot (from recent pipeline diag):
- `gpga_testbench_sched_step`: ~8.6k lines, ~17k ternaries.
- `gpga_testbench`: ~3.5k lines, ~8.7k ternaries.
- Kernel compile stalls in async pipeline despite MSL size 1,045,938 bytes.
- `+firmware=...` does not change the emitted MSL (byte-identical, confirmed).

## Baseline Capture (before/after each option)
- Emit MSL (with and without plusarg):
```
./build/metalfpga_cli goldentests/yosys-tests/bigsim/picorv32/rtl/picorv32.v \
  goldentests/yosys-tests/bigsim/picorv32/sim/testbench.v \
  --top testbench --4state --sched-vm --fallback-diag \
  --emit-msl tmp/picorv32_sched_vm.msl

./build/metalfpga_cli goldentests/yosys-tests/bigsim/picorv32/rtl/picorv32.v \
  goldentests/yosys-tests/bigsim/picorv32/sim/testbench.v \
  --top testbench --4state --sched-vm --fallback-diag \
  --emit-msl tmp/picorv32_sched_vm_plusarg.msl \
  +firmware=goldentests/yosys-tests/bigsim/picorv32/sim/firmware.hex
```
- Size + diff:
```
wc -c tmp/picorv32_sched_vm*.msl
cmp -s tmp/picorv32_sched_vm.msl tmp/picorv32_sched_vm_plusarg.msl
```
- Runtime compile wait time (async pipeline) from the CLI run.
- `sched_vm_fallback_diag.txt` should remain at 0 fallbacks.

## Option 1: Skip Non-VM Kernel When No Fallbacks
**Objective:** remove `gpga_testbench` from the emitted MSL when sched-vm has
no CallGroup fallbacks, eliminating ~8.7k ternaries in one step.

1. Detect "no fallback kernel needed"
- Use existing CallGroup fallback tracking (layout or codegen) to decide
  if `gpga_testbench` is required.
- Gate the fallback kernel emission on that flag.

2. Codegen changes
- In `--sched-vm` mode: emit only the sched-vm kernel when fallback count=0.
- Keep helper functions referenced by `gpga_testbench_sched_step` intact.
- Ensure no host-side references to the removed kernel remain.
- Status: completed; codegen gates `gpga_<module>` emission based on
  `VmLayoutNeedsCallGroup` (see `src/codegen/msl_codegen.cc`).

3. Runtime dispatch changes
- If fallback kernel is absent, dispatch only the sched-vm kernel.
- Guard any paths that allocate fallback buffers or rely on fallback
  constants.
- Status: completed; runtime validates fallback kernel presence against VM layout
  CallGroup usage and keeps sched-vm-only dispatch when fallback is absent.

4. Validation
- MSL size drops and ternary counts for `gpga_testbench` disappear.
- VCD tests still pass (2-state + 4-state).
- `--fallback-diag` stays at 0.

**Risk:** hidden fallback cases. Mitigation: keep a build-time or CLI flag to
force emitting the fallback kernel if needed.

## Option 2: Reduce Ternaries Inside `gpga_testbench_sched_step`
**Objective:** shrink `?:` count inside the sched-vm kernel while preserving
VM behavior.

1. Identify the worst ternary emitters
- Use pipeline diag `top_funcs` and MSL scan to locate the dominant
  ternary-heavy blocks.
- Focus on `gpga_testbench_sched_step` first.

2. Replace ternary-heavy patterns with helpers/tables
- Move repeated ternary selections into small helper functions.
- Table-drive common select patterns (bit/range updates, value/xz choice).
- Prefer data tables over expanded `?:` trees.
- Status: in progress; logical `!`, `&&`, `||` for <=64-bit 4-state now call
  `fs_log_not32/64`, `fs_log_and32/64`, `fs_log_or32/64` (avoids inline ternaries).

3. No-inline barriers
- Keep the helper functions `static` and `noinline` where they reduce
  inlining/clone pressure.
- Avoid per-call-site cloning of large ternary blocks.
- Status: applied `noinline` to sched-vm wide helpers
  (`gpga_sched_vm_wide_mask_bits/value/any/eq/red_xor/extend`).

4. Validation
- Ternary counts drop in `gpga_testbench_sched_step`.
- Async pipeline wait time improves or becomes consistent.
- VCDs + fallback diag still clean.

**Risk:** helper overhead or missed inlining constraints. Mitigation: keep
helpers small and central; test both 2-state and 4-state paths.

## Extras That Tag Along (recommended regardless of option)
1. Pipeline diag pulse
- Keep 5s pulse output during async waits.
- Keep `top_funcs` in diag to point at ternary hot spots.

2. MSL stats sanity
- Track `?:` counts and line counts for top functions before/after each
  change (use the pipeline diag output).

3. Regression gates
- `--fallback-diag` stays at 0.
- 2-state + 4-state VCD tests match gold.
- `--sched-vm` runtime does not request fallback buffers when not emitted.

## Tracking Table
| Step | MSL Size | Async Wait | Fallbacks | Notes |
|-----:|---------:|-----------:|----------:|-------|
| Base | 8,904,374 bytes | long | 0 | Pre-Option 1 baseline. |
| Opt1 | 1,045,938 bytes | pending | 0 | Fallback kernel skipped; plusarg emit identical. |
| Opt2 | 1,045,938 bytes | pending | 0 | Logical op helpers added; measure ternary drop + wait time. |

## Decision Gate
- If Option 1 yields a major wait-time drop and no regressions, keep it and
  proceed to Option 2 only if needed.
- If Option 1 is blocked by fallback requirements, prioritize Option 2.
