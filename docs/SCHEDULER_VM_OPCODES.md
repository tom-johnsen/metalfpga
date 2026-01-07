# Scheduler VM Opcodes Reference

This document describes all opcodes used in the MetalFPGA Scheduler VM bytecode system. as of REV43

## Overview

The Scheduler VM is a bytecode interpreter that executes Verilog behavioral code on the GPU. It uses a compact instruction encoding where each instruction is a 32-bit word with an 8-bit opcode and a 24-bit argument field.

**Instruction Encoding:**
- Bits 0-7: Opcode
- Bits 8-31: Argument (24 bits)

**Constants:**
- `kSchedulerVmOpMask = 0xFF`
- `kSchedulerVmOpShift = 8`
- `kSchedulerVmWordsPerProc = 2`
- `kSchedulerVmCallFrameWords = 4`
- `kSchedulerVmCallFrameDepth = 1`

---

## Main VM Opcodes

Defined in `SchedulerVmOp` enum ([scheduler_vm.hh:9-38](src/core/scheduler_vm.hh#L9-L38)):

### Control Flow Opcodes

#### `kDone` (0)
Marks the end of a process or execution block.

**Usage:** End of process execution, return to scheduler

---

#### `kCallGroup` (1)
Calls a group of processes or functions.

**Usage:** Process group invocation

---

#### `kNoop` (2)
No operation. Does nothing but may be used for alignment or padding.

**Usage:** Padding, alignment, placeholder

---

#### `kJump` (3)
Unconditional jump to a target bytecode address.

**Argument:** Target bytecode offset

**Usage:** Unconditional control flow (goto, loop continuation)

---

#### `kJumpIf` (4)
Conditional jump based on expression evaluation.

**Argument:** Target bytecode offset (condition evaluated separately)

**Usage:** If statements, conditional loops

---

#### `kCase` (5)
Case/switch statement dispatcher. Uses a case table to determine jump target.

**Argument:** Index into case header table

**Related Structures:**
- `SchedulerVmCaseHeader` - Defines case strategy and entries
- `SchedulerVmCaseEntry` - Individual case branch
- `SchedulerVmCaseKind` - kCase, kCaseX, kCaseZ
- `SchedulerVmCaseStrategy` - kLinear, kBucket, kLut

---

#### `kRepeat` (6)
Repeat loop construct.

**Argument:** Index into repeat expression table

**Usage:** `repeat(N) begin ... end`

---

### Assignment Opcodes

#### `kAssign` (7)
Blocking procedural assignment.

**Argument:** Index into assign entry table

**Flags:**
- `kSchedulerVmAssignFlagNonblocking` (1 << 0) - Should be 0 for blocking
- `kSchedulerVmAssignFlagFallback` (1 << 1) - Use fallback execution path
- `kSchedulerVmAssignFlagWideConst` (1 << 6) - RHS is a wide literal stored in
  the expr imm table (skip expr eval, copy words directly)

**Related Structure:** `SchedulerVmAssignEntry`
- `flags` - Assignment flags
- `signal_id` - Target signal identifier
- `rhs_expr` - Right-hand side expression offset

**Usage:** `signal = expr;`

---

#### `kAssignNb` (8)
Non-blocking procedural assignment.

**Argument:** Index into assign entry table

**Flags:** Same as `kAssign` but nonblocking flag should be set

**Usage:** `signal <= expr;`

---

#### `kAssignDelay` (9)
Delayed assignment (blocking or non-blocking with delay).

**Argument:** Index into delay assign entry table

**Flags:**
- `kSchedulerVmDelayAssignFlagNonblocking` (1 << 0)
- `kSchedulerVmDelayAssignFlagInertial` (1 << 1)
- `kSchedulerVmDelayAssignFlagShowcancelled` (1 << 2)
- `kSchedulerVmDelayAssignFlagHasPulse` (1 << 3)
- `kSchedulerVmDelayAssignFlagHasPulseError` (1 << 4)
- `kSchedulerVmDelayAssignFlagIsArray` (1 << 5)
- `kSchedulerVmDelayAssignFlagIsBitSelect` (1 << 6)
- `kSchedulerVmDelayAssignFlagIsRange` (1 << 7)
- `kSchedulerVmDelayAssignFlagIsIndexedRange` (1 << 8)
- `kSchedulerVmDelayAssignFlagIsReal` (1 << 9)
- `kSchedulerVmDelayAssignFlagFallback` (1 << 10)

**Related Structure:** `SchedulerVmDelayAssignEntry`
- Includes delay expression, index expression, width, pulse control

**Usage:** `#delay signal <= expr;` or `signal = #delay expr;`

---

#### `kForce` (10)
Procedural force on a signal (overrides continuous assignments).

**Argument:** Index into force entry table

**Flags:**
- `kSchedulerVmForceFlagProcedural` (1 << 0)
- `kSchedulerVmForceFlagFallback` (1 << 1)
- `kSchedulerVmForceFlagOverrideReg` (1 << 2)

**Related Structure:** `SchedulerVmForceEntry`

**Usage:** `force signal = expr;`

---

#### `kRelease` (11)
Release a previously forced signal.

**Argument:** Index into release entry table

**Related Structure:** `SchedulerVmReleaseEntry`

**Usage:** `release signal;`

---

### Wait/Synchronization Opcodes

#### `kWaitTime` (12)
Wait for a specific time delay.

**Argument:** Time delay value or expression index

**Usage:** `#delay;`

---

#### `kWaitDelta` (13)
Wait for one delta cycle (zero-time delay).

**Usage:** `#0;`

---

#### `kWaitEvent` (14)
Wait for an event trigger.

**Argument:** Event identifier

**Usage:** `@(event_name);`

---

#### `kWaitEdge` (15)
Wait for edge-sensitive event (posedge/negedge/any change).

**Argument:** Edge descriptor index

**Usage:** `@(posedge clk)`, `@(negedge reset)`, `@(signal)`

---

#### `kWaitCond` (16)
Wait for a condition to become true.

**Argument:** Index into condition entry table

**Related Structure:** `SchedulerVmCondEntry`
- `kind` - kDynamic, kConst, kExpr
- `val` - Constant value (if kConst)
- `xz` - X/Z bits
- `expr_offset` - Expression offset (if kExpr)

**Related Enum:** `SchedulerVmCondKind`

**Usage:** `wait(condition);`

---

#### `kWaitJoin` (17)
Wait for fork-join completion.

**Argument:** Join descriptor (includes join kind)

**Join Kinds:**
- `kAll` (0) - Wait for all forked processes (join)
- `kAny` (1) - Wait for any forked process (join_any)
- `kNone` (2) - Don't wait (join_none)

**Usage:** Part of `fork...join` constructs

---

#### `kWaitService` (18)
Wait for a system service call to complete.

**Argument:** Service entry index

**Usage:** Internal synchronization for system tasks

---

### Event and Process Control

#### `kEventTrigger` (19)
Trigger a named event.

**Argument:** Event identifier

**Usage:** `-> event_name;`

---

#### `kFork` (20)
Fork parallel processes.

**Argument:** Packed value containing:
- Bits 0-23: Fork count (number of parallel branches)
- Bits 24-31: Join kind (SchedulerVmJoinKind)

**Helper Functions:**
- `PackSchedulerVmForkArg(count, kind)` - Create argument
- `DecodeSchedulerVmForkCount(arg)` - Extract count
- `DecodeSchedulerVmForkKind(arg)` - Extract join kind

**Usage:** `fork ... join/join_any/join_none`

---

#### `kDisable` (21)
Disable a block, child process, or cross-process.

**Argument:** Disable descriptor

**Disable Kinds:**
- `kBlock` (0) - Disable named block
- `kChildProc` (1) - Disable child process
- `kCrossProc` (2) - Disable across process boundaries

**Usage:** `disable block_name;`

---

### System Tasks and Functions

#### `kServiceCall` (22)
Call a system task or function.

**Argument:** Index into service entry table

**Flags:**
- `kSchedulerVmServiceFlagFallback` (1 << 0)
- `kSchedulerVmServiceFlagGlobalOnly` (1 << 1)
- `kSchedulerVmServiceFlagGuardFd` (1 << 2)
- `kSchedulerVmServiceFlagMonitor` (1 << 3)
- `kSchedulerVmServiceFlagMonitorOn` (1 << 4)
- `kSchedulerVmServiceFlagMonitorOff` (1 << 5)
- `kSchedulerVmServiceFlagStrobe` (1 << 6)
- `kSchedulerVmServiceFlagFinish` (1 << 7)
- `kSchedulerVmServiceFlagStop` (1 << 8)

**Service Argument Flags:**
- `kSchedulerVmServiceArgFlagExpr` (1 << 0)
- `kSchedulerVmServiceArgFlagTime` (1 << 1)
- `kSchedulerVmServiceArgFlagStime` (1 << 2)

**Related Structures:**
- `SchedulerVmServiceEntry` - Service call descriptor
- `SchedulerVmServiceArg` - Individual argument descriptor

**Usage:** `$display()`, `$monitor()`, `$finish()`, etc.

---

#### `kServiceRetAssign` (23)
Assign return value from system function to a signal.

**Argument:** Index into service return assign table

**Related Structure:** `SchedulerVmServiceRetAssignEntry`

**Usage:** `x = $random();`, `fd = $fopen(...);`

---

#### `kServiceRetBranch` (24)
Branch based on system function return value.

**Argument:** Branch target offset

**Usage:** Conditional execution based on system function results

---

### Task Call Opcodes

#### `kTaskCall` (25)
Call a user-defined task.

**Argument:** Task identifier or offset

**Usage:** `task_name(args);`

---

#### `kRet` (26)
Return from task or function.

**Usage:** Implicit at end of task/function

---

### Simulation Control

#### `kHaltSim` (27)
Halt simulation.

**Argument:** Halt kind
- `GPGA_SCHED_HALT_FINISH` (0) - Normal finish
- `GPGA_SCHED_HALT_STOP` (1) - Stop simulation
- `GPGA_SCHED_HALT_ERROR` (2) - Error halt

**Usage:** `$finish;`, `$stop;`, internal error handling

---

## Expression VM Opcodes

Defined in `SchedulerVmExprOp` enum ([scheduler_vm.hh:58-70](src/core/scheduler_vm.hh#L58-L70)):

The expression VM uses a stack-based architecture to evaluate expressions.

**Constants:**
- `kSchedulerVmExprStackMax = 32` - Maximum expression stack depth
- `kSchedulerVmExprNoExtra = 0xFFFFFFFF` - No extra word follows
- `kSchedulerVmExprSignedFlag = 1 << 8` - Signed operation flag

### Expression Opcodes

#### `kDone` (0)
End of expression bytecode.

---

#### `kPushConst` (1)
Push a constant value onto the expression stack.

**Argument:** Index into constant/literal pool

---

#### `kPushConstXz` (11)
Push a constant value with explicit X/Z bits onto the expression stack.

**Argument:** Index into constant/literal pool (4 words: val_lo, val_hi, xz_lo, xz_hi)

---

#### `kPushSignal` (2)
Push a signal's current value onto the expression stack.

**Argument:** Signal identifier

---

#### `kPushImm` (3)
Push an immediate value encoded in the instruction.

**Argument:** Immediate value (24 bits) or index into immediate pool

---

#### `kUnary` (4)
Perform unary operation on top stack value.

**Argument:** SchedulerVmExprUnaryOp value

**Unary Operations:**
- `kPlus` (0) - Unary +
- `kMinus` (1) - Unary -
- `kBitNot` (2) - Bitwise NOT (~)
- `kLogNot` (3) - Logical NOT (!)
- `kRedAnd` (4) - Reduction AND (&)
- `kRedNand` (5) - Reduction NAND (~&)
- `kRedOr` (6) - Reduction OR (|)
- `kRedNor` (7) - Reduction NOR (~|)
- `kRedXor` (8) - Reduction XOR (^)
- `kRedXnor` (9) - Reduction XNOR (~^, ^~)

---

#### `kBinary` (5)
Perform binary operation on top two stack values.

**Argument:** SchedulerVmExprBinaryOp value (may include signed flag)

**Binary Operations:**
- `kAdd` (0) - Addition (+)
- `kSub` (1) - Subtraction (-)
- `kMul` (2) - Multiplication (*)
- `kDiv` (3) - Division (/)
- `kMod` (4) - Modulo (%)
- `kPow` (5) - Power (**)
- `kShl` (6) - Shift left (<<)
- `kShr` (7) - Shift right (>>)
- `kAshr` (8) - Arithmetic shift right (>>>)
- `kAnd` (9) - Bitwise AND (&)
- `kOr` (10) - Bitwise OR (|)
- `kXor` (11) - Bitwise XOR (^)
- `kXnor` (12) - Bitwise XNOR (~^, ^~)
- `kLogAnd` (13) - Logical AND (&&)
- `kLogOr` (14) - Logical OR (||)
- `kEq` (15) - Equality (==)
- `kNeq` (16) - Inequality (!=)
- `kCaseEq` (17) - Case equality (===)
- `kCaseNeq` (18) - Case inequality (!==)
- `kLt` (19) - Less than (<)
- `kLe` (20) - Less than or equal (<=)
- `kGt` (21) - Greater than (>)
- `kGe` (22) - Greater than or equal (>=)

---

#### `kTernary` (6)
Ternary conditional operator (? :).

Pops three values: condition, true_value, false_value

---

#### `kSelect` (7)
Bit or part select operation.

**Usage:** `signal[bit]`, `signal[msb:lsb]`

---

#### `kIndex` (8)
Array indexing operation.

**Usage:** `array[index]`

---

#### `kConcat` (9)
Concatenation operation.

**Argument:** Number of values to concatenate

**Usage:** `{a, b, c}`

---

#### `kCall` (10)
Call a system function within expression.

**Argument:** SchedulerVmExprCallOp value

**System Function Operations:**
- `kTime` (0) - $time
- `kStime` (1) - $stime
- `kRealtime` (2) - $realtime
- `kIToR` (3) - $itor (integer to real)
- `kBitsToReal` (4) - $bitstoreal
- `kRealToBits` (5) - $realtobits
- `kRToI` (6) - $rtoi (real to integer)
- `kLog10` (7) - $log10
- `kLn` (8) - $ln
- `kExp` (9) - $exp
- `kSqrt` (10) - $sqrt
- `kFloor` (11) - $floor
- `kCeil` (12) - $ceil
- `kSin` (13) - $sin
- `kCos` (14) - $cos
- `kTan` (15) - $tan
- `kAsin` (16) - $asin
- `kAcos` (17) - $acos
- `kAtan` (18) - $atan
- `kSinh` (19) - $sinh
- `kCosh` (20) - $cosh
- `kTanh` (21) - $tanh
- `kAsinh` (22) - $asinh
- `kAcosh` (23) - $acosh
- `kAtanh` (24) - $atanh
- `kPow` (25) - $pow
- `kAtan2` (26) - $atan2
- `kHypot` (27) - $hypot

---

## Data Structures

### Signal Entry
```cpp
struct SchedulerVmSignalEntry {
  uint32_t val_slot;      // Slot index for value storage
  uint32_t xz_slot;       // Slot index for X/Z bits
  uint32_t width;         // Bit width
  uint32_t array_size;    // Array size (1 for scalars)
  uint32_t flags;         // kSchedulerVmSignalFlagReal
};
```

**Flags:**
- `kSchedulerVmSignalFlagReal` (1 << 0) - Signal is real type

---

### Expression Table
```cpp
struct SchedulerVmExprTable {
  std::vector<uint32_t> words;      // Expression bytecode stream
  std::vector<uint32_t> imm_words;  // Literal pool storage
};
```

---

### VM Layout
```cpp
struct SchedulerVmLayout {
  uint32_t proc_count;                                 // Number of processes
  uint32_t words_per_proc;                             // Words per process slot
  std::vector<uint32_t> bytecode;                      // Main bytecode array
  std::vector<uint32_t> proc_offsets;                  // Process start offsets
  std::vector<uint32_t> proc_lengths;                  // Process lengths
  std::vector<SchedulerVmPackedSlot> packed_slots;     // Signal storage slots
  std::vector<SchedulerVmSignalEntry> signal_entries;  // Signal descriptors
  std::vector<SchedulerVmCondEntry> cond_entries;      // Wait conditions
  std::vector<SchedulerVmCaseHeader> case_headers;     // Case headers
  std::vector<SchedulerVmCaseEntry> case_entries;      // Case entries
  std::vector<uint64_t> case_words;                    // Case match values
  std::vector<SchedulerVmAssignEntry> assign_entries;  // Assignment entries
  std::vector<SchedulerVmDelayAssignEntry> delay_assign_entries;
  std::vector<SchedulerVmForceEntry> force_entries;
  std::vector<SchedulerVmReleaseEntry> release_entries;
  std::vector<SchedulerVmServiceEntry> service_entries;
  std::vector<SchedulerVmServiceArg> service_args;
  std::vector<SchedulerVmServiceRetAssignEntry> service_ret_entries;
  SchedulerVmExprTable expr_table;                     // Expression bytecode
  std::vector<uint32_t> edge_item_expr_offsets;        // Edge expressions
  std::vector<uint32_t> edge_star_expr_offsets;        // Star expressions
  std::vector<uint32_t> repeat_expr_offsets;           // Repeat counts
};
```

---

## Helper Functions

### Instruction Encoding/Decoding

```cpp
// Create instruction word
constexpr uint32_t MakeSchedulerVmInstr(SchedulerVmOp op, uint32_t arg = 0u);
constexpr uint32_t MakeSchedulerVmExprInstr(SchedulerVmExprOp op, uint32_t arg = 0u);

// Decode instruction word
constexpr SchedulerVmOp DecodeSchedulerVmOp(uint32_t instr);
constexpr uint32_t DecodeSchedulerVmArg(uint32_t instr);

// Fork argument packing
constexpr uint32_t PackSchedulerVmForkArg(uint32_t count, SchedulerVmJoinKind kind);
constexpr uint32_t DecodeSchedulerVmForkCount(uint32_t arg);
constexpr SchedulerVmJoinKind DecodeSchedulerVmForkKind(uint32_t arg);
```

---

## Builder Classes

### SchedulerVmBuilder
Used during code generation to emit main VM opcodes:

```cpp
class SchedulerVmBuilder {
  void Emit(SchedulerVmOp op, uint32_t arg = 0u);
  void EmitCallGroup();
  void EmitDone();
  const std::vector<uint32_t>& words() const;
};
```

### SchedulerVmExprBuilder
Used to build expression bytecode:

```cpp
class SchedulerVmExprBuilder {
  uint32_t EmitOp(SchedulerVmExprOp op, uint32_t arg = 0u,
                  uint32_t extra = kSchedulerVmExprNoExtra);
  uint32_t EmitImmTable(const std::vector<uint32_t>& words);
  const std::vector<uint32_t>& words() const;
  const std::vector<uint32_t>& imm_words() const;
  void Truncate(size_t word_size, size_t imm_size);
};
```

---

## Example Bytecode

### Simple Process
```
kCallGroup    // Execute combinational logic
kDone         // End of process
```

### Conditional Assignment
```
kJumpIf, 10   // If condition false, jump to offset 10
kAssign, 0    // True branch: assign entry 0
kJump, 12     // Skip false branch
kAssign, 1    // False branch: assign entry 1 (offset 10)
kDone         // End (offset 12)
```

### Fork-Join
```
kFork, <packed(count=3, kind=kAll)>  // Fork 3 parallel branches
  // Branch 0 code...
  // Branch 1 code...
  // Branch 2 code...
kWaitJoin, 0  // Wait for all to complete
kDone
```

---

## References

- Main VM opcodes: [scheduler_vm.hh:9-38](src/core/scheduler_vm.hh#L9-L38)
- Expression opcodes: [scheduler_vm.hh:58-70](src/core/scheduler_vm.hh#L58-L70)
- Unary operations: [scheduler_vm.hh:72-83](src/core/scheduler_vm.hh#L72-L83)
- Binary operations: [scheduler_vm.hh:85-109](src/core/scheduler_vm.hh#L85-L109)
- System functions: [scheduler_vm.hh:111-140](src/core/scheduler_vm.hh#L111-L140)
- Code generation: [msl_codegen.cc](src/codegen/msl_codegen.cc)
- GPU runtime header: [gpga_sched.h](include/gpga_sched.h)
