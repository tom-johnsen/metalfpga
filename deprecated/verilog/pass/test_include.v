// EXPECT=PASS
// Test: include directive
// Feature: File inclusion
// Expected: Should fail - include not yet implemented

module test_include;
  reg [7:0] data;

  // This would include definitions from another file
  `include "definitions.vh"

  initial begin
    data = 8'h00;
  end
endmodule
