// EXPECT=PASS
module while_dual_countdown;
  integer i, j;
  reg [7:0] arr [0:7];
  initial begin
    i = 7;
    j = 0;
    while (i >= j) begin
      arr[j] = i;
      i = i - 1;
      j = j + 1;
    end
  end
endmodule
