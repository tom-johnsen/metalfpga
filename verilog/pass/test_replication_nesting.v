// EXPECT=PASS
module repl_nesting(
  input  wire [3:0] a,
  output wire [31:0] y0,
  output wire [31:0] y1
);
  assign y0 = {8{a}};             // replicate a 4-bit value 8 times
  assign y1 = {4{{a, 4'h0}}};     // replicate an 8-bit concat 4 times
endmodule
