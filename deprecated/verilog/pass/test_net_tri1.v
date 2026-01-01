// EXPECT=PASS
// Test: tri1 net type
// Feature: Tristate with pull to 1
// Expected: Should fail - tri1 not yet implemented

module test_net_tri1(
  input data,
  input enable,
  output tri1 bus
);
  assign bus = enable ? data : 1'bz;
  // When all drivers are z, bus pulls to 1
endmodule
