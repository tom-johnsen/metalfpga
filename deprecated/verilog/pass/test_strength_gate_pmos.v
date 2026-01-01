// EXPECT=PASS
// Test: PMOS transistor primitive
// Feature: MOS switch-level primitives
// Expected: Should fail - MOS primitives not yet implemented

module test_strength_gate_pmos(
  input gate,
  input source,
  output drain
);
  pmos p1(drain, source, gate);
  // PMOS: when gate=0, drain=source; when gate=1, drain=z
endmodule
