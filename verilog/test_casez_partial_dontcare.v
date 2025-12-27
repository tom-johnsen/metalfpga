// EXPECT=PASS
// Test casez with don't-care in middle of pattern
module test_casez_partial_dontcare;
    reg [7:0] value;
    reg [3:0] result;

    always @(*) begin
        casez (value)
            8'b1???_0001: result = 4'd1;  // Specific pattern with middle don't-care
            8'b0?1?_0?1?: result = 4'd2;  // Alternating don't-care
            8'b??11_??00: result = 4'd3;  // Specific bits scattered
            8'b1010_????: result = 4'd4;  // Fixed upper, any lower
            default:      result = 4'd0;
        endcase
    end

    initial begin
        value = 8'b1000_0001;
        #1 if (result == 4'd1)
            $display("PASS: casez 1000_0001 matches 1???_0001");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 8'b1111_0001;
        #1 if (result == 4'd1)
            $display("PASS: casez 1111_0001 matches 1???_0001");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 8'b0010_0010;
        #1 if (result == 4'd2)
            $display("PASS: casez 0010_0010 matches 0?1?_0?1?");
        else
            $display("FAIL: result=%d (expected 2)", result);

        value = 8'b0011_1100;
        #1 if (result == 4'd3)
            $display("PASS: casez 0011_1100 matches ??11_??00");
        else
            $display("FAIL: result=%d (expected 3)", result);

        value = 8'b1010_1111;
        #1 if (result == 4'd4)
            $display("PASS: casez 1010_1111 matches 1010_????");
        else
            $display("FAIL: result=%d (expected 4)", result);

        $finish;
    end
endmodule
