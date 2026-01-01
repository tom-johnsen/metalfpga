// EXPECT=PASS
module add_with_flags #(
    parameter WIDTH = 32
)(
    input  wire [WIDTH-1:0] a,
    input  wire [WIDTH-1:0] b,
    input  wire             sub,      // 0 = add, 1 = subtract
    output wire [WIDTH-1:0] result,
    output wire             carry,
    output wire             overflow,
    output wire             zero
);

    // Two's complement subtraction when sub=1
    wire [WIDTH-1:0] b_eff;
    wire [WIDTH:0]   sum_ext;

    assign b_eff   = sub ? ~b : b;
    assign sum_ext = {1'b0, a} + {1'b0, b_eff} + sub;

    assign result  = sum_ext[WIDTH-1:0];
    assign carry   = sum_ext[WIDTH];

    // Signed overflow detection
    assign overflow =
        (~(a[WIDTH-1] ^ b_eff[WIDTH-1])) &
         (a[WIDTH-1] ^ result[WIDTH-1]);

    // Zero flag
    assign zero = (result == {WIDTH{1'b0}});

endmodule
