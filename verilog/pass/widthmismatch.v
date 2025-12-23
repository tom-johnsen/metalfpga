module width_mismatch(
  input  wire [15:0] a,
  input  wire [7:0]  b,
  output wire [15:0] y
);
  assign y = a + b;
endmodule
