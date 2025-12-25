// Nested generate blocks - loops within loops

// 2D generate - nested loops
module gen_nested_2d #(parameter ROWS = 4, parameter COLS = 4) (
    input [ROWS*COLS-1:0] data_in,
    output [ROWS*COLS-1:0] data_out
);
    generate
        genvar i, j;
        for (i = 0; i < ROWS; i = i + 1) begin : row
            for (j = 0; j < COLS; j = j + 1) begin : col
                wire bit_val;
                assign bit_val = data_in[i*COLS + j];
                assign data_out[i*COLS + j] = ~bit_val;
            end
        end
    endgenerate
endmodule

// Generate loop within generate if
module gen_if_loop #(parameter ENABLE = 1, parameter WIDTH = 8) (
    input [WIDTH-1:0] in,
    output [WIDTH-1:0] out
);
    generate
        if (ENABLE) begin : enabled
            genvar i;
            for (i = 0; i < WIDTH; i = i + 1) begin : bits
                assign out[i] = ~in[i];
            end
        end else begin : disabled
            assign out = in;
        end
    endgenerate
endmodule

// Generate if within generate loop
module gen_loop_if #(parameter WIDTH = 8) (
    input [WIDTH-1:0] in,
    output [WIDTH-1:0] out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : bit_proc
            if (i < WIDTH/2) begin : lower_half
                assign out[i] = in[i];
            end else begin : upper_half
                assign out[i] = ~in[i];
            end
        end
    endgenerate
endmodule

// Triple nested generate loops
module gen_nested_3d #(
    parameter DIM0 = 2,
    parameter DIM1 = 2,
    parameter DIM2 = 2
) (
    input [DIM0*DIM1*DIM2-1:0] in,
    output [DIM0*DIM1*DIM2-1:0] out
);
    generate
        genvar i, j, k;
        for (i = 0; i < DIM0; i = i + 1) begin : dim0
            for (j = 0; j < DIM1; j = j + 1) begin : dim1
                for (k = 0; k < DIM2; k = k + 1) begin : dim2
                    localparam INDEX = (i*DIM1*DIM2) + (j*DIM2) + k;
                    assign out[INDEX] = in[INDEX];
                end
            end
        end
    endgenerate
endmodule

// Multiple separate generate blocks in same module
module gen_multi_blocks(
    input [7:0] a,
    input [7:0] b,
    output [7:0] out1,
    output [7:0] out2
);
    // First generate block
    generate
        genvar i;
        for (i = 0; i < 8; i = i + 1) begin : proc_a
            assign out1[i] = a[i];
        end
    endgenerate

    // Second generate block
    generate
        genvar j;
        for (j = 0; j < 8; j = j + 1) begin : proc_b
            assign out2[j] = b[j];
        end
    endgenerate
endmodule
