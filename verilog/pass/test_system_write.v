// Test: $write system task
// Feature: Console output without automatic newline
// Expected: Should fail - $write not yet implemented

module test_system_write;
  reg [7:0] a, b;

  initial begin
    a = 8'h10;
    b = 8'h20;

    $write("a = %h, ", a);
    $write("b = %h", b);
    $write("\n");
  end
endmodule
