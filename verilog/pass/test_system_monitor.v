// Test: $monitor system task
// Feature: Continuous monitoring and display on change
// Expected: Should fail - $monitor not yet implemented

module test_system_monitor;
  reg [7:0] counter;
  reg clk;

  initial begin
    clk = 0;
    counter = 0;

    $monitor("Time=%0t clk=%b counter=%d", $time, clk, counter);

    #10 clk = 1;
    #10 clk = 0; counter = 1;
    #10 clk = 1;
    #10 clk = 0; counter = 2;

    $monitoroff;  // Stop monitoring
    #10 counter = 3;  // Won't be displayed
    $monitoron;   // Resume monitoring
    #10 counter = 4;  // Will be displayed
  end
endmodule
