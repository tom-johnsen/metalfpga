// EXPECT=PASS
// Test: Controlled bidirectional switch
// Feature: Resistive connection primitives with control
// Expected: Should fail - tranif primitives not yet implemented

module test_strength_tranif(
  inout a,
  inout b,
  input control
);
  tranif1 t1(a, b, control);  // Connects a to b when control=1
  // tranif0 would connect when control=0
endmodule
