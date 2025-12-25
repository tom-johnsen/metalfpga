// Test: buf gate primitive with delay
// Feature: Gate-level primitives with delays
// Expected: Should fail - gate primitives not yet implemented

module test_gate_buf(input a, output y);
  buf #(2.5) g1(y, a);  // Buffer with 2.5 time unit delay
endmodule
