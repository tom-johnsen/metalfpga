// EXPECT=PASS
// Test: nand gate primitive
// Feature: Gate-level primitives
// Expected: Should fail - gate primitives not yet implemented

module test_gate_nand(input a, input b, output y);
  nand g1(y, a, b);
endmodule
