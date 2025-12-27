// EXPECT=PASS
module zero_param #(
  parameter W = 1
)(
  input  wire [W-1:0] a,
  output wire [W-1:0] y
);
  assign y = a & {W{1'b1}};
endmodule
