// EXPECT=PASS
// 3D array test - tensor/cube storage
module array_3d_basic(
    input [2:0] z,
    input [2:0] y,
    input [2:0] x,
    output reg [7:0] data_out
);
    // 8x8x8 3D tensor
    reg [7:0] cube [0:7][0:7][0:7];

    always @(*) begin
        data_out = cube[z][y][x];
    end
endmodule

// 3D array with initialization - create a pattern
module array_3d_pattern(
    input [1:0] z,
    input [1:0] y,
    input [1:0] x,
    output reg [7:0] data_out
);
    reg [7:0] tensor [0:3][0:3][0:3];

    initial begin
        integer iz, iy, ix;
        for (iz = 0; iz < 4; iz = iz + 1) begin
            for (iy = 0; iy < 4; iy = iy + 1) begin
                for (ix = 0; ix < 4; ix = ix + 1) begin
                    // Each position gets unique value based on coords
                    tensor[iz][iy][ix] = (iz * 16) + (iy * 4) + ix;
                end
            end
        end
    end

    always @(*) begin
        data_out = tensor[z][y][x];
    end
endmodule
