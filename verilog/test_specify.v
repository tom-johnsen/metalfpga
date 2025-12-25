// Test: specify block for timing
// Feature: Timing specification
// Expected: Should fail - specify blocks not yet implemented

module test_specify(input a, output y);
  assign y = a;

  specify
    (a => y) = (2.0, 3.0);  // rise, fall delays
  endspecify
endmodule
