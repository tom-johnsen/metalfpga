// File B: references module from file A

module top(
  input wire [7:0] x,
  input wire [7:0] y,
  output wire [7:0] result
);

  // Try to instantiate adder_mod from test_multifile_a.v
  adder_mod u_add(
    .a(x),
    .b(y),
    .sum(result)
  );

endmodule
