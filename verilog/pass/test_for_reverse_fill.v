// EXPECT=PASS
module for_reverse_fill;
  integer i;
  reg [7:0] arr [0:9];
  initial begin
    for (i = 9; i >= 0; i = i - 1)
      arr[i] = 9 - i;
  end
endmodule
