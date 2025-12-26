// Test: Weak drive strength
// Feature: Explicit drive strength specification
// Expected: Should fail - drive strength not yet implemented

module test_strength_weak(
  input a,
  output y
);
  assign (weak1, weak0) y = a;
endmodule
