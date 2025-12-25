// Generate conditional tests - if/else blocks

// Simple generate if based on parameter
module gen_if_simple #(parameter USE_FAST = 1) (
    input [7:0] in,
    output [7:0] out
);
    generate
        if (USE_FAST) begin : fast_path
            assign out = in;
        end else begin : slow_path
            assign out = ~in;
        end
    endgenerate
endmodule

// Generate if without else
module gen_if_only #(parameter ENABLE = 1) (
    input [7:0] data,
    output reg [7:0] result
);
    generate
        if (ENABLE) begin : enabled
            wire [7:0] temp;
            assign temp = data;
        end
    endgenerate

    always @(*) begin
        result = data;
    end
endmodule

// Multiple generate if conditions
module gen_if_multi #(
    parameter MODE = 0
) (
    input [7:0] in,
    output [7:0] out
);
    generate
        if (MODE == 0) begin : mode0
            assign out = in;
        end else if (MODE == 1) begin : mode1
            assign out = ~in;
        end else if (MODE == 2) begin : mode2
            assign out = in << 1;
        end else begin : mode_default
            assign out = 8'h00;
        end
    endgenerate
endmodule

// Generate if with complex expressions
module gen_if_expr #(
    parameter WIDTH = 8,
    parameter THRESHOLD = 4
) (
    input [WIDTH-1:0] data,
    output [WIDTH-1:0] result
);
    generate
        if (WIDTH > THRESHOLD) begin : wide_mode
            assign result = data;
        end else begin : narrow_mode
            assign result = {WIDTH{1'b0}};
        end
    endgenerate
endmodule

// Nested generate if
module gen_if_nested #(
    parameter FEATURE_A = 1,
    parameter FEATURE_B = 0
) (
    input [7:0] in,
    output [7:0] out
);
    generate
        if (FEATURE_A) begin : feat_a
            if (FEATURE_B) begin : feat_b
                assign out = in << 2;
            end else begin : no_feat_b
                assign out = in << 1;
            end
        end else begin : no_feat_a
            assign out = in;
        end
    endgenerate
endmodule
