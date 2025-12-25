# Verilog Words and Status

## Implemented
- module
- endmodule
- input
- output
- inout
- wire
- assign
- module instantiation (named + positional port connections)
- reg
- always
- always @*
- begin / end
- posedge
- negedge
- nonblocking `<=` (blocking `=` allowed in v0 always)
- numeric literals in instance connections (inputs only)
- if / else
- bit/part selects (`a[3]`, `a[7:0]`)
- unpacked reg/wire arrays (`reg [7:0] mem [0:255]`)
- indexed access (`mem[addr]` for arrays, `a[i]` for variable bit select)
- parameter (module header and body, constant expressions only)
- localparam (module body, constant expressions only)
- ternary `?:`
- unary operators (`~`, `-`, `+`)
- concatenation / replication (`{}`, `{N{}}`)
- based literals (`'b`, `'h`, `'d`, `'o`)
- equality / relational (`==`, `!=`, `<`, `>`, `<=`, `>=`)
- shifts (`<<`, `>>`)
- nested begin/end blocks inside always/if
- instance parameter overrides (`#(...)`) applied
- case / casez / casex (procedural)

### Operators implemented
- `+` `-`
- `&` `|` `^`
- parentheses for grouping
- `=` in continuous assignments
- `[msb:lsb]` ranges in declarations
- decimal literals

### Elaboration rules
- Unconnected inputs default to 0 (2-state) or X (4-state) with a warning
- Unconnected outputs are ignored with a warning
- Multiple drivers on a net is an error

### Codegen notes
- Shift overshoot uses Verilog semantics: `a << s` or `a >> s` yields 0 when `s >= width`
- Unsized literals are minimally sized and explicitly zext/trunc'd to the context width

## Planned
- generate / genvar
- for / while / repeat
- functions / tasks
- remaining Verilog-2001 constructs as encountered

## SystemVerilog (far later, after MSL is working)
- logic
- always_comb / always_ff / always_latch
- typedef / enum / struct / union
- interfaces / modports
