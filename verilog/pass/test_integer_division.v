// EXPECT=PASS
// Test: Integer division and modulo semantics
// Feature: Division truncation, negative modulo
// Expected: Should pass - testing integer arithmetic

module test_integer_division;
  integer a, b, q, r;

  initial begin
    a = 17; b = 5;
    q = a / b;  // 3
    r = a % b;  // 2

    a = -17; b = 5;
    q = a / b;  // -3 (rounds toward zero)
    r = a % b;  // -2 (sign matches dividend)

    a = 17; b = -5;
    q = a / b;  // -3
    r = a % b;  // 2
  end
endmodule
