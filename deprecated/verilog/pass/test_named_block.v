// EXPECT=PASS
// Test: Named blocks and disable with multiple levels
// Feature: Named begin/end blocks with nested disable
// Expected: May fail - complex named block handling

module test_named_block;
  reg [7:0] counter;
  integer i, j;

  initial begin : outer_block
    counter = 0;
    for (i = 0; i < 10; i = i + 1) begin : loop1
      for (j = 0; j < 10; j = j + 1) begin : loop2
        counter = counter + 1;
        if (counter == 25)
          disable loop1;  // Break out of outer loop
        if (counter == 50)
          disable outer_block;  // Exit entire block
      end
    end
  end
endmodule
