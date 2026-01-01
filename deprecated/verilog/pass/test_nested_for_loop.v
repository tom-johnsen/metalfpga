// EXPECT=PASS
module nested_for_test;
  integer i, j;
  reg [7:0] arr [0:11];
  initial begin
    for (i = 0; i < 3; i = i + 1)
      for (j = 0; j < 4; j = j + 1)
        arr[i*4 + j] = i + j;
  end
endmodule
