// EXPECT=PASS
// Test casez treats only Z/? as don't-care (X must match exactly)
module test_casez_only_z_dontcare;
    reg [3:0] value;
    reg [7:0] result;

    always @(*) begin
        casez (value)
            4'b11??: result = 8'd1;  // ? is don't-care
            4'b10zz: result = 8'd2;  // z is don't-care
            4'b01xx: result = 8'd3;  // x in pattern - NOT don't-care in casez!
            default: result = 8'd0;
        endcase
    end

    initial begin
        // ? and z are don't-care
        value = 4'b1100;
        #1 if (result == 8'd1)
            $display("PASS: casez ? is don't-care (1100 matches 11??)");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 4'b1011;
        #1 if (result == 8'd2)
            $display("PASS: casez z is don't-care (1011 matches 10zz)");
        else
            $display("FAIL: result=%d (expected 2)", result);

        // x in pattern should match literal x in casez (not don't-care)
        value = 4'b01xx;
        #1 if (result == 8'd3)
            $display("PASS: casez x in pattern matches literal x value");
        else
            $display("FAIL: result=%d (expected 3)", result);

        // Concrete bits should NOT match x pattern in casez
        value = 4'b0100;
        #1 if (result == 8'd0)
            $display("PASS: casez 0100 does NOT match 01xx (x not don't-care)");
        else
            $display("FAIL: result=%d (expected 0, no match)", result);

        $finish;
    end
endmodule
