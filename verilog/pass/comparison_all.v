module cmp(input [7:0] a, b, output eq, ne, lt, gt, le, ge);
  assign eq = (a == b);
  assign ne = (a != b);
  assign lt = (a < b);
  assign gt = (a > b);
  assign le = (a <= b);
  assign ge = (a >= b);
endmodule
