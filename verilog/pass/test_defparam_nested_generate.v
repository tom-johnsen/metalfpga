// EXPECT=PASS
// Test defparam precedence across nested generate blocks
module configurable_block #(
    parameter MODE = 0,
    parameter SIZE = 4
) (
    input [SIZE-1:0] in,
    output [SIZE-1:0] out
);
    generate
        if (MODE == 0) begin : mode_invert
            assign out = ~in;
        end else if (MODE == 1) begin : mode_pass
            assign out = in;
        end else begin : mode_shift
            assign out = {in[SIZE-2:0], in[SIZE-1]};
        end
    endgenerate
endmodule

module nested_wrapper #(
    parameter OUTER_MODE = 0
) (
    input [7:0] data_in,
    output [7:0] data_out
);
    genvar i;
    generate
        if (OUTER_MODE == 0) begin : outer_mode_0
            for (i = 0; i < 2; i = i + 1) begin : inner_loop
                configurable_block #(.MODE(0), .SIZE(4)) blk (
                    .in(data_in[i*4 +: 4]),
                    .out(data_out[i*4 +: 4])
                );
            end
        end else begin : outer_mode_1
            for (i = 0; i < 2; i = i + 1) begin : inner_loop
                configurable_block #(.MODE(1), .SIZE(4)) blk (
                    .in(data_in[i*4 +: 4]),
                    .out(data_out[i*4 +: 4])
                );
            end
        end
    endgenerate
endmodule

module test_defparam_nested_generate;
    reg [7:0] test_data;
    wire [7:0] out0, out1, out2;

    // Instance 1: OUTER_MODE=0, blocks should invert
    nested_wrapper #(.OUTER_MODE(0)) wrap0 (
        .data_in(test_data),
        .data_out(out0)
    );

    // Instance 2: Same but override via defparam through nested generate
    nested_wrapper #(.OUTER_MODE(0)) wrap1 (
        .data_in(test_data),
        .data_out(out1)
    );

    // Override MODE in first block of first loop iteration
    defparam wrap1.outer_mode_0.inner_loop[0].blk.MODE = 1;  // Pass instead of invert
    // Second block stays as invert (MODE=0)

    // Instance 3: OUTER_MODE=1, override SIZE
    nested_wrapper #(.OUTER_MODE(1)) wrap2 (
        .data_in(test_data),
        .data_out(out2)
    );

    // This tests defparam precedence in nested generates
    defparam wrap2.outer_mode_1.inner_loop[0].blk.MODE = 2;  // Shift
    defparam wrap2.outer_mode_1.inner_loop[1].blk.MODE = 0;  // Invert

    initial begin
        test_data = 8'b11001010;

        #1 begin
            // wrap0: Both nibbles inverted
            if (out0 == 8'b00110101)
                $display("PASS: Nested generate without defparam override");
            else
                $display("FAIL: out0=%b (expected 00110101)", out0);

            // wrap1: First nibble passed (1010), second inverted (1100->0011)
            if (out1 == 8'b00111010)
                $display("PASS: Defparam in nested generate [0]");
            else
                $display("FAIL: out1=%b (expected 00111010)", out1);

            // wrap2: First nibble shifted, second inverted
            // 1010 shifted = 0101, 1100 inverted = 0011
            if (out2 == 8'b00110101)
                $display("PASS: Multiple defparam overrides in nested generate");
            else
                $display("FAIL: out2=%b (expected 00110101)", out2);
        end

        // Test different data
        #1 test_data = 8'b10101111;

        #1 begin
            // Verify overrides persist with new data
            if (out1 == 8'b00001111)  // First nibble passed (1111), second inverted (1010->0101)
                $display("PASS: Defparam overrides persist");
            else
                $display("FAIL: out1=%b", out1);
        end

        $finish;
    end
endmodule
