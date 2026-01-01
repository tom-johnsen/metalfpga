// EXPECT=PASS
// Test: $time system function
// Feature: Get current simulation time
// Expected: Should fail - $time not yet implemented

module test_system_time;
  reg [63:0] current_time;

  initial begin
    current_time = $time;
    #100;
    current_time = $time;
    #250;
    current_time = $time;
  end
endmodule
