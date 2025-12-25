// Test: $random system function
// Feature: System functions
// Expected: Should fail - $random not yet implemented

module test_random;
  reg [31:0] rand_val;
  integer i;

  initial begin
    for (i = 0; i < 10; i = i + 1) begin
      rand_val = $random;
    end
  end
endmodule
