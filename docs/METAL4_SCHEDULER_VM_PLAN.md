# Metal4 Scheduler Bytecode VM Plan

## Goal
Replace per-proc `switch(pc)` control-flow with a compact bytecode VM so the
scheduler kernel size scales with data, not with the number of statements.
This is the long-term fix for MTLCompilerService CFG/PHI blowups.

## Current State (Baseline)
- A-full is in place: per-proc helpers with `pc_bucket` dispatch.
- Edge-wait switch is already data-driven.
- Compiler still spends time in backend LLVM passes on large kernels.
- VM seed bytecode layout packer is wired (per-proc fixed slots + offsets).
- VM exec loop exists for seed ops (CALL_GROUP/NOOP/JUMP/JUMP_IF/TASK_CALL/RET/DONE).
- VM condition eval helper emits per-design condition tables (IDs + eval).

## Non-Goals
- Do not change Verilog semantics.
- Do not change external CLI behavior.
- Do not add multi-compile concurrency until per-kernel memory is stable.

## Strategy Overview
1) Introduce a bytecode format that encodes scheduler actions.
2) Emit per-proc bytecode streams and metadata tables.
3) Replace `switch(pc)` logic with a VM interpreter loop in MSL.
4) Validate output equality (VCDs + existing tests).
5) Keep A-full as fallback during rollout.

## Principle
The VM shader source must not scale with design size. The design should be
data (buffers), not code (large constants).

## Design Notes
- Bytecode should be small, fixed-width where possible (e.g. 32-bit words).
- Use tables for immediates (constants, signal ids, event ids) to keep
  bytecode compact and place them in read-only buffers.
- VM loop should be branch-light: a small `switch(opcode)` with tight cases.
- Keep opcode handlers tiny and prefer helper calls (best-effort noinline).
- Avoid loop unrolling by keeping trip counts runtime-derived and
  placing `#pragma clang loop unroll(disable)` on the VM fetch loop.
- Add an instruction budget per invocation to avoid long-running loops.
- Prefer two precompiled pipelines for 2-state vs 4-state over a runtime flag,
  to keep the interpreter kernel minimal.
- Specialization identity must be stable (pick `specializedName` or constants,
  version it, and log a hash on archive misses).
- Treat the serializer as a compile-time capture tool; it is not a runtime
  lookup cache.
- Define 4-state control-flow semantics explicitly (X/Z treated as false for
  `if`/`while`, and `case/casez/casex` matching rules encoded in OP_CASE).
- join/join_any/join_none semantics must be encoded in fork metadata.
- disable semantics must cover disable fork vs named blocks/tasks.
- Logical vs bitwise operators + case equality (`===/!==`) must be exact.
- OP_CASE should be table-driven with a small handler; move strategy/shape into
  a per-case header (case kind, width, strategy, offsets).
- Exact `casez` needs X vs Z distinction; commit to a 2-bit encoding per bit:
  00=0, 01=1, 10=X, 11=Z. Keep fast-path helpers for 2-state reads.
- Call frames should live in a flat `device` buffer with fixed stride; avoid
  large `thread` arrays to reduce register pressure and spills.

## Metal 4 API Hooks (Compendium)
- `MTL4BinaryFunction`: shader you precompile from Metal IR to GPU machine code.
- `MTL4Archive`: read-only container that stores pipeline states; can include IR
  binaries, GPU/system binaries, or a mix.
- `MTL4Compiler`: pipeline state and shader function compiler.
- `MTL4CompilerDescriptor`: groups properties for creating a compiler context.
- `MTL4CompilerTask`: async compilation task from a compiler instance.
- `MTL4CompilerTaskOptions`: task-level compilation options.
- `MTL4SpecializedFunctionDescriptor`: configure specialized functions (function
  constants or name) without regenerating source.
- `MTL4FunctionDescriptor` / `MTL4LibraryFunctionDescriptor`: describe shader
  functions sourced from a Metal library.
- `MTL4StitchedFunctionDescriptor`: function suitable for stitching (Plan B for
  opcode dispatch if needed).
- `MTL4BinaryFunctionOptions`: options for creating binary functions.
- `MTL4ComputePipelineDescriptor`: describes a compute pipeline state.
- `MTL4PipelineDataSetSerializer`: collects pipeline creation data and can
  serialize to archives.
- `MTL4PipelineDataSetSerializerDescriptor`: creates the serializer.
- `MTL4PipelineDataSetSerializerConfiguration`: serializer configuration options.

## API Wiring Sketch (Metal 4)
This is a high-level integration flow (class-level only; method names omitted
on purpose to avoid guesswork until we wire it up in code).

### Compile Once (Interpreter Kernel)
1) Create `MTL4PipelineDataSetSerializerDescriptor` and serializer config.
2) Attach `MTL4PipelineDataSetSerializer` to `MTL4CompilerDescriptor`.
3) Create `MTL4Compiler`.
4) Describe the interpreter function using `MTL4LibraryFunctionDescriptor`
   (or `MTL4StitchedFunctionDescriptor` if we later use stitched dispatch).
5) If we need a 2-state vs 4-state specialization, configure a
   `MTL4SpecializedFunctionDescriptor` with function constants or name.
6) Build a `MTL4ComputePipelineDescriptor` for the interpreter.
7) Compile to a `MTL4BinaryFunction` using `MTL4BinaryFunctionOptions`.
8) Build the compute pipeline (via the compiler) and persist to `MTL4Archive`.
9) Track the async compile via `MTL4CompilerTask` and status (log for proof).

### Run Many Designs (Buffers Only)
1) Upload bytecode + tables to `MTLBuffer`s.
2) Open `MTL4Archive` and look up the pipeline via the same descriptor.
3) Bind buffers + launch the precompiled pipeline.
4) No shader recompilation should occur; only buffers change.

### Archive Miss Policy
- Strict mode (CI): fail on archive miss to prove invariance.
- Fallback mode (prod): provide `lookupArchives`, compile on miss, and
  rate-limit logging with specialization + descriptor hashes.
- In strict mode, do archive-only creation (no compiler path).
- In fallback mode, keep the serializer attached to harvest newly compiled
  pipelines for later archive refresh.

### Telemetry Fields (Archive Miss/Unusable)
- Function identity: base function + specialized name (if used).
- Specialization identity: hash of function constants or specialization key.
- Pipeline identity: hash of pipeline descriptor fields we control.
- Linking identity: hash of any dynamic linking descriptor (if used later).
- Environment identity: OS build + GPU/device identifiers.
- Outcome: miss vs incompatible/unusable vs fallback compile.

## Bytecode Sketch (Initial)
These opcodes mirror the existing scheduler logic:
- Control:
  - OP_NOOP
  - OP_JUMP (absolute PC)
  - OP_JUMP_IF (cond true/false)
  - OP_CASE (case table lookup)
  - OP_REPEAT (repeat table id)
  - OP_DONE
- Assign:
  - OP_ASSIGN (blocking)
  - OP_ASSIGN_NB
  - OP_ASSIGN_DELAY (delay-assign table id)
  - OP_FORCE
  - OP_RELEASE
- Waits:
  - OP_WAIT_TIME / OP_WAIT_DELTA
  - OP_WAIT_EVENT
  - OP_WAIT_EDGE (edge table id + edge kind, with prev snapshot)
  - OP_WAIT_COND (cond table id)
  - OP_WAIT_JOIN (join tag)
  - OP_WAIT_SERVICE (service return continuation)
- Events/Fork:
  - OP_EVENT_TRIGGER
  - OP_FORK (child list + join tag + join kind)
  - OP_DISABLE (target kind + target id)
- Services/Tasks:
  - OP_SERVICE_CALL (service kind + args + continuation id)
  - OP_SERVICE_RET_ASSIGN / OP_SERVICE_RET_BRANCH (continuation table)
  - OP_TASK_CALL / OP_RET (call frames; see opcode matrix)
  - OP_HALT_SIM (finish/stop semantics, if distinct from service)

Each opcode references indices in side tables:
- Signal/bit/array metadata table
- Delay info table
- Wait condition table
- Task call table

## Opcode Matrix (Draft)
Statement kinds map to opcode sequences; not all kinds become opcodes.

- kAssign:
  - blocking: OP_ASSIGN
  - nonblocking: OP_ASSIGN_NB
  - delayed: OP_ASSIGN_DELAY (DelayAssignInfo table)
  - syscall assign: OP_SERVICE_CALL + OP_SERVICE_RET_ASSIGN
- kIf:
  - regular: OP_JUMP_IF (cond table id)
  - feof/plusargs: OP_SERVICE_CALL + OP_SERVICE_RET_BRANCH
- kCase:
  - OP_CASE (case table id; header selects strategy)
- kFor:
  - OP_ASSIGN (init) + OP_JUMP_IF (cond) + OP_ASSIGN (step) + OP_JUMP
- kWhile:
  - OP_JUMP_IF (cond) + OP_JUMP (loop)
  - feof: OP_SERVICE_CALL + OP_SERVICE_RET_BRANCH
- kRepeat:
  - OP_REPEAT (repeat id + count expr id + body/after pcs)
  - Treat X/Z repeat counts as 0 iterations (compat with common simulators).
- kDelay:
  - OP_WAIT_TIME / OP_WAIT_DELTA
- kEventControl:
  - named event: OP_WAIT_EVENT
  - edge/list/star: OP_WAIT_EDGE (edge table id)
- kEventTrigger:
  - OP_EVENT_TRIGGER
- kWait:
  - OP_WAIT_COND (cond table id)
- kForever:
  - OP_JUMP (loop) + OP_WAIT_* (body-dependent)
- kFork:
  - OP_FORK (child list id + join tag + join kind) + OP_WAIT_JOIN
  - join_any: resume on first child completion
  - join_none: no OP_WAIT_JOIN
- kDisable:
  - OP_DISABLE (target kind: fork, block, task) + resume
- kTaskCall:
  - OP_TASK_CALL + OP_RET (use a bounded per-proc call frame stack)
- kForce / kRelease:
  - OP_FORCE / OP_RELEASE
- kBlock:
  - no-op unless labeled; labels only affect OP_DISABLE targets

## Coverage Checklist (Codebase)
Procedural statements are covered by the opcode matrix; the VM must preserve
all of these semantics.
- StatementKind:
  - kAssign, kIf, kBlock, kCase, kFor, kWhile, kRepeat, kDelay
  - kEventControl, kEventTrigger, kWait, kForever, kFork, kDisable
  - kTaskCall, kForce, kRelease

Additional scheduler responsibilities (not new opcodes, but must stay invariant
in the interpreter shader):
- Timing checks and specify paths (`module.timing_checks`, specify blocks).
- System tasks/functions via service calls (display/monitor/strobe, file I/O,
  plusargs, $finish/$stop) using OP_SERVICE_CALL + OP_WAIT_SERVICE + resume.
- fork join-kind handling and disable fork vs block/task semantics.
- Procedural assign/deassign handling if present in IR.
- Postponed-region ordering for $strobe/$monitor-style services.

Non-scheduler logic (not part of the VM, stays in comb/tick kernels):
- Continuous assigns, net strengths, trireg charge/decay, tran/tranif/cmos
  switches, unconnected drive defaults.

Expression coverage (data-driven tables required to keep shader invariant):
- ExprKind: kIdentifier, kNumber, kString, kUnary, kBinary, kTernary,
  kSelect, kIndex, kCall, kConcat.
- User-defined functions are inlined during elaboration; system function calls
  must route through OP_SERVICE_CALL and resume paths.

## Codebase Trace Points (Where the Semantics Live)
Use these anchors when translating behavior into bytecode tables/VM rules.
- AST enums and shapes:
  - StatementKind/ExprKind/NetType/SwitchKind: `src/frontend/ast.hh`
- Expression eval (2-state and 4-state emitters):
  - `EmitExpr`, `EmitExprSized`, `emit_expr4`: `src/codegen/msl_codegen.cc`
- Scheduler statement handling (procedural lowering to PC blocks):
  - Statement handling around `StatementKind::kAssign`...: `src/codegen/msl_codegen.cc`
- Service calls + resume/branch:
  - `build_service_args`, `build_syscall_args`, `emit_service_record`,
    `emit_service_record_with_pid`, `is_service_resume/cond` handling:
    `src/codegen/msl_codegen.cc`
- Timing checks + specify paths:
  - `BuildSpecifyPathBlocks`, timing check emission for `module.timing_checks`:
    `src/codegen/msl_codegen.cc`
- Trireg charge/decay and switch nets (non-VM comb/tick logic):
  - `trireg_decay_delay`, `IsTriregNet`, `SwitchKind` handling:
    `src/codegen/msl_codegen.cc`

## Call Frame Model (Draft)
Needed for OP_TASK_CALL/OP_RET to keep interpreter invariant.
- Per-proc bounded stack (fixed small depth, e.g., 4-8 frames).
- Frame fields:
  - return PC
  - optional locals base / task locals id
  - optional saved wait/service continuation state
- Storage:
  - use a flat `device` buffer (proc * depth + sp) to avoid thread-local
    arrays and register pressure
  - keep top-of-stack fields in registers when possible; write on call/ret
  - keep frame stride a multiple of 4 words (16 bytes) for alignment
  - if using structs, force explicit packing/alignment on host + MSL
- OP_TASK_CALL:
  - push frame, set pc to callee entry, mark ready
- OP_RET:
  - pop frame, restore pc, mark ready
- Overflow policy:
  - set sched_error and mark proc done (fail fast, deterministic)

## OP_CASE Table Layout (Draft)
Keep OP_CASE as one tiny handler; encode the strategy in table headers.
- CaseHeader fields:
  - kind: case/casez/casex
  - widthWords
  - strategy: dense LUT / bucketed scan / linear scan
  - defaultPC
  - entryBase, entryCount
  - optional bucketBase, bucketCount (bucketed)
  - optional lutBase, lutSize (dense)
- CaseEntry fields (scan strategies):
  - targetPC
  - wantLo[widthWords], wantHi[widthWords]
  - careLo[widthWords], careHi[widthWords]
- Matching:
  - compute exprLo/exprHi per kind
  - match when ((exprLo ^ wantLo) & careLo) == 0 and
    ((exprHi ^ wantHi) & careHi) == 0
- Semantics note:
  - casez treats Z as don't-care but X as significant; casex treats X and Z
    as don't-care. This requires an encoding that distinguishes X vs Z.
## OP_CASE Strategy Heuristics (Draft)
Pick strategy at codegen time; interpreter reads a header enum and runs a small
loop. Suggested thresholds:
- Linear scan: N <= 8-16 or heavy wildcards; wide buses (K > 32) with small N.
- Bucketed scan: 16 < N <= ~256 with mostly exact-ish patterns; bucket bits 6-8.
- Dense LUT: K <= 10 (<=12 if density is good and no wildcards); avoid LUT for
  large wildcard expansion (fall back to bucket/scan).

## OP_REPEAT Semantics (Draft)
- Evaluate count once on entry; if X/Z => 0 iterations.
- Store remaining count in per-proc VM state (loop stack or frame slot).

## Instruction Budget Policy (Draft)
- Deterministic fail-fast if budget exceeded (sets sched_error, proc done).
- If yielding is required later, define a strict same-delta continuation rule
  to avoid region reordering.

## Open Decisions (Need Final Confirmation)
- Expression table encoding details for all operators.
- OP_CASE exact matching rules using 2-bit 4-state encoding.
- OP_REPEAT counter storage and X/Z count semantics.
- Service continuation data model (resume/branch tables).
- Timing/specify integration points inside VM.
- Procedural assign/deassign handling if present in IR.
- Postponed-region ordering for $strobe/$monitor-style services.

## Step-by-Step Plan

### Step 0: Instrumentation and Baseline
- Record compile time and memory for:
  - `deprecated/verilog/test_clock_big_vcd.v`
  - picorv32 testbench
- Save current MSL and note kernel sizes/case counts.

### Step 0.5: Compile Invariance Proof
- Compile the interpreter kernel once via a dedicated `MTL4Compiler` instance
  with a `MTL4PipelineDataSetSerializer`.
- Store the resulting pipeline state in a `MTL4Archive` (serialized once).
- Run multiple different designs by changing only buffers.
- Confirm no new shader compilation is triggered (watch `MTL4CompilerTask`).
- Repeat in strict mode (archive-only) to catch key mismatches early.
- Explicitly serialize via `serializeAsArchiveAndFlush(...)` after capture.

### Step 1: Define Bytecode + Tables
- Add C++ structs for bytecode and table entries in codegen.
- Decide encoding (suggest 32-bit op word + optional operand word).
- Add emit helpers: `EmitOp(op, a, b)` and `EmitImmTable`.
- Define OP_CASE header/entry layouts and pick a strategy rule (LUT/bucket/scan).
- Upgrade 4-state storage to 2-bit encoding (00/01/10/11 = 0/1/X/Z).
  - Deferred until after VM tables land; keep val/xz for now.
- Ensure tri-state/Hi-Z sources produce Z (11) rather than X.
- Define join kinds and disable target kinds for OP_FORK/OP_DISABLE metadata.
- Define OP_REPEAT counter storage (per-proc slot or small repeat stack) and
  X/Z count semantics (0 iterations).
- Define expression encoding/tables that cover ExprKind without emitting
  per-expression MSL.
  - stack-based expr bytecode: op word + optional extra word(s) + imm pool
  - include logical vs bitwise operators and `===/!==` semantics explicitly.

### Step 2: Emit Bytecode in Codegen
- Emit bytecode + tables into host-side blobs and upload to `MTLBuffer`s.
- Replace per-proc switch emission with buffer-backed lookups:
  - `device const uint* bytecode`
  - `device const uint* proc_bytecode_offset/len`
- Preserve existing `pc` values but map to bytecode offsets.
- Use `MTL4SpecializedFunctionDescriptor` for 2-state vs 4-state where needed.
- Create a matching `MTL4ComputePipelineDescriptor` for each specialization.

### Step 3: VM Interpreter in MSL
- Add a helper function:
  - `gpga_exec_proc_vm(...)` that loops over bytecode until it blocks/done.
- Each opcode updates `sched_pc`, `sched_state`, wait fields, and NB queues.
- Ensure `sched_pc` is updated exactly when the old code did.
- Keep the loop minimal; call helpers for complex ops.
- Emit a `gpga_*_sched_vm_eval_cond` helper and wire `OP_JUMP_IF` to evaluate
  the condition id and store into `sched_vm_cond_val/xz`.

### Step 4: Handle Complex Ops
- Delay assign and task/service ops should route to existing helpers
  (reuse `emit_delay_assign_apply` and service enqueue logic).
- Conditional/jump logic should use precomputed condition tables.

### Step 5: Replace Dispatch in Scheduler
- Group helpers call VM for each proc instead of `switch(pc)`.
- Keep A-full path behind a flag for bisecting.
- Table-drive assign/force/release entries with fallback for complex cases.
- Table-drive service call/return entries with fallback for complex cases.
- Apply force/passign overrides via VM tables (passign first, force wins).

### Step 6: Validate Semantics
- Run VCD diffs against gold:
  - `test_clock_big_vcd` (2-state + 4-state)
- Run `deprecated/verilog/pass/test_system_monitor.v`.
- Run small async runtime profile.

### Step 7: Stress Test
- Run picorv32 compile and measure:
  - compile latency
  - memory footprint
  - whether compiler reaches backend quickly

## Rollout Plan
- Add a build-time flag or CLI switch:
  - `--sched-vm` to enable VM path
  - default remains A-full until stable
- Once validated, flip default and keep A-full for fallback.
- Persist the invariant interpreter pipeline in `MTL4Archive` to avoid
  recompiling for new designs.

## Risks / Watch-outs
- Off-by-one in `pc` translation (bytecode offsets vs old `pc` values).
- Wait state transitions: ensure identical timing vs old scheduler.
- Delay assignments and NB scheduling must preserve order.
- VM loop unrolling by compiler (use loop hints and runtime bounds).
- Avoid per-design specialization; archive lookup should be the default path.
- If stitched dispatch is explored later, validate constraints before using it
  in the interpreter hot path.
- Plan C (escape hatch): if even the small `switch(opcode)` becomes a compiler
  hotspot, consider splitting heavier opcode handlers into separately compiled
  units and link them (dynamic linking descriptor + binary functions). Keep this
  invariant to design size; do not stitch or link per design.
- Archive compatibility varies by OS/device; handle "present but unusable"
  gracefully with fallback (and log it distinctly from a miss).
- Serializer overhead can increase peak compile memory; keep it in the harvest
  path only, not steady-state runtime.
- If 4-state encoding does not distinguish X vs Z, casez semantics will be
  incorrect; fix encoding or lower casez/casex before the VM.

## Success Criteria
- Interpreter shader compilation is invariant to design size.
- Interpreter compile memory stays under ~100 MB (picorv32 baseline).
- Interpreter compile time is ~1s order-of-magnitude (picorv32 baseline).
- Runtime GPU buffers for picorv32 stay around ~1 MB.
- No shader recompilation per design once `MTL4Archive` is populated.
- VCDs match gold for all existing regression tests.

## Progress Log
- [x] Step 0: Baseline metrics recorded
- [x] Step 1: Bytecode format defined (op enum + per-proc slot layout + VM tables for
  cond/delay/case/assign/force/release/service; expression encoding format defined; 2-bit encoding deferred)
- [x] Step 2: Bytecode emission in codegen (bytecode op stream + case/expr table buffers wired; kAssign incl. delay + syscall assigns via service ops; kIf/kWhile
  service branches for feof/plusargs; kTaskCall system tasks; kBlock/kDelay/kRepeat/kCase/kEventControl/kWait/
  kForever/kEventTrigger/kFork/kDisable/kForce/kRelease/kFor; synthetic for-init/for-step assigns registered)
- [x] sched_pc mirrored to sched_vm_ip in VM exec path to preserve legacy pc visibility
- [x] Step 3: VM interpreter in MSL (base loop + JUMP/JUMP_IF/NOOP/TASK_CALL/RET
  + ASSIGN/ASSIGN_NB/ASSIGN_DELAY/REPEAT/CASE/FORK/JOIN/DISABLE/FORCE/RELEASE/SERVICE_CALL/WAIT_SERVICE/
  SERVICE_RET_ASSIGN/SERVICE_RET_BRANCH wired in 4-state and 2-state; cond/delay/case eval helpers wired;
  WAIT_TIME/WAIT_DELTA/WAIT_EVENT/EVENT_TRIGGER/WAIT_EDGE/WAIT_COND/OP_HALT_SIM wired in 4-state and 2-state)
- [x] Signal-triggered OP_HALT_SIM wired (sched_halt_mode buffer + SIGINT/SIGTERM host hook)
- [x] VM condition table (const-only) buffer wired; eval checks table before per-cond switch
- [x] 2-state VM condition bytecode for simple exprs (signal/const/unary/binary/ternary) with signal table; unsupported cases fall back to switch
- [x] 4-state VM condition bytecode for simple exprs (signal/const/unary/binary/ternary) using 4-state ops; unsupported cases fall back to switch
- [x] VM tables bound via argument buffer (MTLArgumentEncoder + MTL4ArgumentTable) to stay within Metal 4 buffer index limits
- [x] VM helpers add packed signal/nb/force setup; guard optional buffers in VM ops (delay/dnba/repeat/event/edge)
- [x] VM signal layout matches 2-state packed slots (val-only, no xz slots) to keep VM signal offsets aligned
- [x] VM delay-assign entries data-driven (entry table + expr-bytecode eval with fallback for complex cases)
- [x] VM assign entries table added (blocking + nonblocking full-signal path via expr bytecode; fallback switch for complex cases)
- [ ] 2-bit 4-state storage migration (deferred from Step 1)
- [x] Step 4: Complex ops handled
- [x] Step 5: Scheduler dispatch updated
- [x] Step 5a: Table-driven assign/force/release entries (fallback on complex cases)
- [x] Step 5b: Table-driven force/passign overrides in scheduler comb update
- [x] Step 5c: Table-driven service call/return entries (fallback on complex cases)
- [ ] Step 6: VCD regressions pass
- [ ] Step 7: picorv32 compile improved
