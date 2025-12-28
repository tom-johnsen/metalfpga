// EXPECT=PASS
// Test casez with actual Z values in comparison expression
module test_casez_with_z_value;
    reg [3:0] value;
    reg [7:0] result;

    always @(*) begin
        casez (value)
            4'b11??: result = 8'd1;  // Match 11xx
            4'b10??: result = 8'd2;  // Match 10xx
            4'bzz??: result = 8'd3;  // Match when upper 2 bits are Z
            4'b0???: result = 8'd4;  // Match 0xxx
            default: result = 8'd0;
        endcase
    end

    initial begin
        // Regular binary values
        value = 4'b1100;
        #1 if (result == 8'd1)
            $display("PASS: casez matched 1100 with 11??");
        else
            $display("FAIL: result=%d (expected 1)", result);

        // Value with Z - should match casez pattern with Z
        value = 4'bzz00;
        #1 if (result == 8'd3)
            $display("PASS: casez matched zz00 with zz??");
        else
            $display("FAIL: result=%d for zz00 (expected 3)", result);

        value = 4'bzz11;
        #1 if (result == 8'd3)
            $display("PASS: casez matched zz11 with zz??");
        else
            $display("FAIL: result=%d for zz11 (expected 3)", result);

        // Value with mixed Z and bits
        value = 4'b0zzz;
        #1 if (result == 8'd4)
            $display("PASS: casez matched 0zzz with 0???");
        else
            $display("FAIL: result=%d for 0zzz (expected 4)", result);

        $finish;
    end
endmodule
