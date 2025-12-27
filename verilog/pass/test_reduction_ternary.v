// EXPECT=PASS
// Test reduction operators in ternary expressions
module test_reduction_ternary(
    input [7:0] data,
    input [7:0] mask,
    input sel,
    output wire result1,
    output wire result2,
    output wire result3,
    output wire result4,
    output wire [7:0] result5
);
    // Reduction in ternary condition
    assign result1 = (&data) ? 1'b1 : 1'b0;
    assign result2 = (|data) ? ^mask : &mask;

    // Ternary selecting between reductions
    assign result3 = sel ? &data : |data;
    assign result4 = sel ? ^data : ~^data;

    // Complex nesting
    assign result5 = (&data) ? mask : (|data) ? ~mask : 8'h00;
endmodule
