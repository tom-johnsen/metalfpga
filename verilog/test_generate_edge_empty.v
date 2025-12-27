// EXPECT=PASS
// Test edge cases: empty generate blocks
module test_generate_edge_empty;
    wire [7:0] data_out;
    reg [7:0] data_in;

    parameter ENABLE = 0;

    // Generate if with false condition - empty else block
    generate
        if (ENABLE) begin : enabled
            assign data_out = ~data_in;
        end
    endgenerate

    // Without the if block, data_out is undriven
    // We need to handle this case
    assign data_out = (ENABLE) ? 8'bx : data_in;

    initial begin
        data_in = 8'b11001100;

        #1 begin
            // Since ENABLE=0, data_out should be data_in
            if (data_out == 8'b11001100)
                $display("PASS: Empty generate if handled correctly");
            else
                $display("FAIL: data_out=%b (expected 11001100)", data_out);
        end

        $finish;
    end
endmodule

// Test zero-iteration generate for loop
module test_generate_zero_iter;
    wire [7:0] result;
    reg [7:0] data;

    assign result = data;  // Default assignment

    genvar i;
    generate
        // Loop never executes (0 < 0 is false)
        for (i = 0; i < 0; i = i + 1) begin : zero_loop
            not (result[i], data[i]);
        end
    endgenerate

    initial begin
        data = 8'b10101010;

        #1 begin
            if (result == 8'b10101010)
                $display("PASS: Zero-iteration generate for handled");
            else
                $display("FAIL: result=%b", result);
        end

        $finish;
    end
endmodule
