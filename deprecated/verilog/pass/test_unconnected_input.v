// EXPECT=PASS
// Test case: Unconnected instance input should default to X in 4-state simulation
// This demonstrates that unconnected inputs propagate X through the logic

module and_gate(
  input wire a,
  input wire b,
  output wire y
);
  assign y = a & b;
endmodule

module top(
  input wire clk,
  input wire data_in,
  output wire result
);
  wire intermediate;

  // Instance with unconnected input 'b' - should be X in 4-state mode
  and_gate u_and(
    .a(data_in),
    // .b not connected - should default to X
    .y(intermediate)
  );

  // This should propagate X through
  assign result = intermediate;
endmodule
