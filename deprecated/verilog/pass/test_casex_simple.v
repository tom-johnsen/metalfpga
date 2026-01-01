// EXPECT=PASS
// Test casex with X values (no wildcard syntax yet)

module casex_simple(
  input wire [3:0] opcode,
  output reg [1:0] result
);

  always @(*) begin
    casex (opcode)
      4'b0000: result = 2'b00;
      4'b0001: result = 2'b01;
      4'b0010: result = 2'b10;
      4'b0011: result = 2'b11;
      4'bxxxx: result = 2'b11;  // Match when opcode is X
      default: result = 2'b00;
    endcase
  end

endmodule
