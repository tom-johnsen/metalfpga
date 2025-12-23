module comb(input a, b, output reg y);
  always @(*) y = a & b;
endmodule