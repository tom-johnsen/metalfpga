// Multiple modules in one file (works)

module adder_mod(
  input wire [7:0] a,
  input wire [7:0] b,
  output wire [7:0] sum
);
  assign sum = a + b;
endmodule

module top(
  input wire [7:0] x,
  input wire [7:0] y,
  output wire [7:0] result
);

  // Reference adder_mod from same file
  adder_mod u_add(
    .a(x),
    .b(y),
    .sum(result)
  );

endmodule
