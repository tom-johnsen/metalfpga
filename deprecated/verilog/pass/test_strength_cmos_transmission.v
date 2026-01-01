// EXPECT=PASS
// Test: CMOS transmission gate
// Feature: Bidirectional MOS switch
// Expected: Should fail - MOS switch primitives not yet implemented

module test_strength_cmos_transmission(
  input control,
  inout a,
  inout b
);
  wire control_n;
  assign control_n = ~control;

  cmos tgate(a, b, control, control_n);
  // When control=1: bidirectional connection between a and b
  // When control=0: high impedance
endmodule
