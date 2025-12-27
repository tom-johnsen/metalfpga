// Test: $size system function
// Feature: Return size of array dimension
// Expected: Should fail - $size not yet implemented

module test_system_size;
  reg [7:0] memory [0:255];
  integer size;

  initial begin
    size = $size(memory);  // 256 (number of elements)
  end
endmodule
