// EXPECT=PASS
// Test: `celldefine and `endcelldefine directives
// Feature: Mark module as a cell for library purposes
// Expected: May pass - directives might be ignored

`celldefine
module library_cell (output out, input in);
  assign out = ~in;
endmodule
`endcelldefine

module test_celldefine;
  wire w;
  reg r;

  library_cell u1 (.out(w), .in(r));
endmodule
