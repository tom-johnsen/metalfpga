module param_leaf #(
  parameter W = 8
)(
  input  wire [W-1:0] a,
  output wire [W-1:0] y
);
  localparam W2 = W + 1;
  assign y = a & {W{1'b1}};
endmodule

module param_mid #(
  parameter W = 4
)(
  input  wire [W-1:0] a,
  output wire [W-1:0] y
);
  param_leaf #(.W(W)) u0 (
    .a(a),
    .y(y)
  );
endmodule

module param_top(
  input  wire [3:0] a,
  output wire [3:0] y
);
  param_mid #(.W(4)) u (
    .a(a),
    .y(y)
  );
endmodule
