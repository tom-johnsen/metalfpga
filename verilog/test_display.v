// Test: $display system task
// Feature: System tasks ($display, $monitor, $finish, etc.)
// Expected: Should fail - system tasks not yet implemented

module test_display;
  reg [7:0] value;
  reg clk;

  initial begin
    value = 8'h42;
    $display("Value is: %h", value);
    $display("Value in decimal: %d", value);
    $display("Value in binary: %b", value);
    $display("Simple message");
  end
endmodule
