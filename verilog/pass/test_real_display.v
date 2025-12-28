// EXPECT=PASS
// Test: Real number display formatting
// Feature: $display with %f, %e, %g format specifiers

module test_real_display;
  real voltage;
  real current;
  real power;
  real tiny;
  real huge;

  initial begin
    voltage = 3.3;
    current = 0.5;
    power = voltage * current;
    tiny = 1.23e-10;
    huge = 6.022e23;

    // Basic %f formatting
    $display("Voltage: %f V", voltage);
    $display("Current: %f A", current);
    $display("Power: %f W", power);

    // Scientific notation %e
    $display("Tiny: %e", tiny);
    $display("Huge: %e", huge);

    // General format %g (auto-select)
    $display("Auto format: %g", voltage);
    $display("Auto format: %g", tiny);

    // Multiple reals
    $display("V=%f A=%f W=%f", voltage, current, power);

    // Mixed integer and real
    $display("Count: %d, Value: %f", 42, voltage);

    // Width and precision
    $display("Formatted: %10.2f", voltage);
    $display("Formatted: %12.6e", tiny);
  end
endmodule
