// Test case: Empty port connection syntax .port()
// This is valid Verilog-2001 syntax for explicitly unconnected ports
// Currently fails with: "expected expression"

module buffer_gate(
  input wire in,
  output wire out
);
  assign out = in;
endmodule

module mux2(
  input wire sel,
  input wire a,
  input wire b,
  output wire y
);
  assign y = sel ? a : b;
endmodule

module top(
  input wire clk,
  input wire data,
  output wire q1,
  output wire q2
);

  // Test 1: Empty connection in middle of port list
  buffer_gate buf1(
    .in(data),
    .out()      // ← Should parse: explicitly unconnected output
  );

  // Test 2: Empty connection at end
  mux2 m1(
    .sel(clk),
    .a(data),
    .b(),       // ← Should parse: explicitly unconnected
    .y(q1)
  );

  // Test 3: Multiple empty connections
  mux2 m2(
    .sel(),     // ← Should parse
    .a(data),
    .b(),       // ← Should parse
    .y(q2)
  );

endmodule
