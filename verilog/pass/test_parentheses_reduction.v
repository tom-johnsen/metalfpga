// Test parentheses with reduction operators
module test_parentheses_reduction(
    input [7:0] a,
    input [7:0] b,
    input [7:0] c,
    output wire red1,
    output wire red2,
    output wire red3,
    output wire red4,
    output wire red5
);
    // Reduction on grouped expressions
    assign red1 = &(a | b);           // Reduce result of OR
    assign red2 = |(a & b);           // Reduce result of AND
    assign red3 = ^(a + b);           // Reduce result of addition
    assign red4 = &((a << 2) | b);    // Reduce complex expression
    assign red5 = |(a & (b | c));     // Nested groups with reduction
endmodule
