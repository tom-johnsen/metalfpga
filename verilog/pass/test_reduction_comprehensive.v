// EXPECT=PASS
// Comprehensive test for reduction operators with various bit widths and edge cases
module test_reduction_comprehensive(
    input [15:0] wide_data,
    input [3:0] narrow_data,
    input [0:0] single_bit,
    input [31:0] word_data,
    output wire [5:0] and_results,
    output wire [5:0] or_results,
    output wire [5:0] xor_results,
    output wire [5:0] nand_results,
    output wire [5:0] nor_results,
    output wire [5:0] xnor_results
);
    // AND reduction - all bits must be 1
    assign and_results[0] = &wide_data;
    assign and_results[1] = &narrow_data;
    assign and_results[2] = &single_bit;
    assign and_results[3] = &word_data;
    assign and_results[4] = &8'hFF;  // Constant - should be 1
    assign and_results[5] = &8'hFE;  // Constant - should be 0

    // OR reduction - any bit is 1
    assign or_results[0] = |wide_data;
    assign or_results[1] = |narrow_data;
    assign or_results[2] = |single_bit;
    assign or_results[3] = |word_data;
    assign or_results[4] = |8'h00;  // Constant - should be 0
    assign or_results[5] = |8'h01;  // Constant - should be 1

    // XOR reduction - odd parity
    assign xor_results[0] = ^wide_data;
    assign xor_results[1] = ^narrow_data;
    assign xor_results[2] = ^single_bit;
    assign xor_results[3] = ^word_data;
    assign xor_results[4] = ^8'h0F;  // 4 ones - even parity, result 0
    assign xor_results[5] = ^8'h07;  // 3 ones - odd parity, result 1

    // NAND reduction
    assign nand_results[0] = ~&wide_data;
    assign nand_results[1] = ~&narrow_data;
    assign nand_results[2] = ~&single_bit;
    assign nand_results[3] = ~&word_data;
    assign nand_results[4] = ~&8'hFF;
    assign nand_results[5] = ~&8'hFE;

    // NOR reduction
    assign nor_results[0] = ~|wide_data;
    assign nor_results[1] = ~|narrow_data;
    assign nor_results[2] = ~|single_bit;
    assign nor_results[3] = ~|word_data;
    assign nor_results[4] = ~|8'h00;
    assign nor_results[5] = ~|8'h01;

    // XNOR reduction
    assign xnor_results[0] = ~^wide_data;
    assign xnor_results[1] = ~^narrow_data;
    assign xnor_results[2] = ~^single_bit;
    assign xnor_results[3] = ~^word_data;
    assign xnor_results[4] = ~^8'h0F;
    assign xnor_results[5] = ~^8'h07;
endmodule
