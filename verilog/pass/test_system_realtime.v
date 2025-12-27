// Test: $realtime system function
// Feature: Get current simulation time as real
// Expected: Should fail - $realtime not yet implemented

module test_system_realtime;
  real current_time;

  initial begin
    current_time = $realtime;
    #100.5;
    current_time = $realtime;
  end
endmodule
