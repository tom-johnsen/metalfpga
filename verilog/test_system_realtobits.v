// Test: $realtobits and $bitstoreal system functions
// Feature: Convert between real and bit representation
// Expected: Should fail - real conversion not yet implemented

module test_system_realtobits;
  real r;
  reg [63:0] bits;

  initial begin
    r = 3.14159;
    bits = $realtobits(r);
    r = $bitstoreal(bits);
  end
endmodule
