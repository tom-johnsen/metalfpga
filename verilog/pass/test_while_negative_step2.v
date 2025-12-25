module while_negative_step2;
  integer i;
  reg [7:0] arr [0:3];
  initial begin
    i = 7;
    while (i >= 0) begin
      arr[(7-i)/2] = i;
      i = i - 2;
    end
  end
endmodule
