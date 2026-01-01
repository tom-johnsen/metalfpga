// EXPECT=PASS
// Test parentheses for expression grouping and precedence
module test_expression_grouping(
    input [7:0] a,
    input [7:0] b,
    input [7:0] c,
    input [7:0] d,
    output wire [7:0] arith1,
    output wire [7:0] arith2,
    output wire [7:0] arith3,
    output wire [7:0] bitwise1,
    output wire [7:0] bitwise2,
    output wire [7:0] mixed1,
    output wire [7:0] mixed2,
    output wire [7:0] mixed3,
    output wire logic1,
    output wire logic2,
    output wire logic3
);
    // Arithmetic grouping
    assign arith1 = (a + b) * c;          // Addition before multiplication
    assign arith2 = a + (b * c);          // Multiplication before addition
    assign arith3 = ((a + b) - c) * d;    // Nested grouping

    // Bitwise grouping
    assign bitwise1 = (a & b) | c;        // AND before OR
    assign bitwise2 = a & (b | c);        // OR before AND

    // Mixed arithmetic and bitwise
    assign mixed1 = (a + b) & (c - d);    // Group arithmetic, then AND
    assign mixed2 = ((a << 2) + b) & 8'hFF;
    assign mixed3 = (a & 8'hF0) | (b & 8'h0F);

    // Logical grouping
    assign logic1 = (a > b) && (c < d);
    assign logic2 = ((a == b) || (c == d)) && (a != c);
    assign logic3 = !(a > b) || (c <= d);
endmodule
