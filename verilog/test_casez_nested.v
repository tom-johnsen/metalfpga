// EXPECT=PASS
// Test nested casez statements
module test_casez_nested;
    reg [3:0] opcode;
    reg [1:0] subcode;
    reg [7:0] result;

    always @(*) begin
        casez (opcode)
            4'b00??: begin
                // Nested casez for subcode
                casez (subcode)
                    2'b0?: result = 8'd1;
                    2'b10: result = 8'd2;
                    2'b11: result = 8'd3;
                    default: result = 8'd0;
                endcase
            end
            4'b01??: result = 8'd10;
            4'b1???: result = 8'd20;
            default: result = 8'd0;
        endcase
    end

    initial begin
        opcode = 4'b0000;
        subcode = 2'b00;
        #1 if (result == 8'd1)
            $display("PASS: Nested casez outer=00??, inner=0?");
        else
            $display("FAIL: result=%d (expected 1)", result);

        opcode = 4'b0011;
        subcode = 2'b10;
        #1 if (result == 8'd2)
            $display("PASS: Nested casez outer=00??, inner=10");
        else
            $display("FAIL: result=%d (expected 2)", result);

        opcode = 4'b0100;
        subcode = 2'b11;  // subcode ignored
        #1 if (result == 8'd10)
            $display("PASS: Nested casez outer=01?? (no inner)");
        else
            $display("FAIL: result=%d (expected 10)", result);

        opcode = 4'b1111;
        #1 if (result == 8'd20)
            $display("PASS: Nested casez outer=1???");
        else
            $display("FAIL: result=%d (expected 20)", result);

        $finish;
    end
endmodule
