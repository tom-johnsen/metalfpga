// EXPECT=PASS
module shift_precedence(
  input  wire [31:0] a,
  input  wire [4:0]  s,
  output wire [31:0] y
);
  //assign y = (a + 32'd1) >> s;
  //assign y = a >> 1 >> s;
  //assign y = (a >> s) & 32'hFF; // Formatting bug: prints 255 not FF
  assign y = (a >> s) & 8'hFF;

endmodule
