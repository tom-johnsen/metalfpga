# Four-State Truth Tables (Draft)

These tables are for the bytecode VM and 4-state execution semantics.
They use Verilog symbols 0/1/x/z and should be verified against the
IEEE 1364/1800 LRM before locking in.

## Bitwise AND (&)

| & | 0 | 1 | x | z |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 |
| 1 | 0 | 1 | x | x |
| x | 0 | x | x | x |
| z | 0 | x | x | x |

## Bitwise OR (|)

| | | 0 | 1 | x | z |
|---|---|---|---|---|
| 0 | 0 | 1 | x | x |
| 1 | 1 | 1 | 1 | 1 |
| x | x | 1 | x | x |
| z | x | 1 | x | x |

## Bitwise XOR (^)

| ^ | 0 | 1 | x | z |
|---|---|---|---|---|
| 0 | 0 | 1 | x | x |
| 1 | 1 | 0 | x | x |
| x | x | x | x | x |
| z | x | x | x | x |

## Bitwise NOT (~)

| ~ | result |
|---|--------|
| 0 | 1 |
| 1 | 0 |
| x | x |
| z | x |

## Gate-Level AND/OR/XOR (X/Z Combined)

These tables use a combined X/Z column/row as shown in the source image.

### AND

| AND | 0 | 1 | X/Z |
|-----|---|---|-----|
| 0   | 0 | 0 | 0 |
| 1   | 0 | 1 | X |
| X/Z | 0 | X | X |

### OR

| OR | 0 | 1 | X/Z |
|----|---|---|-----|
| 0  | 0 | 1 | X |
| 1  | 1 | 1 | 1 |
| X/Z| X | 1 | X |

### XOR

| XOR | 0 | 1 | X/Z |
|-----|---|---|-----|
| 0   | 0 | 1 | X |
| 1   | 1 | 0 | X |
| X/Z | X | X | X |

## Gate-Level NAND/NOR/XNOR (X/Z Combined)

These tables use a combined X/Z column/row as shown in the source image.

### NAND

| NAND | 0 | 1 | X/Z |
|------|---|---|-----|
| 0    | 1 | 1 | 1 |
| 1    | 1 | 0 | X |
| X/Z  | 1 | X | X |

### NOR

| NOR | 0 | 1 | X/Z |
|-----|---|---|-----|
| 0   | 1 | 0 | X |
| 1   | 0 | 0 | 0 |
| X/Z | X | 0 | X |

### XNOR

| XNOR | 0 | 1 | X/Z |
|------|---|---|-----|
| 0    | 1 | 0 | X |
| 1    | 0 | 1 | X |
| X/Z  | X | X | X |

## Gate-Level BUF/NOT (X/Z Combined)

These tables use a combined X/Z input as shown in the source image.

### NOT

| IN | OUT |
|----|-----|
| 0  | 1 |
| 1  | 0 |
| X/Z| X |

### BUF

| IN | OUT |
|----|-----|
| 0  | 0 |
| 1  | 1 |
| X/Z| X |

## Gate-Level BUFIF0/BUFIF1 (X/Z Combined)

These tables use a combined X/Z input as shown in the source image.
Rows/columns are labeled 0/1/X/Z in the image; values include L/H as shown.

### BUFIF0

| BUFIF0 | 0 | 1 | X/Z |
|--------|---|---|-----|
| 0      | 0 | Z | L |
| 1      | 1 | Z | H |
| X/Z    | X | Z | X |

### BUFIF1

| BUFIF1 | 0 | 1 | X/Z |
|--------|---|---|-----|
| 0      | Z | 0 | L |
| 1      | Z | 1 | H |
| X/Z    | Z | X | X |

## Gate-Level NOTIF0/NOTIF1 (X/Z Combined)

These tables use a combined X/Z input as shown in the source image.
Rows/columns are labeled 0/1/X/Z in the image; values include L/H as shown.

### NOTIF0

| NOTIF0 | 0 | 1 | X/Z |
|--------|---|---|-----|
| 0      | 1 | Z | H |
| 1      | 0 | Z | L |
| X/Z    | X | Z | X |

### NOTIF1

| NOTIF1 | 0 | 1 | X/Z |
|--------|---|---|-----|
| 0      | Z | 1 | H |
| 1      | Z | 0 | L |
| X/Z    | Z | X | X |

## Notes
- `z` is treated as unknown for most bitwise ops (except the forced 0/1
  outcomes above).
- `~&`, `~|`, and `~^` are bitwise NOT applied to the result of `&`, `|`, `^`.
- Add tables for logical ops, equality/inequality, shifts, and reductions as
  they are confirmed.

## Unresolved Nets (uwire)

`uwire` is an unresolved or unidriver net that allows only a single driver.
If more than one driver attempts to drive a `uwire`, this is a compile-time
error (no runtime resolution).

## Supply Nets

`supply0` and `supply1` model power supplies and have supply strengths.

## Delay Categories (Gate-Level)

- Rise delay: time for output to change to 1 from 0/X/Z.
- Fall delay: time for output to change to 0 from 1/X/Z.
- Turn-off delay: time for output to change to Z from 0/1/X.

## Delay Specification Formats

| Specification | Usage | Format |
|---------------|-------|--------|
| One delay | Same value for Rise, Fall and Turn-off transitions | `#(delay)` |
| Two delay | Rise, Fall transitions | `#(rise, fall)` |
| Three delay | Rise, Fall and Turn-off transitions | `#(rise, fall, turn-off)` |

## Min/Typ/Max Delays

Delays vary by process, temperature, and other fabrication conditions.
Verilog allows min/typ/max values for each delay type.

For each delay type (rise, fall, turn-off), specify:
- min: minimum delay
- typ: typical delay
- max: maximum delay

## Switch-Level Modeling (Notes)

Verilog supports transistor-level primitives (nmos/pmos/cmos) and
bidirectional switches (tran/tranif0/tranif1), plus supply rails.
These are typically modeled via driver resolution and Z behavior rather
than direct truth tables.

### NMOS/PMOS (example behavior)
- With ctrl=0: nmos output is Z; pmos passes the input (0/1).
- With ctrl=1: nmos passes the input (0/1); pmos output is Z.

### CMOS (example behavior)
- When both controls are 0: output follows input.
- When nctrl=1 and pctrl=1: output follows input.
- When nctrl=0 and pctrl=1: output is Z.

### Bidirectional Switches (example behavior)
- tran: io2 follows io1 (bidirectional pass).
- tranif0: io2 follows io1 when ctrl=0; otherwise Z.
- tranif1: io2 follows io1 when ctrl=1; otherwise Z.

### Power and Ground
- supply1 resolves to 1 (vdd); supply0 resolves to 0 (gnd).

## Wire/Tri Resolution (Draft)

| wire/tri | 0 | 1 | x | z |
|----------|---|---|---|---|
| 0        | 0 | 0 | x | 0 |
| 1        | x | 1 | x | 1 |
| x        | x | x | x | x |
| z        | 0 | 1 | x | z |

Notes:
- Used by driver resolution on wire/tri nets (including trireg charge/decay).
- Apply after strength resolution; this table is for the logical 4-state merge.

## Tri0 Resolution (Draft)

| tri0 | 0 | 1 | x | z |
|------|---|---|---|---|
| 0    | 0 | x | x | 0 |
| 1    | x | 1 | x | 1 |
| x    | x | x | x | x |
| z    | 0 | 1 | x | 0 |

Notes:
- Like wire/tri, but Z+Z resolves to 0 (pull-down).

## Tri1 Resolution (Draft)

| tri1 | 0 | 1 | x | z |
|------|---|---|---|---|
| 0    | 0 | x | x | 0 |
| 1    | x | 1 | x | 1 |
| x    | x | x | x | x |
| z    | 0 | 1 | x | 1 |

Notes:
- Like wire/tri, but Z+Z resolves to 1 (pull-up).

## Wor/Trior Resolution (Draft)

| wor/trior | 0 | 1 | x | z |
|-----------|---|---|---|---|
| 0         | 0 | 1 | x | 0 |
| 1         | 1 | 1 | 1 | 1 |
| x         | x | 1 | x | x |
| z         | 0 | 1 | x | z |

Notes:
- Used by wor/trior nets after strength resolution.
- z behaves like high-impedance input to the wired-OR merge.

## Wand/Triand Resolution (Draft)

| wand/triand | 0 | 1 | x | z |
|-------------|---|---|---|---|
| 0           | 0 | 0 | 0 | 0 |
| 1           | 0 | 1 | x | 1 |
| x           | 0 | x | x | x |
| z           | 0 | 1 | x | z |

Notes:
- Used by wand/triand nets after strength resolution.
- z behaves like high-impedance input to the wired-AND merge.
