// EXPECT=PASS
// Test: Real with signed integer operations
// Feature: Mixed real and signed/unsigned integer arithmetic

module test_real_signed;
  real r;
  integer signed_int;
  reg signed [7:0] signed_byte;
  reg [7:0] unsigned_byte;

  initial begin
    signed_int = -10;
    signed_byte = -5;
    unsigned_byte = 250;  // Would be -6 if signed

    // Signed integer to real
    r = signed_int;  // -10.0
    r = signed_byte;  // -5.0

    // Unsigned to real
    r = unsigned_byte;  // 250.0 (not -6.0)

    // Real with signed arithmetic
    r = 3.5 + signed_int;  // -6.5
    r = 3.5 * signed_int;  // -35.0

    // Real to signed conversion
    r = -3.7;
    signed_int = r;  // -3 (truncate toward zero)

    r = 3.7;
    signed_int = r;  // 3

    // Mixed comparisons
    if (r > signed_int) begin
      r = 1.0;
    end

    // Edge case: -0.0
    r = -0.0;
    signed_int = r;  // 0
  end
endmodule
