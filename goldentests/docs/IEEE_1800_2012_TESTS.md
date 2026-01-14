# IEEE 1800-2012 Test Suite Catalog

## Overview

This document catalogs all 77 test files from the ISPRAS SystemVerilog test suite
for IEEE 1800-2012 (SystemVerilog standard). These tests validate compliance with the
IEEE Standard for SystemVerilog—Unified Hardware Design, Specification, and Verification Language.

**Source**: ISPRAS (Institute for System Programming of the Russian Academy of Sciences)

**Standard**: IEEE Std 1800-2012

## Summary Statistics

- **Total Tests**: 77
- **IEEE Sections Covered**: 41
- **IEEE Chapters Covered**: 1 (Chapter 16: Assertions)

### Tests by Type

- **NEGATIVE**: 0 tests (0.0%)
- **POSITIVE**: 69 tests (89.6%)
- **VARYING**: 8 tests (10.4%)

### Type Definitions

- **POSITIVE**: Test should compile and run successfully. These tests validate correct implementation of language features.
- **NEGATIVE**: Test should fail to compile. These tests validate proper error detection and reporting.
- **VARYING**: Test has implementation-defined behavior. Results may vary across different compliant implementations.

## Chapter Coverage

| Chapter | Title | Tests |
|---------|-------|-------|
| 16 | Assertions | 77 |

---

## Tests by IEEE Section

*Sections are listed in numerical order with all associated test cases.*


## Chapter 16: Assertions

### Section 16.3: Immediate assertions

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_03_00` | POSITIVE | Immediate assertions |

### Section 16.4.2: Deferred assertion flush points

*16.3 Deferred assertions → 16.4.2 Deferred assertion flush points*

**Test Count**: 2

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_04_02_1` | POSITIVE | Deferred assertion flush points |
| `test_16_04_02_2` | POSITIVE | Deferred assertion flush points |

### Section 16.4.3: Deferred assertions outside procedural code

*16.3 Deferred assertions → 16.4.3 Deferred assertions outside procedural code*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_04_03` | POSITIVE | Deferred assertions outside procedural code |

### Section 16.6: Boolean expressions

**Test Count**: 2

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_06_00_1` | POSITIVE | Boolean expressions |
| `test_16_06_00_2` | POSITIVE | Boolean expressions |

### Section 16.8: Declaring sequences

**Test Count**: 6

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_08_00_1` | VARYING | Declaring sequences |
| `test_16_08_00_2` | POSITIVE | Declaring sequences |
| `test_16_08_00_3` | POSITIVE | Declaring sequences |
| `test_16_08_00_4` | POSITIVE | Declaring sequences |
| `test_16_08_00_5` | POSITIVE | Declaring sequences |
| `test_16_08_00_6` | VARYING | Declaring sequences |

### Section 16.8.1: Typed formal arguments in sequence declarations

*16.8 Declaring sequences → 16.8.1 Typed formal arguments in sequence declarations*

**Test Count**: 5

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_08_01_1` | POSITIVE | Typed formal arguments in sequence declarations |
| `test_16_08_01_2` | POSITIVE | Typed formal arguments in sequence declarations |
| `test_16_08_01_3` | POSITIVE | Typed formal arguments in sequence declarations |
| `test_16_08_01_4` | POSITIVE | Typed formal arguments in sequence declarations |
| `test_16_08_01_5` | POSITIVE | Typed formal arguments in sequence declarations |

### Section 16.8.2: Local variable formal arguments in sequence declarations

*16.8 Declaring sequences → 16.8.2 Local variable formal arguments in sequence declarations*

**Test Count**: 4

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_08_02_1` | POSITIVE | Local variable formal arguments in sequence declarations |
| `test_16_08_02_2` | VARYING | Local variable formal arguments in sequence declarations |
| `test_16_08_02_3` | POSITIVE | Local variable formal arguments in sequence declarations |
| `test_16_08_02_4` | POSITIVE | Local variable formal arguments in sequence declarations |

### Section 16.9.2: Repetition in sequences

*16.9 Sequence operations → 16.9.2 Repetition in sequences*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_02` | POSITIVE | Repetition in sequences |

### Section 16.9.3: Sampled value functions

*16.9 Sequence operations → 16.9.3 Sampled value functions*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_03` | POSITIVE | Sampled value functions |

### Section 16.9.4: Global clocking past and future sampled value functions

*16.9 Sequence operations → 16.9.4 Global clocking past and future sampled value functions*

**Test Count**: 5

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_04_0` | POSITIVE | Global clocking past and future sampled value functions |
| `test_16_09_04_1` | POSITIVE | Global clocking past and future sampled value functions |
| `test_16_09_04_2` | POSITIVE | Global clocking past and future sampled value functions |
| `test_16_09_04_3` | POSITIVE | Global clocking past and future sampled value functions |
| `test_16_09_04_4` | POSITIVE | Global clocking past and future sampled value functions |

### Section 16.9.5: AND operation

*16.9 Sequence operations → 16.9.5 AND operation*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_05` | POSITIVE | AND operation |

### Section 16.9.6: Intersection (AND with length restriction)

*16.9 Sequence operations → 16.9.6 Intersection (AND with length restriction)*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_06` | POSITIVE | Intersection (AND with length restriction) |

### Section 16.9.7: OR operation

*16.9 Sequence operations → 16.9.7 OR operation*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_07` | POSITIVE | OR operation |

### Section 16.9.8: First_match operation

*16.9 Sequence operations → 16.9.8 First_match operation*

**Test Count**: 2

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_08_1` | POSITIVE | First_match operation |
| `test_16_09_08_2` | POSITIVE | First_match operation |

### Section 16.9.9: Conditions over sequences

*16.9 Sequence operations → 16.9.9 Conditions over sequences*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_09` | POSITIVE | Conditions over sequences |

### Section 16.9.10: Sequence contained within another sequence

*16.9 Sequence operations → 16.9.10 Sequence contained within another sequence*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_10` | POSITIVE | Sequence contained within another sequence |

### Section 16.9.11: Composing sequences from simpler subsequences

*16.9 Sequence operations → 16.9.11 Composing sequences from simpler subsequences*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_09_11` | POSITIVE | Composing sequences from simpler subsequences |

### Section 16.10: Local variables

**Test Count**: 7

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_10_00_1` | POSITIVE | Local variables |
| `test_16_10_00_2` | VARYING | Local variables |
| `test_16_10_00_3` | POSITIVE | Local variables |
| `test_16_10_00_4` | POSITIVE | Local variables |
| `test_16_10_00_5` | POSITIVE | Local variables |
| `test_16_10_00_6` | POSITIVE | Local variables |
| `test_16_10_00_7` | VARYING | Local variables |

### Section 16.11: Calling subroutines on match of a sequence

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_11_00` | POSITIVE | Calling subroutines on match of a sequence |

### Section 16.12.1: Sequence property

*16.12 Declaring properties → 16.12.1 Sequence property*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_01` | POSITIVE | Sequence property |

### Section 16.12.2: Negation property

*16.12 Declaring properties → 16.12.2 Negation property*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_02` | POSITIVE | Negation property |

### Section 16.12.6: Implication

*16.12 Declaring properties → 16.12.6 Implication*

**Test Count**: 3

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_06_1` | POSITIVE | Implication |
| `test_16_12_06_2` | POSITIVE | Implication |
| `test_16_12_06_3` | POSITIVE | Implication |

### Section 16.12.10: Nexttime property

*16.12 Declaring properties → 16.12.10 Nexttime property*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_10` | POSITIVE | Nexttime property |

### Section 16.12.11: Always property

*16.12 Declaring properties → 16.12.11 Always property*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_11` | VARYING | Always property |

### Section 16.12.12: Until property

*16.12 Declaring properties → 16.12.12 Until property*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_12` | POSITIVE | Until property |

### Section 16.12.13: Eventually property

*16.12 Declaring properties → 16.12.13 Eventually property*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_13` | POSITIVE | Eventually property |

### Section 16.12.14: Abort properties

*16.12 Declaring properties → 16.12.14 Abort properties*

**Test Count**: 7

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_14-1` | POSITIVE | Abort properties |
| `test_16_12_14-2` | POSITIVE | Abort properties |
| `test_16_12_14-3` | POSITIVE | Abort properties |
| `test_16_12_14-4` | POSITIVE | Abort properties |
| `test_16_12_14-5` | POSITIVE | Abort properties |
| `test_16_12_14-6` | POSITIVE | Abort properties |
| `test_16_12_14-7` | POSITIVE | Abort properties |

### Section 16.12.16: Case

*16.12 Declaring properties → 16.12.16 Case*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_16` | VARYING | Case |

### Section 16.12.17: Recursive properties

*16.12 Declaring properties → 16.12.17 Recursive properties*

**Test Count**: 5

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_17_1` | POSITIVE | Recursive properties |
| `test_16_12_17_2` | VARYING | Recursive properties |
| `test_16_12_17_3` | POSITIVE | Recursive properties |
| `test_16_12_17_4` | POSITIVE | Recursive properties |
| `test_16_12_17_5` | POSITIVE | Recursive properties |

### Section 16.12.20: Property examples

*16.12 Declaring properties → 16.12.20 Property examples*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_12_20` | POSITIVE | Property examples |

### Section 16.13.4: Examples

*16.13 Multiclock support → 16.13.4 Examples*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_13_04` | POSITIVE | Multiclock support examples |

### Section 16.13.7: Local variable initialization assignments

*16.13 Multiclock support → 16.13.7 Local variable initialization assignments*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_13_07` | POSITIVE | Local variable initialization assignments |

### Section 16.14.1: Assert statement

*16.14 Concurrent assertions → 16.14.1 Assert statement*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_14_01` | POSITIVE | Assert statement |

### Section 16.14.2: Assume statement

*16.14 Concurrent assertions → 16.14.2 Assume statement*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_14_02` | POSITIVE | Assume statement |

### Section 16.14.5: Using concurrent assertion statements outside procedural code

*16.14 Concurrent assertions → 16.14.5 Using concurrent assertion statements outside procedural code*

**Test Count**: 1

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_14_05` | POSITIVE | Using concurrent assertion statements outside procedural code |

### Section 16.14.6: Embedding concurrent assertions in procedural code

*16.14 Concurrent assertions → 16.14.6 Embedding concurrent assertions in procedural code*

**Test Count**: 2

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_14_06_00_1` | POSITIVE | Embedding concurrent assertions in procedural code |
| `test_16_14_06_00_2` | POSITIVE | Embedding concurrent assertions in procedural code |

### Section 16.14.6.1: Arguments to procedural concurrent assertions

*16.14 Concurrent assertions → 16.14.6 Embedding concurrent assertions in procedural code → 16.14.6.1 Arguments to procedural concurrent assertions*

**Test Count**: 3

| Test ID | Type | Description |
|---------|------|-------------|
| `test_16_14_06_01_1` | POSITIVE | Arguments to procedural concurrent assertions |
| `test_16_14_06_01_2` | POSITIVE | Arguments to procedural concurrent assertions |
| `test_16_14_06_01_3` | POSITIVE | Arguments to procedural concurrent assertions |


---

## Appendix: Quick Reference

### Test Naming Convention

Test files follow the naming pattern: `test_XX_YY_ZZ_N.sv`

- `XX`: Chapter number (16 for Assertions)
- `YY`: Section number within chapter
- `ZZ`: Subsection number
- `N`: Test sequence number for that subsection

### Finding Tests

To find tests for a specific IEEE section:
```bash
# Find all tests for section 16.8 (Declaring sequences)
ls test_16_08_*.sv

# Find all tests for section 16.12 (Declaring properties)
ls test_16_12_*.sv

# Find all tests for Chapter 16 (all assertion tests)
ls test_16_*.sv
```

### Test Structure

Each test file contains:
- Copyright and license header
- IEEE section reference with full hierarchy
- Test type declaration (`// ! TYPE: POSITIVE/NEGATIVE/VARYING`)
- SystemVerilog test code demonstrating the feature

### Key Features Tested

The IEEE 1800-2012 test suite focuses exclusively on **Chapter 16: Assertions**, which includes:

- **Immediate and Deferred Assertions**: Direct assertion checking
- **Sequences**: Temporal patterns and operations
- **Properties**: Complex temporal specifications
- **Concurrent Assertions**: Parallel assertion checking
- **Multiclock Support**: Assertions across clock domains

These SystemVerilog assertion features are critical for formal verification and advanced hardware validation workflows.
