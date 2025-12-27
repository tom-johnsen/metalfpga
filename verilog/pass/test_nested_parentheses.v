// EXPECT=PASS
// Test deeply nested parentheses
module test_nested_parentheses(
    input [7:0] a,
    input [7:0] b,
    input [7:0] c,
    input [7:0] d,
    input [7:0] e,
    output wire [7:0] deep1,
    output wire [7:0] deep2,
    output wire [7:0] deep3,
    output wire [7:0] deep4
);
    // Deep nesting - 3 levels
    assign deep1 = (((a + b) * c) - d);

    // Deep nesting - 4 levels
    assign deep2 = ((((a & b) | c) ^ d) + e);

    // Multiple nested groups
    assign deep3 = ((a + b) * (c - d)) + ((e & 8'hF) << 2);

    // Complex nested expression
    assign deep4 = (((a + (b << 1)) & 8'hFE) | ((c - d) & 8'h01));
endmodule
