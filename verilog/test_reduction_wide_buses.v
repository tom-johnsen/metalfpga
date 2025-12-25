// Test reduction operators on very wide buses
module test_reduction_wide_buses(
    input [63:0] data64,
    input [127:0] data128,
    input [255:0] data256,
    output wire and64,
    output wire or64,
    output wire xor64,
    output wire and128,
    output wire or128,
    output wire xor128,
    output wire and256,
    output wire or256,
    output wire xor256,
    output wire mixed1,
    output wire mixed2
);
    // 64-bit reductions
    assign and64 = &data64;
    assign or64 = |data64;
    assign xor64 = ^data64;

    // 128-bit reductions
    assign and128 = &data128;
    assign or128 = |data128;
    assign xor128 = ^data128;

    // 256-bit reductions
    assign and256 = &data256;
    assign or256 = |data256;
    assign xor256 = ^data256;

    // Mixed width operations
    assign mixed1 = &data64 & &data128;
    assign mixed2 = |data64 | |data128 | |data256;
endmodule
