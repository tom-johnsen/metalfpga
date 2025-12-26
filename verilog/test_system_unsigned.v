// Test: $unsigned system function
// Feature: Cast to unsigned interpretation
// Expected: Should fail - $unsigned not yet implemented

module test_system_unsigned;
  reg signed [7:0] signed_val;
  reg [7:0] unsigned_val;

  initial begin
    signed_val = -1;
    unsigned_val = $unsigned(signed_val);  // 255
  end
endmodule
