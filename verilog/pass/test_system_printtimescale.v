// Test: $printtimescale system task
// Feature: Display timescale of module
// Expected: Should fail - $printtimescale not yet implemented

`timescale 1ns/1ps

module test_system_printtimescale;
  initial begin
    $printtimescale(test_system_printtimescale);
    $printtimescale;  // Current module
  end
endmodule
