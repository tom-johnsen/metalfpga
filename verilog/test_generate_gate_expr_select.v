// EXPECT=PASS
// Test gate outputs with complex genvar expressions in selects
module test_generate_gate_expr_select;
    reg [15:0] data;
    wire [15:0] reversed;
    wire [7:0] even_bits;

    genvar i;

    // Generate with reversed indexing
    generate
        for (i = 0; i < 16; i = i + 1) begin : reverse_gen
            buf (reversed[15-i], data[i]);
        end
    endgenerate

    // Generate even bits only (i*2)
    generate
        for (i = 0; i < 8; i = i + 1) begin : even_gen
            buf (even_bits[i], data[i*2]);
        end
    endgenerate

    initial begin
        data = 16'b1010_0101_1100_0011;

        #1 begin
            // Reversed should be 1100_0011_1010_0101
            if (reversed == 16'b1100_0011_1010_0101)
                $display("PASS: Generate with reversed index (15-i)");
            else
                $display("FAIL: reversed=%b", reversed);

            // Even bits: positions 0,2,4,6,8,10,12,14 -> 10101100
            if (even_bits == 8'b1010_1100)
                $display("PASS: Generate with expression index (i*2)");
            else
                $display("FAIL: even_bits=%b (expected 10101100)", even_bits);
        end

        $finish;
    end
endmodule
