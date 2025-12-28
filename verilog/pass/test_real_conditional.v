// EXPECT=PASS
// Test: Real numbers in conditional expressions
// Feature: Ternary operator and if statements with real operands

module test_real_conditional;
  real a;
  real b;
  real result;
  reg flag;

  initial begin
    a = 3.5;
    b = 2.7;
    flag = 1;

    // Ternary with real result
    result = flag ? a : b;  // 3.5
    result = !flag ? a : b;  // 2.7

    // Ternary with real condition
    result = a ? 10.0 : 20.0;  // 10.0 (non-zero is true)
    result = 0.0 ? 10.0 : 20.0;  // 20.0 (zero is false)

    // Ternary with mixed real/integer
    result = flag ? 42 : 3.14;  // 42.0 (integer promoted to real)
    result = flag ? 3.14 : 42;  // 3.14

    // Nested ternary
    result = (a > b) ? ((a > 5.0) ? 100.0 : 50.0) : 0.0;  // 50.0

    // Real in if condition
    if (a > b) begin
      result = a * 2.0;
    end else begin
      result = b * 2.0;
    end

    // Real condition (non-zero check)
    if (a) result = 1.0;  // true
    if (0.0) result = 2.0; else result = 3.0;  // 3.0
  end
endmodule
