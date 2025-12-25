// Genvar usage in bit ranges and indices

// Genvar in bit select
module gen_genvar_bitsel #(parameter WIDTH = 8) (
    input [WIDTH-1:0] data_in,
    output [WIDTH-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : bitwise
            // Using genvar directly in bit select
            assign data_out[i] = ~data_in[i];
        end
    endgenerate
endmodule

// Genvar in part-select (range)
module gen_genvar_partsel #(parameter NIBBLES = 4) (
    input [NIBBLES*4-1:0] data_in,
    output [NIBBLES*4-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < NIBBLES; i = i + 1) begin : nibble_proc
            // Using genvar in part-select range
            wire [3:0] nibble;
            assign nibble = data_in[i*4 +: 4];
            assign data_out[i*4 +: 4] = ~nibble;
        end
    endgenerate
endmodule

// Genvar in indexed part-select
module gen_genvar_indexed #(parameter COUNT = 8) (
    input [COUNT*8-1:0] data_in,
    output [COUNT*8-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < COUNT; i = i + 1) begin : byte_swap
            // Genvar used to compute base address
            localparam BASE = i * 8;
            assign data_out[BASE +: 8] = {data_in[BASE+0], data_in[BASE+1],
                                          data_in[BASE+2], data_in[BASE+3],
                                          data_in[BASE+4], data_in[BASE+5],
                                          data_in[BASE+6], data_in[BASE+7]};
        end
    endgenerate
endmodule

// Genvar in array index
module gen_genvar_array #(parameter SIZE = 8) (
    input [SIZE-1:0] wr_data,
    output reg [SIZE-1:0] rd_data
);
    reg [7:0] mem [0:SIZE-1];

    generate
        genvar i;
        for (i = 0; i < SIZE; i = i + 1) begin : init_mem
            initial begin
                // Using genvar as array index
                mem[i] = i * 2;
            end
        end
    endgenerate

    always @(*) begin
        rd_data = mem[0][SIZE-1:0];
    end
endmodule

// Genvar in multi-dimensional array index
module gen_genvar_array2d #(parameter ROWS = 4, parameter COLS = 4) (
    output reg [7:0] checksum
);
    reg [7:0] grid [0:ROWS-1][0:COLS-1];

    generate
        genvar i, j;
        for (i = 0; i < ROWS; i = i + 1) begin : row_init
            for (j = 0; j < COLS; j = j + 1) begin : col_init
                initial begin
                    // Genvar used in 2D array indexing
                    grid[i][j] = (i * COLS) + j;
                end
            end
        end
    endgenerate

    always @(*) begin
        checksum = 8'h00;
    end
endmodule

// Genvar in slice with arithmetic
module gen_genvar_arithmetic #(parameter CHUNKS = 4, parameter CHUNK_SIZE = 8) (
    input [CHUNKS*CHUNK_SIZE-1:0] data_in,
    output [CHUNKS*CHUNK_SIZE-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < CHUNKS; i = i + 1) begin : chunk_process
            // Complex genvar arithmetic in ranges
            localparam START_BIT = (CHUNKS - 1 - i) * CHUNK_SIZE;
            localparam END_BIT = START_BIT + CHUNK_SIZE - 1;

            wire [CHUNK_SIZE-1:0] chunk;
            assign chunk = data_in[START_BIT +: CHUNK_SIZE];
            assign data_out[i*CHUNK_SIZE +: CHUNK_SIZE] = chunk;
        end
    endgenerate
endmodule

// Genvar in conditional range selection
module gen_genvar_cond_range #(parameter WIDTH = 16) (
    input [WIDTH-1:0] data_in,
    output [WIDTH-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : bit_cond
            if (i < WIDTH/2) begin : lower
                // Genvar in range, first half
                assign data_out[i] = data_in[i];
            end else begin : upper
                // Genvar in range, second half with offset
                assign data_out[i] = data_in[WIDTH - 1 - i];
            end
        end
    endgenerate
endmodule
