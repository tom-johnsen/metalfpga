// Test: $monitor system task
// Feature: System tasks
// Expected: Should fail - system tasks not yet implemented

module test_monitor;
  reg [7:0] counter;
  reg clk;

  initial begin
    clk = 0;
    counter = 0;
    $monitor("Time=%0t clk=%b counter=%d", $time, clk, counter);
  end

  always #5 clk = ~clk;

  always @(posedge clk) begin
    counter = counter + 1;
  end
endmodule
