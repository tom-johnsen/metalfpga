// EXPECT=PASS
// Test reduction operators in complex expressions
module test_reduction_nested(
    input [7:0] a,
    input [7:0] b,
    input [7:0] c,
    output wire result1,
    output wire result2,
    output wire result3,
    output wire result4,
    output wire result5,
    output wire result6,
    output wire result7,
    output wire result8
);
    // Nested reductions
    assign result1 = &a | &b;  // OR of two AND reductions
    assign result2 = |a & |b;  // AND of two OR reductions
    assign result3 = ^a ^ ^b;  // XOR of two XOR reductions

    // Reductions with bitwise operations
    assign result4 = &(a & b);  // Reduction of bitwise AND
    assign result5 = |(a | b);  // Reduction of bitwise OR
    assign result6 = ^(a ^ b);  // Reduction of bitwise XOR

    // Reductions with arithmetic
    assign result7 = &(a + b);  // Reduction of sum
    assign result8 = |(a - b);  // Reduction of difference
endmodule
