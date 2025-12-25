// Test constant propagation and folding
module constant_prop(
    input [7:0] x,
    output wire [7:0] out1,
    output wire [7:0] out2,
    output wire [7:0] out3,
    output wire [7:0] out4
);
    // These should be optimized at compile time
    assign out1 = x & 8'hFF;  // Should simplify to x
    assign out2 = x | 8'h00;  // Should simplify to x
    assign out3 = x ^ 8'h00;  // Should simplify to x
    assign out4 = (8'h55 + 8'hAA) & x;  // 8'hFF & x → x
endmodule
