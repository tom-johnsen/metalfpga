# GPGA Overview

## Goal
Build a Metal-backed "GPGA" prototype that translates a small Verilog subset
into MSL compute kernels plus a host-side runtime to execute them.

## Pipeline
1. Verilog (v0 subset)
2. AST (modules, ports, assigns, always blocks)
3. Elaboration (flatten module hierarchy + debug name map)
4. IR (signals, ops, regs, memories)
5. Codegen
   - MSL kernels (combinational + tick-based sequential)
   - Host-side Metal pipeline/buffer layout
6. Runtime + CLI
   - Dispatch kernels and run test vectors

## Versioned Scope
- v0: Verilog-2001 subset (combinational + single-clock sequential)
- v1: Later Verilog features as needed (generate, params, richer ops)
- v2: SystemVerilog extensions (far later)

## Components
- Frontend: parse Verilog to AST
- IR: lower AST to a compact, explicit graph
- Codegen: emit MSL + host wiring
- Runtime: Metal dispatch and buffer management
- CLI: compile/run flows and test vectors
- Testing: golden codegen outputs + functional comparisons

See `PLAN.md` for the working milestone list.
See `docs/gpga/verilog_words.md` for keyword coverage status.
See `docs/gpga/ir_invariants.md` for flattened netlist guarantees.
