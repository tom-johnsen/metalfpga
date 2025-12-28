// EXPECT=PASS
// Test: User-defined primitive (UDP)
// Feature: Primitive definition

primitive mux2to1(output out, input a, input b, input sel);
  table
    // a b sel : out
       0 ? 0   : 0;
       1 ? 0   : 1;
       ? 0 1   : 0;
       ? 1 1   : 1;
  endtable
endprimitive

module test_udp(input a, input b, input sel, output y);
  mux2to1 m1(y, a, b, sel);
endmodule
