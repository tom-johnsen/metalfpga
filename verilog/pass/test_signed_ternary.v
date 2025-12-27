// EXPECT=PASS
// Test signed arithmetic in ternary expressions
module test_signed_ternary(
    input signed [7:0] a,
    input signed [7:0] b,
    input signed [7:0] c,
    input sel,
    output wire signed [7:0] result1,
    output wire signed [7:0] result2,
    output wire signed [7:0] result3,
    output wire signed [7:0] result4
);
    // Signed comparison in condition
    assign result1 = (a < b) ? a : b;  // Minimum
    assign result2 = (a > b) ? a : b;  // Maximum

    // Signed arithmetic in branches
    assign result3 = sel ? (a + b) : (a - b);
    assign result4 = (a < 0) ? -a : a;  // Absolute value
endmodule
