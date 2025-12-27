// EXPECT=PASS
// Test signed arithmetic edge cases
module test_signed_edge_cases(
    input signed [7:0] val,
    output wire signed [7:0] neg_max,
    output wire signed [7:0] pos_max,
    output wire signed [7:0] zero,
    output wire signed [7:0] neg_one,
    output wire signed [7:0] overflow_add,
    output wire signed [7:0] overflow_sub,
    output wire signed [7:0] underflow_add,
    output wire signed [7:0] underflow_sub,
    output wire signed [15:0] overflow_mul,
    output wire cmp_max_vs_min,
    output wire cmp_zero_vs_neg,
    output wire cmp_identical_neg
);
    // Constants
    assign neg_max = 8'sh80;  // -128
    assign pos_max = 8'sh7F;  // +127
    assign zero = 8'sh00;
    assign neg_one = 8'shFF;  // -1

    // Overflow cases
    assign overflow_add = 8'sh7F + 8'sh01;  // 127 + 1 = -128 (overflow)
    assign overflow_sub = 8'sh80 - 8'sh01;  // -128 - 1 = 127 (underflow)
    assign underflow_add = 8'sh80 + 8'shFF; // -128 + (-1) = 127 (underflow)
    assign underflow_sub = 8'sh7F - 8'shFF; // 127 - (-1) = -128 (overflow)

    // Multiplication overflow
    assign overflow_mul = 8'sh40 * 8'sh40;  // 64 * 64 = 4096

    // Edge case comparisons
    assign cmp_max_vs_min = 8'sh7F > 8'sh80;  // 127 > -128 should be true
    assign cmp_zero_vs_neg = 8'sh00 > 8'shFF; // 0 > -1 should be true
    assign cmp_identical_neg = 8'shFF == 8'shFF; // -1 == -1 should be true
endmodule
