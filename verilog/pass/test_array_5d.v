// EXPECT=PASS
// 5D array test - batched video processing
// Dimensions: [batch][frame][channel][row][col]
module array_5d_video(
    input [1:0] batch,   // 4 batches
    input [1:0] frame,   // 4 frames per batch
    input [1:0] channel, // 4 channels (RGBA)
    input [2:0] row,     // 8 rows
    input [2:0] col,     // 8 cols
    output reg [7:0] pixel
);
    // 5D tensor: 4x4x4x8x8 = 4096 elements
    reg [7:0] video [0:3][0:3][0:3][0:7][0:7];

    always @(*) begin
        pixel = video[batch][frame][channel][row][col];
    end
endmodule

// 5D array with initialization pattern
module array_5d_pattern(
    input [1:0] w, z, y, x, t,
    output reg [15:0] data_out
);
    // Smaller 5D array: 4x4x4x4x4 = 1024 elements
    reg [15:0] hyper [0:3][0:3][0:3][0:3][0:3];

    initial begin
        integer iw, iz, iy, ix, it;
        for (iw = 0; iw < 4; iw = iw + 1) begin
            for (iz = 0; iz < 4; iz = iz + 1) begin
                for (iy = 0; iy < 4; iy = iy + 1) begin
                    for (ix = 0; ix < 4; ix = ix + 1) begin
                        for (it = 0; it < 4; it = it + 1) begin
                            // Unique value based on all 5 coordinates
                            hyper[iw][iz][iy][ix][it] =
                                (iw * 256) + (iz * 64) + (iy * 16) + (ix * 4) + it;
                        end
                    end
                end
            end
        end
    end

    always @(*) begin
        data_out = hyper[w][z][y][x][t];
    end
endmodule

// 5D array with write operation (clocked)
module array_5d_write(
    input clk,
    input [1:0] wr_d4, wr_d3, wr_d2, wr_d1, wr_d0,
    input [7:0] wr_data,
    input wr_en,
    input [1:0] rd_d4, rd_d3, rd_d2, rd_d1, rd_d0,
    output reg [7:0] rd_data
);
    reg [7:0] tensor [0:3][0:3][0:3][0:3][0:3];

    // Sequential write
    always @(posedge clk) begin
        if (wr_en) begin
            tensor[wr_d4][wr_d3][wr_d2][wr_d1][wr_d0] <= wr_data;
        end
    end

    // Combinational read
    always @(*) begin
        rd_data = tensor[rd_d4][rd_d3][rd_d2][rd_d1][rd_d0];
    end
endmodule

// 5D convolution-like access pattern (read neighboring elements)
module array_5d_neighbor_sum(
    input [1:0] b, f, c, y, x,
    output reg [15:0] sum
);
    reg [7:0] data [0:3][0:3][0:3][0:3][0:3];

    always @(*) begin
        reg [15:0] total;
        total = 0;

        // Sum center + neighbors in last 2 dimensions (spatial)
        // Skip boundary checking for simplicity (assume valid coords)
        if (y > 0 && y < 3 && x > 0 && x < 3) begin
            total = total + data[b][f][c][y-1][x];    // top
            total = total + data[b][f][c][y+1][x];    // bottom
            total = total + data[b][f][c][y][x-1];    // left
            total = total + data[b][f][c][y][x+1];    // right
            total = total + data[b][f][c][y][x];      // center
        end else begin
            total = data[b][f][c][y][x];
        end

        sum = total;
    end
endmodule
