# Scheduler VM Resume Notes

This captures the current state of the Scheduler VM work so we can restart VSCode
and pick up without re-reading prior threads.

## Current Problem
- Picorv32 sched-vm run completes immediately (no $display/$finish output, no VCD)
  even with `+vcd` and `--vcd-dir`.
- `--run-verbose` shows all procs go `done` with `blocked=0` and waits all zero.
- Status transitions to `status=2` (FINISHED) and `phase=1` with `time=0`.

## Last Known Good
- Smaller VCD tests pass in sched-vm mode (2-state/4-state).
- `$display` and `$monitor` tests now work:
  - `deprecated/verilog/pass/test_system_display.v`
  - `deprecated/verilog/pass/test_system_monitor.v`
- MSL size reduced to ~1.06 MB for picorv32.

## Most Recent Commands
Run (picorv32):
```
./build/metalfpga_cli goldentests/yosys-tests/bigsim/picorv32/rtl/picorv32.v \
  goldentests/yosys-tests/bigsim/picorv32/sim/testbench.v \
  --top testbench --4state --sched-vm --run --run-verbose --fallback-diag \
  --vcd-dir artifacts/vcd/pico +vcd \
  +firmware=goldentests/yosys-tests/bigsim/picorv32/sim/firmware.hex
```

MSL emit (with firmware plusarg):
- `tmp/picorv32_sched_vm_plusarg.msl`

## Observed Runtime Pattern
- Metal compile is fast (single-digit ms).
- Scheduler kernel launches and iterates ~45 times.
- `ready` count decreases to 0; `done` rises to 45; `blocked` remains 0.
- `waits=[none:45 ...]` throughout, even though proc first-op stats include `wait_edge`.
- `sched-vm: proc first-op counts` shows many `wait_edge` first ops.
- `sched-vm: watch ...` lines show wait ops by pid, but state remains `ready/done`.

## Working Hypothesis
Wait ops are not blocking because the GPU likely is not reading the intended
bytecode/VM tables (e.g., sees `kDone` instead of `kWaitEdge`), or the
`sched_vm_args` binding is incorrect so the VM reads zeroed buffers.

## Next Steps (Priority Order)
1) Verify `sched_vm_args` binding correctness.
   - Check `BuildSchedulerVmArgBuffer` in `src/main.mm` and
     `GpgaSchedVmArgs` in `src/codegen/msl_codegen.cc` (ids 0..21).
   - Confirm kernel buffer index for `sched_vm_args` matches binding index.
2) Confirm what opcode the GPU sees at runtime.
   - Add a small debug readback (GPU writes out a few opcode words at
     `bytecode[base + ip]` for a couple pids) and print on host.
   - Compare with host-side `sched_vm_bytecode` contents.
3) Check `sched.max_steps` and dumpvars path.
   - `RunMetal` clamps `max_steps=1` when dumpvars is used; still should block,
     but confirm this is not masking the issue.
4) If opcodes are correct, inspect wait-edge setup:
   - Ensure `gpga_sched_edge_wait_*` tables are non-empty and match layout.
   - Verify `GPGA_SCHED_EDGE_WAIT_COUNT` is non-zero in the MSL.

## Files to Resume
- `src/main.mm` (run-verbose status, VM buffers, arg buffer creation)
- `src/codegen/msl_codegen.cc` (scheduler VM exec/wait op emission, GpgaSchedVmArgs)
- `include/gpga_sched.h` (status/phase enums)
- `tmp/picorv32_sched_vm_plusarg.msl` (current MSL reference)
- `sched_vm_fallback_diag.txt` (latest fallback diagnostics)

## Recent Debug Additions
- Pipeline diag prints every 5s (shows top funcs, lines, loops).
- VM watch output for select pids (wait_time / wait_edge / service).

