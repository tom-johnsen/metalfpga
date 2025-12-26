// Test: Explicit sensitivity list with multiple signals
// Feature: Sensitivity lists beyond @* and @(posedge/negedge clk)
// Expected: Should fail - complex sensitivity lists not yet implemented

module test_sensitivity_list;
  reg [7:0] a, b, c;
  reg [7:0] result;

  // Combinational logic sensitive to a, b, and c
  always @(a, b, c) begin
    result = a + b + c;
  end

  initial begin
    a = 8'd10;
    b = 8'd20;
    c = 8'd30;
  end
endmodule
