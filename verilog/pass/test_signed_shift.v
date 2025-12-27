// EXPECT=PASS
// Test shift operations with signed values
module test_signed_shift(
    input signed [7:0] val,
    input [2:0] shift_amt,
    output wire signed [7:0] arith_right,
    output wire signed [7:0] logic_right,
    output wire signed [7:0] logic_left,
    output wire signed [7:0] arith_right_pos,
    output wire signed [7:0] arith_right_neg,
    output wire signed [7:0] left_shift_pos,
    output wire signed [7:0] left_shift_neg
);
    // Arithmetic right shift (preserves sign)
    assign arith_right = val >>> shift_amt;

    // Logical right shift (fills with zeros)
    assign logic_right = val >> shift_amt;

    // Logical left shift
    assign logic_left = val << shift_amt;

    // Constant shifts on positive value
    assign arith_right_pos = 8'sh40 >>> 2;  // 64 >>> 2 = 16
    assign left_shift_pos = 8'sh08 << 2;     // 8 << 2 = 32

    // Constant shifts on negative value
    assign arith_right_neg = 8'shC0 >>> 2;  // -64 >>> 2 = -16 (sign extended)
    assign left_shift_neg = 8'shF8 << 2;     // -8 << 2 = -32
endmodule
