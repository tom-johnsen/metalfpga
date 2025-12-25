// Test: Sensitivity list with 'or' keyword
// Feature: Sensitivity lists beyond @* and @(posedge/negedge clk)
// Expected: Should fail - complex sensitivity lists not yet implemented

module test_sensitivity_or;
  reg [7:0] a, b;
  reg [7:0] result;

  // Combinational logic with 'or' in sensitivity list
  always @(a or b) begin
    result = a & b;
  end

  initial begin
    a = 8'hFF;
    b = 8'h0F;
  end
endmodule
