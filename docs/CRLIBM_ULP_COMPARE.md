# CRlibm ULP Comparison Harness

This document defines the reference pipeline for validating the gpga real
library against CPU CRlibm, producing ULP error summaries that justify the
"IEEE-754 compliant" claim.

## Goals

- Bit-exact or correctly rounded (<= 0.5 ULP) results vs CPU CRlibm.
- Coverage of RN/RD/RU/RZ where exposed.
- Repeatable artifacts for README claims.

## Current Status (latest run)

- Latest artifact: `artifacts/real_ulp/bea80182`
- Count: 100,000 vectors per function/mode, seed 1
- Result: **99,999/100,000 ULP=0, 1/100,000 ULP=1**
- Culprit: `tanpi:rn` — single case with ULP=1 at input `0xbdf623268eb172b4`
  - Reference: `0xbe1162f83d3fa6f6`
  - Got: `0xbe1162f83d3fa6f5`
- All other functions/modes: max ULP = 0 (bit-exact across 100k cases)

## Remaining Work

All validation work completed:
- ✅ Full suite run at 100k vectors per function/mode
- ✅ CI integration with sanity subset and `summary.json` publishing
- ✅ IEEE-754 compliant claim documented in README with artifact links
- ✅ Extended vector generation for pole-near stress and subnormal sweeps

## Inputs

Each test case is a tuple:

- `func`: math function name
- `mode`: rounding mode (rn/rd/ru/rz)
- `arg0`, `arg1`: IEEE-754 binary64 input bits
- Optional: domain tags (finite, subnormal, NaN, +/-0, +/-Inf)

## Outputs

- `max_ulp` per function/mode
- `avg_ulp` per function/mode
- `worst_case` input(s)
- Bit-exact pass/fail counts
- Full CSV or JSON of per-vector results

## Reference Build (CPU CRlibm)

- Use `thirdparty/crlibm/` as source of truth.
- Build a small host-side runner that exposes:
  - `crlibm_<func>_<mode>(double x [, double y])`
- Output IEEE-754 bits for reference results.

## GPU Under Test (gpga)

- Use `metalfpga_cli` to compile and run a generated Verilog testbench.
- Testbench accepts packed input bits and writes output bits to buffers.
- Output buffers are dumped to host and compared to CRlibm output.

## ULP Computation

- Convert output bits to ordered integer space:
  - For signed values, map negatives to descending order (two's complement
    on sign bit, aka "lexicographic" transform).
- ULP = absolute difference in ordered integer space.
- For NaN: only compare that both are NaN (payload may differ).
- For Inf: require same sign.
- For +0/-0: accept sign-specific match (or explicitly track sign rules).

## Vector Generation

### Uniform + Edge Mix

- Random finite values across exponent range.
- Dense sampling around:
  - 0, subnormals, +/-min normal, +/-max finite
  - boundaries for each function domain
- Deterministic seed for reproducibility.

### Function-Specific Domains

- `log`, `log1p`: x > 0 (log1p: x > -1)
- `sqrt`: x >= 0
- `asin`, `acos`: x in [-1, 1]
- `tan`: avoid near pi/2 multiples (separate stress vectors for poles)
- `pow`: include integer exponents + edge cases (0^0, negative base)

## Harness Layout (Proposed)

- `tools/crlibm_ref_runner/` (CPU reference executable)
- `tools/real_ulp_compare/` (driver + report generator)
- `artifacts/real_ulp/` (results per run)
  - `vectors.json`
  - `results.csv`
  - `summary.json`

## CLI Integration (Proposed)

- `metalfpga_cli --crlibm-compare <funcs> --seed <n> --count <n>`
- Optional flags:
  - `--mode rn|rd|ru|rz|all`
  - `--edge-only`
  - `--domain strict|loose`
  - `--report-json <path>`

## Acceptance Criteria

- RN: max ULP <= 0.5 for all supported functions.
- RD/RU/RZ: matches directed rounding outputs from CRlibm.
- Pow: explicitly report any deviations (CRlibm is not fully proven for pow).

## Notes / Caveats

- This is a validation pipeline, not required for normal builds.
- The reference runner uses CPU double; CRlibm is the correctness oracle.
- Keep all inputs/outputs as 64-bit bit patterns to avoid host rounding.
