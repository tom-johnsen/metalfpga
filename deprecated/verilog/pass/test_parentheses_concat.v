// EXPECT=PASS
// Test parentheses with concatenation and replication
module test_parentheses_concat(
    input [7:0] a,
    input [7:0] b,
    input [3:0] c,
    output wire [15:0] concat1,
    output wire [15:0] concat2,
    output wire [15:0] concat3,
    output wire [31:0] repl1,
    output wire [31:0] repl2
);
    // Parentheses with concatenation
    assign concat1 = {(a + b), c, 4'h0};
    assign concat2 = {a[7:4], (b & 8'h0F)};
    assign concat3 = {(a >> 4), (b << 4)};

    // Parentheses with replication
    assign repl1 = {4{(a & 8'hFF)}};
    assign repl2 = {2{(a + b)}};
endmodule
