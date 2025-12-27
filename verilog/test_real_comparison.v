// EXPECT=PASS
// Test: Real number comparisons
// Feature: All relational operators with real operands

module test_real_comparison;
  real a;
  real b;
  reg result;

  initial begin
    a = 3.5;
    b = 2.7;

    // Greater than
    result = (a > b);   // 1
    result = (b > a);   // 0

    // Less than
    result = (a < b);   // 0
    result = (b < a);   // 1

    // Greater or equal
    result = (a >= b);  // 1
    result = (a >= a);  // 1
    result = (b >= a);  // 0

    // Less or equal
    result = (a <= b);  // 0
    result = (a <= a);  // 1
    result = (b <= a);  // 1

    // Equality
    result = (a == b);  // 0
    result = (a == a);  // 1

    // Inequality
    result = (a != b);  // 1
    result = (a != a);  // 0

    // Edge cases
    result = (0.0 == -0.0);  // 1 (IEEE 754: +0 equals -0)
  end
endmodule
