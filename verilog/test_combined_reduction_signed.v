// Test combinations of reduction and signed operations
module test_combined_reduction_signed(
    input signed [7:0] signed_val,
    input [7:0] unsigned_val,
    output wire reduction_of_signed,
    output wire reduction_of_negative,
    output wire signed_of_reduction,
    output wire compare_reduction,
    output wire arith_with_reduction,
    output wire [7:0] mask_from_reduction
);
    // Reduction operators on signed values
    assign reduction_of_signed = &signed_val;
    assign reduction_of_negative = |8'shFF;  // Reduction of -1

    // Using reduction result in signed context
    assign signed_of_reduction = (|signed_val) ? 1'b1 : 1'b0;

    // Compare signed value with reduction result
    assign compare_reduction = (signed_val < 0) && (&unsigned_val);

    // Arithmetic with reduction results
    assign arith_with_reduction = (&unsigned_val) + (|signed_val);

    // Create mask based on reduction
    assign mask_from_reduction = (&signed_val) ? 8'hFF : 8'h00;
endmodule
