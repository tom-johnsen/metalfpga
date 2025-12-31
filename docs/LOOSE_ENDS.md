# Loose Ends / Open Gaps

This is a running list of known gaps and stubs in the codebase, collected from
the current source. Use it to decide what to tackle next.

## Runtime / Host Integration

- File I/O system functions in expressions are still limited to standalone
  calls (assignment RHS) or `$feof`/plusargs conditions; compound expressions
  are rejected. The MSL stub paths remain for non-supported contexts.
  File: src/codegen/msl_codegen.cc (ExprKind::kCall handling)

## Elaboration / Semantic Restrictions

- Function calls are rejected in runtime expressions (assigns/always/tasks).
  File: src/core/elaboration.cc (ValidateNoFunctionCalls)
- File I/O system functions are rejected in continuous assignments.
  File: src/core/elaboration.cc

## Constant Evaluation Limits

- Constant functions do not allow array assignments.
  File: src/core/elaboration.cc (AssignConstVar)
- Unsupported statements in constant functions (e.g., non-assign/if/for/etc).
  File: src/core/elaboration.cc (EvalConstStatement)
- Recursive function calls are not allowed in constant evaluation.
  File: src/core/elaboration.cc (EvalConstFunction)

## Function Body Limits

- Array assignments not supported in function bodies.
  File: src/core/elaboration.cc (InlineFunctionAssignment)
- Bit/part select on real values not allowed in function bodies.
  File: src/core/elaboration.cc (InlineFunctionAssignment)

## Parser Gaps (Explicit "v0" Unsupported)
- Switch arrays not supported in v0.
  File: src/frontend/verilog_parser.cc
- Switch primitives in generate blocks not supported in v0.
  File: src/frontend/verilog_parser.cc
- `unique` / `priority` statements not supported in v0.
  File: src/frontend/verilog_parser.cc
- `fork/join_any/join_none` not supported in v0.
  File: src/frontend/verilog_parser.cc
- Streaming operator not supported in v0.
  File: src/frontend/verilog_parser.cc
- Arrayed reg locals not supported in functions.
  File: src/frontend/verilog_parser.cc
- Arrayed real locals not supported in functions.
  File: src/frontend/verilog_parser.cc
- Real array initializer not supported.
  File: src/frontend/verilog_parser.cc
- Initializers in generate declarations not supported.
  File: src/frontend/verilog_parser.cc

## SystemVerilog Scope

- SystemVerilog test corpus is expected to fail; support is out-of-scope for
  v1 (see `verilog/systemverilog/`).
  File: docs/VERILOG_TEST_OVERVIEW.md

## Notes

- There are documentation TODOs in `docs/SOFTFLOAT64_IMPLEMENTATION.md` for
  alternative algorithm implementations, but those are doc-only.
    - That one is already implemented, just not updated docs.
