// Test: Both edge sensitivity
// Feature: Sensitivity lists with posedge and negedge combined
// Expected: Should fail - complex sensitivity lists not yet implemented

module test_edge_both;
  reg clk, reset;
  reg [7:0] counter;

  // Trigger on both edges of clock and reset
  always @(posedge clk, negedge reset) begin
    if (!reset)
      counter = 0;
    else
      counter = counter + 1;
  end

  initial begin
    clk = 0;
    reset = 1;
  end
endmodule
