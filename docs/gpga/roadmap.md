# GPGA Roadmap

## Milestones
1. Scope definition (versioned)
   - v0: Verilog-2001 subset (combinational + single-clock sequential)
   - v1: Add later Verilog features as needed
   - v2: SystemVerilog extensions (far later)
   - Define supported types (bit/vec widths, simple arrays)
   - Choose parser frontend (minimal first, later Yosys/Verilator if needed)
2. IR and front-end
   - AST for modules, ports, assigns, always blocks
   - Elaboration pass to flatten module instances
   - Lower to a simple IR (signals, ops, regs, memories)
3. Codegen
   - MSL kernels for combinational + tick-based sequential logic
   - Host-side Metal pipeline/buffer layout generator
4. Runtime + CLI
   - Metal dispatch wrapper
   - CLI for compile/run and test vectors
5. Testing
   - Golden tests for codegen output
   - Functional tests vs CPU reference for small circuits
