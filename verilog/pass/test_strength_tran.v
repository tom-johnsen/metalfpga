// EXPECT=PASS
// Test: Bidirectional pass switch
// Feature: Resistive connection primitives
// Expected: Should fail - tran primitives not yet implemented

module test_strength_tran(
  inout a,
  inout b
);
  tran t1(a, b);  // Bidirectional resistive connection (always on)
endmodule
