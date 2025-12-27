// EXPECT=PASS
module leaf(
  input  wire a,
  input  wire b,
  output wire y
);
  assign y = a;
endmodule

module mid(
  input  wire x,
  input  wire y,
  output wire z
);
  leaf u0 (.a(x), .b(y), .y(z));
endmodule

module top(
  input  wire a,
  input  wire b,
  output wire y
);
  mid u (.x(a), .y(b), .z(y));
endmodule
