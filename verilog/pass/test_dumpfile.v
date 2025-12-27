// EXPECT=PASS
// Test: VCD dump system tasks
// Feature: Waveform dumping
// Expected: Should fail - VCD tasks not yet implemented

module test_dumpfile;
  reg [7:0] data;
  reg clk;

  initial begin
    $dumpfile("waves.vcd");
    $dumpvars(0, test_dumpfile);
  end

  always #5 clk = ~clk;

  always @(posedge clk) begin
    data = data + 1;
  end
endmodule
