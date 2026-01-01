// EXPECT=PASS
module repeat_always;
  reg clk;
  integer i;
  reg [7:0] arr [0:3];

  always @(posedge clk) begin
    i = 0;
    repeat (4) begin
      arr[i] <= arr[i] << 1;
      i = i + 1;
    end
  end
endmodule
