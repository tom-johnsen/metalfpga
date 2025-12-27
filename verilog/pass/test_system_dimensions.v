// Test: $dimensions, $left, $right, $low, $high system functions
// Feature: Array dimension query functions
// Expected: Should fail - dimension functions not yet implemented

module test_system_dimensions;
  reg [7:0] memory [15:0];
  reg [31:16] data;
  integer dims, left, right, low, high;

  initial begin
    dims = $dimensions(memory);   // 1 (1D array)
    left = $left(memory);         // 15
    right = $right(memory);       // 0
    low = $low(data);             // 16
    high = $high(data);           // 31
  end
endmodule
