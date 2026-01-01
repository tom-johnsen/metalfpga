// EXPECT=PASS
// Test: $clog2 system function
// Feature: Ceiling of log base 2
// Expected: Should fail - $clog2 not yet implemented

module test_system_clog2;
  parameter DATA_WIDTH = 256;
  parameter ADDR_WIDTH = $clog2(DATA_WIDTH);  // 8 bits needed

  reg [ADDR_WIDTH-1:0] address;

  initial begin
    address = ADDR_WIDTH'hFF;
  end
endmodule
