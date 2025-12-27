// EXPECT=PASS
// Generate with parameters - creating parameterized hardware

// Ripple carry adder using generate
module gen_ripple_adder #(parameter WIDTH = 8) (
    input [WIDTH-1:0] a,
    input [WIDTH-1:0] b,
    input cin,
    output [WIDTH-1:0] sum,
    output cout
);
    wire [WIDTH:0] carry;
    assign carry[0] = cin;
    assign cout = carry[WIDTH];

    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : add_stage
            assign sum[i] = a[i] ^ b[i] ^ carry[i];
            assign carry[i+1] = (a[i] & b[i]) | (a[i] & carry[i]) | (b[i] & carry[i]);
        end
    endgenerate
endmodule

// Parameterized mux using generate
module gen_mux #(
    parameter WIDTH = 8,
    parameter INPUTS = 4
) (
    input [WIDTH*INPUTS-1:0] data_in,
    input [$clog2(INPUTS)-1:0] sel,
    output reg [WIDTH-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < INPUTS; i = i + 1) begin : input_slice
            wire [WIDTH-1:0] input_data;
            assign input_data = data_in[i*WIDTH +: WIDTH];
        end
    endgenerate

    always @(*) begin
        data_out = data_in[sel*WIDTH +: WIDTH];
    end
endmodule

// Barrel shifter using generate
module gen_barrel_shift #(parameter WIDTH = 8) (
    input [WIDTH-1:0] data_in,
    input [$clog2(WIDTH)-1:0] shift_amt,
    output [WIDTH-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : shift_bit
            wire [$clog2(WIDTH)-1:0] src_idx;
            assign src_idx = (i + shift_amt) % WIDTH;
            // This would need more complex logic in real implementation
            assign data_out[i] = data_in[0]; // Simplified
        end
    endgenerate
endmodule

// Priority encoder using generate
module gen_priority #(parameter WIDTH = 8) (
    input [WIDTH-1:0] req,
    output reg [WIDTH-1:0] grant
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : priority_stage
            wire higher_priority;
            if (i == 0) begin : first
                assign higher_priority = 1'b0;
            end else begin : others
                assign higher_priority = |req[i-1:0];
            end
        end
    endgenerate

    always @(*) begin
        grant = 8'h00; // Simplified
    end
endmodule

// Parameterized register file
module gen_regfile #(
    parameter DEPTH = 8,
    parameter WIDTH = 8
) (
    input clk,
    input [$clog2(DEPTH)-1:0] wr_addr,
    input [WIDTH-1:0] wr_data,
    input wr_en,
    input [$clog2(DEPTH)-1:0] rd_addr,
    output [WIDTH-1:0] rd_data
);
    generate
        genvar i;
        for (i = 0; i < DEPTH; i = i + 1) begin : reg_gen
            reg [WIDTH-1:0] storage;

            always @(posedge clk) begin
                if (wr_en && wr_addr == i)
                    storage <= wr_data;
            end
        end
    endgenerate

    assign rd_data = 8'h00; // Simplified read logic
endmodule
