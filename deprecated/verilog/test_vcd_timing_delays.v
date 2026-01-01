// EXPECT=PASS
// Various timing delays - tests VCD time advancement
module test_vcd_timing_delays;
  reg clk;
  reg [7:0] delayed_a;
  reg [7:0] delayed_b;
  reg [7:0] delayed_c;

  initial begin
    $dumpfile("timing_delays.vcd");
    $dumpvars(0, test_vcd_timing_delays);

    clk = 0;
    delayed_a = 0;
    delayed_b = 0;
    delayed_c = 0;

    // Different delay values
    #3  delayed_a = 8'hAA;
    #2  delayed_b = 8'hBB;
    #4  delayed_c = 8'hCC;
    #3  delayed_a = 8'hDD;
    #1  delayed_b = 8'hEE;
    #2  delayed_c = 8'hFF;

    #5 $finish;
  end

  always #1 clk = ~clk;
endmodule
