// EXPECT=PASS
// Test: $random system function
// Feature: Pseudo-random number generation

module test_system_random;
  integer seed;
  integer rand_val;
  reg [7:0] rand_byte;

  initial begin
    seed = 12345;
    rand_val = $random;              // Random with automatic seed
    rand_val = $random(seed);        // Random with specific seed
    rand_byte = $random % 256;       // Random in range [0, 255]

    // Generate 10 random values
    repeat (10) begin
      #10 rand_val = $random(seed);
    end
  end
endmodule
