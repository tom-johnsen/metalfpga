// EXPECT=PASS
// Basic counter with VCD output - tests incremental value changes
module test_vcd_counter_basic;
  reg clk;
  reg [7:0] counter;

  initial begin
    $dumpfile("counter_basic.vcd");
    $dumpvars(0, test_vcd_counter_basic);

    clk = 0;
    counter = 0;

    #10 $finish;
  end

  always @(posedge clk) begin
    counter = counter + 1;
  end

  always #1 clk = ~clk;
endmodule
