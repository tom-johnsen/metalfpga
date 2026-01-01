// EXPECT=PASS
module unsized_vs_sized(
  input  wire [31:0] a,
  output wire [31:0] y0,
  output wire [31:0] y1,
  output wire [31:0] y2
);
  // Unsized literal "1" (semantics depend on your rules)
  assign y0 = a + 1;

  // Sized, explicit
  assign y1 = a + 32'd1;

  // Another unsized constant
  assign y2 = a & 255;
endmodule
