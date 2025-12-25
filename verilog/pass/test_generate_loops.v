// Generate loop tests - basic iteration

// Simple generate loop with assigns
module gen_loop_assigns(
    input [7:0] in,
    output [7:0] out
);
    generate
        genvar i;
        for (i = 0; i < 8; i = i + 1) begin : bit_invert
            assign out[i] = ~in[i];
        end
    endgenerate
endmodule

// Generate loop creating multiple signals
module gen_loop_signals(
    input [3:0] a, b,
    output [3:0] sum
);
    generate
        genvar i;
        for (i = 0; i < 4; i = i + 1) begin : adder
            wire carry_in;
            wire carry_out;
            assign sum[i] = a[i] ^ b[i] ^ carry_in;
        end
    endgenerate
endmodule

// Generate loop with parameter-based bounds
module gen_loop_param #(parameter WIDTH = 8) (
    input [WIDTH-1:0] data_in,
    output [WIDTH-1:0] data_out
);
    generate
        genvar i;
        for (i = 0; i < WIDTH; i = i + 1) begin : passthrough
            assign data_out[i] = data_in[i];
        end
    endgenerate
endmodule

// Generate loop with multiple statements per iteration
module gen_loop_multi(
    input [15:0] in,
    output [15:0] out1,
    output [15:0] out2
);
    generate
        genvar i;
        for (i = 0; i < 16; i = i + 1) begin : dual_output
            assign out1[i] = in[i];
            assign out2[i] = ~in[i];
        end
    endgenerate
endmodule

// Generate loop creating array of wires
module gen_loop_array(
    input [7:0] sel,
    output reg result
);
    generate
        genvar i;
        for (i = 0; i < 8; i = i + 1) begin : bitcheck
            wire bit_set;
            assign bit_set = sel[i];
        end
    endgenerate

    always @(*) begin
        result = sel[0]; // Simple use case
    end
endmodule
