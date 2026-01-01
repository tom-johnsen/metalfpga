// EXPECT=PASS
// SystemVerilog variant: loop variable declarations in for statements.
module test_real_array_bounds_sv;
  real data [0:7];
  real matrix [0:2][0:3];
  real result;
  integer idx;

  initial begin
    data[0] = 1.5;
    data[1] = 2.5;
    data[2] = 3.5;
    data[3] = 4.5;
    data[4] = 5.5;
    data[5] = 6.5;
    data[6] = 7.5;
    data[7] = 8.5;

    idx = 0;
    result = data[idx];

    idx = 7;
    result = data[idx];

    idx = 8;
    result = data[idx];

    idx = -1;
    result = data[idx];

    idx = 3;
    result = data[idx];

    idx = 100;
    result = data[idx];

    matrix[0][0] = 1.1;
    matrix[0][3] = 1.4;
    matrix[2][0] = 3.1;
    matrix[2][3] = 3.4;

    result = matrix[0][0];
    result = matrix[2][3];

    result = matrix[0][4];
    result = matrix[3][0];
    result = matrix[3][4];

    for (int i = 0; i < 4; i = i + 1) begin
      for (int j = 0; j < 5; j = j + 1) begin
        result = matrix[i][j];
      end
    end

    result = matrix[-1][0];
    result = matrix[0][-1];
    result = matrix[-1][-1];
  end
endmodule
