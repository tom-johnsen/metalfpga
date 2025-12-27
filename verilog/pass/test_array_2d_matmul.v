// EXPECT=PASS
// Matrix operations - multiply, transpose
module matrix_multiply_4x4(
    input [1:0] out_row,
    input [1:0] out_col,
    output reg [15:0] result
);
    // 4x4 matrices
    reg [7:0] matA [0:3][0:3];
    reg [7:0] matB [0:3][0:3];

    always @(*) begin
        integer k;
        reg [15:0] sum;
        sum = 0;

        // C[i][j] = sum(A[i][k] * B[k][j])
        for (k = 0; k < 4; k = k + 1) begin
            sum = sum + (matA[out_row][k] * matB[k][out_col]);
        end

        result = sum;
    end
endmodule

// Matrix transpose
module matrix_transpose(
    input [2:0] row,
    input [2:0] col,
    output reg [7:0] transposed
);
    reg [7:0] matrix [0:7][0:7];

    always @(*) begin
        // Transpose: swap row and col indices
        transposed = matrix[col][row];
    end
endmodule

// Small matrix multiply with initialization
module matrix_multiply_2x2_init(
    output reg [7:0] c00, c01, c10, c11
);
    reg [3:0] a [0:1][0:1];
    reg [3:0] b [0:1][0:1];

    initial begin
        // A = [[2, 3],
        //      [4, 5]]
        a[0][0] = 2;
        a[0][1] = 3;
        a[1][0] = 4;
        a[1][1] = 5;

        // B = [[1, 0],
        //      [0, 1]]  (identity)
        b[0][0] = 1;
        b[0][1] = 0;
        b[1][0] = 0;
        b[1][1] = 1;
    end

    always @(*) begin
        // C = A * B (should equal A since B is identity)
        c00 = (a[0][0] * b[0][0]) + (a[0][1] * b[1][0]);
        c01 = (a[0][0] * b[0][1]) + (a[0][1] * b[1][1]);
        c10 = (a[1][0] * b[0][0]) + (a[1][1] * b[1][0]);
        c11 = (a[1][0] * b[0][1]) + (a[1][1] * b[1][1]);
    end
endmodule
