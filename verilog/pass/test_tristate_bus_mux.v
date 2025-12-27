// EXPECT=PASS
// Test: Tristate bus multiplexer
// Feature: Classic tristate bus with multiple sources
// Expected: Should fail - tristate resolution not yet implemented

module test_tristate_bus_mux(
  input [7:0] src0,
  input [7:0] src1,
  input [7:0] src2,
  input [7:0] src3,
  input [1:0] sel,
  output wire [7:0] bus
);
  assign bus = (sel == 2'd0) ? src0 : 8'bz;
  assign bus = (sel == 2'd1) ? src1 : 8'bz;
  assign bus = (sel == 2'd2) ? src2 : 8'bz;
  assign bus = (sel == 2'd3) ? src3 : 8'bz;
endmodule
