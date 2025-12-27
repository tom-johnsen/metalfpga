// EXPECT=PASS
// Test: Real number arrays
// Feature: Arrays and memories of real type

module test_real_array;
  // 1D real arrays
  real coefficients [0:3];
  real samples [7:0];

  // 2D real array
  real matrix [0:2][0:2];

  real temp;
  integer i, j;

  initial begin
    // Initialize 1D array
    coefficients[0] = 1.0;
    coefficients[1] = 2.5;
    coefficients[2] = -3.7;
    coefficients[3] = 0.5;

    // Read from array
    temp = coefficients[0];
    temp = coefficients[3];

    // Array computation
    temp = coefficients[0] + coefficients[1];
    temp = coefficients[2] * coefficients[3];

    // Loop with array
    for (i = 0; i < 8; i = i + 1) begin
      samples[i] = i * 0.5;
    end

    // Read back
    temp = samples[5];  // 2.5

    // 2D array operations
    matrix[0][0] = 1.0;
    matrix[0][1] = 0.0;
    matrix[0][2] = 0.0;
    matrix[1][0] = 0.0;
    matrix[1][1] = 1.0;
    matrix[1][2] = 0.0;
    matrix[2][0] = 0.0;
    matrix[2][1] = 0.0;
    matrix[2][2] = 1.0;

    // Read 2D
    temp = matrix[1][1];  // 1.0
    temp = matrix[0][2];  // 0.0

    // Double loop
    for (i = 0; i < 3; i = i + 1) begin
      for (j = 0; j < 3; j = j + 1) begin
        temp = matrix[i][j] * 2.0;
      end
    end
  end
endmodule
