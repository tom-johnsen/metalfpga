# REV14 - Event queue infrastructure and timing system

**Commit:** 5cf03ba
**Date:** Fri Dec 26 02:20:18 2025 +0100
**Message:** more keywords added, working on event queue

## Overview

Massive implementation commit adding event queue infrastructure for timing and event-driven simulation. This is foundational work for runtime execution, implementing the core machinery needed for Verilog's event semantics. Adds parsing and AST support for tasks, events, timing controls (delays, wait, forever, fork/join), and significantly expands the parser (+2,212 lines) and codegen (+763 lines). Also adds complete net type infrastructure (wand, wor, tri variants), drive strength, and switch primitives. This represents the beginning of v0.4 development focusing on simulation runtime.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | Added tasks, events, timing controls, net types, switches (+2,212 lines) |
| **Elaborate** | ✓ Enhanced | Event queue setup, timing expansion (+352 lines) |
| **Codegen (2-state)** | ✓ MSL emission | Event queue infrastructure (+763 lines) |
| **Codegen (4-state)** | ✓ Complete | Extended for event handling |
| **Host emission** | ✗ Stubbed only | Minor updates (+186 lines in main.mm) |
| **Runtime** | ⚠ In Progress | Event queue foundation laid, not yet executable |

## User-Visible Changes

**New Parser Support (not yet fully functional):**
- **Tasks**: `task` declarations with input/output/inout arguments
- **Events**: `event` declarations and triggers (`-> event_name`)
- **Timing controls**: `#delay`, `@(event)`, `wait(condition)`
- **Advanced loops**: `forever`
- **Concurrency**: `fork`/`join` parallel execution
- **Control**: `disable` statement for block termination
- **Task calls**: Procedural task invocation

**Net Types (fully parsed):**
- All special net types: wand, wor, tri0, tri1, triand, trior, trireg, supply0, supply1
- Drive strength specifications
- Switch primitives: tran, tranif0/1, cmos

**Event Queue System (infrastructure):**
- Time-based event scheduling
- Delta-cycle simulation semantics
- Event queue management in MSL
- Non-blocking assignment scheduling

**No New Passing Tests:**
- This is implementation-only commit
- Features parsed but runtime not ready
- Tests remain in `verilog/` (unimplemented)

## Architecture Changes

### Frontend: AST Extensions for Events and Timing

**File**: `src/frontend/ast.hh` (+98 lines)

**New enums and structures:**

**Net types (fully enumerated):**
```cpp
enum class NetType {
  kWire,
  kReg,
  kWand,      // NEW: Wired-AND
  kWor,       // NEW: Wired-OR
  kTri0,      // NEW: Pull to 0
  kTri1,      // NEW: Pull to 1
  kTriand,    // NEW: Tristate AND
  kTrior,     // NEW: Tristate OR
  kTrireg,    // NEW: Tristate with storage
  kSupply0,   // NEW: Ground
  kSupply1,   // NEW: Power
};
```

**Drive strength:**
```cpp
enum class Strength {
  kHighZ,     // High-impedance (weakest)
  kWeak,      // Weak drive
  kPull,      // Pull (resistive)
  kStrong,    // Strong drive (default)
  kSupply,    // Supply drive (strongest)
};
```

**Switch primitives:**
```cpp
enum class SwitchKind {
  kTran,      // Bidirectional pass
  kTranif1,   // Conditional pass (enable=1)
  kTranif0,   // Conditional pass (enable=0)
  kCmos,      // CMOS transmission gate
};

struct Switch {
  SwitchKind kind = SwitchKind::kTran;
  std::string a;        // Terminal A
  std::string b;        // Terminal B
  std::unique_ptr<Expr> control;    // Control signal
  std::unique_ptr<Expr> control_n;  // Complementary control (CMOS)
  Strength strength0 = Strength::kStrong;
  Strength strength1 = Strength::kStrong;
  bool has_strength = false;
};
```

**Continuous assignment with strength:**
```cpp
struct Assign {
  std::string lhs;
  int lhs_msb = 0;
  int lhs_lsb = 0;
  bool lhs_has_range = false;
  std::unique_ptr<Expr> rhs;
  Strength strength0 = Strength::kStrong;  // NEW
  Strength strength1 = Strength::kStrong;  // NEW
  bool has_strength = false;               // NEW
};
```

**Sequential assignment with delay:**
```cpp
struct SequentialAssign {
  std::string lhs;
  std::unique_ptr<Expr> lhs_index;
  std::vector<std::unique_ptr<Expr>> lhs_indices;
  std::unique_ptr<Expr> rhs;
  std::unique_ptr<Expr> delay;  // NEW: #delay in assignment
  bool nonblocking = true;
};
```

**New statement kinds:**
```cpp
enum class StatementKind {
  kAssign,
  kIf,
  kBlock,
  kCase,
  kFor,
  kWhile,
  kRepeat,
  kDelay,         // NEW: #delay statement
  kEventControl,  // NEW: @(event) statement
  kEventTrigger,  // NEW: -> event statement
  kWait,          // NEW: wait(condition) statement
  kForever,       // NEW: forever loop
  kFork,          // NEW: fork/join parallel
  kDisable,       // NEW: disable block
  kTaskCall,      // NEW: task invocation
};
```

**Statement fields for new kinds:**
```cpp
struct Statement {
  // ... existing fields ...

  // Delay statement: #time_expr body
  std::unique_ptr<Expr> delay;
  std::vector<Statement> delay_body;

  // Event control: @(event_expr) body
  std::unique_ptr<Expr> event_expr;
  std::vector<Statement> event_body;

  // Wait statement: wait(condition) body
  std::unique_ptr<Expr> wait_condition;
  std::vector<Statement> wait_body;

  // Forever loop: forever body
  std::vector<Statement> forever_body;

  // Fork/join: fork branches join
  std::vector<Statement> fork_branches;

  // Disable: disable block_name
  std::string disable_target;

  // Task call: task_name(args)
  std::string task_name;
  std::vector<std::unique_ptr<Expr>> task_args;

  // Event trigger: -> event_name
  std::string trigger_target;

  // Named blocks: begin : label ... end
  std::string block_label;
};
```

**Task support:**
```cpp
enum class TaskArgDir {
  kInput,
  kOutput,
  kInout,
};

struct TaskArg {
  TaskArgDir dir = TaskArgDir::kInput;
  std::string name;
  int width = 1;
  bool is_signed = false;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
};

struct Task {
  std::string name;
  std::vector<TaskArg> args;
  std::vector<Statement> body;
};
```

**Event declarations:**
```cpp
struct EventDecl {
  std::string name;  // Named event for triggering
};
```

**DefParam support:**
```cpp
struct DefParam {
  std::string instance;  // Instance name
  std::string param;     // Parameter name
  std::unique_ptr<Expr> expr;  // Override value
  int line = 0;
  int column = 0;
};
```

**Module structure extended:**
```cpp
struct Module {
  std::string name;
  std::vector<Port> ports;
  std::vector<Net> nets;
  std::vector<Assign> assigns;
  std::vector<Switch> switches;          // NEW
  std::vector<Instance> instances;
  std::vector<AlwaysBlock> always_blocks;
  std::vector<Parameter> parameters;
  std::vector<Function> functions;
  std::vector<Task> tasks;               // NEW
  std::vector<EventDecl> events;         // NEW
  std::vector<DefParam> defparams;       // NEW
};
```

### Frontend: Parser Expansion

**File**: `src/frontend/verilog_parser.cc` (+2,212 lines, -96 lines = +2,116 net)

This is the largest single-file change in the project. Major additions:

**Task parsing:**
```cpp
// Verilog:
// task add_numbers;
//   input [7:0] x;
//   input [7:0] y;
//   output [7:0] sum;
//   begin
//     sum = x + y;
//   end
// endtask

bool ParseTask(Module* module) {
  // 1. Expect 'task' keyword
  // 2. Parse task name
  // 3. Parse argument declarations (input/output/inout)
  // 4. Parse task body (statements)
  // 5. Expect 'endtask'
}

bool ParseTaskArgDecl(TaskArgDir dir, Task* task) {
  // Parse: input [width-1:0] name;
  // Or: output reg [width-1:0] name;
}
```

**Event declaration parsing:**
```cpp
// Verilog:
// event data_ready;
// event start, stop, reset;

bool ParseEventDecl(Module* module) {
  // 1. Expect 'event' keyword
  // 2. Parse comma-separated event names
  // 3. Store in module.events
}
```

**Timing control statements:**

**Delay statement:**
```cpp
// Verilog: #10 data = value;
bool ParseDelayStatement(Statement* out) {
  // 1. Expect '#'
  // 2. Parse delay expression
  // 3. Parse body statement
}
```

**Event control:**
```cpp
// Verilog: @(posedge clk) q <= d;
// Verilog: @(data_ready) process();
bool ParseEventControlStatement(Statement* out) {
  // 1. Expect '@'
  // 2. Parse event expression (edge or identifier)
  // 3. Parse body statement
}
```

**Event trigger:**
```cpp
// Verilog: -> event_name;
bool ParseEventTriggerStatement(Statement* out) {
  // 1. Expect '->'
  // 2. Parse event identifier
}
```

**Wait statement:**
```cpp
// Verilog: wait (ready) begin ... end
bool ParseWaitStatement(Statement* out) {
  // 1. Expect 'wait'
  // 2. Parse condition expression
  // 3. Parse body statement
}
```

**Forever loop:**
```cpp
// Verilog: forever begin #10 clk = ~clk; end
bool ParseForeverStatement(Statement* out) {
  // 1. Expect 'forever'
  // 2. Parse body statement (typically begin/end block)
}
```

**Fork/join:**
```cpp
// Verilog:
// fork
//   #10 a = 1;
//   #20 b = 1;
// join

bool ParseForkJoinStatement(Statement* out) {
  // 1. Expect 'fork'
  // 2. Parse statements until 'join'
  // 3. Each statement becomes a parallel branch
}
```

**Disable statement:**
```cpp
// Verilog: disable block_name;
bool ParseDisableStatement(Statement* out) {
  // 1. Expect 'disable'
  // 2. Parse block identifier
}
```

**Task call:**
```cpp
// Verilog: add_numbers(a, b, result);
bool ParseTaskCallStatement(Statement* out) {
  // 1. Parse task name (identifier)
  // 2. Expect '('
  // 3. Parse argument expressions
  // 4. Expect ')'
}
```

**Switch primitive parsing:**
```cpp
// Verilog:
// tran (a, b);
// tranif1 (a, b, enable);
// cmos (out, in, ncontrol, pcontrol);

bool ParseSwitch(Module* module) {
  // 1. Parse switch kind (tran, tranif0, tranif1, cmos)
  // 2. Optional strength specification
  // 3. Parse terminal connections
}
```

**DefParam parsing:**
```cpp
// Verilog: defparam inst.WIDTH = 16;
bool ParseDefParam(Module* module) {
  // 1. Expect 'defparam'
  // 2. Parse instance.parameter path
  // 3. Expect '='
  // 4. Parse override expression
}
```

### Elaboration: Event Queue Setup

**File**: `src/core/elaboration.cc` (+352 lines, -11 lines = +341 net)

**Event queue infrastructure:**
- Time-based event scheduling
- Delta-cycle management
- Non-blocking assignment queuing
- Event sensitivity computation

**Key additions:**
- Event queue initialization
- Timing event expansion
- Fork/join elaboration (convert to sequential with scheduling)
- Task instantiation framework
- Event trigger propagation

**Example transformation:**

```verilog
// Input:
initial begin
  #10 data = 1;
  #5 data = 0;
end

// Elaboration creates event queue entries:
// Time 0: Schedule event at time 10
// Time 10: Execute data=1, schedule event at time 15
// Time 15: Execute data=0
```

### Codegen: Event Queue MSL Generation

**File**: `src/codegen/msl_codegen.cc` (+763 lines, -65 lines = +698 net)

**Event queue structures in MSL:**

```metal
// Event queue entry
struct Event {
  uint time;           // Simulation time
  uint type;           // Event type (assignment, trigger, etc.)
  uint target_id;      // Signal or event ID
  uint64_t value;      // Value to assign
  uint64_t xz;         // X/Z bits (4-state)
};

// Event queue in device memory
device Event* event_queue;
device atomic_uint event_count;
device atomic_uint current_time;
```

**Event queue operations:**

```metal
// Insert event into queue
void schedule_event(
  device Event* queue,
  device atomic_uint* count,
  uint time,
  uint type,
  uint target,
  uint64_t value,
  uint64_t xz
) {
  uint index = atomic_fetch_add_explicit(count, 1, memory_order_relaxed);
  queue[index].time = time;
  queue[index].type = type;
  queue[index].target_id = target;
  queue[index].value = value;
  queue[index].xz = xz;
}

// Process events at current time
kernel void process_events(
  device Event* queue,
  device uint* current_time,
  device uint64_t* signals,
  // ...
) {
  // Find events scheduled for current_time
  // Execute assignments
  // Update signal values
  // Trigger dependent events
}
```

**Delay implementation:**
```metal
// #10 data = value;
// Becomes:
schedule_event(queue, &count, current_time + 10, EVENT_ASSIGN,
               signal_id_data, value, xz);
```

**Non-blocking assignments:**
```metal
// data <= value;  (non-blocking)
// Scheduled for end of current time step
schedule_event(queue, &count, current_time, EVENT_NBA,
               signal_id_data, value, xz);
```

**Event triggers:**
```metal
// -> event_name;
// Wake up all processes waiting on event_name
schedule_event(queue, &count, current_time, EVENT_TRIGGER,
               event_id, 0, 0);
```

### Main: Enhanced Output

**File**: `src/main.mm` (+186 lines, -5 lines = +181 net)

**Event queue visualization:**
- Dump event queue state
- Show scheduled events
- Display simulation time progress
- Event statistics

**New debug output:**
```
Event Queue Status:
==================
Current Time: 15ns
Pending Events: 3
  [20ns] NBA: data <= 8'hFF
  [25ns] DELAY: clk = ~clk
  [30ns] TRIGGER: -> done
```

## Implementation Details

### Event Queue Semantics

**Verilog event semantics:**

**Time slots:**
```
Time 0: Active events + NBA updates
Time 0+δ: Monitor events
Time 10: Active events + NBA updates
Time 10+δ: Monitor events
...
```

**Event ordering within time slot:**
1. Active events (blocking assignments, $display)
2. Non-blocking assignment updates
3. Monitor events ($monitor)

**Delta cycles:**
- Multiple evaluations at same time
- Resolve combinational dependencies
- Update until stable

**Event types:**
```cpp
enum EventType {
  EVENT_ACTIVE,      // Immediate execution
  EVENT_NBA,         // Non-blocking (end of time step)
  EVENT_TRIGGER,     // Named event trigger
  EVENT_MONITOR,     // $monitor output
};
```

### Fork/Join Implementation

**Verilog fork/join:**
```verilog
fork
  #10 a = 1;
  #20 b = 1;
  #30 c = 1;
join
// Wait until all branches complete
```

**Implementation strategy:**
1. Parse all fork branches
2. Convert to event queue entries
3. Track completion of all branches
4. Resume parent process when all complete

**In event queue:**
```
Time 10: Assign a=1, mark branch 0 done
Time 20: Assign b=1, mark branch 1 done
Time 30: Assign c=1, mark branch 2 done, resume parent
```

### Task Implementation

**Tasks vs. Functions:**

**Functions (REV11):**
- Return value
- No timing
- Inlined at elaboration
- Pure (no side effects)

**Tasks (REV14):**
- No return value
- Can have timing (#, @, wait)
- NOT inlined (runtime calls)
- Side effects allowed (reg assignments)

**Task calling convention (planned):**
```cpp
// Task arguments passed via parameter stack
// Output arguments written back
// Task body executed inline (but with event queue)
```

### Switch Primitive Semantics

**Bidirectional passes:**

**tran:**
```verilog
tran (a, b);  // Bidirectional connection
// a ↔ b (always connected)
```

**tranif0/tranif1:**
```verilog
tranif1 (a, b, enable);
// if (enable) a ↔ b
// else a and b disconnected
```

**cmos:**
```verilog
cmos (out, in, ncontrol, pcontrol);
// CMOS transmission gate
// if (!ncontrol && pcontrol) out ↔ in
```

**Implementation:**
- In 4-state mode: Z propagates through switches
- Drive strength affects resolution
- Capacitive storage for trireg

## Known Gaps and Limitations

### Improvements Over REV13

**Now Parsed (not yet executable):**
- Tasks with input/output/inout arguments
- Event declarations and triggers
- Timing controls: #delay, @(event), wait(condition)
- Forever loops
- Fork/join parallel blocks
- Disable statements
- Task calls
- All net types (wand, wor, tri variants, supply)
- Switch primitives (tran, tranif, cmos)
- DefParam overrides
- Drive strength on assigns

**Event Queue Infrastructure:**
- Event queue data structures defined
- Scheduling logic implemented
- MSL event queue management
- Delta-cycle framework

**Still Not Executable:**
- Runtime not complete (event queue needs host integration)
- No actual simulation yet
- Features parsed but not tested
- 162 passing tests (same as REV13 - no new passing tests)

**Still Missing:**
- System tasks ($display, $monitor, $finish) - parsed but not executed
- Actual event queue execution
- Host-side runtime
- GPU kernel execution
- Time advancement logic
- Real numbers
- UDPs

### Semantic Notes (v0.4 in progress)

**Event-driven simulation:**
- All timing constructs create event queue entries
- Non-blocking assignments scheduled for end of time step
- Combinational logic evaluates until stable (delta cycles)
- Named events trigger waiting processes

**Task execution:**
- Task calls are NOT inlined (unlike functions)
- Task body can contain timing controls
- Arguments passed by value (inputs) or reference (outputs)
- Tasks can call other tasks

**Switch primitives:**
- Bidirectional signal flow
- Drive strength affects conflict resolution
- Z propagates through inactive switches
- Required for analog/mixed-signal modeling

## Statistics

- **Files changed**: 5
- **Lines added**: 3,439
- **Lines removed**: 172
- **Net change**: +3,267 lines

**Breakdown:**
- Parser: +2,212 lines (tasks, events, timing, switches, net types)
- Codegen: +763 lines (event queue MSL, scheduling logic)
- Elaboration: +352 lines (event queue setup, timing expansion)
- Main: +186 lines (event queue visualization, debug output)
- AST: +98 lines (event structures, task definitions, timing fields)
- Removals: -172 lines (refactoring, cleanup)

**Implementation scale:**
- This is the **largest single commit** by line count
- Represents ~3,300 lines of new infrastructure
- Foundational for runtime execution
- Event queue is core of v0.4 development

**Test suite:**
- `verilog/pass/`: 162 files (same as REV13)
- `verilog/`: 28 files (same as REV13)
- **No new passing tests** - this is infrastructure work
- Features parsed but not yet executable

**Major infrastructure:**
- Complete event queue system
- Task support framework
- Timing control parsing
- All Verilog net types
- Switch primitive modeling
- Drive strength resolution

This commit represents the **foundation for runtime execution**. While it doesn't add passing tests, it implements the core event-driven simulation machinery needed for metalfpga to actually run Verilog code on the GPU. The event queue system will enable proper timing, non-blocking assignments, and event-driven behavior - essential for realistic hardware simulation.
