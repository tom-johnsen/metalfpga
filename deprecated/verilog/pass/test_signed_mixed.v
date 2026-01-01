// EXPECT=PASS
// Test mixed signed/unsigned operations
module test_signed_mixed(
    input signed [7:0] signed_val,
    input [7:0] unsigned_val,
    input signed [15:0] wide_signed,
    input [15:0] wide_unsigned,
    output wire signed [8:0] mixed_add1,
    output wire signed [8:0] mixed_add2,
    output wire [8:0] unsigned_add,
    output wire signed [16:0] mixed_mul1,
    output wire signed [16:0] mixed_mul2,
    output wire cmp1,
    output wire cmp2,
    output wire cmp3,
    output wire cmp4
);
    // Mixed arithmetic - sign extension behavior
    assign mixed_add1 = signed_val + $signed(unsigned_val);
    assign mixed_add2 = $signed(unsigned_val) + signed_val;
    assign unsigned_add = unsigned_val + $unsigned(signed_val);

    // Mixed multiplication
    assign mixed_mul1 = signed_val * $signed(unsigned_val);
    assign mixed_mul2 = wide_signed * $signed(wide_unsigned);

    // Mixed comparisons
    assign cmp1 = signed_val < $signed(unsigned_val);
    assign cmp2 = $unsigned(signed_val) < unsigned_val;
    assign cmp3 = wide_signed > $signed(wide_unsigned);
    assign cmp4 = $unsigned(wide_signed) > wide_unsigned;
endmodule
