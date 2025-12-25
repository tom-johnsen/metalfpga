// Test large bit widths (beyond 32, 64, etc.)
module large_width(
    input [127:0] big_a,
    input [127:0] big_b,
    output wire [127:0] big_sum,
    output wire [127:0] big_and
);
    assign big_sum = big_a + big_b;  // 128-bit addition
    assign big_and = big_a & big_b;  // 128-bit AND
endmodule
