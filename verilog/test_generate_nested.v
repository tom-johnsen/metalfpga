// EXPECT=PASS
// Test nested generate loops
module test_generate_nested;
    wire [3:0][3:0] grid_out;
    reg [3:0][3:0] grid_in;

    // Generate 4x4 grid of inverters using nested loops
    genvar row, col;
    generate
        for (row = 0; row < 4; row = row + 1) begin : row_gen
            for (col = 0; col < 4; col = col + 1) begin : col_gen
                not (grid_out[row][col], grid_in[row][col]);
            end
        end
    endgenerate

    initial begin
        // Initialize grid with checkerboard pattern
        grid_in[0] = 4'b1010;
        grid_in[1] = 4'b0101;
        grid_in[2] = 4'b1010;
        grid_in[3] = 4'b0101;

        #1 begin
            if (grid_out[0] == 4'b0101 &&
                grid_out[1] == 4'b1010 &&
                grid_out[2] == 4'b0101 &&
                grid_out[3] == 4'b1010)
                $display("PASS: Nested generate created 4x4 inverter grid");
            else
                $display("FAIL: grid_out incorrect");
        end

        $finish;
    end
endmodule
