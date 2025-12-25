module for_always_test;
  reg clk;
  integer i;
  reg [7:0] arr [0:7];

  always @(posedge clk) begin
    for (i = 0; i < 8; i = i + 1)
      arr[i] <= arr[i] + 1;
  end
endmodule
