// EXPECT=PASS
// Test: notif tristate gates
// Feature: Inverting tristate buffers
// Expected: Should fail - tristate not yet implemented

module test_tristate_notif(
  input data,
  input enable,
  input enable_n,
  output y1,
  output y2
);
  notif1 not1(y1, data, enable);    // Inverting buffer with active-high enable
  notif0 not0(y2, data, enable_n);  // Inverting buffer with active-low enable
endmodule
