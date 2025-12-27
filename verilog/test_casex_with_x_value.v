// EXPECT=PASS
// Test casex with actual X values in comparison expression
module test_casex_with_x_value;
    reg [3:0] value;
    reg [7:0] result;

    always @(*) begin
        casex (value)
            4'b11xx: result = 8'd1;  // Match 11xx
            4'bxx11: result = 8'd2;  // Match xx11
            4'b1x1x: result = 8'd3;  // Match 1x1x
            4'bx0x0: result = 8'd4;  // Match x0x0
            default: result = 8'd0;
        endcase
    end

    initial begin
        // Regular matching
        value = 4'b1100;
        #1 if (result == 8'd1)
            $display("PASS: casex matched 1100 with 11xx");
        else
            $display("FAIL: result=%d (expected 1)", result);

        // Value with X should match pattern with x
        value = 4'bxx11;
        #1 if (result == 8'd2)
            $display("PASS: casex matched xx11 with xx11");
        else
            $display("FAIL: result=%d for xx11 (expected 2)", result);

        // Mixed X pattern
        value = 4'b1x1x;
        #1 if (result == 8'd3)
            $display("PASS: casex matched 1x1x with 1x1x");
        else
            $display("FAIL: result=%d for 1x1x (expected 3)", result);

        // X in different positions
        value = 4'bx0x0;
        #1 if (result == 8'd4)
            $display("PASS: casex matched x0x0 with x0x0");
        else
            $display("FAIL: result=%d for x0x0 (expected 4)", result);

        // Value with X matching concrete pattern
        value = 4'b11xx;
        #1 if (result == 8'd1)
            $display("PASS: casex matched 11xx value with 11xx pattern");
        else
            $display("FAIL: result=%d for 11xx (expected 1)", result);

        $finish;
    end
endmodule
