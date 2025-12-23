module memory(input [7:0] addr, data, output reg [7:0] out);
  reg [7:0] mem [0:255];
  always @(*) out = mem[addr];
endmodule
