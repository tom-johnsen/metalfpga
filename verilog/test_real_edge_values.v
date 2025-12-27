// EXPECT=PASS
// Test: Real number edge cases and special values
// Feature: Test infinity, very large/small numbers, precision edge cases
// Expected: Proper handling of edge values

module test_real_edge_values;
  real pos_inf;
  real neg_inf;
  real very_large;
  real very_small;
  real epsilon;
  real result;
  integer i;

  initial begin
    // Very large numbers (approach overflow)
    very_large = 1.7976931348623157e308;  // Near max double
    result = very_large * 1.0;

    // Very small numbers (approach underflow)
    very_small = 2.2250738585072014e-308;  // Near min positive double
    result = very_small * 1.0;

    // Epsilon precision tests
    epsilon = 2.220446049250313e-16;  // Machine epsilon for double
    result = 1.0 + epsilon;  // Should be distinguishable from 1.0
    result = 1.0 + (epsilon / 2.0);  // May round to 1.0

    // Division edge cases
    result = 1.0 / very_large;  // Should approach 0
    result = 1.0 / very_small;  // Should be very large

    // Negative zero
    result = -0.0;
    result = 0.0 - 0.0;

    // Comparisons with edge values
    if (very_large > 1.0e100) begin
      result = 1.0;
    end

    if (very_small < 1.0e-100) begin
      result = 2.0;
    end

    // Arithmetic with very large/small
    result = very_large + 1.0;  // 1.0 should be lost in precision
    result = very_small - very_small;  // Should be 0

    // Conversion edge cases
    i = $rtoi(very_large);  // Overflow: implementation defined
    i = $rtoi(very_small);  // Should be 0

    // Subnormal numbers (denormalized)
    very_small = 5e-324;  // Smallest subnormal
    result = very_small / 2.0;

    // Powers of 2 (exact representation)
    result = 1024.0;
    result = result / 2.0;  // 512.0
    result = result / 2.0;  // 256.0
    result = result / 2.0;  // 128.0
  end
endmodule
