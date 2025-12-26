// Test: Array indexing edge cases
// Feature: Out of bounds, negative indices, variable indices
// Expected: May fail - bounds checking behavior

module test_array_bounds;
  reg [7:0] mem [0:15];
  reg [7:0] data;
  integer idx;

  initial begin
    // Normal access
    mem[0] = 8'h00;
    mem[15] = 8'hFF;

    // Variable index
    idx = 5;
    mem[idx] = 8'h55;

    // Edge case: what happens with out of bounds?
    idx = 16;  // Out of bounds
    data = mem[idx];  // Implementation defined behavior

    idx = -1;  // Negative index
    data = mem[idx];  // Implementation defined behavior
  end
endmodule
