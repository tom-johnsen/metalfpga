// Test: Deeply nested ternary with mixed types
// Feature: Complex nested ternary operator expressions
// Expected: Should pass - stress test for ternary handling

module test_ternary_nested_complex;
  reg [7:0] a, b, c, d, e;
  reg [2:0] sel;
  wire [7:0] result;

  assign result = sel[2] ?
                    (sel[1] ?
                      (sel[0] ? a : b) :
                      (sel[0] ? c : d)) :
                    (sel[1] ?
                      (sel[0] ? e : a) :
                      (sel[0] ? b : c));

  initial begin
    a = 8'h11; b = 8'h22; c = 8'h33; d = 8'h44; e = 8'h55;
    sel = 3'b000;
    #10 sel = 3'b111;
    #10 sel = 3'b101;
  end
endmodule
