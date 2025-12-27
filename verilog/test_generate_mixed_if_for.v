// EXPECT=PASS
// Test mixing generate if and generate for
module test_generate_mixed_if_for;
    parameter MODE = 1;
    parameter SIZE = 4;

    wire [SIZE-1:0] result;
    reg [SIZE-1:0] data;

    genvar i;

    generate
        if (MODE == 0) begin : invert_mode
            // Invert all bits
            for (i = 0; i < SIZE; i = i + 1) begin : inv_loop
                not (result[i], data[i]);
            end
        end else if (MODE == 1) begin : passthrough_mode
            // Pass through all bits
            for (i = 0; i < SIZE; i = i + 1) begin : buf_loop
                buf (result[i], data[i]);
            end
        end else begin : xor_mode
            // XOR with pattern
            for (i = 0; i < SIZE; i = i + 1) begin : xor_loop
                xor (result[i], data[i], i[0]);  // XOR with LSB of index
            end
        end
    endgenerate

    initial begin
        data = 4'b1010;

        #1 begin
            // MODE=1: passthrough
            if (result == 4'b1010)
                $display("PASS: Generate if+for passthrough mode");
            else
                $display("FAIL: result=%b (expected 1010)", result);
        end

        $finish;
    end
endmodule
