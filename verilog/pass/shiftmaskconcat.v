module shift_concat(
  input  wire [15:0] a,
  input  wire [3:0]  s,
  output wire [31:0] y
);
  assign y = { (a >> s) & 16'hFF, a[7:0] };
endmodule
