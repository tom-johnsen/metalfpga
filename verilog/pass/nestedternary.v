module ternary_nest(
  input  wire a,
  input  wire b,
  input  wire c,
  input  wire d,
  output wire y
);
  assign y = a ? b : c ? d : 1'b0;
endmodule
