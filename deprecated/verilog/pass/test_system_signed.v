// EXPECT=PASS
// Test: $signed system function
// Feature: Cast to signed interpretation
// Expected: Should fail - $signed not yet implemented

module test_system_signed;
  reg [7:0] unsigned_val;
  reg signed [7:0] signed_val;

  initial begin
    unsigned_val = 8'hFF;  // 255 unsigned
    signed_val = $signed(unsigned_val);  // -1 signed
  end
endmodule
