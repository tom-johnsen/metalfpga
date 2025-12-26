// Test: NMOS transistor primitive
// Feature: MOS switch-level primitives
// Expected: Should fail - MOS primitives not yet implemented

module test_strength_gate_nmos(
  input gate,
  input source,
  output drain
);
  nmos n1(drain, source, gate);
  // NMOS: when gate=1, drain=source; when gate=0, drain=z
endmodule
