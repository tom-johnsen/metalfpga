// EXPECT=PASS
// Test: Sequential UDP (with state)
// Feature: UDP with current state column

primitive d_latch (output reg q, input d, enable);
  table
    // d  enable : q : q+
       0    1    : ? :  0;
       1    1    : ? :  1;
       ?    0    : ? :  -;  // Hold state
  endtable
endprimitive

module test_udp_sequential;
  wire q;
  reg d, en;

  d_latch u1 (q, d, en);

  initial begin
    en = 0; d = 0;
    #10 en = 1; d = 1;
    #10 en = 0;
    #10 d = 0;  // Should not affect q while en=0
  end
endmodule
