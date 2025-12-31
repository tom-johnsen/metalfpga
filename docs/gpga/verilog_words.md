# Verilog Words and Status

## Implemented
- module
- endmodule
- input
- output
- inout
- wire
- tri (alias of wire)
- assign
- continuous assign drive strengths (`(strong0, strong1)` etc)
- module instantiation (named + positional port connections)
- reg
- integer (32-bit signed reg)
- signed (ports/nets)
- local reg declarations inside procedural blocks (treated as locals)
- compiler directives: `define` / `undef`, `ifdef` / `ifndef` / `else` / `endif`, `include`, `timescale` (ignored)
- always
- always @*
- always @(a, b, c) / always @(a or b)
- always @(posedge clk, negedge reset) (edge lists; first edge used for tick scheduling)
- initial
- begin / end
- posedge
- negedge
- nonblocking `<=` (blocking `=` allowed in v0 always)
- numeric literals in instance connections (inputs only)
- if / else
- bit/part selects (`a[3]`, `a[7:0]`, `a[i +: 4]`, `a[i -: 4]`)
- unpacked reg/wire arrays (`reg [7:0] mem [0:255]`, multi-dimensional)
- indexed access (`mem[addr]`, `mem[row][col]`, `a[i]` for variable bit select)
- parameter (module header and body, constant expressions only)
- localparam (module body, constant expressions only)
- defparam (single-level instance overrides)
- function (inputs + single return assignment, inlined during elaboration)
- specify / endspecify (parsed and ignored; timing not modeled)
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
- generate blocks (genvar, for/if-generate with wire/reg/assign/instance/always/initial/localparam items; parameterized port widths supported)
- instance parameter overrides (`#(...)`) applied
- case / casez / casex (procedural)
- `$signed(...)` / `$unsigned(...)` casts
- `$clog2(...)` constant folding
- pullup / pulldown (pull strength + highz for opposite value)
- gate primitives (`buf`, `not`, `and`, `or`, `nand`, `nor`, `xor`, `xnor`,
  `bufif0/1`, `notif0/1`, `nmos`, `pmos`) with drive strengths (no delays)
- switch primitives (`tran`, `tranif0/1`, `cmos`) in 4-state mode (no delays)
- special net types (`wand`, `wor`, `tri0`, `tri1`, `triand`, `trior`,
  `trireg`, `supply0`, `supply1`)

### System I/O words

Progress: 22/22 words added [######################]

Functions:
- `$fopen`
- `$fclose`
- `$fgetc`
- `$fgets`
- `$feof`
- `$ftell`
- `$fseek`
- `$ferror`
- `$ungetc`
- `$fread`
- `$fscanf`
- `$sscanf`
- `$test$plusargs`
- `$value$plusargs`

Tasks:
- `$fdisplay`
- `$fwrite`
- `$fflush`
- `$rewind`
- `$readmemh`
- `$readmemb`
- `$writememh`
- `$writememb`

Notes:
- File I/O functions must appear as standalone assignment expressions.
- `$feof`, `$test$plusargs`, and `$value$plusargs` are also supported as direct
  `if`/`while` conditions (no compound expressions yet).

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
- Multiple continuous drivers on wire nets are resolved in 4-state; regs/always conflicts still error

### Codegen notes
- Shift overshoot uses Verilog semantics: `a << s` or `a >> s` yields 0 when `s >= width`
- Unsized literals are minimally sized and explicitly zext/trunc'd to the context width
- Tristate `Z` is treated as high-impedance for driver resolution; reading `Z` behaves like `X`

## Planned
- tasks
- remaining Verilog-2001 constructs as encountered

## SystemVerilog (far later, after MSL is working)
- logic
- always_comb / always_ff / always_latch
- typedef / enum / struct / union
- interfaces / modports
