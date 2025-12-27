// EXPECT=PASS
// Test: Continuous assignment with delays
// Feature: Delay on assign statements
// Expected: Should fail - continuous assign delays not yet implemented

module test_assign_delay;
  wire a, b, c;
  reg x, y;

  assign #5 a = x & y;      // Delay on assignment
  assign #(2,3) b = x | y;  // Rise and fall delays
  assign #(1:2:3) c = x ^ y; // Min:typ:max delays

  initial begin
    x = 0; y = 0;
    #10 x = 1;
    #10 y = 1;
  end
endmodule
