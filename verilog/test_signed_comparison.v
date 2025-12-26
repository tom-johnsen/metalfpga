// Test: Signed comparisons with edge cases
// Feature: Signed vs unsigned comparison behavior
// Expected: Should pass - testing signed comparison semantics

module test_signed_comparison;
  reg signed [7:0] a, b;
  reg [7:0] au, bu;
  wire lt_signed, lt_unsigned;
  wire gt_signed, gt_unsigned;

  assign lt_signed = a < b;
  assign lt_unsigned = au < bu;
  assign gt_signed = a > b;
  assign gt_unsigned = au > bu;

  initial begin
    a = -1; b = 1;      // -1 < 1 (signed)
    au = -1; bu = 1;    // 255 > 1 (unsigned)

    #10;
    a = -128; b = 127;  // Min vs max
    au = -128; bu = 127;
  end
endmodule
