// EXPECT=PASS
// Test casex with all don't-care pattern
module test_casex_all_dontcare;
    reg [3:0] value;
    reg [7:0] result;

    always @(*) begin
        casex (value)
            4'b1111: result = 8'd1;  // Exact match first
            4'bxxxx: result = 8'd2;  // Match anything
            default: result = 8'd3;  // Should never reach
        endcase
    end

    initial begin
        value = 4'b1111;
        #1 if (result == 8'd1)
            $display("PASS: casex exact match has priority");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 4'b0000;
        #1 if (result == 8'd2)
            $display("PASS: casex xxxx matches 0000");
        else
            $display("FAIL: result=%d (expected 2)", result);

        value = 4'b1010;
        #1 if (result == 8'd2)
            $display("PASS: casex xxxx matches 1010");
        else
            $display("FAIL: result=%d (expected 2)", result);

        value = 4'bxxxx;
        #1 if (result == 8'd2)
            $display("PASS: casex xxxx matches xxxx value");
        else
            $display("FAIL: result=%d (expected 2)", result);

        value = 4'bzzzz;
        #1 if (result == 8'd2)
            $display("PASS: casex xxxx matches zzzz value");
        else
            $display("FAIL: result=%d (expected 2)", result);

        $finish;
    end
endmodule
