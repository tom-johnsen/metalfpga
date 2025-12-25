// Test: nor gate primitive
// Feature: Gate-level primitives
// Expected: Should fail - gate primitives not yet implemented

module test_gate_nor(input a, input b, output y);
  nor g1(y, a, b);
endmodule
