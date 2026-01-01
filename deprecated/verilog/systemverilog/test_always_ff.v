// Test: SystemVerilog always_ff
// Feature: always_ff procedural block
// Expected: Should fail - SystemVerilog construct

module test_always_ff;
  reg clk, rst;
  reg [7:0] counter;

  always_ff @(posedge clk or posedge rst) begin
    if (rst)
      counter <= 8'h00;
    else
      counter <= counter + 1;
  end

  initial begin
    clk = 0;
    rst = 1;
    #10 rst = 0;
    forever #5 clk = ~clk;
  end
endmodule
