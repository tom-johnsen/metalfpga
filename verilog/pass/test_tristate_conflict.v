// EXPECT=PASS
// Test: Tristate bus contention (both enabled)
// Feature: Handling driver conflicts on tristate bus
// Expected: Should fail - conflict resolution not yet implemented

module test_tristate_conflict(
  input [7:0] data_a,
  input [7:0] data_b,
  output wire [7:0] bus
);
  reg enable_a, enable_b;

  assign bus = enable_a ? data_a : 8'bz;
  assign bus = enable_b ? data_b : 8'bz;

  initial begin
    enable_a = 1;
    enable_b = 1;  // Both enabled - contention!
    // Should result in X when drivers conflict
  end
endmodule
