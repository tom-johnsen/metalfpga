module mix(input clk, input [7:0] in, output reg [7:0] out);
  reg [7:0] temp;
  always @(posedge clk) begin
    temp = in + 1;      // blocking
    out <= temp + 1;    // non-blocking reads blocking result
  end
endmodule
