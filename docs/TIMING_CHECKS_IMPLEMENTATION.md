# Timing Checks Implementation Plan

Goal: implement full IEEE 1364-2005 specify timing checks in MetalFPGA, including SDF annotation, runtime enforcement, and diagnostics.

This document is a living checklist for a complete implementation (no MVP shortcuts).

## Scope (what "full" means here)

Timing checks:
- $setup
- $hold
- $setuphold
- $recovery
- $removal
- $recrem
- $skew
- $timeskew
- $fullskew
- $width
- $period
- $pulsewidth
- $nochange

Specify paths:
- Path declarations with delays (simple and conditional).
- Showcancelled / noshowcancelled (if supported).

Annotation:
- SDF timing checks, including COND matching.
- Min/typ/max selection.

Violation behavior:
- Notifier support.
- X-propagation policy (configurable).
- Diagnostics hooks (service events or debug logs).

## Current state

Parsing:
- Full timing-check argument parsing is implemented:
  - Events (data/ref), edge lists, `&&&` conditions, limits, notifier,
    threshold, delayed_ref/data, and flags are captured into `TimingCheck`.
- Specify block handling also tracks showcancelled/noshowcancelled ordering
  and conditional path output selection (no path delay AST yet).
- SDF timing checks are parsed and matched in `src/main.mm` (values not applied).

MSL emission:
- Timing checks are emitted as live scheduler logic with edge detection,
  timing windows, and notifier assignment.
- Threshold is parsed but currently unused in width/pulsewidth checks.

Runtime:
- Timing check state buffers are allocated (prev val/xz, data/ref times,
  window start/end) and wired into the scheduler.
- No per-check metadata arrays (IDs/kinds) are stored; codegen inlines checks.

## AST and parser changes

### Expand TimingCheck representation (required)

[done] `TimingCheck` in `src/frontend/ast.hh` includes:
- kind enum (setup/hold/setuphold/width/period/etc.)
- data/ref events with edge lists + `&&&` conds
- min/typ/max limits (two slots for dual-limit checks)
- notifier, threshold, delayed_ref/data, event_based/remain_active flags
- raw/normalized strings for SDF matching

### Parse full timing-check arguments (required)

[done] `HandleSpecifyTimingCheck` parses full argument lists for all
supported checks, including edge lists, `&&&` conditions, limits,
notifier, threshold, delayed_ref/data, and flags.

### Specify path parsing (required)

[partial] Parsing tracks showcancelled ordering and conditional output
selection, but no AST or delay modeling exists yet.

## Elaboration / semantic resolution

### Timing-check binding (required)

[done] Elaboration clones/simplifies timing check expressions and renames
signals for flattened modules (events, limits, conditions, notifier,
delayed_ref/data).

### Specify path resolution (required)

[pending] No specify path resolution/delay modeling yet.

## Runtime data structures (GPU)

[partial] Per-check state arrays exist and are allocated:
- prev val/xz, data/ref edge times, window start/end.

[pending] Metadata arrays (kind, signal IDs, resolved limits) are not stored;
codegen currently inlines logic directly.

## Scheduler integration (MSL codegen)

### Edge detection integration (required)

Hook into the existing scheduler edge-evaluation stage:
- When a relevant edge is detected on data or ref signal:
  - Record edge time.
  - Evaluate condition (&&&).
  - Start or close timing windows.

### Timing window logic (required)

[done] Implemented in codegen for setup/hold/setuphold/recovery/removal/recrem,
period, width/pulsewidth, skew/timeskew/fullskew, and nochange.

### Condition evaluation (required)

[done] Conditions are evaluated via emitted boolean expressions; X/Z behavior
follows current `cond_bool` semantics (not explicitly documented yet).

### Notifier and violation behavior (required)

[done] Notifier assignment is emitted on violation; no implicit X-prop.
[pending] Optional diagnostic/X-prop flags not implemented.

## SDF annotation support

### Matching (existing)

`src/main.mm` already matches SDF entries to timing checks by:
- name, edge, signal, condition

### Apply SDF values (required)

[pending] SDF matching exists, but values are not applied to checks.

### COND semantics (required)

Ensure conditional checks only apply when condition matches.

## Host-side setup

### Parameter packing (required)

[partial] Per-check state buffers are allocated; metadata packing is not.

### Debug/verbose hooks (required)

[pending] No service record / VCD hooks for timing violations yet.

## Specify path delays (required)

[pending] Not implemented (only parser bookkeeping).

## Tests and validation

### Unit / parser tests (required)

[partial] Parser handles full arg lists; no dedicated unit tests added.

### Goldens / functional tests (required)

[partial] Timing-check MSL compiles clean; functional timing-violation
goldens are not wired yet.

### SDF tests (required)

[pending] SDF value application tests not implemented.

## Config knobs

Add CLI flags / environment variables:
- Enable/disable timing checks.
- Select min/typ/max values.
- Enable/disable X-propagation on violation.
- Verbose timing diagnostics.

Implemented:
- `METALFPGA_SPECIFY_DELAY_SELECT` (fast/slow/typ selection).
- `METALFPGA_NEGATIVE_SETUP_MODE` (allow/clamp/error for setuphold).

## Implementation sequencing

- MSL emission review completed; timing-check logic already integrated.

## Milestones

1) [done] Parser + AST upgrade for full timing-check args.
2) [done] Elaborate and bind checks to signals.
3) [done] Runtime state + scheduler edge integration.
4) [done] Notifier + violation policy.
5) [pending] SDF annotation values applied to checks.
6) [pending] Specify path delays modeled.

## Open questions

- Condition evaluation when signals are X/Z: pessimistic or optimistic?
- Default violation behavior: notifier only, or X-prop?
- Performance tradeoffs for per-edge checks on GPU.
- SDF precedence for multiple matching checks.
