module parens(
  input  wire [31:0] a,
  input  wire [4:0]  s,
  output wire [31:0] y0,
  output wire [31:0] y1,
  output wire [31:0] y2
);
  assign y0 = (a + 32'd1) >> s;
  assign y1 = (a >> s) & 32'hFF;
  assign y2 = ((a ^ 32'hA5A5A5A5) + 32'd3) << 1;
endmodule
