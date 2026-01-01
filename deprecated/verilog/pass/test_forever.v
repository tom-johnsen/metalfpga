// EXPECT=PASS
// Test: forever loop
// Feature: Infinite loops
// Expected: Should fail - forever not yet implemented

module test_forever;
  reg clk;

  initial begin
    clk = 0;
    forever #5 clk = ~clk;  // Clock generator
  end
endmodule
