# Flattened Netlist Invariants (v0)

This document describes what the elaboration step guarantees about the
flattened netlist it produces.

## Structure
- Exactly one flat top module is produced.
- All instance hierarchies are inlined; no module instances remain.
- All signal references in assigns/always blocks point to flat signal names.
- Every flat signal name is unique in the netlist.
- All always blocks in the flattened netlist reference only flat signals.

## Naming + Debug Map
- Instance-local nets are prefixed with `<instance>__` (repeated per depth).
- `flat_to_hier` maps flat names back to a hierarchical path like
  `top.inst.subinst.signal`.
- Top-level ports are always present in `flat_to_hier`.

## Connectivity
- Named and positional port connections are resolved to flat signals.
- Literal connections are allowed only for input ports; they synthesize an
  internal wire plus a constant assign.
- Unconnected inputs are defaulted to 0 with a warning and a synthesized assign.
- Unconnected outputs are allowed with a warning.
- Multiple drivers for the same signal are rejected as an error.

## Expressions
- All selects are constant indices (bit or part select) after elaboration.
- Replication counts are constant expressions.
- Parameter values are constant expressions.

## Known v0 Limits
- Instance parameter overrides are parsed but widths are not re-evaluated per
  instance yet.
- The flattening output does not insert explicit cast nodes; the dump may show
  `zext/trunc` for readability, but the IR remains structural.
