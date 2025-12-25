// Test: disable statement
// Feature: Named block control
// Expected: Should fail - disable not yet implemented

module test_disable;
  reg [7:0] counter;
  reg [7:0] i;

  initial begin : main_block
    counter = 0;
    for (i = 0; i < 100; i = i + 1) begin
      counter = counter + 1;
      if (counter == 10)
        disable main_block;
    end
  end
endmodule
