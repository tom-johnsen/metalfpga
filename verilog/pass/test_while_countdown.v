module while_countdown;
  integer i;
  reg [7:0] arr [0:7];
  initial begin
    i = 7;
    while (i >= 0) begin
      arr[i] = 7 - i;
      i = i - 1;
    end
  end
endmodule
