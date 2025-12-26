// Test: Case statement edge cases
// Feature: Multiple defaults, overlapping cases
// Expected: May fail - illegal but tests error handling

module test_case_default_multiple;
  reg [1:0] sel;
  reg [7:0] out;

  always @* begin
    case (sel)
      2'b00: out = 8'h00;
      2'b01: out = 8'h11;
      2'b01: out = 8'h22;  // Duplicate case (should be error or warning)
      default: out = 8'hFF;
    endcase
  end
endmodule
