// EXPECT=PASS
// Test basic case statement functionality

module case_basic(
  input wire [2:0] sel,
  input wire [7:0] data,
  output reg [7:0] out
);

  always @(*) begin
    case (sel)
      3'b000: out = 8'h00;
      3'b001: out = 8'hAA;
      3'b010: out = 8'h55;
      3'b011: out = data;
      3'b100: out = ~data;
      3'b101: out = data + 8'h01;
      default: out = 8'hFF;
    endcase
  end

endmodule
