// EXPECT=PASS
module test_repeat_dynamic;
  integer n;
  reg clk;
  initial begin
    n = 3;
    clk = 0;
    repeat (n) #1 clk = ~clk;
    $finish;
  end
endmodule
