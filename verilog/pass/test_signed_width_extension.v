// Test signed width extension and truncation
module test_signed_width_extension(
    input signed [7:0] narrow,
    input signed [15:0] wide,
    output wire signed [15:0] extended_pos,
    output wire signed [15:0] extended_neg,
    output wire signed [7:0] truncated_pos,
    output wire signed [7:0] truncated_neg,
    output wire signed [15:0] explicit_extend,
    output wire [15:0] unsigned_extend,
    output wire signed [31:0] double_extend,
    output wire signed [15:0] arith_extend,
    output wire signed [15:0] concat_extend
);
    // Automatic sign extension
    assign extended_pos = narrow;  // Should sign-extend positive
    assign extended_neg = 8'shFF;  // Should sign-extend -1 to 16'hFFFF

    // Truncation
    assign truncated_pos = wide;
    assign truncated_neg = 16'shFFFF;  // Truncate -1 to 8'hFF

    // Explicit sign extension using $signed
    assign explicit_extend = $signed(narrow);
    assign unsigned_extend = narrow;  // What happens without $signed?

    // Double extension: 8 -> 16 -> 32
    assign double_extend = $signed(narrow);

    // Sign extension via arithmetic
    assign arith_extend = narrow + 16'sh0000;

    // Sign extension via concatenation (manual)
    assign concat_extend = {{8{narrow[7]}}, narrow};
endmodule
