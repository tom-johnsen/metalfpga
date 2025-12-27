// EXPECT=PASS
module for_test;
  integer i;
  reg [7:0] arr [0:7];
  initial for (i = 0; i < 8; i = i + 1)
    arr[i] = i;
endmodule