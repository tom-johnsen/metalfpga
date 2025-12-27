// EXPECT=PASS
// Test: Parameter override in instantiation
// Feature: #() parameter passing syntax
// Expected: May fail - parameter override not yet tested

module parameterized #(
  parameter WIDTH = 8,
  parameter DEPTH = 16
) (
  input [WIDTH-1:0] data,
  output [WIDTH-1:0] out
);
  reg [WIDTH-1:0] mem [0:DEPTH-1];
  assign out = mem[0];
endmodule

module test_parameter_override;
  wire [15:0] out1;
  wire [31:0] out2;

  parameterized #(16, 32) inst1 (.data(16'h1234), .out(out1));
  parameterized #(.WIDTH(32), .DEPTH(64)) inst2 (.data(32'h5678), .out(out2));
endmodule
