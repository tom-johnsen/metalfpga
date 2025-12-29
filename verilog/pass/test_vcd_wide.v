// EXPECT=PASS
// Test: VCD dump of wide (>64-bit) signal values
module test_vcd_wide;
  reg [127:0] wide;
  reg clk;

  initial begin
    $dumpfile("wide.vcd");
    $dumpvars(0, test_vcd_wide);

    wide = 128'h00000000000000000000000000000000;
    clk = 0;
    #1;
    clk = 1;
    wide = 128'h0123456789abcdef0011223344556677;
    #1;
    clk = 0;
    wide = 128'h80000000000000000000000000000000;
    #1;
    wide = {4'hx, 124'h0};
    #1;
    $finish;
  end
endmodule
