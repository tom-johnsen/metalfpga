// EXPECT=PASS
// Test: Real array bounds checking with variable indices
// Feature: Off-by-one, X-guard, and edge case handling
// Expected: Out of bounds should return 0.0, valid indices work correctly

module test_real_array_bounds;
  real data [0:7];      // Array with 8 elements
  real matrix [0:2][0:3];  // 3x4 matrix
  real result;
  integer idx;
  integer i;
  integer j;

  initial begin
    // Initialize array
    data[0] = 1.5;
    data[1] = 2.5;
    data[2] = 3.5;
    data[3] = 4.5;
    data[4] = 5.5;
    data[5] = 6.5;
    data[6] = 7.5;
    data[7] = 8.5;

    // Valid boundary cases
    idx = 0;
    result = data[idx];  // First element: 1.5

    idx = 7;
    result = data[idx];  // Last element: 8.5

    // Off-by-one tests
    idx = 8;
    result = data[idx];  // Out of bounds (high)

    idx = -1;
    result = data[idx];  // Out of bounds (negative)

    // Variable index edge cases
    idx = 3;
    result = data[idx];  // Middle element: 4.5

    idx = 100;
    result = data[idx];  // Way out of bounds

    // 2D array bounds
    matrix[0][0] = 1.1;
    matrix[0][3] = 1.4;
    matrix[2][0] = 3.1;
    matrix[2][3] = 3.4;

    // Valid 2D access
    result = matrix[0][0];  // 1.1
    result = matrix[2][3];  // 3.4

    // 2D out of bounds
    result = matrix[0][4];  // Column out of bounds
    result = matrix[3][0];  // Row out of bounds
    result = matrix[3][4];  // Both out of bounds

    // Variable indices in 2D
    for (i = 0; i < 4; i = i + 1) begin
      for (j = 0; j < 5; j = j + 1) begin
        result = matrix[i][j];  // Some valid, some out of bounds
      end
    end

    // Negative indices in 2D
    result = matrix[-1][0];
    result = matrix[0][-1];
    result = matrix[-1][-1];
  end
endmodule
