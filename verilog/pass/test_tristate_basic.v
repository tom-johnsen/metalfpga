// Test: Basic tristate buffer
// Feature: Tristate logic with bufif primitives
// Expected: Should fail - tristate not yet implemented

module test_tristate_basic(
  input data,
  input enable,
  output y
);
  bufif1 buf1(y, data, enable);  // Buffer with active-high enable
endmodule
