// Test: Mixed strength levels on same net
// Feature: Complex strength resolution
// Expected: Should fail - strength resolution not yet implemented

module test_strength_mixed(
  input a,
  input b,
  input c,
  output wire y
);
  // Three drivers with different strengths
  assign (supply1, supply0) y = a;  // Highest strength
  assign (strong1, strong0) y = b;   // Medium strength
  assign (weak1, weak0) y = c;       // Lowest strength
  // Result should be 'a' (supply beats everything)
endmodule
