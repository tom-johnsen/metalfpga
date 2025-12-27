// EXPECT=PASS
// Test: Real number parameters
// Feature: Real parameters and localparams

module test_real_parameter;
  // Basic real parameters
  parameter real PI = 3.14159265359;
  parameter real E = 2.71828182846;
  parameter real ZERO = 0.0;
  parameter real NEGATIVE = -42.5;

  // Scientific notation parameters
  parameter real PLANCK = 6.62607e-34;
  parameter real AVOGADRO = 6.022e23;
  parameter real TINY = 1.0e-100;

  // Computed parameters
  parameter real TWO_PI = 2.0 * PI;
  parameter real HALF_PI = PI / 2.0;
  parameter real PI_SQUARED = PI * PI;

  // Localparam
  localparam real GOLDEN_RATIO = 1.618033988749;
  localparam real SQRT_TWO = 1.41421356237;

  real result;

  initial begin
    // Use parameters
    result = PI;
    result = E;
    result = TWO_PI;
    result = PLANCK;
    result = AVOGADRO;
    result = GOLDEN_RATIO;

    // Compute with parameters
    result = PI * E;
    result = TWO_PI + HALF_PI;
    result = PLANCK * AVOGADRO;
  end
endmodule
