// EXPECT=PASS
// Test: $dumpoff/$dumpon/$dumpflush behavior
// Feature: VCD dump control toggling

module test_system_dump_control;
  reg clk;
  reg [3:0] counter;

  initial begin
    clk = 0;
    counter = 0;
    $dumpfile("dump_control.vcd");
    $dumpvars(0, test_system_dump_control);

    #5 clk = ~clk;
    counter = counter + 1;

    $dumpoff;
    #5 clk = ~clk;          // Should not appear in VCD.
    counter = counter + 1;  // Should not appear in VCD.

    $dumpon;
    $dumpflush;
    #5 clk = ~clk;          // Should appear in VCD.
    counter = counter + 1;  // Should appear in VCD.

    #5;
    $finish;
  end
endmodule
