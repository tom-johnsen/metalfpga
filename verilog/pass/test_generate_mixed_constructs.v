// Generate blocks with mixed constructs: assigns, instances, always blocks

// Helper modules for instantiation tests
module inverter(input a, output b);
    assign b = ~a;
endmodule

module dff(input clk, input d, output reg q);
    always @(posedge clk) begin
        q <= d;
    end
endmodule

module full_adder(input a, input b, input cin, output sum, output cout);
    assign sum = a ^ b ^ cin;
    assign cout = (a & b) | (a & cin) | (b & cin);
endmodule

// Generate with assigns only
module gen_assigns_only #(parameter WIDTH = 8) (
    input [WIDTH-1:0] in,
    output [WIDTH-1:0] out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : assign_bits
            assign out[i] = ~in[i];
        end
    endgenerate
endmodule

// Generate with module instances
module gen_instances #(parameter WIDTH = 8) (
    input [WIDTH-1:0] in,
    output [WIDTH-1:0] out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : inv_array
            inverter u_inv (
                .a(in[i]),
                .b(out[i])
            );
        end
    endgenerate
endmodule

// Generate with always blocks
module gen_always_only #(parameter WIDTH = 8) (
    input clk,
    input [WIDTH-1:0] data_in,
    output reg [WIDTH-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : flop_array
            always @(posedge clk) begin
                data_out[i] <= data_in[i];
            end
        end
    endgenerate
endmodule

// Generate mixing assigns and instances
module gen_assign_instance #(parameter WIDTH = 4) (
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
            // Mix: assign + instance
            full_adder u_fa (
                .a(a[i]),
                .b(b[i]),
                .cin(carry[i]),
                .sum(sum[i]),
                .cout(carry[i+1])
            );
        end
    endgenerate
endmodule

// Generate mixing instances and always blocks
module gen_instance_always #(parameter STAGES = 4) (
    input clk,
    input [STAGES-1:0] data_in,
    output [STAGES-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < STAGES; i = i + 1) begin : pipeline
            wire inverted;

            // Instance
            inverter u_inv (
                .a(data_in[i]),
                .b(inverted)
            );

            // Always block
            always @(posedge clk) begin
                data_out[i] <= inverted;
            end
        end
    endgenerate
endmodule

// Generate mixing assigns and always blocks
module gen_assign_always #(parameter WIDTH = 8) (
    input clk,
    input [WIDTH-1:0] data_in,
    output [WIDTH-1:0] comb_out,
    output reg [WIDTH-1:0] reg_out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : mixed
            // Combinational assign
            assign comb_out[i] = data_in[i];

            // Sequential always
            always @(posedge clk) begin
                reg_out[i] <= ~data_in[i];
            end
        end
    endgenerate
endmodule

// Generate with ALL THREE: assigns, instances, always
module gen_all_three #(parameter WIDTH = 4) (
    input clk,
    input [WIDTH-1:0] data_in,
    output [WIDTH-1:0] inverted,
    output reg [WIDTH-1:0] registered,
    output [WIDTH-1:0] delayed
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : complete
            wire inv_wire;

            // 1. Assign
            assign inverted[i] = ~data_in[i];

            // 2. Instance
            inverter u_inv (
                .a(data_in[i]),
                .b(inv_wire)
            );

            // 3. Always block (combinational)
            always @(*) begin
                registered[i] = inv_wire;
            end

            // 4. Another always block (sequential)
            reg stage;
            always @(posedge clk) begin
                stage <= inv_wire;
            end
            assign delayed[i] = stage;
        end
    endgenerate
endmodule

// Generate with conditional instance selection
module gen_cond_instance #(
    parameter USE_INVERTER = 1,
    parameter WIDTH = 8
) (
    input [WIDTH-1:0] data_in,
    output [WIDTH-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : bit_proc
            if (USE_INVERTER) begin : use_inv
                inverter u_inv (
                    .a(data_in[i]),
                    .b(data_out[i])
                );
            end else begin : direct
                assign data_out[i] = data_in[i];
            end
        end
    endgenerate
endmodule

// Generate creating array of instances
module gen_instance_array #(parameter COUNT = 4) (
    input clk,
    input [COUNT-1:0] d_in,
    output [COUNT-1:0] q_out
);
    generate
        genvar i;
        for (i = 0; i < COUNT; i = i + 1) begin : dff_array
            dff u_dff (
                .clk(clk),
                .d(d_in[i]),
                .q(q_out[i])
            );
        end
    endgenerate
endmodule

// Generate with nested instances
module gen_nested_inst #(parameter ROWS = 2, parameter COLS = 2) (
    input [ROWS*COLS-1:0] in,
    output [ROWS*COLS-1:0] out
);
    generate
        genvar i, j;
        for (i = 0; i < ROWS; i = i + 1) begin : row
            for (j = 0; j < COLS; j = j + 1) begin : col
                localparam IDX = i * COLS + j;

                inverter u_inv (
                    .a(in[IDX]),
                    .b(out[IDX])
                );
            end
        end
    endgenerate
endmodule
