module repeat_nested;
  integer i, j;
  reg [7:0] arr [0:11];
  initial begin
    i = 0;
    repeat (3) begin
      j = 0;
      repeat (4) begin
        arr[i*4 + j] = i + j;
        j = j + 1;
      end
      i = i + 1;
    end
  end
endmodule
