// EXPECT=PASS
// Test: Time literal delays (#)
// Feature: General timing controls (# delays)
// Expected: Should fail - timing controls not yet implemented

module test_time_literal;
  reg clk;
  reg [7:0] data;

  initial begin
    clk = 0;
    #10 clk = 1;
    #10 clk = 0;
    #5 data = 8'h55;
    #15 data = 8'hAA;
  end
endmodule
