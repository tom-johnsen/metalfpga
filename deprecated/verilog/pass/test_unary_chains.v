// EXPECT=PASS
module unary_chain(
  input  wire [7:0] a,
  output wire [7:0] y0,
  output wire [7:0] y1
);
  assign y0 = ~a + 1;
  assign y1 = ~(a + 1);
endmodule
