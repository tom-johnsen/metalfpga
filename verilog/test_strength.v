// Test: Drive strength specification
// Feature: Net strength (strong, weak, etc.)
// Expected: Should fail - drive strength not yet implemented

module test_strength(input a, output y);
  assign (strong1, weak0) y = a;
endmodule
