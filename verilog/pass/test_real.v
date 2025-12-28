// EXPECT=PASS
// Test: real data type
// Feature: Floating-point numbers

module test_real;
  real voltage;
  real current;
  real power;

  initial begin
    voltage = 3.3;
    current = 0.5;
    power = voltage * current;
  end
endmodule
