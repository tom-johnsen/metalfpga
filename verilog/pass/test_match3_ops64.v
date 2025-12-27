// EXPECT=PASS
// Test: Match-3 operators on >32-bit widths
// Feature: ===, !==, ==?, !=? with 64-bit path
// Expected: Should pass with --4state

module test_match3_ops64(
  input [39:0] a,
  input [39:0] b,
  output y_eq,
  output y_ne,
  output y_wild,
  output y_nwild
);
  assign y_eq = (a === b);
  assign y_ne = (a !== b);
  assign y_wild = (a ==? 40'b1010_zxxx_0000_1111_0000_zzzz_1010_1xxx_1100_zzzz);
  assign y_nwild = (a !=? 40'b1010_zxxx_0000_1111_0000_zzzz_1010_1xxx_1100_zzzz);
endmodule
