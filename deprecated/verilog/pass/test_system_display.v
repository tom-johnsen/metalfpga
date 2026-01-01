// EXPECT=PASS
// Test: $display system task
// Feature: Console output with formatting
// Expected: Should fail - $display not yet implemented

module test_system_display;
  reg [7:0] data;
  integer count;

  initial begin
    data = 8'hAB;
    count = 42;

    $display("Simple message");
    $display("Data = %h, Count = %d", data, count);
    $display("Binary: %b, Octal: %o, Decimal: %d, Hex: %h", data, data, data, data);
    $display("Time: %t", $time);
  end
endmodule
