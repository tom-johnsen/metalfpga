// EXPECT=PASS
// Test casex with mixed X and Z in both pattern and value
module test_casex_mixed_xz;
    reg [7:0] value;
    reg [3:0] result;

    always @(*) begin
        casex (value)
            8'b1111_xxxx: result = 4'd1;  // Upper nibble all 1
            8'bxxxx_0000: result = 4'd2;  // Lower nibble all 0
            8'b1x1x_z0z0: result = 4'd3;  // Mixed x and z pattern
            8'bzzzz_xxxx: result = 4'd4;  // All don't-care
            default:      result = 4'd0;
        endcase
    end

    initial begin
        value = 8'b1111_1010;
        #1 if (result == 4'd1)
            $display("PASS: casex 1111_1010 matched 1111_xxxx");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 8'b1010_0000;
        #1 if (result == 4'd2)
            $display("PASS: casex 1010_0000 matched xxxx_0000");
        else
            $display("FAIL: result=%d (expected 2)", result);

        // Value with X and Z should match mixed pattern
        value = 8'b1x1x_z0z0;
        #1 if (result == 4'd3)
            $display("PASS: casex 1x1x_z0z0 matched 1x1x_z0z0");
        else
            $display("FAIL: result=%d (expected 3)", result);

        // Concrete value matching mixed x/z pattern
        value = 8'b1010_0000;
        #1 if (result == 4'd2)
            $display("PASS: casex concrete value matches x pattern");
        else
            $display("FAIL: result=%d (expected 2)", result);

        // All don't-care pattern (should match first)
        value = 8'bxxxx_xxxx;
        #1 if (result == 4'd4)
            $display("PASS: casex xxxx_xxxx matched zzzz_xxxx");
        else
            $display("FAIL: result=%d (expected 4)", result);

        $finish;
    end
endmodule
