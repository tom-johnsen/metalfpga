// Test: Supply drive strength
// Feature: Explicit drive strength specification
// Expected: Should fail - drive strength not yet implemented

module test_strength_supply(
  output y
);
  assign (supply1, supply0) y = 1'b1;
endmodule
