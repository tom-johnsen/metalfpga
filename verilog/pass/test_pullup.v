// EXPECT=PASS
// Test: pullup primitive
// Feature: Gate-level primitives
// Expected: Should fail - pullup not yet implemented

module test_pullup(output y);
  pullup(y);  // Pull net to logic 1
endmodule
