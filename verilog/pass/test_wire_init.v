module wire_init(input [7:0] a, output [7:0] b);
  wire [7:0] temp = a + 1;
  assign b = temp;
endmodule
