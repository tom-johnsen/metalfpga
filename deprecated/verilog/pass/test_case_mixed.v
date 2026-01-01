// EXPECT=PASS
// Test mixing case/casex/casez with various features

module case_mixed(
  input wire [3:0] a,
  input wire [3:0] b,
  input wire [1:0] mode,
  output reg [7:0] result
);

  wire [3:0] sum = a + b;

  always @(*) begin
    case (mode)
      2'b00: begin
        // Nested case inside case
        case (a[1:0])
          2'b00: result = 8'h00;
          2'b01: result = 8'h11;
          2'b10: result = 8'h22;
          2'b11: result = 8'h33;
        endcase
      end

      2'b01: begin
        // casex with multiple statements per branch
        casex (b)
          4'b00??: result = {4'h0, b};
          4'b01??: result = {4'h1, b};
          4'b10??: result = {4'h2, b};
          4'b11??: result = {4'h3, b};
        endcase
      end

      2'b10: begin
        // casez with expressions
        casez (sum)
          4'b1???: result = 8'hAA;  // sum >= 8
          4'b01??: result = 8'h55;  // sum >= 4 && sum < 8
          default: result = 8'h00;  // sum < 4
        endcase
      end

      default: result = 8'hFF;
    endcase
  end

endmodule
