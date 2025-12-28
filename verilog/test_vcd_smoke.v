module test_vcd_smoke;
  reg clk;
  reg [3:0] counter;

  initial begin
    clk = 0;
    counter = 0;
    $dumpfile("dump.vcd");
    $dumpvars(0, test_vcd_smoke);
    repeat (4) begin
      #1;
      counter = counter + 1;
      clk = ~clk;
    end
    #1;
    $finish;
  end
endmodule
