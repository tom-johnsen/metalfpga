// EXPECT=PASS
// Test: Implicit net declarations
// Feature: Undeclared wires get implicit wire type
// Expected: May fail - implicit net handling

module test_implicit_net;
  // implicit_wire is not declared but used in assign
  assign implicit_wire = 1'b1;

  // Used in module instantiation
  buf b1(implicit_buf_out, 1'b0);

  reg test;
  initial begin
    test = implicit_wire & implicit_buf_out;
  end
endmodule
