// Test: SystemVerilog unique case
// Feature: unique case statement
// Expected: Should fail - SystemVerilog construct

module test_unique_case;
  reg [1:0] sel;
  reg [7:0] out;

  always @* begin
    unique case (sel)
      2'b00: out = 8'h00;
      2'b01: out = 8'h11;
      2'b10: out = 8'h22;
      2'b11: out = 8'h33;
    endcase
  end
endmodule
