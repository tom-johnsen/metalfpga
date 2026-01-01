// EXPECT=PASS
// Test: Case statement with X and Z values
// Feature: casex and casez with explicit X/Z testing
// Expected: Should pass - testing case X/Z handling

module test_case_x_z;
  reg [3:0] data;
  reg [7:0] result;

  always @* begin
    casez (data)
      4'b1???: result = 8'h80;  // Match if MSB is 1
      4'b01??: result = 8'h40;
      4'b001?: result = 8'h20;
      4'b0001: result = 8'h10;
      default: result = 8'h00;
    endcase
  end

  initial begin
    data = 4'b1010;  // Should match first
    #10 data = 4'b01zx;  // Should match second with casez
  end
endmodule
