module boss(
  input  wire [31:0] a,
  input  wire [7:0]  b,
  input  wire [4:0]  s,
  output wire [31:0] y
);
  wire [31:0] t;

  assign t = (a >> s) + b;
  assign y = (t[15:0] == 16'd0) ? {16'hFFFF, t[15:0]} : t;
endmodule
