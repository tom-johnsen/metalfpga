# Scheduler VM Next Opcodes (Candidate List)

These are candidate opcode-level changes I agree are worth pursuing based on
the latest `sched_vm_fallback_diag.txt`. This is a scoped, actionable list for
review and sequencing.

## Agreed Candidates

### 1) Partial / indexed LHS writes (single opcode family)
Status: done (assign entry now carries index/range metadata; VM apply handles
array/bit/range/indexed-range writes directly).
**Problem:** `lhs_has_index` (69) + `lhs_has_range` (65).

**Goal:** support array index, bit select, range select, and index+range in a
single write-back path.

**Proposal:** extend `SchedulerVmAssignEntry` with `lhs_kind` + `lhs_aux`
and add `kAssignPart` (or reinterpret `kAssign` to dispatch by `lhs_kind`).

**Notes:**
- `lhs_aux` can store an expr offset for index, plus a packed range descriptor.
- Avoids expression VM blowups; moves only the *placement* into the opcode.

### 2) Explicit array element store
Status: done (array element writes flow through the same assign entry path).
**Problem:** array writes such as `mem[(addr >> 2)] <= ...` and `cpuregs[idx]`.

**Proposal:** `kAssignArray` (or `lhs_kind = array` in `kAssignPart`).

**Notes:**
- Index comes from an expr offset.
- Width comes from `SchedulerVmSignalEntry`.
- Optional bounds checks (debug-only).

### 3) X/Z routing for RHS literals and ternaries
Status: done (rhs_unencodable now 3, all from call expressions like
`$fopen/$test$plusargs`, not X/Z literals).
**Problem:** `rhs_unencodable` (23) dominated by X/Z literals and
`cond ? value : 'x` patterns.

**Proposal:** add a direct X/Z write path:
- `kAssignXZ` or `lhs_kind = xz_literal` with packed value/xz payload.
- `kAssignSelect` for `cond ? value : 'x/z` (branch + 2 simple assigns).

**Notes:**
- Keeps expression VM simple.
- Avoids embedding X/Z into arithmetic paths.

### 4) Service argument staging (small shim)
Status: done (service args now accept `%s` ternaries over string literals and
`$readmem*` filename identifiers; filesystem calls embedded in trivial boolean
forms now route through service assigns, clearing `rhs_unencodable`).
**Problem:** `$display("%s", expr)` and `$readmemh(var, mem)` fallbacks.

**Proposal:** emit a small staging temp for `%s` cases:
```
tmp = cond ? "A" : "B";
$display("%s", tmp);
```

**Notes:**
- No new semantics, only canonicalization.
- Could be a new mini-op or a compiler-side rewrite.

### 5) Wide string literal assigns (no fallback)
Status: done (wide string literals now emit imm-table words and use a wide-const
assign path).
**Problem:** wide string regs (e.g. 1024-bit) assigned from string literals.

**Proposal:** allow wide literal assignment by storing the string bytes as
wide words in the expr imm table and copying them directly in VM assign (skip
expr eval).

## Optional / Deferred

### Masked merge write
**Problem:** range updates that are effectively `dest = (dest & ~mask) | (rhs << shift)`.

**Proposal:** `kAssignMasked` (mask + shift + rhs expr).

**Notes:**
- Useful when the range writes are dense and repetitive.
- Might be redundant if `kAssignPart` + wider expr VM is sufficient.

## Why This Order

1) `kAssignPart` / array support: clears ~134 of 159 top fallbacks.
2) X/Z routing: removes most remaining `rhs_unencodable` (now just call exprs).
3) Service arg staging: eliminates remaining service fallbacks.

## Open Questions

- Do we want one unified `kAssignPart` with `lhs_kind`, or distinct opcodes?
- Is X/Z best handled via assign-level opcodes or extending expr VM literal support?
- For service args, is a compiler-side rewrite preferred over a runtime opcode?
