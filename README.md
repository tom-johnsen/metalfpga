# metalfpga

Prototype Verilog-to-Metal (MSL) compiler and runtime for a tiny v0 subset.
The pipeline parses Verilog, flattens module hierarchy, and emits MSL + host
stubs for GPU-backed simulation.

## Quick start
```sh
cmake -S . -B build
cmake --build build
./build/metalfpga_cli path/to/design.v --dump-flat
```

## Docs
- Project overview: `docs/gpga/README.md`
- Verilog subset status: `docs/gpga/verilog_words.md`
- Flattened netlist guarantees: `docs/gpga/ir_invariants.md`
