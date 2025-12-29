// EXPECT=PASS
// Test: wide $monitor/$strobe/$display on 128-bit values
module test_wide_monitor_strobe_display;
  reg [127:0] wide;

  initial begin
    wide = 128'h00000000000000000000000000000001;
    $monitor("MON=%h", wide);

    #1;
    wide = 128'h00010000000000000000000000000001;

    #1;
    $display("DISP=%h", wide);

    #1;
    $strobe("STB=%h", wide);

    #1;
    $finish;
  end
endmodule
