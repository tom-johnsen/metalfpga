// EXPECT=PASS
// Test: Multiple continuous assignments to same wire
// Feature: Multiple assign statements for wire resolution
// Expected: May fail - multi-driver continuous assigns

module test_continuous_assign_multi;
  wire w;
  reg a, b, c;

  // Multiple assigns to same wire - should be AND'ed/resolved
  assign w = a;
  assign w = b;
  assign w = c;

  initial begin
    a = 1; b = 1; c = 0;  // w should be 0
    #10 c = 1;             // w should be 1
  end
endmodule
