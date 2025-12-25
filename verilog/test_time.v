// Test: time data type
// Feature: 64-bit time values
// Expected: Should fail - time type not yet implemented

module test_time;
  time current_time;
  time start_time;

  initial begin
    start_time = $time;
    #100;
    current_time = $time;
  end
endmodule
