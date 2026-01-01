// EXPECT=PASS
// Test generate for with complex loop expressions
module test_generate_for_expressions;
    wire [15:0] result;
    reg [15:0] data;

    genvar i;

    // Generate with step of 2
    generate
        for (i = 0; i < 16; i = i + 2) begin : step2
            not (result[i], data[i]);
        end
    endgenerate

    // Generate with step of 2, offset by 1
    generate
        for (i = 1; i < 16; i = i + 2) begin : step2_offset
            buf (result[i], data[i]);
        end
    endgenerate

    initial begin
        data = 16'b1010101010101010;

        #1 begin
            // Even bits inverted, odd bits passed through
            if (result == 16'b1001100110011001)
                $display("PASS: Generate for with step expressions");
            else
                $display("FAIL: result=%b (expected 1001100110011001)", result);
        end

        $finish;
    end
endmodule
