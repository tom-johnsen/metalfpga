// EXPECT=PASS
// Test: Real number unary operations
// Feature: Unary plus, minus, and logical operators

module test_real_unary;
  real a;
  real result_real;
  reg result_bool;

  initial begin
    a = 3.5;

    // Unary plus
    result_real = +a;  // 3.5

    // Unary minus (negation)
    result_real = -a;  // -3.5
    result_real = -(-a);  // 3.5

    // Logical NOT (real used in boolean context)
    result_bool = !a;  // 0 (non-zero is true)
    result_bool = !0.0;  // 1 (zero is false)
    result_bool = !(-0.0);  // 1 (negative zero is also false)

    // Double negation
    a = -5.2;
    result_real = -a;  // 5.2

    // Zero cases
    result_real = +0.0;
    result_real = -0.0;
    result_bool = !0.0;  // 1
  end
endmodule
