module shr_overshift(
  input  wire [31:0] a,
  input  wire [31:0] s,   // intentionally wide
  output wire [31:0] y
);
  assign y = a >> s;
endmodule