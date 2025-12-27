// EXPECT=PASS
// Test: trior net type
// Feature: Tristate with wired-OR resolution
// Expected: Should fail - trior not yet implemented

module test_net_trior(
  input [7:0] data_a,
  input [7:0] data_b,
  input enable_a,
  input enable_b,
  output trior [7:0] bus
);
  assign bus = enable_a ? data_a : 8'bz;
  assign bus = enable_b ? data_b : 8'bz;
  // When both enabled, result is data_a | data_b
  // When one enabled, result is that data
  // When neither enabled, result is z
endmodule
