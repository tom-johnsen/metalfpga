// EXPECT=PASS
// Test: $urandom system function (SystemVerilog)
// Feature: Unsigned random number generation

module test_system_urandom;
  integer unsigned rand_val;
  reg [7:0] rand_byte;

  initial begin
    rand_val = $urandom;
    rand_byte = $urandom % 256;
    rand_val = $urandom_range(100, 200);  // Range [100, 200]
  end
endmodule
