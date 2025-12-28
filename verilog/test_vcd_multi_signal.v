// EXPECT=PASS
// Multiple signals with different widths changing at different rates
module test_vcd_multi_signal;
  reg clk;
  reg [3:0] fast_counter;
  reg [7:0] slow_counter;
  reg [1:0] toggle;
  reg single_bit;

  initial begin
    $dumpfile("multi_signal.vcd");
    $dumpvars(0, test_vcd_multi_signal);

    clk = 0;
    fast_counter = 0;
    slow_counter = 0;
    toggle = 0;
    single_bit = 0;

    #20 $finish;
  end

  always #1 clk = ~clk;

  always @(posedge clk) begin
    fast_counter = fast_counter + 1;
    if (fast_counter == 4'hF) begin
      slow_counter = slow_counter + 1;
    end
    if (fast_counter[1:0] == 2'b00) begin
      toggle = toggle + 1;
    end
    single_bit = ~single_bit;
  end
endmodule
