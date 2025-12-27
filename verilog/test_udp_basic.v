// EXPECT=PASS
// Test: User-Defined Primitive (UDP)
// Feature: Combinational UDP with truth table

primitive my_and (output out, input a, b);
  table
    // a  b  : out
       0  0  :  0;
       0  1  :  0;
       1  0  :  0;
       1  1  :  1;
  endtable
endprimitive

module test_udp_basic;
  wire out;
  reg a, b;

  my_and u1 (out, a, b);

  initial begin
    a = 0; b = 0;
    #10 a = 1;
    #10 b = 1;
  end
endmodule
