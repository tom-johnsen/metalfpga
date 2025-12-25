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
- integer (32-bit signed reg)
- signed (ports/nets)
- local reg declarations inside procedural blocks (treated as locals)
- always
- always @*
- initial
- begin / end
- posedge
- negedge
- nonblocking `<=` (blocking `=` allowed in v0 always)
- numeric literals in instance connections (inputs only)
- if / else
- bit/part selects (`a[3]`, `a[7:0]`)
- unpacked reg/wire arrays (`reg [7:0] mem [0:255]`, multi-dimensional)
- indexed access (`mem[addr]`, `mem[row][col]`, `a[i]` for variable bit select)
- parameter (module header and body, constant expressions only)
- localparam (module body, constant expressions only)
- function (inputs + single return assignment, inlined during elaboration)
- ternary `?:`
- unary operators (`~`, `-`, `+`)
- logical operators (`!`, `&&`, `||`)
- reduction operators (`&`, `|`, `^`, `~&`, `~|`, `~^`)
- concatenation / replication (`{}`, `{N{}}`)
- based literals (`'b`, `'h`, `'d`, `'o`)
- signed based literals (`'sb`, `'sh`, `'sd`, `'so`)
- equality / relational (`==`, `!=`, `<`, `>`, `<=`, `>=`)
- shifts (`<<`, `>>`)
- arithmetic shift (`>>>`)
- nested begin/end blocks inside always/if
- for loops (constant bounds, unrolled during elaboration)
- while loops (constant bounds, unrolled during elaboration)
- repeat loops (constant bounds, unrolled during elaboration)
- instance parameter overrides (`#(...)`) applied
- case / casez / casex (procedural)
- `$signed(...)` / `$unsigned(...)` casts

### Operators implemented
- `+` `-` `%`
- `&` `|` `^`
- reduction `&` `|` `^`
- logical `!` `&&` `||`
- shifts `<<` `>>` `>>>`
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
- tasks
- remaining Verilog-2001 constructs as encountered

## SystemVerilog (far later, after MSL is working)
- logic
- always_comb / always_ff / always_latch
- typedef / enum / struct / union
- interfaces / modports
