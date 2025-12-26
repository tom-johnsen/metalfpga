// Test: $finish system task
// Feature: End simulation
// Expected: Should fail - $finish not yet implemented

module test_system_finish;
  reg [7:0] counter;

  initial begin
    counter = 0;

    repeat (10) begin
      #10 counter = counter + 1;
      if (counter == 5)
        $finish;  // Exit simulation early
    end
  end
endmodule
