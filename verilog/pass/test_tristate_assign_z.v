// EXPECT=PASS
// Test: Tristate using assign with Z
// Feature: High-impedance assignment
// Expected: Should fail - tristate resolution not yet implemented

module test_tristate_assign_z(
  input data,
  input enable,
  output wire bus
);
  assign bus = enable ? data : 1'bz;
endmodule
