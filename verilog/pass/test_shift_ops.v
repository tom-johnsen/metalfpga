// EXPECT=PASS
module shl_shr(
  input  wire [31:0] a,
  input  wire [4:0]  s,
  output wire [31:0] y1,
  output wire [31:0] y2
);
  assign y1 = a << s;
  assign y2 = a >> s;
endmodule