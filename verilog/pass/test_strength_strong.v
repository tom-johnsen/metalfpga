// Test: Strong drive strength
// Feature: Explicit drive strength specification
// Expected: Should fail - drive strength not yet implemented

module test_strength_strong(
  input a,
  output y
);
  assign (strong1, strong0) y = a;
endmodule
