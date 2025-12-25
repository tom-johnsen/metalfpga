module param_math #(
  parameter W = 8,
  parameter D = (1 << W) - 1  // complex expr
)(input [W-1:0] in, output [W-1:0] out);
  assign out = in;
endmodule
