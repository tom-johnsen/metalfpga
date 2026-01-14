== DONE ==

# Scheduler VM Fallback Plan (IEEE 1364-2005)

Source: `goldentests/results/fallback_20260113_025744` (43 tests, all 4-state).

## Summary by fallback class (tests with entries)

- CallGroup fallback: 3 tests
- Assign fallback: 31 tests
- Delay-assign fallback: 2 tests
- Force fallback: 2 tests
- Release fallback: 1 test
- Service fallback: 4 tests
- Service-ret-assign fallback: 0 tests

## Grouped issues to patch

### 1) CallGroup fallback (proc starts with CallGroup)

Tests:
- `test_03_08_01_1_4state`
- `test_03_08_01_2_4state`
- `test_09_05_02_1_4state`

Plan:
- Identify which construct emits `kCallGroup` in these procs.
- Add missing VM opcode or emit bytecode instead of CallGroup.

### 2) Assign fallback: rhs_unencodable (17 tests, 27 entries)

Subgroup A: system/real function calls in RHS
- `$time`: `test_04_09_03_1_2_4state`
- `$ln/$log10/$exp/$sqrt/$pow/$floor/$ceil/$sin/$cos/$tan/$asin/$acos/$atan`:
  `test_17_11_02_01_4state` .. `test_17_11_02_13_4state`

Subgroup B: power operator in RHS (`p` in diag, likely `**`)
- `test_05_01_05_1_4state`
- `test_05_01_05_2_4state`
- `test_05_04_03_1_4state`

Plan:
- Enable VM expr emission for `ExprKind::kCall` real/time functions.
- Implement VM expression support for power operator (integer and real).

### 3) Assign fallback: lhs_is_real (5 tests)

Tests:
- `test_03_05_02_1_4state`
- `test_06_02_01_4_4state`
- `test_06_02_01_5_4state`
- `test_12_02_00_2_4state`
- `test_17_10_02_1_top_ieee1364_example_4state`

Plan:
- Add real-typed LHS assignment support in VM (store real value path).

### 4) Assign fallback: signal_id_missing (4 tests)

Tests:
- `test_10_03_00_5_4state`
- `test_12_05_00_1_4state`
- `test_12_05_00_2_4state`
- `test_12_06_00_1_4state`

Plan:
- Ensure scheduler VM signal layout includes these LHS regs
  (likely internal/held regs or hierarchical names).

### 5) Assign fallback: lhs_width_invalid (>64)

Tests:
- width=85: `test_03_05_01_4_4state`
- width=112: `test_03_06_02_1_4state`, `test_05_02_03_1_4state`

Plan:
- Add wide LHS assignment support (>64 bits), or route through wide-const path
  for non-string RHS literals.

### 6) Assign fallback: override_target (force/release target)

Tests:
- `test_09_03_01_1_4state`
- `test_09_03_02_2_4state`

Plan:
- Define safe assignment semantics when target has force/release (shadow or
  ordered write) and allow VM encoding.

### 7) Delay-assign fallback (2 tests)

Tests:
- `test_14_02_04_2_2_4state`
- `test_14_02_04_4_1_4state`

Plan:
- Investigate delay-assign encoding failure (entries flagged fallback with
  nonblocking/inertial flags); add VM support for these delay assigns.
  - Note: keep real LHS delay-assign fallback until this phase.

### 8) Force/release fallback

Force fallback tests:
- `test_12_03_07_1_4state`
- `test_17_08_00_1_4state`

Release fallback tests:
- `test_09_03_02_1_4state`

Plan:
- Ensure force/release targets resolve to force/passign slots and allow VM
  entries for procedural force/release patterns.

### 9) Service fallback

Unsupported task names:
- `$displayb`, `$async$and$plane`: `test_17_05_04_3_4state`
- `$dumpports`: `test_18_04_02_1_4state`

Unsupported syscall name:
- `$rewind` (function form): `test_17_02_05_2_4state`

Format/arg handling:
- `$display("%s ...", ...)` arg[1] constraints: `test_17_01_01_2_1_4state`

Plan:
- Add task aliases/support for `$displayb`, `$async$and$plane`, `$dumpports`.
- Add syscall support for `$rewind` (function form).
- Extend `%s` formatting to accept string/identifier forms used here.
