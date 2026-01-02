# metalfpga

metalfpga is a Verilog-2005 frontend and Metal (MSL) backend for GPU-based
hardware simulation on Apple GPUs. It parses and elaborates Verilog designs and
can emit Metal kernels and host-side scaffolding for execution.

## Status

- Frontend: Verilog-2005 parsing and elaboration complete; golden tests pass.
- Backend/runtime: functional for a subset; not yet a full simulator.
- VCD dumping works, including real signals.
- Ongoing work: scheduling semantics, timing, and system task behavior.

This project is best used today for frontend validation and targeted runtime
experiments. Full testbench execution is still in progress.

## Components

- Verilog-2005 frontend: parser, elaborator, and netlist flattener.
- Metal backend/runtime: MSL codegen, host scaffolding, and GPU scheduler.
- 4-state logic library: X/Z support when `--4state` is enabled.
- Real math library: high-accuracy real functions used by system tasks
  (see `docs/GPGA_REAL_API.md`).
- VCD writer with real signal support.

## Requirements

- macOS with Apple GPU and Metal support
- CMake + C++17 toolchain

## Build

```sh
cmake -S . -B build
cmake --build build
```

Optional sanity check:

```sh
./build/metalfpga_smoke
```

Binaries:

- `./build/metalfpga_cli` - main CLI
- `./build/metalfpga_smoke` - quick sanity test
- `./build/metalfpga_crlibm_compare` - real math accuracy tester

## Run

```sh
# Check syntax and elaborate design
./build/metalfpga_cli path/to/design.v

# Dump flattened netlist
./build/metalfpga_cli path/to/design.v --dump-flat

# Emit flattened netlist to a file
./build/metalfpga_cli path/to/design.v --emit-flat flat.txt

# Emit Metal shader code
./build/metalfpga_cli path/to/design.v --emit-msl output.metal

# Emit host runtime stub
./build/metalfpga_cli path/to/design.v --emit-host output.mm

# Run on GPU (runtime support is partial)
./build/metalfpga_cli path/to/design.v --run

# Enable 4-state logic (X/Z support)
./build/metalfpga_cli path/to/design.v --4state

# VCD waveform output (requires --run)
./build/metalfpga_cli path/to/design.v --run --vcd-dir ./waves/
```

## CLI options

- `--emit-msl PATH` - write Metal shader source.
- `--emit-host PATH` - write host-side runtime stub.
- `--emit-flat PATH` - write flattened design.
- `--dump-flat` - print flattened design.
- `--top MODULE` - select top-level module.
- `--4state` - enable 4-state logic (X/Z).
- `--auto` - auto-discover `.v` files under the input directory.
- `--strict-1364` - stricter IEEE-1364 parsing and semantics checks.
- `--sdf PATH` - load SDF and match timing checks.
- `--version` - print version and exit.
- `--run` - execute on GPU (runtime support is partial).
- `--count N` - number of kernel instances.
- `--service-capacity N` - service record buffer capacity.
- `--max-steps N` - max scheduler steps per dispatch.
- `--max-proc-steps N` - max scheduler steps per process.
- `--dispatch-timeout-ms N` - GPU dispatch timeout.
- `--run-verbose` - verbose runtime logging.
- `--source-bindings` - use source-level shader bindings.
- `--vcd-dir PATH` - directory for VCD output.
- `--vcd-steps N` - scheduler step interval between VCD samples.
- `+ARG[=VALUE]` - plusargs for `$test$plusargs` and `$value$plusargs`.

## CRLIBM options

The `metalfpga_crlibm_compare` tool compares the real math library against
CRlibm on the CPU. Results are written to `artifacts/real_ulp/<tag>` by default.

- `--func list|all` - function list (comma-separated) or `all`.
- `--mode rn|rd|ru|rz|all` - rounding mode selection.
- `--count N` - number of random vectors per function/mode.
- `--seed N` - RNG seed.
- `--out-dir PATH` - output directory for summary and artifacts.
- `--trace` - emit per-vector `results.csv`.

See `docs/CRLIBM_ULP_COMPARE.md` for methodology and artifact layout.

## Environment variables

- `METALFPGA_STRING_PAD=zero|space` - string literal padding (default `zero`).
- `METALFPGA_SPECIFY_DELAY_SELECT=fast|slow` - specify delay selection (default `fast`).
- `METALFPGA_NEGATIVE_SETUP_MODE=allow|clamp|error` - negative setup handling (default `allow`).
- `METALFPGA_SDF_VERBOSE=1` - log SDF match/mismatch details.

## Documentation

- Implementation-defined behavior decisions: `docs/IEEE_1364_2005_IMPLEMENTATION_DEFINED_BEHAVIORS.md`
- VARY decisions: `docs/IEEE_1364_2005_VARY_DECISIONS.md`
- Real math API: `docs/GPGA_REAL_API.md`
- 4-state API: `docs/GPGA_4STATE_API.md`

## License

See `LICENSE` and `LICENSE.COMMERCIAL`.
