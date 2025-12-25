// Test reduction operators on slices and concatenations
module test_reduction_slices(
    input [15:0] data,
    output wire slice_and_upper,
    output wire slice_or_lower,
    output wire slice_xor_middle,
    output wire concat_and,
    output wire concat_or,
    output wire concat_xor,
    output wire mixed1,
    output wire mixed2
);
    // Reductions on slices
    assign slice_and_upper = &data[15:8];
    assign slice_or_lower = |data[7:0];
    assign slice_xor_middle = ^data[11:4];

    // Reductions on concatenations
    assign concat_and = &{data[7:0], data[15:8]};
    assign concat_or = |{data[3:0], data[7:4], data[11:8], data[15:12]};
    assign concat_xor = ^{data[0], data[4], data[8], data[12]};

    // Mixed operations
    assign mixed1 = &{data[7:0]} | &{data[15:8]};
    assign mixed2 = ^{data[3:0], 4'b0000} & |data[15:12];
endmodule
