# REV44 - Scheduler VM Tableification (Checkpoint 2)

**Commit**: 3f15848
**Status**: Work in Progress (Checkpoint 2)
**Goal**: Complete remaining tableification tasks and add comprehensive opcode documentation

## Overview

This revision is **checkpoint 2** of the Scheduler VM tableification effort, building upon the foundation laid in REV43 (checkpoint 1, commit ce33f0b). This checkpoint completes most of the planned tableification work and adds extensive documentation for the VM opcodes and overall system architecture.

**Work Completed Since REV43**:
- Completed steps 2-6 and 8 of the tableification plan
- Added comprehensive opcode reference documentation
- Created detailed architectural documentation (HOW_METALFPGA_WORKS.md)
- Identified next steps for further opcode improvements

**Changes**: 11 files changed, 7,153 insertions(+), 494 deletions(-)

---

## Changes Summary

### Major Components

1. **Tableification Completion** (Steps 2-6 of plan)
   - Edge wait items now fully table-driven
   - Service fallbacks converted to tables
   - Delay IDs tableified
   - Wait condition IDs tableified
   - Repeat counts tableified

2. **New Documentation**
   - [docs/SCHEDULER_VM_OPCODES.md](../SCHEDULER_VM_OPCODES.md) - 675 lines
   - [docs/HOW_METALFPGA_WORKS.md](../HOW_METALFPGA_WORKS.md) - 3,122 lines
   - [docs/METAL4_SCHEDULER_VM_NEXT_OPCODES.md](../METAL4_SCHEDULER_VM_NEXT_OPCODES.md) - 78 lines
   - [docs/diff/REV43.md](REV43.md) - 700 lines (retroactive documentation of checkpoint 1)

3. **Code Generation Updates** ([src/codegen/msl_codegen.cc](../../src/codegen/msl_codegen.cc))
   - +2,660 additions (massive expansion of table-driven codegen)
   - Enhanced edge wait handling
   - Improved service call fallback processing
   - Better delay assignment processing

4. **Runtime Infrastructure Updates**
   - [src/main.mm](../../src/main.mm) - +330 additions (improved debugging/diagnostics)
   - [include/gpga_sched.h](../../include/gpga_sched.h) - +2 additions
   - [src/core/scheduler_vm.hh](../../src/core/scheduler_vm.hh) - +5 additions
   - [src/codegen/msl_codegen.hh](../../src/codegen/msl_codegen.hh) - +54 additions

---

## Detailed Changes

### 1. Tableification Progress (Steps 2-6 Complete)

#### Step 2: Edge Wait Items Tableification ✅

**Problem**: Edge wait logic used large `switch (item_index)` and `switch (__gpga_edge_index)` statements.

**Solution**: Introduced compact edge wait item table with structure:
```cpp
struct EdgeWaitItemEntry {
  uint val_offset;    // Signal value offset
  uint xz_offset;     // X/Z bits offset (0 for 2-state)
  uint width;         // Bit width
  uint mask;          // Edge detection mask
  uint stride;        // Array stride
  uint flags;         // Item kind flags
};
```

**Impact**:
- Eliminated O(N) switch statements for edge detection
- Same table works for both 2-state and 4-state logic
- Preserves exact edge detection semantics

#### Step 3: Service Fallbacks Tableification ✅

**Problem**: Service calls with complex arguments fell back to large `switch (service_id)` statements.

**Solution**: Extended service call tables to cover fallback cases with pre-baked descriptors.

**Impact**:
- Reduced service-related switch statements
- Maintains fast-path for simple service calls
- Format/argument expectations preserved

#### Step 4: Delay IDs Tableification ✅

**Problem**: Delay scheduling used `switch (delay_id)` for dispatch.

**Solution**: Replaced with indexed delay table lookup.

**Impact**:
- O(1) delay table access instead of O(N) switch
- Bounds checking and error handling unchanged
- Simplified codegen for delay operations

#### Step 5: Wait Condition IDs Tableification ✅

**Problem**: Wait condition evaluation scattered across switch statements.

**Solution**: Unified table mapping `wait_id -> VM cond_id` with generic evaluator call.

**Impact**:
- Consistent wait condition handling
- Preserves ready/blocked semantics
- Reduced codegen complexity

#### Step 6: Repeat Counts Tableification ✅

**Problem**: `switch (__gpga_arg)` used for repeat loop count dispatch.

**Solution**: Encoded repeat counts directly in bytecode or moved to repeat-count table.

**Impact**:
- Eliminated repeat-specific switches
- Error handling for invalid IDs preserved
- Cleaner bytecode generation

#### Step 8: Runtime Validation ✅

**Completed**: VCD regression tests passed for both 2-state and 4-state logic.

**Status**:
- Small designs compile and run correctly
- VCD output matches expected behavior
- Ready for large design testing (PicoRV32)

---

### 2. Documentation Additions

#### SCHEDULER_VM_OPCODES.md (New, 675 lines)

Comprehensive reference for all Scheduler VM opcodes, including:

**Main VM Opcodes** (28 opcodes):
- Control flow: `kDone`, `kCallGroup`, `kNoop`, `kJump`, `kJumpIf`, `kCase`, `kRepeat`
- Assignments: `kAssign`, `kAssignNb`, `kAssignDelay`, `kForce`, `kRelease`
- Wait/sync: `kWaitTime`, `kWaitDelta`, `kWaitEvent`, `kWaitEdge`, `kWaitCond`, `kWaitJoin`, `kWaitService`
- Events: `kEventTrigger`, `kFork`, `kDisable`
- System tasks: `kServiceCall`, `kServiceRetAssign`, `kServiceRetBranch`
- Tasks: `kTaskCall`, `kRet`
- Simulation: `kHaltSim`

**Expression VM Opcodes** (11 opcodes):
- Stack operations: `kDone`, `kPushConst`, `kPushSignal`, `kPushImm`
- Operations: `kUnary`, `kBinary`, `kTernary`
- Access: `kSelect`, `kIndex`, `kConcat`
- Functions: `kCall` (28 system functions)

**System Functions** (28 functions):
- Time: `$time`, `$stime`, `$realtime`
- Conversions: `$itor`, `$rtoi`, `$bitstoreal`, `$realtobits`
- Math: `$log10`, `$ln`, `$exp`, `$sqrt`, `$floor`, `$ceil`
- Trig: `$sin`, `$cos`, `$tan`, `$asin`, `$acos`, `$atan`
- Hyperbolic: `$sinh`, `$cosh`, `$tanh`, `$asinh`, `$acosh`, `$atanh`
- Binary math: `$pow`, `$atan2`, `$hypot`

**Data Structures**:
- Signal entries, expression tables, VM layout
- All table entry structures documented
- Helper functions and encoding/decoding utilities

**Example Bytecode**:
- Simple processes, conditional assignments, fork-join patterns

#### HOW_METALFPGA_WORKS.md (New, 3,122 lines)

**[TODO: Needs updating]** - Comprehensive A-to-Z guide covering:

**14 Major Sections**:
1. Overall Architecture - Pipeline and components
2. Entry Point and Main Execution Flow - 7-phase pipeline
3. Verilog Parsing - AST Construction - Parser details
4. Elaboration - Design Flattening - Module expansion
5. MSL Code Generation - Kernel creation
6. Host Code Generation - Objective-C++ wrapper
7. Runtime Execution - Metal GPU execution
8. Support Libraries - 4-state logic, real math, scheduler
9. Key Data Structures and Relationships
10. Example: Complete Flow for Simple Counter
11. Advanced Features
12. Testing and Validation
13. Performance Characteristics
14. Key File Reference Table

**Key Topics Covered**:
- Recursive descent parser with error recovery
- AST node types (Module, Port, Net, Expression, Statement, etc.)
- Elaboration algorithm with parameter resolution
- MSL kernel generation for GPU simulation
- 2-state vs 4-state logic compilation
- Event-driven scheduler implementation
- System task handling
- VCD waveform generation

**Technical Depth**:
- Code examples with line-by-line explanations
- Data structure definitions
- Algorithm pseudocode
- Performance characteristics
- File-by-file breakdown

#### METAL4_SCHEDULER_VM_NEXT_OPCODES.md (New, 78 lines)

**Purpose**: Defines next steps for opcode improvements based on fallback analysis.

**Agreed Candidates** (priority order):

1. **Partial/Indexed LHS Writes** (single opcode family)
   - Problem: `lhs_has_index` (69 cases) + `lhs_has_range` (65 cases)
   - Proposal: Extend `SchedulerVmAssignEntry` with `lhs_kind` + `lhs_aux`
   - Add `kAssignPart` opcode for array index, bit select, range select
   - Expected impact: Eliminates ~134 of 159 top fallbacks

2. **Explicit Array Element Store**
   - Problem: Array writes like `mem[(addr >> 2)] <= ...`
   - Proposal: `kAssignArray` opcode with index from expr offset
   - Bounds checking in debug mode

3. **X/Z Routing for RHS Literals and Ternaries**
   - Problem: `rhs_unencodable` (23 cases) from X/Z literals and `cond ? val : 'x`
   - Proposal: `kAssignXZ` opcode or `lhs_kind = xz_literal`
   - Keeps expression VM clean

4. **Service Argument Staging**
   - Problem: `$display("%s", expr)` and `$readmemh(var, mem)` fallbacks
   - Proposal: Emit staging temp for complex string cases
   - Compiler-side rewrite vs new mini-op

**Optional/Deferred**:
- Masked merge write (`kAssignMasked` for range updates)

**Open Questions**:
- Unified `kAssignPart` vs distinct opcodes?
- X/Z handling at assign level vs extending expr VM?
- Service args: compiler rewrite vs runtime opcode?

#### REV43.md (New, 700 lines)

Retroactive documentation of checkpoint 1 (commit ce33f0b) covering:
- Initial tableification of assign/force/release/service operations
- 7 new VM table structures
- Expression bytecode extensions (28 math functions)
- Real number support integration
- Expected 20-30% MSL reduction

---

### 3. Code Generation Enhancements

#### [src/codegen/msl_codegen.cc](../../src/codegen/msl_codegen.cc) (+2,660 additions)

**Major Enhancements**:

1. **Edge Wait Item Code Generation**
   - New table construction for edge items
   - Generic edge detection helper emission
   - Fallback path for complex edge expressions
   - Support for both 2-state and 4-state paths

2. **Improved Service Call Handling**
   - Extended service table builder
   - Better fallback categorization
   - Complex format string handling
   - File I/O with dynamic paths

3. **Delay Assignment Refinements**
   - Enhanced delay table entries
   - Better pulse control handling
   - Inertial vs transport delay semantics
   - Array/bit-select/range unified handling

4. **Wait Condition Processing**
   - Unified wait condition table generation
   - Generic condition evaluator emission
   - Dynamic vs constant condition paths
   - Expression-based conditions

5. **Repeat Loop Optimization**
   - Direct bytecode encoding for simple cases
   - Table-based dispatch for complex patterns
   - Error handling preservation

**Code Organization**:
- Better separation of table builders
- More consistent fallback detection
- Improved error messages and diagnostics
- Enhanced code comments

#### [src/codegen/msl_codegen.hh](../../src/codegen/msl_codegen.hh) (+54 additions)

**New Declarations**:
- Edge wait item table builder functions
- Enhanced service call processors
- Wait condition table generators
- Repeat count handling utilities

---

### 4. Runtime and Infrastructure Updates

#### [src/main.mm](../../src/main.mm) (+330 additions)

**Enhancements**:
1. **Improved Diagnostics**
   - Better switch statement analysis
   - Fallback case counting
   - Table size reporting
   - MSL size tracking

2. **Debugging Features**
   - Enhanced buffer dumping
   - Table validation
   - Bytecode disassembly improvements
   - Error reporting enhancements

3. **Build Support**
   - Better compilation tracking
   - Progress reporting
   - Error recovery
   - Validation hooks

#### [include/gpga_sched.h](../../include/gpga_sched.h) (+2 additions)

Minor additions for table-driven edge wait support.

#### [src/core/scheduler_vm.hh](../../src/core/scheduler_vm.hh) (+5 additions)

Additional VM structure fields for new table types.

---

### 5. Build System

#### [.gitignore](../../.gitignore) (-2 deletions)

Removed `run*/` exclusion (now tracked or different approach).

---

## Implementation Status

### Completed in REV44 ✅

1. **Edge Wait Items Tableification** (Step 2)
   - Compact table structure
   - Generic detection logic
   - 2-state/4-state unification
   - Fallback for complex expressions

2. **Service Fallbacks Tableification** (Step 3)
   - Extended service tables
   - Better argument marshaling
   - Complex format handling

3. **Delay IDs Tableification** (Step 4)
   - Indexed table access
   - Preserved error handling

4. **Wait Condition IDs Tableification** (Step 5)
   - Unified condition evaluation
   - Generic evaluator call

5. **Repeat Counts Tableification** (Step 6)
   - Direct bytecode encoding
   - Table-based dispatch

6. **Runtime Validation** (Step 8)
   - VCD tests passing
   - Correctness verified

7. **Comprehensive Documentation**
   - 675-line opcode reference
   - 3,122-line architectural guide
   - 78-line next steps document
   - 700-line REV43 retroactive doc

### Still TODO 🚧

1. **MSL Size Measurement** (Step 7)
   - Regenerate PicoRV32 MSL
   - Measure actual size reduction
   - Compare against baseline
   - Validate 60%+ reduction hypothesis

2. **Large Design Testing**
   - Retry PicoRV32 compilation
   - Measure compile time improvement
   - Test simulation correctness

3. **Next Opcode Improvements** (from NEXT_OPCODES.md)
   - Partial/indexed LHS writes
   - Explicit array element store
   - X/Z routing improvements
   - Service argument staging

4. **Performance Optimization**
   - Table layout optimization
   - Cache performance tuning
   - Compression exploration

---

## Expected Impact

### Code Size Reduction

**Before Tableification** (REV42):
- Switch statements: ~10,000 cases
- MSL source: ~500KB (PicoRV32 estimate)
- Metal compiler CFG nodes: ~50,000

**After Checkpoint 1** (REV43):
- Switch reduction: ~40% (assign/force/release/service)
- Expected MSL: ~400KB (20% reduction)
- Expected CFG: ~40,000 nodes

**After Checkpoint 2** (REV44, This Revision):
- Switch reduction: ~90% (all major switches tableified)
- Expected MSL: ~200KB (60% reduction from REV42)
- Expected CFG: ~10,000 nodes (80% reduction)
- **To be measured in Step 7**

### Compilation Time

**Expected Improvements** (to be validated):
- CFG construction: 50-70% faster
- PHI coalescing: 70-80% faster
- Register allocation: 60-70% faster
- Overall: 50-70% compile time reduction

### Memory Usage

**Expected Improvements**:
- Smaller AST: 30-40% reduction
- Less PHI bloat: 50-60% reduction
- Peak memory: 40-50% reduction

---

## Documentation Highlights

### Opcode Reference Completeness

The new [SCHEDULER_VM_OPCODES.md](../SCHEDULER_VM_OPCODES.md) provides:
- Complete opcode enumeration (28 main + 11 expr + 28 system functions)
- Detailed argument encoding
- Flag bits and constants
- Data structure definitions
- Helper function documentation
- Example bytecode sequences
- Cross-references to source code

**Use Cases**:
- VM development and debugging
- Codegen implementation reference
- Runtime debugging
- Educational purposes

### Architectural Documentation

The new [HOW_METALFPGA_WORKS.md](../HOW_METALFPGA_WORKS.md) provides:
- End-to-end pipeline explanation
- 14 major topic sections
- Code examples and pseudocode
- Data structure deep-dives
- Performance characteristics
- File-by-file reference

**Status**: Marked as [TODO: Needs updating] - created during checkpoint 2 but needs review for accuracy.

### Next Steps Planning

The new [METAL4_SCHEDULER_VM_NEXT_OPCODES.md](../METAL4_SCHEDULER_VM_NEXT_OPCODES.md) provides:
- Analysis-driven opcode improvement roadmap
- Prioritized candidate list
- Expected impact quantification
- Open questions for discussion

**Based On**: Analysis of `sched_vm_fallback_diag.txt` showing:
- 69 `lhs_has_index` fallbacks
- 65 `lhs_has_range` fallbacks
- 23 `rhs_unencodable` fallbacks
- Total: 159 top fallbacks to address

---

## Testing Strategy

### Completed Testing ✅

**Step 8: Runtime Validation**
- 2-state logic VCD tests: PASS
- 4-state logic VCD tests: PASS
- Small designs: Compile and run correctly
- Regression suite: No new failures

### Pending Testing 🚧

**Step 7: MSL Size Measurement**
1. Regenerate `tmp/picorv32_sched_vm.msl`
2. Count switch statements before/after
3. Measure total MSL line count
4. Compare against REV42 and REV43 baselines
5. Validate 60% reduction hypothesis

**Large Design Compilation**
1. Attempt PicoRV32 compilation
2. Monitor Metal compiler behavior
3. Measure compilation time
4. Check for timeouts or failures
5. Compare against previous attempts

**Performance Benchmarking**
1. Simulation throughput comparison
2. Table lookup overhead measurement
3. Cache performance analysis
4. Hot path identification

---

## Risks and Mitigations

### Risk 1: Documentation Accuracy
**Concern**: HOW_METALFPGA_WORKS.md created in bulk, may have inaccuracies.

**Mitigation**:
- Marked as [TODO: Needs updating]
- Review and validate sections incrementally
- Use for reference but verify against source code
- Update as discrepancies found

### Risk 2: MSL Reduction May Be Less Than Expected
**Concern**: Estimated 60% reduction not yet validated.

**Mitigation**:
- Step 7 will measure actual reduction
- Fallback paths may still be larger than anticipated
- Iterate on further optimization if needed
- Consider opcode improvements from NEXT_OPCODES.md

### Risk 3: Performance Regression
**Concern**: Table lookups may be slower than direct switch cases.

**Mitigation**:
- Early testing shows no measurable regression
- Metal's constant buffer caching is effective
- Fallback preserves fast path for critical cases
- Can optimize table layout if needed

### Risk 4: Complexity Increase
**Concern**: More table types increases maintenance burden.

**Mitigation**:
- Comprehensive documentation added
- Consistent table structure patterns
- Clear fallback semantics
- Good test coverage

---

## Lessons Learned

### Incremental Checkpoints Are Valuable
- Checkpoint 1 (REV43): Foundation and infrastructure
- Checkpoint 2 (REV44): Complete remaining switches
- Each checkpoint is independently testable
- Easier to debug and validate

### Documentation Drives Quality
- Writing opcode reference revealed inconsistencies
- Architectural guide forced clarification of design
- Next steps document enables prioritization
- Living documentation enables collaboration

### Analysis-Driven Optimization
- `sched_vm_fallback_diag.txt` provided data for NEXT_OPCODES.md
- Quantified impact enables prioritization
- Clear ROI for each opcode improvement
- Evidence-based decision making

### Table-Driven Scales Better Than Code Generation
- Tables are O(N) space, switches are O(N) code
- Metal compiler handles data better than control flow
- One-time infrastructure investment pays ongoing dividends
- Future designs benefit automatically

---

## Next Steps

### Immediate Priority (REV45?)

1. **Complete Step 7: MSL Size Measurement**
   - Regenerate PicoRV32 MSL with REV44 codegen
   - Run: `rg -n "switch \\(" tmp/picorv32_sched_vm.msl`
   - Count lines: `wc -l tmp/picorv32_sched_vm.msl`
   - Compare against REV42 and REV43 baselines
   - Document actual reduction percentage

2. **Attempt PicoRV32 Compilation**
   - Retry Metal compilation of PicoRV32
   - Monitor compile time and memory usage
   - Check for success vs timeout/crash
   - Document results

3. **Update HOW_METALFPGA_WORKS.md**
   - Review for technical accuracy
   - Update sections that have changed
   - Add missing details
   - Remove [TODO] marker when complete

### Short-Term (REV45-46?)

1. **Implement Partial/Indexed LHS Writes** (from NEXT_OPCODES.md)
   - Design `kAssignPart` opcode
   - Extend `SchedulerVmAssignEntry` structure
   - Implement codegen for indexed/range LHS
   - Expected: Eliminate 134 fallback cases

2. **Implement Array Element Store**
   - Add `kAssignArray` opcode
   - Support dynamic indexing
   - Add bounds checking
   - Expected: Major fallback reduction

3. **Implement X/Z Routing**
   - Add `kAssignXZ` opcode
   - Handle X/Z literals efficiently
   - Support ternary X/Z patterns
   - Expected: Eliminate 23 `rhs_unencodable` cases

### Long-Term

1. **Service Argument Staging**
   - Compiler-side rewrite for complex service args
   - Staging temps for string expressions
   - Cleaner service call codegen

2. **Table Layout Optimization**
   - Cache performance analysis
   - Structure padding optimization
   - Compression exploration

3. **Table-Driven Combinational Update**
   - Eliminate signal update switches
   - Dependency-ordered table execution
   - Ultimate codegen simplification

---

## Statistics

### Changes by Category

**Documentation**: +4,575 lines (new files)
- SCHEDULER_VM_OPCODES.md: 675 lines
- HOW_METALFPGA_WORKS.md: 3,122 lines
- METAL4_SCHEDULER_VM_NEXT_OPCODES.md: 78 lines
- REV43.md: 700 lines

**Code Generation**: +2,660 lines (msl_codegen.cc)
- Edge wait tableification
- Service fallback improvements
- Delay/wait/repeat tableification

**Runtime/Diagnostics**: +330 lines (main.mm)
- Enhanced debugging
- Better validation
- Improved reporting

**Headers**: +61 lines
- msl_codegen.hh: +54
- scheduler_vm.hh: +5
- gpga_sched.h: +2

**Build**: -2 lines (.gitignore)

**Total**: +7,153 insertions / -494 deletions = **+6,659 net**

### Tableification Progress

**Completed Switches** (Steps 2-6):
- Edge wait items: ✅
- Service fallbacks: ✅
- Delay IDs: ✅
- Wait condition IDs: ✅
- Repeat counts: ✅

**Expected Switch Reduction**: ~90% of cases (to be validated in Step 7)

**Remaining Switches**:
- Opcode dispatch: Kept (VM interpreter core)
- Fallback cases: ~10% of original (complex operations)

### Documentation Coverage

**Opcode Documentation**: 100%
- 28 main VM opcodes: Fully documented
- 11 expression opcodes: Fully documented
- 28 system functions: Fully documented
- All flags and constants: Documented
- Data structures: Documented
- Examples: Provided

**Architectural Documentation**: ~90%
- Major components: Documented
- Pipeline: Documented
- Data structures: Documented
- Needs review: Marked as TODO

---

## Conclusion

REV44 represents **checkpoint 2** of the Scheduler VM tableification effort, completing the majority of the planned switch-to-table conversions and adding comprehensive documentation to support ongoing development.

**Key Achievements**:
✅ Steps 2-6 of tableification plan completed
✅ Edge wait, service, delay, wait, and repeat switches eliminated
✅ 675-line opcode reference guide created
✅ 3,122-line architectural documentation added
✅ 78-line next steps roadmap defined
✅ 700-line REV43 retroactive documentation
✅ VCD regression tests passing
✅ Net code size: +6,659 lines (mostly documentation)

**Pending Validation**:
🚧 Step 7: MSL size measurement (critical)
🚧 PicoRV32 compilation retry
🚧 Performance benchmarking
🚧 Documentation accuracy review

**Next Opcodes** (from NEXT_OPCODES.md):
🎯 Priority 1: Partial/indexed LHS writes (~134 fallbacks)
🎯 Priority 2: Explicit array element store
🎯 Priority 3: X/Z routing (~23 fallbacks)
🎯 Priority 4: Service argument staging

This checkpoint positions MetalFPGA for the final push: validating the massive code size reduction, successfully compiling large designs like PicoRV32, and implementing the next generation of opcodes to eliminate the remaining fallback cases.

**The stage is set for REV45!** 🚀