// EXPECT=PASS
// Test 2D gate array with genvar-based row/column selects
module test_generate_gate_matrix;
    reg [3:0][3:0] matrix_in;
    wire [3:0][3:0] matrix_out;

    genvar row, col;

    // Generate 4x4 matrix of inverters
    generate
        for (row = 0; row < 4; row = row + 1) begin : row_gen
            for (col = 0; col < 4; col = col + 1) begin : col_gen
                not (matrix_out[row][col], matrix_in[row][col]);
            end
        end
    endgenerate

    initial begin
        // Checkerboard pattern
        matrix_in[0] = 4'b1010;
        matrix_in[1] = 4'b0101;
        matrix_in[2] = 4'b1010;
        matrix_in[3] = 4'b0101;

        #1 begin
            if (matrix_out[0] == 4'b0101 &&
                matrix_out[1] == 4'b1010 &&
                matrix_out[2] == 4'b0101 &&
                matrix_out[3] == 4'b1010)
                $display("PASS: 4x4 gate matrix with genvar selects");
            else
                $display("FAIL: matrix output incorrect");
        end

        $finish;
    end
endmodule
