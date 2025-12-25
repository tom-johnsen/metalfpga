// Test reduction operators: &, |, ^, ~&, ~|, ~^
module reduction_ops(
    input [7:0] data,
    output wire and_result,
    output wire or_result,
    output wire xor_result,
    output wire nand_result,
    output wire nor_result,
    output wire xnor_result
);
    assign and_result = &data;   // All bits must be 1
    assign or_result = |data;    // Any bit is 1
    assign xor_result = ^data;   // Odd number of 1s
    assign nand_result = ~&data; // NOT of AND reduction
    assign nor_result = ~|data;  // NOT of OR reduction
    assign xnor_result = ~^data; // NOT of XOR reduction
endmodule
