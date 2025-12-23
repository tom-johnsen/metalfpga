module case_test(input [1:0] sel, output reg [7:0] out);
  always @(*) case (sel)
    0: out = 8'h00;
    1: out = 8'hFF;
    default: out = 8'h55;
  endcase
endmodule
