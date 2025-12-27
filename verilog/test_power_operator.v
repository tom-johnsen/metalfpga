// EXPECT=PASS
// Test: Power operator **
// Feature: Exponentiation operator

module test_power_operator;
  reg [15:0] base, exponent, result;

  initial begin
    base = 2;
    exponent = 8;
    result = base ** exponent;  // 2^8 = 256
  end
endmodule
