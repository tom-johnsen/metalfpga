// EXPECT=PASS
// Edge detection - tests posedge/negedge sensitivity in VCD
module test_vcd_edge_detection;
  reg clk;
  reg reset;
  reg [7:0] posedge_count;
  reg [7:0] negedge_count;
  reg [7:0] anyedge_count;

  initial begin
    $dumpfile("edge_detection.vcd");
    $dumpvars(0, test_vcd_edge_detection);

    clk = 0;
    reset = 0;
    posedge_count = 0;
    negedge_count = 0;
    anyedge_count = 0;

    #20 $finish;
  end

  always #1 clk = ~clk;

  always @(posedge clk) begin
    posedge_count <= posedge_count + 1;
  end

  always @(negedge clk) begin
    negedge_count <= negedge_count + 1;
  end

  always @(clk) begin
    anyedge_count <= anyedge_count + 1;
  end
endmodule
