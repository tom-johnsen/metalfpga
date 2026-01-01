// EXPECT=PASS
// Test: Boolean to real type conversion
// Feature: Real number receives boolean comparison result

module test_real_bool_assign;
  // Parameter cases
  parameter real R1 = (3.0 < 4.0);   // true → 1.0
  parameter real R2 = (5.0 > 6.0);   // false → 0.0
  parameter real R3 = (2.5 == 2.5);  // true → 1.0
  parameter real R4 = (1.0 != 1.0);  // false → 0.0

  // Runtime cases
  real voltage;
  real current;
  real result;

  initial begin
    voltage = 3.3;
    current = 0.5;

    // Direct boolean to real assignment
    result = (voltage > current);  // true → 1.0
    result = (voltage < current);  // false → 0.0

    // Complex expressions
    result = (voltage > 3.0) && (current < 1.0);  // true && true → 1.0
    result = (voltage == 3.3) || (current == 0.0); // true || false → 1.0

    // Ternary with boolean
    result = (voltage > current) ? 10.5 : 0.0;

    // Nested comparison
    result = ((voltage * 2.0) > (current * 10.0));  // 6.6 > 5.0 → 1.0
  end
endmodule
