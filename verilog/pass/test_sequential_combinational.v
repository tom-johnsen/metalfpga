// EXPECT=PASS
module seq_combo(
  input  wire       clk,
  input  wire [7:0] a,
  output reg  [7:0] r,
  output wire [7:0] y
);
  always @(posedge clk)
    r <= a;

  assign y = r + 1;
endmodule
