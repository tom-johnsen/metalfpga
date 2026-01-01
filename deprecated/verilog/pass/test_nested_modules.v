// EXPECT=PASS
module leaf_unit #(
  parameter W = 8
)(
  input  wire [W-1:0] a,
  input  wire [W-1:0] b,
  output wire [W-1:0] y
);
  assign y = (a + b) & {W{1'b1}};
endmodule

module mid_unit #(
  parameter W = 8
)(
  input  wire [W-1:0] x,
  input  wire [W-1:0] y,
  output wire [W-1:0] z
);
  wire [W-1:0] t0;
  wire [W-1:0] t1;

  leaf_unit #(.W(W)) u0 (
    .a(x),
    .b(y),
    .y(t0)
  );

  leaf_unit #(.W(W)) u1 (
    .a(t0),
    .b(x),
    .y(t1)
  );

  assign z = t1;
endmodule

module top_nested(
  input  wire [7:0] a,
  input  wire [7:0] b,
  output wire [7:0] y
);
  mid_unit #(.W(8)) u_mid (
    .x(a),
    .y(b),
    .z(y)
  );
endmodule

module top_positional(
  input  wire [7:0] a,
  input  wire [7:0] b,
  output wire [7:0] y
);
  mid_unit #(8) u_mid (a, b, y);
endmodule
