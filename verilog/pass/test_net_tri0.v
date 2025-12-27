// EXPECT=PASS
// Test: tri0 net type
// Feature: Tristate with pull to 0
// Expected: Should fail - tri0 not yet implemented

module test_net_tri0(
  input data,
  input enable,
  output tri0 bus
);
  assign bus = enable ? data : 1'bz;
  // When all drivers are z, bus pulls to 0
endmodule
