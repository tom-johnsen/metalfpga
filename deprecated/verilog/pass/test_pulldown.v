// EXPECT=PASS
// Test: pulldown primitive
// Feature: Gate-level primitives
// Expected: Should fail - pulldown not yet implemented

module test_pulldown(output y);
  pulldown(y);  // Pull net to logic 0
endmodule
