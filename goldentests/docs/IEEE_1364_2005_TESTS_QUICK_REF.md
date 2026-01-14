# IEEE 1364-2005 Test Suite Quick Reference

**Quick lookup table for 355 ISPRAS Verilog-2005 compliance tests**

## How to Use This Reference

- **Find tests by ID**: Tests are sorted numerically for quick scanning
- **Type badges**: [POS]=POSITIVE (should pass), [NEG]=NEGATIVE (should fail), [VAR]=VARYING (implementation-defined), [???]=UNKNOWN
- **Section numbers**: Use section notation (e.g., "3.5.1") to reference IEEE Std 1364-2005
- **Full details**: See `IEEE_1364_2005_TESTS.md` for complete test descriptions

## Summary: 355 tests - 325 POS | 2 NEG | 27 VAR | 1 ???

---

## Quick Reference Table

```
test_03_05_01_1 | [VAR] 3.5.1 Integer constants - Unsized constants
test_03_05_01_2 | [POS] 3.5.1 Integer constants - Sized constants
test_03_05_01_3 | [VAR] 3.5.1 Integer constants - Sign with constants
test_03_05_01_4 | [POS] 3.5.1 Integer constants - Auto left padding
test_03_05_01_5 | [POS] 3.5.1 Integer constants - Underscore in numbers
test_03_05_02_1 | [VAR] 3.5.2 Real constants
test_03_06_01_1 | [POS] 3.6.1 String variable declaration
test_03_06_02_1 | [POS] 3.6.2 String manipulation
test_03_06_03_1 | [POS] 3.6.3 Special characters in strings
test_03_07_00_1 | [POS] 3.7 Identifiers
test_03_07_01_1 | [POS] 3.7.1 Escaped identifiers
test_03_07_03_1 | [POS] 3.7.3 System tasks and functions
test_03_07_04_1 | [POS] 3.7.4 Compiler directives
test_03_08_01_1 | [POS] 3.8.1 Attributes - Example 1
test_03_08_01_2 | [POS] 3.8.1 Attributes - Example 2
test_03_08_01_3 | [POS] 3.8.1 Attributes - Example 3
test_03_08_01_4 | [POS] 3.8.1 Attributes - Example 4
test_03_08_01_5 | [POS] 3.8.1 Attributes - Example 5
test_03_08_01_6 | [POS] 3.8.1 Attributes - Example 6
test_03_08_01_7 | [POS] 3.8.1 Attributes - Example 7
test_03_08_01_8 | [POS] 3.8.1 Attributes - Example 8
test_04_03_01_1 | [POS] 4.3.1 Specifying vectors
test_04_03_02_1 | [POS] 4.3.2 Vector net accessibility
test_04_04_01_1 | [POS] 4.4.1 Charge strength
test_04_05_00_1 | [POS] 4.5 Implicit declarations - Ex 1
test_04_05_00_2 | [POS] 4.5 Implicit declarations - Ex 2
test_04_05_00_3 | [POS] 4.5 Implicit declarations - Ex 3
test_04_08_00_1 | [POS] 4.8 Integers, reals, times, realtimes
test_04_09_00_1 | [POS] 4.9 Arrays
test_04_09_03_1_1 | [POS] 4.9.3.1.1 Array declarations
test_04_09_03_1_2 | [POS] 4.9.3.1.2 Assignment to array elements
test_04_09_03_1_3 | [POS] 4.9.3.1.3 Memory differences
test_04_10_01_1 | [POS] 4.10.1 Module parameters
test_04_10_03_1 | [POS] 4.10.3 Specify parameters
test_04_10_03_2 | [VAR] 4.10.3 Specify parameters
test_05_01_03_1 | [POS] 5.1.3 Using integer numbers in expressions
test_05_01_04_1 | [POS] 5.1.4 Expression evaluation order
test_05_01_05_1 | [POS] 5.1.5 Arithmetic operators
test_05_01_05_2 | [POS] 5.1.5 Arithmetic operators
test_05_01_06_1 | [POS] 5.1.6 Arithmetic expressions with regs/integers
test_05_01_07_1 | [POS] 5.1.7 Relational operators
test_05_01_07_2 | [POS] 5.1.7 Relational operators
test_05_01_08_1 | [POS] 5.1.8 Equality operators
test_05_01_09_1 | [POS] 5.1.9 Logical operators
test_05_01_09_2 | [POS] 5.1.9 Logical operators
test_05_01_09_3 | [POS] 5.1.9 Logical operators
test_05_01_12_1 | [POS] 5.1.12 Shift operators - Right shift
test_05_01_12_2 | [POS] 5.1.12 Shift operators - Left shift
test_05_01_13_1 | [POS] 5.1.13 Conditional operator
test_05_01_14_1 | [POS] 5.1.14 Concatenations
test_05_01_14_2 | [VAR] 5.1.14 Concatenations
test_05_01_14_3 | [VAR] 5.1.14 Concatenations
test_05_01_14_4 | [POS] 5.1.14 Concatenations
test_05_02_01_1 | [VAR] 5.2.1 Vector bit-select and part-select
test_05_02_01_2 | [POS] 5.2.1 Vector bit-select and part-select
test_05_02_01_3 | [POS] 5.2.1 Vector bit-select - Single bit
test_05_02_01_4 | [POS] 5.2.1 Vector part-select addressing
test_05_02_02_1 | [POS] 5.2.2 Array and memory addressing
test_05_02_02_2 | [POS] 5.2.2 Array and memory addressing
test_05_02_03_1 | [POS] 5.2.3 Strings
test_05_02_03_2_1 | [VAR] 5.2.3.2 String value padding/problems
test_05_03_00_1 | [POS] 5.3 Min:typ:max delay expressions
test_05_04_00_1 | [POS] 5.4 Expression bit lengths - 16/17 bits
test_05_04_02_1 | [VAR] 5.4.2 Expression bit-length problem
test_05_04_02_2 | [POS] 5.4.2 Expression bit-length problem
test_05_04_03_1 | [POS] 5.4.3 Self-determined expressions
test_05_05_00_1 | [POS] 5.5 Signed expressions
test_05_05_01_1 | [POS] 5.5.1 Rules for expression types
test_05_06_00_1 | [POS] 5.6 Assignments and truncation
test_05_06_00_2 | [POS] 5.6 Assignments and truncation
test_05_06_00_3 | [POS] 5.6 Assignments and truncation
test_06_01_01_1 | [POS] 6.1.1 Net declaration assignment
test_06_01_02_1 | [POS] 6.1.2 Continuous assignment statement
test_06_01_02_2 | [POS] 6.1.2 Continuous assign - 4-bit adder
test_06_01_02_3 | [POS] 6.1.2 Continuous assignment statement
test_06_01_03_1 | [POS] 6.1.3 Delays in continuous assignments
test_06_02_01_1 | [POS] 6.2.1 Variable declaration assignment
test_06_02_01_2 | [VAR] 6.2.1 Variable declaration assignment
test_06_02_01_3 | [POS] 6.2.1 Variable declaration assignment
test_06_02_01_4 | [POS] 6.2.1 Variable declaration assignment
test_06_02_01_5 | [POS] 6.2.1 Variable declaration assignment
test_07_01_02_1 | [POS] 7.1.2 Drive strength specification
test_07_01_05_1 | [VAR] 7.1.5 Range specification
test_07_01_06_1 | [POS] 7.1.6 Primitive instance connection list
test_07_01_06_2 | [POS] 7.1.6 Primitive instance connection list
test_07_01_06_3 | [POS] 7.1.6 Primitive instance connection list
test_07_01_06_4 | [POS] 7.1.6 Primitive instance connection list
test_07_02_00_1 | [POS] 7.2 and/nand/nor/or/xor/xnor gates
test_07_03_00_1 | [POS] 7.3 buf and not gates
test_07_04_00_1 | [POS] 7.4 bufif1/bufif0/notif1/notif0 gates
test_07_05_00_1 | [POS] 7.5 MOS switches
test_07_06_00_1 | [POS] 7.6 Bidirectional pass switches
test_07_07_00_1 | [POS] 7.7 CMOS switches
test_07_08_00_1 | [POS] 7.8 pullup and pulldown sources
test_07_14_00_1 | [POS] 7.14 Gate/net delays - 1/2/3 delays
test_07_14_00_2 | [POS] 7.14 Gate and net delays
test_07_14_01_1 | [POS] 7.14.1 min:typ:max delays
test_07_14_01_2 | [POS] 7.14.1 min:typ:max delays
test_07_14_02_2_1 | [POS] 7.14.2.2 Charge decay time delay spec
test_07_14_02_2_2 | [POS] 7.14.2.2 Charge decay time delay spec
test_08_02_00_1 | [POS] 8.2 Combinational UDPs
test_08_02_00_2 | [POS] 8.2 Combinational UDPs
test_08_03_00_1 | [POS] 8.3 Level-sensitive sequential UDPs
test_08_04_00_1 | [POS] 8.4 Edge-sensitive sequential UDPs
test_08_05_00_1 | [POS] 8.5 Sequential UDP initialization
test_08_05_00_2 | [POS] 8.5 Sequential UDP initialization
test_08_06_00_1 | [POS] 8.6 UDP instances
test_08_07_00_1 | [POS] 8.7 Mix level/edge-sensitive descriptions
test_09_01_00_1 | [POS] 9.1 Behavioral model overview
test_09_02_01_1 | [POS] 9.2.1 Blocking procedural assignments
test_09_02_02_1 | [POS] 9.2.2 Nonblocking procedural assignment
test_09_02_02_2 | [POS] 9.2.2 Nonblocking procedural assignment
test_09_02_02_3 | [POS] 9.2.2 Nonblocking procedural assignment
test_09_02_02_4 | [POS] 9.2.2 Nonblocking procedural assignment
test_09_02_02_5 | [POS] 9.2.2 Nonblocking procedural assignment
test_09_02_02_6 | [POS] 9.2.2 Nonblocking procedural assignment
test_09_02_02_7 | [POS] 9.2.2 Nonblocking procedural assignment
test_09_03_01_1 | [POS] 9.3.1 assign/deassign statements
test_09_03_02_1 | [POS] 9.3.2 force/release statements
test_09_03_02_2 | [POS] 9.3.2 force/release statements
test_09_04_00_1 | [POS] 9.4 Conditional statement
test_09_04_00_2 | [POS] 9.4 Conditional statement
test_09_04_00_3 | [POS] 9.4 Conditional statement
test_09_04_01_1 | [POS] 9.4.1 If-else-if construct
test_09_05_00_1 | [POS] 9.5 Case statement
test_09_05_00_2 | [POS] 9.5 Case statement
test_09_05_00_3 | [POS] 9.5 Case statement
test_09_05_01_1 | [POS] 9.5.1 casez - Instruction decode
test_09_05_01_2 | [POS] 9.5.1 casex - Extreme don't-cares
test_09_05_02_1 | [POS] 9.5.2 Constant expression in case
test_09_06_00_1 | [POS] 9.6 Looping statements
test_09_06_00_2 | [POS] 9.6 Looping statements
test_09_06_00_3 | [POS] 9.6 Looping statements
test_09_07_01_1 | [POS] 9.7.1 Delay control
test_09_07_01_2 | [POS] 9.7.1 Delay control
test_09_07_02_1 | [POS] 9.7.2 Event control
test_09_07_04_1 | [POS] 9.7.4 Event or operator
test_09_07_04_2 | [POS] 9.7.4 Event or operator
test_09_07_04_3 | [POS] 9.7.4 Event or operator
test_09_07_04_4 | [POS] 9.7.4 Event or operator
test_09_07_05_1 | [POS] 9.7.5 Event or operator
test_09_07_05_2 | [POS] 9.7.5 Event or operator
test_09_07_05_3 | [POS] 9.7.5 Event or operator
test_09_07_05_4 | [POS] 9.7.5 Event or operator
test_09_07_05_5 | [POS] 9.7.5 Event or operator
test_09_07_05_6 | [POS] 9.7.5 Event or operator
test_09_07_06_1 | [POS] 9.7.6 Level-sensitive event control
test_09_07_07_1 | [POS] 9.7.7 Intra-assignment timing controls
test_09_07_07_2 | [POS] 9.7.7 Intra-assignment timing controls
test_09_07_07_3 | [POS] 9.7.7 Intra-assignment timing controls
test_09_07_07_4 | [POS] 9.7.7 Intra-assignment timing controls
test_09_07_07_5 | [POS] 9.7.7 Intra-assignment timing controls
test_09_07_07_6 | [POS] 9.7.7 Intra-assignment timing controls
test_09_07_07_7 | [POS] 9.7.7 Intra-assignment timing controls
test_09_07_07_8 | [POS] 9.7.7 Repeat event - Intra-assignment
test_09_07_07_9 | [POS] 9.7.7 Repeat event - Intra-assignment
test_09_07_07_10 | [POS] 9.7.7 Repeat event with expressions
test_09_08_01_1 | [POS] 9.8.1 Sequential blocks
test_09_08_01_2 | [???] 9.8.1 Sequential blocks
test_09_08_01_3 | [POS] 9.8.1 Sequential blocks
test_09_08_02_1 | [POS] 9.8.2 Parallel blocks - Waveform
test_09_08_04_1 | [POS] 9.8.4 Start and finish times
test_09_08_04_2 | [POS] 9.8.4 Start and finish times
test_09_08_04_3 | [POS] 9.8.4 Start and finish times
test_09_09_01_1 | [POS] 9.9.1 Initial construct
test_09_09_01_2 | [POS] 9.9.1 Initial construct
test_09_09_02_1 | [POS] 9.9.2 Always construct - Infinite loop
test_09_09_02_2 | [POS] 9.9.2 Always construct
test_10_01_00_1 | [POS] 10.1 Distinctions tasks/functions
test_10_01_00_2 | [POS] 10.1 Distinctions tasks/functions
test_10_02_02_1_1 | [POS] 10.2.2 Task enabling/argument passing
test_10_02_02_1_2 | [POS] 10.2.2 Task enabling/argument passing
test_10_02_02_2 | [POS] 10.2.2 Task enabling/argument passing
test_10_03_00_1 | [POS] 10.3 Disabling named blocks/tasks
test_10_03_00_2 | [POS] 10.3 Disabling named blocks/tasks
test_10_03_00_3 | [POS] 10.3 Disabling named blocks/tasks
test_10_03_00_4 | [POS] 10.3 Disabling named blocks/tasks
test_10_03_00_5 | [POS] 10.3 Disabling named blocks/tasks
test_10_03_00_6 | [POS] 10.3 Disabling named blocks/tasks
test_10_04_01_1 | [POS] 10.4.1 Function declarations
test_10_04_01_2 | [POS] 10.4.1 Function declarations
test_10_04_03_1 | [POS] 10.4.3 Calling a function
test_10_04_04_1 | [POS] 10.4.4 Function rules
test_10_04_05_1 | [POS] 10.4.5 Use of constant functions
test_11_04_01_1 | [POS] 11.4.1 Determinism
test_11_05_00_1 | [POS] 11.5 Determinism
test_12_01_02_1 | [POS] 12.1.2 Module instantiation
test_12_01_02_2 | [POS] 12.1.2 Module instantiation
test_12_02_00_1 | [POS] 12.2 Overriding module parameter values
test_12_02_00_2 | [POS] 12.2 Override params - Float 3.1415
test_12_02_01_1 | [VAR] 12.2.1 defparam statement
test_12_02_01_2 | [POS] 12.2.1 defparam statement
test_12_02_02_1_1 | [POS] 12.2.2.1 Param value by ordered list
test_12_02_02_1_2 | [POS] 12.2.2.1 Param declaration expression
test_12_02_02_2_1 | [POS] 12.2.2.2 Param value assignment by name
test_12_02_02_2_2 | [VAR] 12.2.2.2 Param value assignment by name
test_12_02_03_1 | [POS] 12.2.3 Parameter dependence
test_12_03_03_1 | [VAR] 12.3.3 Port declarations
test_12_03_03_2 | [NEG] 12.3.3 Port declarations
test_12_03_03_3 | [POS] 12.3.3 Port declarations
test_12_03_05_1 | [POS] 12.3.5 Connect ports by ordered list
test_12_03_06_1 | [POS] 12.3.6 Connect ports by name
test_12_03_06_2 | [POS] 12.3.6 Connect ports by name
test_12_03_06_3 | [VAR] 12.3.6 Connect ports by name
test_12_03_07_1 | [POS] 12.3.7 Real numbers in port connections
test_12_04_01_1 | [VAR] 12.4.1 Loop generate - Legal/illegal
test_12_04_01_2 | [POS] 12.4.1 Loop generate constructs
test_12_04_01_3 | [POS] 12.4.1 Loop generate constructs
test_12_04_01_4 | [POS] 12.4.1 Loop generate constructs
test_12_04_01_5 | [POS] 12.4.1 Loop generate constructs
test_12_04_02_1 | [POS] 12.4.2 Conditional generate constructs
test_12_04_02_2 | [POS] 12.4.2 Conditional generate constructs
test_12_04_02_3 | [POS] 12.4.2 Conditional generate constructs
test_12_04_02_4 | [POS] 12.4.2 Conditional generate constructs
test_12_04_03_1 | [POS] 12.4.3 External names - Unnamed generate
test_12_05_00_1 | [POS] 12.5 Hierarchical names
test_12_05_00_2 | [POS] 12.5 Hierarchical names
test_12_06_00_1 | [POS] 12.6 Upwards name referencing
test_12_07_00_1 | [POS] 12.7 Scope rules
test_12_08_02_1 | [NEG] 12.8.2 Early resolution of hierarchical names
test_14_01_00_1 | [POS] 14.1 Specify block declaration
test_14_02_02_1 | [POS] 14.2.2 Simple module paths
test_14_02_03_1 | [POS] 14.2.3 Edge-sensitive paths
test_14_02_03_2 | [POS] 14.2.3 Edge-sensitive paths
test_14_02_03_3 | [POS] 14.2.3 Edge-sensitive paths
test_14_02_04_2_1 | [POS] 14.2.4.2 Simple state-dependent paths
test_14_02_04_2_2 | [POS] 14.2.4.2 Simple state-dependent paths
test_14_02_04_3_1 | [POS] 14.2.4.3 Edge-sensitive state-dependent
test_14_02_04_3_2 | [POS] 14.2.4.3 Edge-sensitive state-dependent
test_14_02_04_3_3 | [POS] 14.2.4.3 Edge-sensitive state-dependent
test_14_02_04_3_4 | [VAR] 14.2.4.3 Illegal state-dependent paths
test_14_02_04_4_1 | [POS] 14.2.4.4 ifnone condition - Valid combo
test_14_02_04_4_2 | [POS] 14.2.4.4 ifnone condition - Illegal combo
test_14_02_05_1 | [POS] 14.2.5 Full/parallel connection paths
test_14_02_06_1 | [POS] 14.2.6 Multiple paths in single statement
test_14_02_07_1_1 | [POS] 14.2.7.1 Unknown polarity
test_14_02_07_2_1 | [POS] 14.2.7.2 Positive polarity
test_14_02_07_3_1 | [POS] 14.2.7.3 Negative polarity
test_14_03_00_1 | [POS] 14.3 Assigning delays to module paths
test_14_03_01_1 | [POS] 14.3.1 Transition delays on module paths
test_14_03_01_2 | [POS] 14.3.1 Transition delays on module paths
test_14_03_01_3 | [POS] 14.3.1 Transition delays on module paths
test_14_03_01_4 | [POS] 14.3.1 Transition delays on module paths
test_14_03_01_5 | [POS] 14.3.1 Transition delays on module paths
test_14_03_03_1 | [POS] 14.3.3 Delay selection
test_14_03_03_2 | [VAR] 14.3.3 Delay selection
test_14_06_01_1 | [POS] 14.6.1 Pulse limit values control
test_14_06_04_2_1 | [POS] 14.6.4.2 Negative pulse detection
test_14_06_04_2_2 | [VAR] 14.6.4.2 Negative pulse detection
test_14_06_04_2_3 | [POS] 14.6.4.2 Negative pulse detection
test_14_06_04_2_4 | [POS] 14.6.4.2 Negative pulse detection
test_15_02_03_1 | [POS] 15.2.3 $setuphold
test_15_02_06_1 | [POS] 15.2.6 $recrem
test_15_03_02_1 | [POS] 15.3.2 $timeskew
test_15_03_03_1 | [POS] 15.3.3 $fullskew
test_15_03_04_1 | [POS] 15.3.4 $width
test_15_03_04_2 | [VAR] 15.3.4 $width - Legal/illegal calls
test_15_03_06_1 | [POS] 15.3.6 $nochange
test_15_04_00_1 | [POS] 15.4 Edge-control specifiers
test_15_05_00_1 | [POS] 15.5 Notifiers - User-defined responses
test_15_05_00_2 | [POS] 15.5 Notifiers - User-defined responses
test_15_05_01_1 | [POS] 15.5.1 Requirements for accurate simulation
test_15_05_01_2 | [POS] 15.5.1 Requirements for accurate simulation
test_15_05_01_3 | [POS] 15.5.1 Requirements for accurate simulation
test_15_05_01_4 | [VAR] 15.5.1 Requirements for accurate simulation
test_15_05_02_1 | [POS] 15.5.2 Conditions in negative timing checks
test_15_05_02_2 | [POS] 15.5.2 Conditions in negative timing checks
test_15_05_02_3 | [POS] 15.5.2 Conditions in negative timing checks
test_15_06_00_1 | [POS] 15.6 Conditioned vs unconditioned timing
test_15_06_00_2 | [POS] 15.6 Conditioned event timing checks
test_15_06_00_3 | [POS] 15.6 Setup check on positive edge
test_15_07_00_1 | [POS] 15.7 Vector signals in timing checks
test_16_02_01_1 | [POS] 16.2.1 SDF delay to Verilog mapping
test_16_02_01_2 | [POS] 16.2.1 SDF delay to Verilog mapping
test_16_02_01_3 | [POS] 16.2.1 SDF delay to Verilog mapping
test_16_02_02_1 | [POS] 16.2.2 SDF timing checks to Verilog
test_16_02_02_2 | [VAR] 16.2.2 SDF timing checks to Verilog
test_16_02_02_3 | [VAR] 16.2.2 SDF timing checks to Verilog
test_16_02_03_1 | [POS] 16.2.3 SDF annotation of specparams
test_17_01_01_1_1 | [POS] 17.1.1.1 Escape sequences - Special chars
test_17_01_01_2_1 | [POS] 17.1.1.2 Format specifications
test_17_01_01_3_1 | [POS] 17.1.1.3 Size of displayed data
test_17_01_01_3_2 | [POS] 17.1.1.3 Size of displayed data
test_17_01_01_4_1 | [POS] 17.1.1.4 Unknown/high-impedance values
test_17_01_01_5_1 | [POS] 17.1.1.5 Strength format
test_17_01_02_1 | [POS] 17.1.2 Strobed monitoring
test_17_02_02_1 | [POS] 17.2.2 File output system tasks
test_17_02_04_1_1 | [POS] 17.2.4.1 $fgetc - Read char from file
test_17_02_04_1_2 | [POS] 17.2.4.1 $ungetc - Insert char to buffer
test_17_02_04_2_1 | [POS] 17.2.4.2 Reading a line at a time
test_17_02_04_3_1 | [POS] 17.2.4.3 Reading formatted data
test_17_02_04_4_1 | [POS] 17.2.4.4 Reading binary data
test_17_02_05_1 | [POS] 17.2.5 $ftell - File positioning
test_17_02_05_2 | [POS] 17.2.5 $fseek - Set file position
test_17_02_06_1 | [POS] 17.2.6 Flushing output
test_17_02_07_1 | [POS] 17.2.7 Flushing output
test_17_02_08_1 | [POS] 17.2.8 Detecting EOF
test_17_02_09_1 | [POS] 17.2.9 Loading memory data from file
test_17_03_01_1 | [POS] 17.3.1 $printtimescale
test_17_03_02_1 | [POS] 17.3.2 $timeformat
test_17_05_01_1 | [POS] 17.5.1 PLA array types
test_17_05_02_1 | [POS] 17.5.2 PLA array logic types
test_17_05_03_1 | [POS] 17.5.3 PLA personality declaration/loading
test_17_05_04_1 | [POS] 17.5.4 PLA personality formats
test_17_05_04_2 | [POS] 17.5.4 PLA personality formats
test_17_05_04_3 | [POS] 17.5.4 PLA plane format usage
test_17_07_01_1 | [POS] 17.7.1 $time
test_17_07_03_1 | [POS] 17.7.3 $realtime
test_17_08_00_1 | [POS] 17.8 Conversion functions
test_17_09_01_1 | [VAR] 17.9.1 $random function
test_17_09_01_2 | [VAR] 17.9.1 $random with concatenation
test_17_10_01_1 | [POS] 17.10.1 $test$plusargs
test_17_10_02_1 | [POS] 17.10.2 $value$plusargs
test_17_11_01_1 | [POS] 17.11.1 Integer math functions
test_17_11_02_01 | [POS] 17.11.2 Real math functions
test_17_11_02_02 | [POS] 17.11.2 Real math functions
test_17_11_02_03 | [POS] 17.11.2 Real math functions
test_17_11_02_04 | [POS] 17.11.2 Real math functions
test_17_11_02_05 | [POS] 17.11.2 Real math functions
test_17_11_02_06 | [POS] 17.11.2 Real math functions
test_17_11_02_07 | [POS] 17.11.2 Real math functions
test_17_11_02_08 | [POS] 17.11.2 Real math functions
test_17_11_02_09 | [POS] 17.11.2 Real math functions
test_17_11_02_10 | [POS] 17.11.2 Real math functions
test_17_11_02_11 | [POS] 17.11.2 Real math functions
test_17_11_02_12 | [POS] 17.11.2 Real math functions
test_17_11_02_13 | [POS] 17.11.2 Real math functions
test_17_11_02_14 | [POS] 17.11.2 Real math functions
test_17_11_02_15 | [POS] 17.11.2 Real math functions
test_17_11_02_16 | [POS] 17.11.2 Real math functions
test_17_11_02_17 | [POS] 17.11.2 Real math functions
test_17_11_02_18 | [POS] 17.11.2 Real math functions
test_17_11_02_19 | [POS] 17.11.2 Real math functions
test_17_11_02_20 | [POS] 17.11.2 Real math functions
test_17_11_02_21 | [POS] 17.11.2 Real math functions
test_18_01_01_1 | [POS] 18.1.1 $dumpfile - VCD file name
test_18_01_02_1 | [POS] 18.1.2 $dumpvars - Dump all vars depth 1
test_18_01_02_2 | [POS] 18.1.2 $dumpvars - Dump module vars
test_18_01_02_3 | [POS] 18.1.2 $dumpvars - Module and variables
test_18_01_03_1 | [POS] 18.1.3 $dumpoff/$dumpon
test_18_01_06_1 | [POS] 18.1.6 $dumpflush - Read dump during sim
test_18_01_06_2 | [POS] 18.1.6 VCD file production example
test_18_04_02_1 | [POS] 18.4.2 Extended VCD node information
test_19_03_01_1 | [POS] 19.3.1 `define
test_19_03_01_2 | [VAR] 19.3.1 `define
test_19_04_00_1 | [POS] 19.4 `ifdef conditional compilation
test_19_04_00_2 | [POS] 19.4 Nested conditional directives
test_19_04_00_3 | [POS] 19.4 Chained nested conditional directives
test_19_05_00_1 | [POS] 19.5 `include
test_19_07_00_1 | [POS] 19.7 `line
test_19_08_00_1 | [POS] 19.8 `timescale
test_19_08_00_2 | [POS] 19.8 `timescale
test_19_08_00_3 | [POS] 19.8 `timescale - 10 ns precision
test_19_11_00_1 | [POS] 19.11 `begin_keywords, `end_keywords
test_19_11_00_2 | [VAR] 19.11 `begin_keywords, `end_keywords
```

---

## Test File Location

All test files are located in: `/Users/tom/cpp/metalfpga/goldentests/`

Files follow pattern: `test_XX_YY_ZZ_N.v` where XX=chapter, YY=section, ZZ=subsection, N=sequence

## Quick Search Tips

```bash
# Find all arithmetic operator tests
grep "5.1.5" IEEE_1364_2005_TESTS_QUICK_REF.md

# Find all negative tests (should fail compilation)
grep "\[NEG\]" IEEE_1364_2005_TESTS_QUICK_REF.md

# Find all Chapter 9 behavioral modeling tests
grep "^test_09_" IEEE_1364_2005_TESTS_QUICK_REF.md

# Find all VARYING tests (implementation-defined behavior)
grep "\[VAR\]" IEEE_1364_2005_TESTS_QUICK_REF.md
```

---

**Document Version**: 1.0
**Source**: ISPRAS SystemVerilog Test Suite
**Standard**: IEEE Std 1364-2005
**Total Tests**: 355
