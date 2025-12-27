// EXPECT=PASS
module selects(
  input  wire [31:0] a,
  output wire [7:0]  lo,
  output wire [7:0]  hi,
  output wire        bit7
);
  assign lo  = a[7:0];
  assign hi  = a[31:24];
  assign bit7 = a[7];
endmodule
