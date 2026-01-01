// EXPECT=PASS
// 2D convolution - 3x3 box blur filter
module convolution_box_blur(
    input [3:0] y,
    input [3:0] x,
    output reg [9:0] blurred  // 10 bits to hold sum of 9x 8-bit values
);
    reg [7:0] image [0:15][0:15];

    always @(*) begin
        integer dy, dx;
        reg [9:0] sum;
        sum = 0;

        // 3x3 box filter (skip edges for simplicity)
        if (y > 0 && y < 15 && x > 0 && x < 15) begin
            for (dy = -1; dy <= 1; dy = dy + 1) begin
                for (dx = -1; dx <= 1; dx = dx + 1) begin
                    sum = sum + image[y + dy][x + dx];
                end
            end
            blurred = sum / 9;  // Average of 9 pixels
        end else begin
            blurred = image[y][x];  // Edge pixels unchanged
        end
    end
endmodule

// 3x3 Sobel edge detection (horizontal gradient)
module convolution_sobel(
    input [3:0] y,
    input [3:0] x,
    output reg signed [15:0] gx  // Signed gradient
);
    reg [7:0] image [0:15][0:15];

    always @(*) begin
        reg signed [15:0] sum;
        sum = 0;

        if (y > 0 && y < 15 && x > 0 && x < 15) begin
            // Sobel Gx kernel:
            // -1  0  +1
            // -2  0  +2
            // -1  0  +1
            sum = sum - $signed({1'b0, image[y-1][x-1]});
            sum = sum + $signed({1'b0, image[y-1][x+1]});
            sum = sum - $signed({2'b0, image[y][x-1], 1'b0});  // *2
            sum = sum + $signed({2'b0, image[y][x+1], 1'b0});  // *2
            sum = sum - $signed({1'b0, image[y+1][x-1]});
            sum = sum + $signed({1'b0, image[y+1][x+1]});
            gx = sum;
        end else begin
            gx = 0;
        end
    end
endmodule
