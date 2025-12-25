// Test multi-dimensional arrays (if supported)
module multi_dim_array(
    input clk,
    input [1:0] row,
    input [1:0] col,
    input [7:0] wr_data,
    input wr_en,
    output reg [7:0] rd_data
);
    // 4x4 array of bytes
    reg [7:0] matrix [0:3][0:3];

    always @(posedge clk) begin
        if (wr_en)
            matrix[row][col] <= wr_data;
        rd_data <= matrix[row][col];
    end
endmodule
