# Golden Test Suites for metalfpga

This directory contains industry-standard test suites for validating metalfpga's Verilog/SystemVerilog compliance.

## Test Repositories

### 1. **ISPRAS IEEE 1364-2005 Tests** (`ispras-sv-tests/ieee-1364-2005/`)
- **Source**: [github.com/ispras/sv-tests](https://github.com/ispras/sv-tests)
- **Standard**: IEEE 1364-2005 (Verilog-2005)
- **Tests**: 355 tests covering 185 sections of the standard
- **Status**: Primary compliance target for metalfpga v1.0
- **Runner**: `run_ieee_1364_2005_tests.sh`

### 1.1. **ISPRAS IEEE 1800-2012 Tests** (`ispras-sv-tests/ieee-1800-2012/`)
- **Source**: [github.com/ispras/sv-tests](https://github.com/ispras/sv-tests)
- **Standard**: IEEE 1800-2012 (SystemVerilog)
- **Tests**: 77 tests covering SystemVerilog assertions (Chapter 16)
- **Status**: Future SystemVerilog compliance target
- **Runner**: `run_ieee_1800_2012_tests.sh`

### 2. **Icarus Verilog Test Suite** (`ivtest/`)
- **Source**: [github.com/steveicarus/ivtest](https://github.com/steveicarus/ivtest)
- **Tests**: 2,790 test files
- **Coverage**: Comprehensive Verilog-2005 and some SystemVerilog
- **Status**: Gold standard reference implementation

### 3. **Yosys Test Suite** (`yosys-tests/`)
- **Source**: [github.com/YosysHQ/yosys-tests](https://github.com/YosysHQ/yosys-tests)
- **Tests**: 836 test files
- **Focus**: Synthesis and simulation validation
- **Status**: Secondary validation target

### 4. **ChipsAlliance SystemVerilog Tests** (`sv-tests/`)
- **Source**: [github.com/chipsalliance/sv-tests](https://github.com/chipsalliance/sv-tests)
- **Tests**: 1,032 test files
- **Coverage**: SystemVerilog features (IEEE 1800)
- **Status**: Future target for SystemVerilog support

**Total Test Files**: 5,168 (355 Verilog + 77 SystemVerilog + 4,736 from other suites)

---

## Quick Start

### Run IEEE 1364-2005 Tests (Verilog-2005)

```bash
# Run all Verilog-2005 tests
./run_ieee_1364_2005_tests.sh

# Run with verbose output
./run_ieee_1364_2005_tests.sh --verbose

# Run only Chapter 5 (Expressions) tests
./run_ieee_1364_2005_tests.sh --filter "test_05_*"

# Run only POSITIVE tests
./run_ieee_1364_2005_tests.sh --mode positive

# Parallel execution (8 jobs)
./run_ieee_1364_2005_tests.sh --parallel --jobs 8

# See what would run without executing
./run_ieee_1364_2005_tests.sh --filter "test_03_*" --dry-run
```

### Run IEEE 1800-2012 Tests (SystemVerilog)

```bash
# Run all SystemVerilog assertion tests
./run_ieee_1800_2012_tests.sh

# Run with verbose output
./run_ieee_1800_2012_tests.sh --verbose

# Run only sequence tests (section 16.8)
./run_ieee_1800_2012_tests.sh --filter "test_16_08_*"

# Run only POSITIVE tests
./run_ieee_1800_2012_tests.sh --mode positive
```

### Test Runner Options

```
-h, --help              Show help message
-v, --verbose           Show compilation details
-s, --stop-on-fail      Stop on first failure
-f, --filter PATTERN    Only run tests matching PATTERN
-m, --mode MODE         Test mode: all, positive, negative, varying
-d, --dry-run           Show what would be tested
-p, --parallel          Run tests in parallel
-j, --jobs N            Number of parallel jobs (default: 4)
--metalfpga PATH        Path to metalfpga_cli binary
```

### Environment Variables

```bash
# Override metalfpga location
export METALFPGA_CLI=/path/to/metalfpga_cli
./run_ieee_1364_2005_tests.sh
```

---

## Documentation

### Test Catalogs

#### IEEE 1364-2005 (Verilog-2005)
- **[docs/IEEE_1364_2005_TESTS.md](docs/IEEE_1364_2005_TESTS.md)** - Detailed catalog of all 355 Verilog-2005 tests with full descriptions, organized by IEEE section
- **[docs/IEEE_1364_2005_TESTS_QUICK_REF.md](docs/IEEE_1364_2005_TESTS_QUICK_REF.md)** - Quick reference with one line per test for fast lookups

#### IEEE 1800-2012 (SystemVerilog)
- **[docs/IEEE_1800_2012_TESTS.md](docs/IEEE_1800_2012_TESTS.md)** - Detailed catalog of all 77 SystemVerilog assertion tests with full descriptions
- **[docs/IEEE_1800_2012_TESTS_QUICK_REF.md](docs/IEEE_1800_2012_TESTS_QUICK_REF.md)** - Quick reference with one line per test for fast lookups

### Test Classification

Tests are classified by type in their headers:

- **POSITIVE** (325 tests, 91.5%): Should compile and run successfully
- **NEGATIVE** (2 tests, 0.6%): Should fail to compile (error detection tests)
- **VARYING** (27 tests, 7.6%): Implementation-defined behavior (multiple valid results)

### Finding Tests

Tests follow the naming pattern: `test_XX_YY_ZZ_N.v`

- `XX`: Chapter number (e.g., `03` = Chapter 3: Lexical Conventions)
- `YY`: Section number within chapter
- `ZZ`: Subsection number
- `N`: Test sequence number

**Examples:**
```bash
# Find all tests for Chapter 5 (Expressions)
ls ispras-sv-tests/ieee-1364-2005/test_05_*.v

# Find all tests for section 5.1 (Operators)
ls ispras-sv-tests/ieee-1364-2005/test_05_01_*.v

# Count tests by chapter
ls ispras-sv-tests/ieee-1364-2005/test_*.v | cut -d_ -f2 | sort | uniq -c
```

---

## Test Results

Test results are saved to `results/` directory:

```
results/
├── test_run_YYYYMMDD_HHMMSS.log     # Full test log
└── summary_YYYYMMDD_HHMMSS.txt      # Summary report
```

### Example Summary Output

```
Total tests:   355
Passed:        287
Failed:        41
Varying:       27
Skipped:       0
Pass rate:     80.8%

Failed tests:
  - test_05_01_13_1 [POSITIVE should pass, but failed]
  - test_09_07_04_3 [POSITIVE should pass, but failed]
  ...
```

---

## Coverage by IEEE Chapter

| Chapter | Title | Tests |
|---------|-------|-------|
| 3 | Lexical Conventions | 22 |
| 4 | Data Types | 14 |
| 5 | Expressions | 35 |
| 6 | Assignments | 10 |
| 7 | Gate and Switch Level Modeling | 19 |
| 8 | User-Defined Primitives (UDPs) | 8 |
| 9 | Behavioral Modeling | 60 |
| 10 | Tasks and Functions | 17 |
| 11 | Disable Statement and Named Blocks | 2 |
| 12 | Hierarchical Structures | 32 |
| 14 | Specify Blocks | 31 |
| 15 | Timing Checks | 21 |
| 16 | SDF Backannotation | 8 |
| 17 | System Tasks and Functions | 56 |
| 18 | Value Change Dump (VCD) | 8 |
| 19 | Compiler Directives | 11 |

---

## Development Workflow

### Phase 1: Baseline (Current)

1. Run test suite to establish baseline pass rate
2. Identify top failing patterns
3. Document known limitations

```bash
./run_ieee_1364_2005_tests.sh > baseline.txt 2>&1
```

### Phase 2: Systematic Improvement

1. Pick top 10 most common failure patterns
2. Fix underlying issues in metalfpga
3. Re-run tests to validate fixes
4. Repeat until 95%+ pass rate

```bash
# Track progress over time
for run in {1..10}; do
    ./run_ieee_1364_2005_tests.sh --mode positive | tee "progress_run_$run.txt"
done
```

### Phase 3: Edge Cases

1. Address VARYING tests (match Icarus Verilog behavior)
2. Handle NEGATIVE tests (proper error detection)
3. Document intentional deviations from spec

```bash
# Test varying behavior
./run_ieee_1364_2005_tests.sh --mode varying --verbose
```

---

## Known Issues

### IEEE Standard Bugs

See `ispras-sv-tests/ieee-1364-2005/KNOWN_TEXT_BUGS` for documented errors in the IEEE 1364-2005 specification itself.

**Policy**: metalfpga implements **correct behavior**, not spec bugs. Known deviations are documented.

### Implementation-Defined Behavior

VARYING tests have multiple valid outcomes. metalfpga aims to match Icarus Verilog behavior when practical.

---

## Contributing

### Adding New Test Suites

1. Clone repository to this directory
2. Create test runner script (follow `run_ieee_1364_2005_tests.sh` pattern)
3. Document in this README
4. Add to CI/CD pipeline

### Reporting Test Failures

When reporting metalfpga bugs based on test failures:

1. Include test name (e.g., `test_05_01_13_1`)
2. Show expected vs actual behavior
3. Attach relevant log snippet from `results/`
4. Reference IEEE section from test catalog

---

## License

Test suites retain their original licenses:
- **ISPRAS tests**: Apache 2.0
- **Icarus Verilog tests**: GPL-2.0
- **Yosys tests**: ISC License
- **ChipsAlliance tests**: Apache 2.0

See individual repository LICENSE files for details.

---

## References

- [IEEE Std 1364-2005](https://ieeexplore.ieee.org/document/1620780) - Verilog HDL Standard
- [IEEE Std 1800-2017](https://ieeexplore.ieee.org/document/8299595) - SystemVerilog Standard
- [Icarus Verilog](http://iverilog.icarus.com/) - Reference Verilog simulator
- [Verilator](https://www.veripool.org/verilator/) - High-performance Verilog simulator
- [Yosys](http://www.clifford.at/yosys/) - Verilog synthesis tool

---

**Last Updated**: 2025-12-28
