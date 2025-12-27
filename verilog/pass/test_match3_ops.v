// EXPECT=PASS
// Test: Match-3 operators (===, !==, ==?, !=?)
// Feature: Case equality and wildcard equality
// Expected: Should pass with --4state

module test_match3_ops(
  input [3:0] a,
  input [3:0] b,
  output y_eq,
  output y_ne,
  output y_wild,
  output y_nwild
);
  assign y_eq = (a === b);
  assign y_ne = (a !== b);
  assign y_wild = (a ==? 4'b10x1);
  assign y_nwild = (a !=? 4'b10x1);
endmodule
