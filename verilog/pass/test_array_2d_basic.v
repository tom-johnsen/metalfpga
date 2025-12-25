// Basic 2D array test - simple read/write
module array_2d_basic(
    input [3:0] row,
    input [3:0] col,
    input [7:0] data_in,
    output reg [7:0] data_out
);
    // 16x16 2D array
    reg [7:0] matrix [0:15][0:15];

    // Combinational read
    always @(*) begin
        data_out = matrix[row][col];
    end
endmodule

// Test with initial block to set values
module array_2d_init(
    input [3:0] row,
    input [3:0] col,
    output reg [7:0] data_out
);
    reg [7:0] grid [0:3][0:3];

    initial begin
        integer i, j;
        for (i = 0; i < 4; i = i + 1) begin
            for (j = 0; j < 4; j = j + 1) begin
                grid[i][j] = (i * 4) + j;
            end
        end
    end

    always @(*) begin
        data_out = grid[row][col];
    end
endmodule
