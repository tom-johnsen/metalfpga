module for_negative_step2;
  integer i;
  reg [7:0] arr [0:3];
  initial begin
    for (i = 7; i >= 0; i = i - 2)
      arr[(7-i)/2] = i;
  end
endmodule
