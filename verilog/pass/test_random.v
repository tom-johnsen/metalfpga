// EXPECT=PASS
// Test: $random system function
// Feature: System functions

module test_random;
  reg [31:0] rand_val;
  integer i;

  initial begin
    for (i = 0; i < 10; i = i + 1) begin
      rand_val = $random;
    end
  end
endmodule
