// EXPECT=PASS
// Complex generate combinations - stress testing

// Generate creating reg arrays with initial values
module gen_reg_array #(parameter SIZE = 4, parameter WIDTH = 8) (
    output reg [WIDTH-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < SIZE; i = i + 1) begin : regs
            reg [WIDTH-1:0] storage;

            initial begin
                storage = i;
            end
        end
    endgenerate

    always @(*) begin
        data_out = 8'h00;
    end
endmodule

// Generate with localparam calculations
module gen_localparam #(parameter BASE_WIDTH = 4) (
    input [BASE_WIDTH*4-1:0] data_in,
    output [BASE_WIDTH*4-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < 4; i = i + 1) begin : stage
            localparam OFFSET = i * BASE_WIDTH;
            localparam NEXT_OFFSET = (i + 1) * BASE_WIDTH;

            wire [BASE_WIDTH-1:0] slice;
            assign slice = data_in[OFFSET +: BASE_WIDTH];
            assign data_out[OFFSET +: BASE_WIDTH] = ~slice;
        end
    endgenerate
endmodule

// Generate creating hierarchical named blocks
module gen_hierarchy #(parameter STAGES = 3) (
    input [7:0] in,
    output [7:0] out
);
    generate
        genvar i;
        for (i = 0; i < STAGES; i = i + 1) begin : pipeline_stage
            if (i == 0) begin : first_stage
                wire [7:0] stage_out;
                assign stage_out = in;
            end else if (i == STAGES-1) begin : last_stage
                wire [7:0] stage_out;
                assign stage_out = in;
            end else begin : middle_stage
                wire [7:0] stage_out;
                assign stage_out = in;
            end
        end
    endgenerate

    assign out = in;
endmodule

// Generate with mixed always blocks
module gen_mixed_blocks #(parameter COUNT = 4) (
    input clk,
    input [COUNT-1:0] data_in,
    output reg [COUNT-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < COUNT; i = i + 1) begin : proc
            reg storage;

            always @(posedge clk) begin
                storage <= data_in[i];
            end

            always @(*) begin
                data_out[i] = storage;
            end
        end
    endgenerate
endmodule

// Extreme nesting - if/loop/if/loop
module gen_extreme_nest #(
    parameter ENABLE_OUTER = 1,
    parameter OUTER_COUNT = 2,
    parameter ENABLE_INNER = 1,
    parameter INNER_COUNT = 2
) (
    input [OUTER_COUNT*INNER_COUNT-1:0] data_in,
    output [OUTER_COUNT*INNER_COUNT-1:0] data_out
);
    generate
        if (ENABLE_OUTER) begin : outer_enabled
            genvar i;
            for (i = 0; i < OUTER_COUNT; i = i + 1) begin : outer_loop
                if (ENABLE_INNER) begin : inner_enabled
                    genvar j;
                    for (j = 0; j < INNER_COUNT; j = j + 1) begin : inner_loop
                        localparam IDX = i * INNER_COUNT + j;
                        assign data_out[IDX] = ~data_in[IDX];
                    end
                end else begin : inner_disabled
                    genvar j;
                    for (j = 0; j < INNER_COUNT; j = j + 1) begin : inner_loop_alt
                        localparam IDX = i * INNER_COUNT + j;
                        assign data_out[IDX] = data_in[IDX];
                    end
                end
            end
        end else begin : outer_disabled
            assign data_out = data_in;
        end
    endgenerate
endmodule

// Generate with case-like parameter selection
module gen_param_case #(parameter MODE = 0, parameter WIDTH = 8) (
    input [WIDTH-1:0] in,
    output [WIDTH-1:0] out
);
    generate
        if (MODE == 0) begin : passthrough
            assign out = in;
        end else if (MODE == 1) begin : invert
            genvar i;
            for (i = 0; i < WIDTH; i = i + 1) begin : bits
                assign out[i] = ~in[i];
            end
        end else if (MODE == 2) begin : shift
            assign out = {in[WIDTH-2:0], in[WIDTH-1]};
        end else begin : zero
            assign out = {WIDTH{1'b0}};
        end
    endgenerate
endmodule
