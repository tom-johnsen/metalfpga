// EXPECT=PASS
// Test parentheses with signed operations
module test_parentheses_signed(
    input signed [7:0] a,
    input signed [7:0] b,
    input signed [7:0] c,
    output wire signed [7:0] sign1,
    output wire signed [7:0] sign2,
    output wire signed [7:0] sign3,
    output wire cmp1,
    output wire cmp2
);
    // Signed arithmetic grouping
    assign sign1 = (a + b) - c;
    assign sign2 = a + (b - c);
    assign sign3 = ((a * b) >>> 2) + c;

    // Signed comparisons with grouping
    assign cmp1 = (a + b) < (c - 8'sh10);
    assign cmp2 = ((a > 8'sh00) && (b < 8'sh00)) || (c == 8'sh00);
endmodule
