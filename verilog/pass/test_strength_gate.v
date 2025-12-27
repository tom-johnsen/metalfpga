// EXPECT=PASS
// Test: Gate-level primitive with drive strength
// Feature: Drive strength on gate instantiation
// Expected: Should fail - gate primitives with strength not yet implemented

module test_strength_gate(
  input a,
  input b,
  output y
);
  and (strong1, weak0) g1(y, a, b);  // AND gate with specified strengths
endmodule
