// EXPECT=PASS
// 2D array with sequential write (clock edge)
module array_2d_write_sync(
    input clk,
    input [3:0] wr_row,
    input [3:0] wr_col,
    input [7:0] wr_data,
    input wr_en,
    input [3:0] rd_row,
    input [3:0] rd_col,
    output reg [7:0] rd_data
);
    reg [7:0] storage [0:15][0:15];

    // Sequential write
    always @(posedge clk) begin
        if (wr_en) begin
            storage[wr_row][wr_col] <= wr_data;
        end
    end

    // Combinational read
    always @(*) begin
        rd_data = storage[rd_row][rd_col];
    end
endmodule

// 3D array write test
module array_3d_write(
    input clk,
    input [2:0] wr_z, wr_y, wr_x,
    input [7:0] wr_data,
    input wr_en,
    input [2:0] rd_z, rd_y, rd_x,
    output reg [7:0] rd_data
);
    reg [7:0] cube [0:7][0:7][0:7];

    always @(posedge clk) begin
        if (wr_en) begin
            cube[wr_z][wr_y][wr_x] <= wr_data;
        end
    end

    always @(*) begin
        rd_data = cube[rd_z][rd_y][rd_x];
    end
endmodule

// Test multiple simultaneous reads from 2D array
module array_2d_multi_read(
    input [2:0] addr_a_row, addr_a_col,
    input [2:0] addr_b_row, addr_b_col,
    input [2:0] addr_c_row, addr_c_col,
    output reg [7:0] data_a,
    output reg [7:0] data_b,
    output reg [7:0] data_c
);
    reg [7:0] memory [0:7][0:7];

    always @(*) begin
        data_a = memory[addr_a_row][addr_a_col];
        data_b = memory[addr_b_row][addr_b_col];
        data_c = memory[addr_c_row][addr_c_col];
    end
endmodule
