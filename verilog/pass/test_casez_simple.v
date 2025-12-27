// EXPECT=PASS
// Test casez with Z values (no wildcard syntax yet)

module casez_simple(
  input wire [3:0] data,
  output reg [1:0] result
);

  always @(*) begin
    casez (data)
      4'b0000: result = 2'b00;
      4'b0001: result = 2'b01;
      4'b0010: result = 2'b10;
      4'bzzzz: result = 2'b11;  // Match when data is Z
      default: result = 2'b00;
    endcase
  end

endmodule
