// Test: Real number literals
// Feature: Real constant syntax
// Expected: Should fail - real literals not yet implemented

module test_real_literal;
  real a, b, c, d, e;

  initial begin
    a = 1.0;
    b = 3.14159;
    c = 2.5e10;     // Scientific notation
    d = 6.022e-23;  // Negative exponent
    e = .5;         // Leading decimal point
  end
endmodule
