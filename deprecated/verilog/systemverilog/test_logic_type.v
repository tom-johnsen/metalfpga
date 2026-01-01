// Test: SystemVerilog logic type
// Feature: logic data type (can be driven like reg or wire)
// Expected: Should fail - SystemVerilog construct

module test_logic_type;
  logic [7:0] data;
  logic clk;

  assign data = 8'hAA;

  always @(posedge clk) begin
    data <= data + 1;
  end
endmodule
