// Test: Conditional path delays in specify
// Feature: if() conditional path specifications
// Expected: May fail - conditional specify paths

module test_specify_conditional (
  input sel, a, b,
  output y
);
  assign y = sel ? a : b;

  specify
    if (sel) (a => y) = 1.0;
    if (!sel) (b => y) = 2.0;
  endspecify
endmodule
