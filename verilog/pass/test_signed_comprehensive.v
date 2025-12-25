// Comprehensive signed arithmetic tests
module test_signed_comprehensive(
    input signed [7:0] a,
    input signed [7:0] b,
    input signed [15:0] wide_a,
    input signed [15:0] wide_b,
    input signed [3:0] narrow_a,
    input signed [3:0] narrow_b,
    output wire signed [7:0] add_8bit,
    output wire signed [7:0] sub_8bit,
    output wire signed [15:0] mul_8bit,
    output wire signed [15:0] add_16bit,
    output wire signed [15:0] sub_16bit,
    output wire signed [31:0] mul_16bit,
    output wire signed [3:0] add_4bit,
    output wire signed [3:0] sub_4bit,
    output wire signed [7:0] mul_4bit,
    output wire cmp_lt_8,
    output wire cmp_gt_8,
    output wire cmp_le_8,
    output wire cmp_ge_8,
    output wire cmp_eq_8,
    output wire cmp_ne_8
);
    // 8-bit signed arithmetic
    assign add_8bit = a + b;
    assign sub_8bit = a - b;
    assign mul_8bit = a * b;

    // 16-bit signed arithmetic
    assign add_16bit = wide_a + wide_b;
    assign sub_16bit = wide_a - wide_b;
    assign mul_16bit = wide_a * wide_b;

    // 4-bit signed arithmetic (small values)
    assign add_4bit = narrow_a + narrow_b;
    assign sub_4bit = narrow_a - narrow_b;
    assign mul_4bit = narrow_a * narrow_b;

    // Signed comparisons
    assign cmp_lt_8 = a < b;
    assign cmp_gt_8 = a > b;
    assign cmp_le_8 = a <= b;
    assign cmp_ge_8 = a >= b;
    assign cmp_eq_8 = a == b;
    assign cmp_ne_8 = a != b;
endmodule
