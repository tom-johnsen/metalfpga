// Test: Edge-sensitive UDP
// Feature: UDP with edge detection
// Expected: Should fail - UDP not yet implemented

primitive d_ff (output reg q, input d, clk);
  table
    // d  clk : q : q+
       0  (01): ? :  0;  // Rising edge
       1  (01): ? :  1;
       ?  (0x): ? :  -;  // Potential rising edge
       ?  (?0): ? :  -;  // Falling edge - hold
       ?  (1x): ? :  -;  // Potential falling edge
  endtable
endprimitive

module test_udp_edge;
  wire q;
  reg d, clk;

  d_ff u1 (q, d, clk);

  initial begin
    clk = 0;
    d = 0;

    forever #5 clk = ~clk;
  end

  initial begin
    #3 d = 1;
    #20 d = 0;
  end
endmodule
