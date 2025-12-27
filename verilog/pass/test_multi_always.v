// EXPECT=PASS
module multi(input clk, input [7:0] d, output reg [7:0] q1, q2);
  always @(posedge clk) q1 <= d + 1;
  always @(posedge clk) q2 <= d + 2;
endmodule
