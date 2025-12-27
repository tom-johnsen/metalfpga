// EXPECT=PASS
// Test: Real number arithmetic
// Feature: Floating-point operations

module test_real_arithmetic;
  real a, b, sum, product, quotient;

  initial begin
    a = 3.14159;
    b = 2.71828;

    sum = a + b;
    product = a * b;
    quotient = a / b;

    if (a > b)
      sum = a;
  end
endmodule
