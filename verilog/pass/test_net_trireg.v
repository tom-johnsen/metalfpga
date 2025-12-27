// EXPECT=PASS
// Test: trireg net type (capacitive)
// Feature: Tristate with charge storage
// Expected: Should fail - trireg not yet implemented

module test_net_trireg(
  input data,
  input enable
);
  trireg bus;

  assign bus = enable ? data : 1'bz;
  // When all drivers go to z, bus retains last driven value
endmodule
