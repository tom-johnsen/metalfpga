// EXPECT=PASS
// Test unary operations with signed values
module test_signed_unary(
    input signed [7:0] val,
    output wire signed [7:0] negated,
    output wire signed [7:0] neg_pos_max,
    output wire signed [7:0] neg_neg_max,
    output wire signed [7:0] neg_zero,
    output wire signed [7:0] neg_neg_one,
    output wire [7:0] bitwise_not,
    output wire logical_not,
    output wire reduction_and,
    output wire reduction_or,
    output wire reduction_xor
);
    // Unary minus
    assign negated = -val;
    assign neg_pos_max = -(8'sh7F);  // -(127) = -127
    assign neg_neg_max = -(8'sh80);  // -(-128) = -128 (special case!)
    assign neg_zero = -(8'sh00);     // -(0) = 0
    assign neg_neg_one = -(8'shFF);  // -(-1) = 1

    // Bitwise NOT (different from unary minus)
    assign bitwise_not = ~val;

    // Logical NOT
    assign logical_not = !val;

    // Reduction operators on signed values
    assign reduction_and = &val;
    assign reduction_or = |val;
    assign reduction_xor = ^val;
endmodule
