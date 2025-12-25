// Test parentheses with ternary operators
module test_parentheses_ternary(
    input [7:0] a,
    input [7:0] b,
    input [7:0] c,
    input sel1,
    input sel2,
    output wire [7:0] tern1,
    output wire [7:0] tern2,
    output wire [7:0] tern3,
    output wire [7:0] tern4
);
    // Parentheses in ternary condition
    assign tern1 = (a > b) ? c : 8'h00;
    assign tern2 = ((a + b) > c) ? a : b;

    // Parentheses in ternary branches
    assign tern3 = sel1 ? (a + b) : (c - a);
    assign tern4 = sel1 ? ((a & b) | c) : ((a | b) & c);
endmodule
