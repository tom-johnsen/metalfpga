// Test: $rtoi and $itor system functions
// Feature: Convert between real and integer
// Expected: Should fail - real conversion not yet implemented

module test_system_rtoi;
  real r;
  integer i;

  initial begin
    r = 3.7;
    i = $rtoi(r);  // 3 (truncate)

    i = 42;
    r = $itor(i);  // 42.0
  end
endmodule
