// EXPECT=PASS
// Test: $rtoi truncation toward zero for negative numbers
// Feature: Ensure $rtoi truncates toward zero, not floor
// Expected: -3.7 -> -3 (not -4), -0.5 -> 0 (not -1)

module test_real_rtoi_negative;
  real r;
  integer i;

  initial begin
    // Negative truncation toward zero
    r = -3.7;
    i = $rtoi(r);  // Should be -3 (toward zero), not -4 (floor)

    r = -3.2;
    i = $rtoi(r);  // Should be -3

    r = -0.9;
    i = $rtoi(r);  // Should be 0 (toward zero), not -1

    r = -0.1;
    i = $rtoi(r);  // Should be 0

    // Positive cases (should still work)
    r = 3.7;
    i = $rtoi(r);  // Should be 3

    r = 0.9;
    i = $rtoi(r);  // Should be 0

    // Edge cases
    r = -1.0;
    i = $rtoi(r);  // Should be -1 (exact)

    r = -0.0;
    i = $rtoi(r);  // Should be 0
  end
endmodule
