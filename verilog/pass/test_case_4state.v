// Test case statements with 4-state values (X and Z)
// This should show how X propagates through case matching

module case_4state(
  input wire [2:0] sel,
  output reg [7:0] out1,  // Using case (exact match)
  output reg [7:0] out2,  // Using casex (X as wildcard)
  output reg [7:0] out3   // Using casez (Z as wildcard)
);

  always @(*) begin
    // Standard case: X in sel should only match if case item has X
    case (sel)
      3'b000: out1 = 8'h00;
      3'b001: out1 = 8'h11;
      3'bxxx: out1 = 8'hXX;  // Only matches if sel is exactly xxx
      default: out1 = 8'hFF;
    endcase
  end

  always @(*) begin
    // casex: X treated as don't-care
    casex (sel)
      3'b000: out2 = 8'h00;
      3'b001: out2 = 8'h11;
      3'b0xx: out2 = 8'h22;  // Matches 0xx, 00x, 0x0, etc.
      default: out2 = 8'hFF;
    endcase
  end

  always @(*) begin
    // casez: Z and ? as don't-care, X must match
    casez (sel)
      3'b000: out3 = 8'h00;
      3'b001: out3 = 8'h11;
      3'b0??: out3 = 8'h33;  // Matches 0zz, 0??, etc., but NOT 0xx
      3'bxxx: out3 = 8'hXX;  // Matches only xxx
      default: out3 = 8'hFF;
    endcase
  end

endmodule
