// EXPECT=PASS
module while_complex;
  integer i, j;
  reg [7:0] arr [0:15];
  initial begin
    i = 0;
    j = 15;
    while (i < j) begin
      arr[i] = j;
      i = i + 1;
      j = j - 1;
    end
  end
endmodule
