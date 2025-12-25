module while_always;
  reg clk;
  integer i;
  reg [7:0] arr [0:7];

  always @(posedge clk) begin
    i = 0;
    while (i < 8) begin
      arr[i] <= arr[i] + 1;
      i = i + 1;
    end
  end
endmodule
