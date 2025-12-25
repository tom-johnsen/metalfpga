// Test: $finish system task
// Feature: System tasks
// Expected: Should fail - system tasks not yet implemented

module test_finish;
  reg [3:0] counter;

  initial begin
    counter = 0;
  end

  always @(counter) begin
    if (counter == 4'd10) begin
      $finish;
    end
  end
endmodule
