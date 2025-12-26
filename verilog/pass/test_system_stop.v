// Test: $stop system task
// Feature: Pause simulation (interactive mode)
// Expected: Should fail - $stop not yet implemented

module test_system_stop;
  reg [7:0] data;

  initial begin
    data = 8'h00;
    #10 data = 8'h55;
    $stop;  // Pause for interactive debugging
    #10 data = 8'hAA;
  end
endmodule
