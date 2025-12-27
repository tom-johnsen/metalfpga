// EXPECT=PASS
// Test the difference between casex and casez
module test_casex_vs_casez_difference;
    reg [3:0] value;
    reg [7:0] result_x, result_z;

    // casex: treats X and Z as don't-care
    always @(*) begin
        casex (value)
            4'b11xx: result_x = 8'd1;
            4'b10xx: result_x = 8'd2;
            default: result_x = 8'd0;
        endcase
    end

    // casez: treats only Z/? as don't-care
    always @(*) begin
        casez (value)
            4'b11??: result_z = 8'd1;
            4'b10??: result_z = 8'd2;
            default: result_z = 8'd0;
        endcase
    end

    initial begin
        // Concrete value - both should match
        value = 4'b1100;
        #1 begin
            if (result_x == 8'd1 && result_z == 8'd1)
                $display("PASS: Both casex and casez match 1100");
            else
                $display("FAIL: result_x=%d result_z=%d", result_x, result_z);
        end

        // Value with Z - both should match (Z is don't-care in both)
        value = 4'b11zz;
        #1 begin
            if (result_x == 8'd1 && result_z == 8'd1)
                $display("PASS: Both match 11zz (Z is don't-care)");
            else
                $display("FAIL: result_x=%d result_z=%d", result_x, result_z);
        end

        // Value with X - casex matches, casez treats X as literal
        value = 4'b11xx;
        #1 begin
            if (result_x == 8'd1)
                $display("PASS: casex matches 11xx (X is don't-care)");
            else
                $display("FAIL: casex result_x=%d (expected 1)", result_x);

            // In casez, 'x' in pattern means literal x match
            // But our patterns use '?' not 'x', so value 11xx should match 11??
            if (result_z == 8'd1)
                $display("PASS: casez matches 11xx with 11?? (? is don't-care)");
            else
                $display("FAIL: casez result_z=%d (expected 1)", result_z);
        end

        $finish;
    end
endmodule
