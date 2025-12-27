// EXPECT=PASS
// Test: defparam statement
// Feature: Hierarchical parameter override
// Expected: Should fail - defparam not yet implemented

module inner #(parameter WIDTH = 8) (
  input [WIDTH-1:0] data_in,
  output [WIDTH-1:0] data_out
);
  assign data_out = data_in;
endmodule

module test_defparam;
  wire [15:0] data_in, data_out;

  inner inst (.data_in(data_in), .data_out(data_out));

  defparam inst.WIDTH = 16;
endmodule
