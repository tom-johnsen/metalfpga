module concat_nesting(
  input  wire [7:0]  a,
  input  wire [7:0]  b,
  input  wire [3:0]  c,
  output wire [31:0] y
);
  assign y = { a, { b[3:0], c }, 8'h00, b[7:4] };
endmodule
