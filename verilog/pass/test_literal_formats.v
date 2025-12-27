// EXPECT=PASS
module lit_matrix(
  output wire [31:0] y0,
  output wire [31:0] y1,
  output wire [31:0] y2,
  output wire [31:0] y3,
  output wire [31:0] y4,
  output wire [31:0] y5,
  output wire [31:0] y6,
  output wire [31:0] y7
);
  // hex casing and underscores
  assign y0 = 32'hDEAD_BEEF;
  assign y1 = 32'hdead_beef;
  assign y2 = 32'HDEAD_BEEF;  // upper-case base

  // binary with underscores
  assign y3 = 32'b1010_0101_0000_1111_1100_0011_0011_1100;

  // octal + decimal
  assign y4 = 32'o377;
  assign y5 = 32'd255;

  // small widths
  assign y6 = 8'hFF;
  assign y7 = 1'b1;
endmodule
