// Test: Pull drive strength
// Feature: Explicit drive strength specification
// Expected: Should fail - drive strength not yet implemented

module test_strength_pull(
  input a,
  output y
);
  assign (pull1, pull0) y = a;
endmodule
