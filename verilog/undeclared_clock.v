module no_clk_decl(input d, output reg q);
  always @(posedge clk) q <= d;  // clk not in port list
endmodule
