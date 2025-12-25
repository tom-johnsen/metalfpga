// Test complex conditional assignments
module conditional_assign(
    input [7:0] a,
    input [7:0] b,
    input [7:0] c,
    input [1:0] sel,
    output wire [7:0] out
);
    // Nested ternary
    assign out = (sel == 2'b00) ? a :
                 (sel == 2'b01) ? b :
                 (sel == 2'b10) ? c :
                 8'h00;
endmodule
