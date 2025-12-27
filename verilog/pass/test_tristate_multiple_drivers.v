// EXPECT=PASS
// Test: Multiple tristate drivers on same bus
// Feature: Net resolution with multiple drivers
// Expected: Should fail - multi-driver resolution not yet implemented

module test_tristate_multiple_drivers(
  input [7:0] data_a,
  input [7:0] data_b,
  input enable_a,
  input enable_b,
  output wire [7:0] bus
);
  // Two drivers on same bus - should resolve based on enables
  assign bus = enable_a ? data_a : 8'bz;
  assign bus = enable_b ? data_b : 8'bz;
endmodule
