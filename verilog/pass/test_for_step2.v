module for_step2;
  integer i;
  reg [7:0] arr [0:3];
  initial begin
    for (i = 0; i < 8; i = i + 2)
      arr[i/2] = i;
  end
endmodule
