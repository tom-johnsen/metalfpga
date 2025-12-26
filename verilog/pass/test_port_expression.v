// Test: Expressions in port connections
// Feature: Complex expressions and concatenations in port maps
// Expected: May fail - complex port expressions

module adder(input [7:0] a, b, output [7:0] sum);
  assign sum = a + b;
endmodule

module test_port_expression;
  reg [15:0] data;
  wire [7:0] result;

  // Slice expressions in ports
  adder u1 (.a(data[15:8]), .b(data[7:0]), .sum(result));

  // Concatenation in ports
  adder u2 (.a({4'h0, data[3:0]}), .b(8'h55), .sum(result));
endmodule
