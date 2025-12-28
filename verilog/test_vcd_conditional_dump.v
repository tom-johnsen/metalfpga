// EXPECT=PASS
// Conditional VCD dumping - tests $dumpon/$dumpoff
module test_vcd_conditional_dump;
  reg clk;
  reg [7:0] always_visible;
  reg [7:0] sometimes_visible;

  initial begin
    $dumpfile("conditional_dump.vcd");
    $dumpvars(0, test_vcd_conditional_dump);

    clk = 0;
    always_visible = 0;
    sometimes_visible = 0;

    #5  $dumpoff;   // Stop dumping
    #5  $dumpon;    // Resume dumping
    #5  $dumpoff;   // Stop again
    #5  $dumpon;    // Resume again

    #5 $finish;
  end

  always #1 clk = ~clk;

  always @(posedge clk) begin
    always_visible <= always_visible + 1;
    sometimes_visible <= sometimes_visible + 2;
  end
endmodule
