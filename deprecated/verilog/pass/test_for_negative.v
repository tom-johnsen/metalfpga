// EXPECT=PASS
module for_negative;
  integer i;
  reg [7:0] arr [0:7];
  initial begin
    for (i = 7; i >= 0; i = i - 1)
      arr[i] = 7 - i;
  end
endmodule
