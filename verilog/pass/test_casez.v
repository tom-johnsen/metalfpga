// Test casez statement - treats only Z and ? as don't-care (X must match exactly)

module casez_priority(
  input wire [7:0] request,
  output reg [2:0] grant
);

  always @(*) begin
    // casez treats Z and ? as don't-care, but X must match exactly
    casez (request)
      8'b1???????: grant = 3'd7;  // Highest priority
      8'b01??????: grant = 3'd6;
      8'b001?????: grant = 3'd5;
      8'b0001????: grant = 3'd4;
      8'b00001???: grant = 3'd3;
      8'b000001??: grant = 3'd2;
      8'b0000001?: grant = 3'd1;
      8'b00000001: grant = 3'd0;  // Lowest priority
      default:     grant = 3'd7;  // No request
    endcase
  end

endmodule
