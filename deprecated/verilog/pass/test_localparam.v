// EXPECT=PASS
// Test: Local parameters
// Feature: localparam declarations (cannot be overridden)
// Expected: Should fail - localparam not yet implemented

module test_localparam;
  parameter WIDTH = 8;
  localparam DOUBLE = WIDTH * 2;
  localparam MASK = (1 << WIDTH) - 1;

  reg [DOUBLE-1:0] data;

  initial begin
    data = {2{MASK}};
  end
endmodule
