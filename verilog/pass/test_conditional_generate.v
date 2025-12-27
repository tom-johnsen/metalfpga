// Test: Generate if/else with parameters
// Feature: Conditional compilation based on parameters
// Expected: May fail - complex generate conditionals

module test_conditional_generate;
  parameter USE_FAST = 1;
  parameter WIDTH = 8;

  generate
    if (USE_FAST) begin : fast_impl
      wire [WIDTH-1:0] result;
      assign result = WIDTH'hFF;
    end else begin : slow_impl
      reg [WIDTH-1:0] result;
      always @* result = WIDTH'h00;
    end
  endgenerate

  wire [WIDTH-1:0] out;
  assign out = USE_FAST ? fast_impl.result : slow_impl.result;
endmodule
