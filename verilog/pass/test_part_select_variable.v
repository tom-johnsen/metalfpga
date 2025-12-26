// Test: Variable part select (indexed part select)
// Feature: +: and -: part select operators
// Expected: Should fail - variable part select not yet implemented

module test_part_select_variable;
  reg [31:0] data;
  reg [7:0] byte_out;
  integer idx;

  initial begin
    data = 32'hDEADBEEF;
    idx = 8;

    // Indexed part select: data[idx +: 8] means 8 bits starting at idx
    byte_out = data[idx +: 8];  // Extract bits [15:8]

    // Indexed part select: data[idx -: 8] means 8 bits ending at idx
    byte_out = data[idx -: 8];  // Extract bits [8:1]
  end
endmodule
