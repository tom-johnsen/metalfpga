# Metal4 Scheduler VM Docusprint

## Purpose
Concise snapshot of the bytecode VM plan, opcode surface, and open decisions
before implementation. Intended for final external feedback.

## Core Principle
Interpreter shader is invariant to design size. All design data (bytecode,
tables) lives in buffers; no per-design shader generation.

## Invariants / Verilog-True Requirements
- Preserve 4-state semantics; use 2-bit encoding per bit:
  - 00=0, 01=1, 10=X, 11=Z.
- case/casez/casex must be exact (casez needs X/Z distinction).
- join/join_any/join_none semantics must match Verilog (fork metadata).
- disable semantics must cover disable fork vs named blocks/tasks.
- Logical vs bitwise operators + case equality (`===/!==`) must be exact.
- Timing checks + specify paths must remain correct.
- Services ($display/$monitor/$strobe/$finish/$stop, file I/O, plusargs) must
  resume correctly via continuations.
- Trireg charge/decay and switch-level nets remain in comb/tick kernels.

## Metal 4 Compile/Archive Flow (Compile Once, Run Many)
- Use MTL4Compiler + MTL4PipelineDataSetSerializer to compile interpreter.
- Persist pipeline state in MTL4Archive.
- Strict mode (CI): archive-only creation (no compiler).
- Fallback mode (prod): lookupArchives + compile-on-miss + telemetry.

## Telemetry (Archive Miss/Unusable)
- Function identity, specialization hash, pipeline descriptor hash.
- Dynamic linking descriptor hash (if used).
- OS build + GPU/device identifiers.
- Outcome: miss vs incompatible/unusable vs fallback compile.

## Opcode Set (Draft)
Control:
- OP_NOOP
- OP_JUMP
- OP_JUMP_IF
- OP_CASE
- OP_REPEAT
- OP_DONE

Assign:
- OP_ASSIGN
- OP_ASSIGN_NB
- OP_ASSIGN_DELAY
- OP_FORCE
- OP_RELEASE

Waits:
- OP_WAIT_TIME / OP_WAIT_DELTA
- OP_WAIT_EVENT
- OP_WAIT_EDGE
- OP_WAIT_COND
- OP_WAIT_JOIN
- OP_WAIT_SERVICE

Events/Fork:
- OP_EVENT_TRIGGER
- OP_FORK (join kind in metadata; count in low bits)
- OP_DISABLE (target kind in metadata)

Services/Tasks:
- OP_SERVICE_CALL
- OP_SERVICE_RET_ASSIGN / OP_SERVICE_RET_BRANCH
- OP_TASK_CALL / OP_RET
- OP_HALT_SIM (if distinct from service)

## Opcode Matrix (StatementKind Coverage)
- kAssign -> OP_ASSIGN / OP_ASSIGN_NB / OP_ASSIGN_DELAY / service resume
- kIf -> OP_JUMP_IF (cond table) / service-conditional resume
- kCase -> OP_CASE (case table)
- kFor -> init + OP_JUMP_IF + step + OP_JUMP
- kWhile -> OP_JUMP_IF + OP_JUMP
- kRepeat -> OP_REPEAT
- kDelay -> OP_WAIT_TIME / OP_WAIT_DELTA
- kEventControl -> OP_WAIT_EVENT / OP_WAIT_EDGE
- kEventTrigger -> OP_EVENT_TRIGGER
- kWait -> OP_WAIT_COND
- kForever -> OP_JUMP + OP_WAIT_* (body-dependent)
- kFork -> OP_FORK + OP_WAIT_JOIN
- kDisable -> OP_DISABLE (target kind: fork, block, task) + resume
- kTaskCall -> OP_TASK_CALL + OP_RET
- kForce / kRelease -> OP_FORCE / OP_RELEASE
- kBlock -> no-op unless labeled (disable targets)

## Data Tables (Draft)
- Bytecode stream per proc + offset/len tables.
- Expression tables (all ExprKind) with 2-bit 4-state semantics.
- OP_CASE tables with header (kind/width/strategy/offsets) + entries.
- Delay tables (rise/fall/turn-off; min/typ/max).
- Wait condition tables, edge tables, fork/join tables, service continuation
  tables.

## Expression Bytecode (Draft)
- Stack-based bytecode stream: 32-bit op word + optional extra word(s).
- Word format: low 8 bits = op, upper 24 bits = arg.
- Constants/imm pool stored in a separate word array.
- Fork metadata: arg encodes join kind in high 8 bits and child count in low 24
  bits.
- Disable metadata: arg values (0=block label, 1=child proc, 2=cross-proc).

## Call Frame Model
- Per-proc bounded stack in flat device buffer.
- Frame: return PC + locals base + continuation state (if needed).
- Overflow policy: deterministic fail-fast (sched_error, proc done).
- Use fixed 16-byte frame stride and `uint` fields to avoid layout drift.
- Keep only top-of-stack fields in registers; spill on call/ret boundaries.

## OP_CASE Strategy (Draft Heuristics)
- Linear scan for small N or heavy wildcards.
- Bucketed scan for mid-size N (16-256).
- Dense LUT for small K (<=10, <=12 if dense).

## OP_CASE Data Layout (Draft)
- Store expression as two bit-planes per word: `loBits` (LSB), `hiBits` (MSB).
- Per case item: `careLo/careHi` + `wantLo/wantHi` + target PC.
- Match predicate per word:
  - `((exprLo ^ wantLo) & careLo) == 0` and `((exprHi ^ wantHi) & careHi) == 0`.
- casez/casex adjust care masks on both sides (expr + item) to honor don’t-care.

## OP_REPEAT Semantics (Draft)
- Evaluate count once on entry; if X/Z => 0 iterations.
- Store remaining count in per-proc VM state (loop stack or frame slot).

## Instruction Budget Policy (Draft)
- Deterministic fail-fast if budget exceeded (sets sched_error, proc done).
- If yielding is required later, define a strict “same-delta continuation”
  rule to avoid region reordering.

## Open Decisions (Need Final Confirmation)
- Expression table encoding details for all operators.
- OP_CASE exact matching rules using 2-bit 4-state encoding.
- OP_REPEAT counter storage and X/Z count semantics.
- Service continuation data model (resume/branch tables).
- Timing/specify integration points inside VM.
- Procedural assign/deassign handling if present in IR.
- Postponed-region ordering for $strobe/$monitor-style services.

## Questions for Metal4GPT
1) Any missing statement/operator semantics in the opcode matrix?
2) Recommended OP_CASE strategy thresholds or data layout changes?
3) Any pitfalls with 2-bit 4-state encoding + casez/casex?
4) Best practice for OP_REPEAT counter storage and X/Z count behavior?
5) Call frame layout sanity check for Metal (alignment/stride expectations)?
