// EXPECT=PASS
// Non-blocking assignment ordering - tests NBA queue and delta cycles
module test_vcd_nba_ordering;
  reg clk;
  reg [7:0] a, b, c, d;

  initial begin
    $dumpfile("nba_ordering.vcd");
    $dumpvars(0, test_vcd_nba_ordering);

    clk = 0;
    a = 8'd1;
    b = 8'd2;
    c = 8'd3;
    d = 8'd4;

    #10 $finish;
  end

  always #1 clk = ~clk;

  // NBA chain - all assignments happen simultaneously at end of time slot
  always @(posedge clk) begin
    a <= b;  // a gets old value of b
    b <= c;  // b gets old value of c
    c <= d;  // c gets old value of d
    d <= a;  // d gets old value of a (rotation)
  end
endmodule
