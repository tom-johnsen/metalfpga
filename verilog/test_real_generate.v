// EXPECT=PASS
// Test: Real parameters in generate/compile-time contexts
// Feature: Real const-expr in generate loop bounds and array sizes
// Expected: Real parameters should be converted to integers at compile time

module test_real_generate;
  // Real parameters used in compile-time contexts
  parameter real SIZE_REAL = 4.0;
  parameter real COUNT = 8.5;  // Should truncate to 8
  parameter integer SIZE = $rtoi(SIZE_REAL);

  // Array sized with real-derived parameter
  reg [7:0] data [0:SIZE-1];

  // Generate blocks with real parameters
  genvar i;
  generate
    for (i = 0; i < SIZE; i = i + 1) begin : gen_loop
      wire [7:0] item;
      assign item = data[i];
    end
  endgenerate

  // Conditional generate with real comparison
  parameter real THRESHOLD = 3.5;
  generate
    if (THRESHOLD > 3.0) begin : large_config
      reg enable;
    end else begin : small_config
      reg disable;
    end
  endgenerate

  initial begin
    // Use real parameters in array initialization
    for (integer j = 0; j < SIZE; j = j + 1) begin
      data[j] = j * 2;
    end
  end
endmodule
