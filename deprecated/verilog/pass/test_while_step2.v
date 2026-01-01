// EXPECT=PASS
module while_step2;
  integer i;
  reg [7:0] arr [0:3];
  initial begin
    i = 0;
    while (i < 8) begin
      arr[i/2] = i;
      i = i + 2;
    end
  end
endmodule
