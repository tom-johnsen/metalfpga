# IEEE 1800-2012 Test Suite Quick Reference

**Quick lookup table for 77 ISPRAS SystemVerilog compliance tests**

## How to Use This Reference

- **Find tests by ID**: Tests are sorted numerically for quick scanning
- **Type badges**: [POS]=POSITIVE (should pass), [NEG]=NEGATIVE (should fail), [VAR]=VARYING (implementation-defined), [???]=UNKNOWN
- **Section numbers**: Use section notation (e.g., "16.8.1") to reference IEEE Std 1800-2012
- **Full details**: See `IEEE_1800_2012_TESTS.md` for complete test descriptions

## Summary: 77 tests - 69 POS | 0 NEG | 8 VAR | 0 ???

---

## Quick Reference Table

```
test_16_03_00 | [POS] 16.3 Immediate assertions
test_16_04_02_1 | [POS] 16.4.2 Deferred assertion flush points
test_16_04_02_2 | [POS] 16.4.2 Deferred assertion flush points
test_16_04_03 | [POS] 16.4.3 Deferred assertions outside procedural code
test_16_06_00_1 | [POS] 16.6 Boolean expressions
test_16_06_00_2 | [POS] 16.6 Boolean expressions
test_16_08_00_1 | [VAR] 16.8 Declaring sequences
test_16_08_00_2 | [POS] 16.8 Declaring sequences
test_16_08_00_3 | [POS] 16.8 Declaring sequences
test_16_08_00_4 | [POS] 16.8 Declaring sequences
test_16_08_00_5 | [POS] 16.8 Declaring sequences
test_16_08_00_6 | [VAR] 16.8 Declaring sequences
test_16_08_01_1 | [POS] 16.8.1 Typed formal arguments in sequence declarations
test_16_08_01_2 | [POS] 16.8.1 Typed formal arguments in sequence declarations
test_16_08_01_3 | [POS] 16.8.1 Typed formal arguments in sequence declarations
test_16_08_01_4 | [POS] 16.8.1 Typed formal arguments in sequence declarations
test_16_08_01_5 | [POS] 16.8.1 Typed formal arguments in sequence declarations
test_16_08_02_1 | [POS] 16.8.2 Local variable formal arguments
test_16_08_02_2 | [VAR] 16.8.2 Local variable formal arguments
test_16_08_02_3 | [POS] 16.8.2 Local variable formal arguments
test_16_08_02_4 | [POS] 16.8.2 Local variable formal arguments
test_16_09_02 | [POS] 16.9.2 Repetition in sequences
test_16_09_03 | [POS] 16.9.3 Sampled value functions
test_16_09_04_0 | [POS] 16.9.4 Global clocking sampled value functions
test_16_09_04_1 | [POS] 16.9.4 Global clocking sampled value functions
test_16_09_04_2 | [POS] 16.9.4 Global clocking sampled value functions
test_16_09_04_3 | [POS] 16.9.4 Global clocking sampled value functions
test_16_09_04_4 | [POS] 16.9.4 Global clocking sampled value functions
test_16_09_05 | [POS] 16.9.5 AND operation
test_16_09_06 | [POS] 16.9.6 Intersection (AND with length restriction)
test_16_09_07 | [POS] 16.9.7 OR operation
test_16_09_08_1 | [POS] 16.9.8 First_match operation
test_16_09_08_2 | [POS] 16.9.8 First_match operation
test_16_09_09 | [POS] 16.9.9 Conditions over sequences
test_16_09_10 | [POS] 16.9.10 Sequence contained within another sequence
test_16_09_11 | [POS] 16.9.11 Composing sequences from simpler subsequences
test_16_10_00_1 | [POS] 16.10 Local variables
test_16_10_00_2 | [VAR] 16.10 Local variables
test_16_10_00_3 | [POS] 16.10 Local variables
test_16_10_00_4 | [POS] 16.10 Local variables
test_16_10_00_5 | [POS] 16.10 Local variables
test_16_10_00_6 | [POS] 16.10 Local variables
test_16_10_00_7 | [VAR] 16.10 Local variables
test_16_11_00 | [POS] 16.11 Calling subroutines on match of sequence
test_16_12_01 | [POS] 16.12.1 Sequence property
test_16_12_02 | [POS] 16.12.2 Negation property
test_16_12_06_1 | [POS] 16.12.6 Implication
test_16_12_06_2 | [POS] 16.12.6 Implication
test_16_12_06_3 | [POS] 16.12.6 Implication
test_16_12_10 | [POS] 16.12.10 Nexttime property
test_16_12_11 | [VAR] 16.12.11 Always property
test_16_12_12 | [POS] 16.12.12 Until property
test_16_12_13 | [POS] 16.12.13 Eventually property
test_16_12_14-1 | [POS] 16.12.14 Abort properties - Example 1
test_16_12_14-2 | [POS] 16.12.14 Abort properties - Example 2
test_16_12_14-3 | [POS] 16.12.14 Abort properties - Example 3
test_16_12_14-4 | [POS] 16.12.14 Abort properties - Example 4
test_16_12_14-5 | [POS] 16.12.14 Abort properties - Example 5
test_16_12_14-6 | [POS] 16.12.14 Abort properties - Example 6
test_16_12_14-7 | [POS] 16.12.14 Abort properties - Example 7
test_16_12_16 | [VAR] 16.12.16 Case
test_16_12_17_1 | [POS] 16.12.17 Recursive properties
test_16_12_17_2 | [VAR] 16.12.17 Recursive properties
test_16_12_17_3 | [POS] 16.12.17 Recursive properties
test_16_12_17_4 | [POS] 16.12.17 Recursive properties
test_16_12_17_5 | [POS] 16.12.17 Recursive properties
test_16_12_20 | [POS] 16.12.20 Property examples
test_16_13_04 | [POS] 16.13.4 Multiclock support examples
test_16_13_07 | [POS] 16.13.7 Local variable initialization assignments
test_16_14_01 | [POS] 16.14.1 Assert statement
test_16_14_02 | [POS] 16.14.2 Assume statement
test_16_14_05 | [POS] 16.14.5 Concurrent assertions outside procedural code
test_16_14_06_00_1 | [POS] 16.14.6 Embedding concurrent assertions
test_16_14_06_00_2 | [POS] 16.14.6 Embedding concurrent assertions
test_16_14_06_01_1 | [POS] 16.14.6.1 Arguments to procedural assertions
test_16_14_06_01_2 | [POS] 16.14.6.1 Arguments to procedural assertions
test_16_14_06_01_3 | [POS] 16.14.6.1 Arguments to procedural assertions
```

---

## Test File Location

All test files are located in: `/Users/tom/cpp/metalfpga/goldentests/ispras-sv-tests/ieee-1800-2012/`

Files follow pattern: `test_XX_YY_ZZ_N.sv` where XX=chapter (16), YY=section, ZZ=subsection, N=sequence

## Quick Search Tips

```bash
# Find all sequence declaration tests
grep "16.8" IEEE_1800_2012_TESTS_QUICK_REF.md

# Find all property tests
grep "16.12" IEEE_1800_2012_TESTS_QUICK_REF.md

# Find all VARYING tests (implementation-defined behavior)
grep "\[VAR\]" IEEE_1800_2012_TESTS_QUICK_REF.md

# Find all concurrent assertion tests
grep "16.14" IEEE_1800_2012_TESTS_QUICK_REF.md
```

---

## Feature Coverage Summary

This test suite focuses on **Chapter 16: Assertions** from IEEE 1800-2012:

| Feature Area | Sections | Test Count |
|-------------|----------|------------|
| Immediate/Deferred Assertions | 16.3, 16.4 | 4 |
| Boolean Expressions | 16.6 | 2 |
| Sequence Declarations | 16.8 | 15 |
| Sequence Operations | 16.9 | 14 |
| Local Variables | 16.10 | 7 |
| Subroutine Calls | 16.11 | 1 |
| Property Declarations | 16.12 | 23 |
| Multiclock Support | 16.13 | 2 |
| Concurrent Assertions | 16.14 | 9 |

**Total Coverage**: 41 unique subsections, 77 test cases

---

**Document Version**: 1.0
**Source**: ISPRAS SystemVerilog Test Suite
**Standard**: IEEE Std 1800-2012
**Total Tests**: 77
