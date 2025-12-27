// EXPECT=PASS
// Test: Specify block with path delays
// Feature: Path delay specifications
// Expected: May fail - detailed specify semantics

module test_specify_path (
  input a, b,
  output y
);
  and g1(y, a, b);

  specify
    (a => y) = (1.5, 2.0);  // Rise, fall delays
    (b => y) = 1.8;
    $setup(a, posedge b, 0.5);
    $hold(posedge b, a, 0.3);
  endspecify
endmodule
