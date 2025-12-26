// Test: Genvar scoping rules
// Feature: Multiple genvars with same name in different scopes
// Expected: May fail - genvar scoping

module test_genvar_scope;
  genvar i;

  generate
    for (i = 0; i < 4; i = i + 1) begin : outer
      wire [7:0] data;
      genvar i;  // Different scope
      for (i = 0; i < 8; i = i + 1) begin : inner
        wire bit_val;
        assign bit_val = data[i];
      end
    end
  endgenerate
endmodule
