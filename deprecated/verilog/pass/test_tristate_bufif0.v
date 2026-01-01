// EXPECT=PASS
// Test: bufif0 tristate buffer
// Feature: Tristate logic with active-low enable
// Expected: Should fail - tristate not yet implemented

module test_tristate_bufif0(
  input data,
  input enable_n,
  output y
);
  bufif0 buf0(y, data, enable_n);  // Buffer with active-low enable
endmodule
