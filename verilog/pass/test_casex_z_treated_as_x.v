// EXPECT=PASS
// Test casex treats both X and Z as don't-care
module test_casex_z_treated_as_x;
    reg [3:0] value;
    reg [7:0] result;

    always @(*) begin
        casex (value)
            4'b11xx: result = 8'd1;  // x in pattern
            4'b10zz: result = 8'd2;  // z in pattern - treated as don't-care
            4'bxx00: result = 8'd3;  // x in pattern
            default: result = 8'd0;
        endcase
    end

    initial begin
        // Z in pattern should match like X
        value = 4'b1000;
        #1 if (result == 8'd2)
            $display("PASS: casex pattern 10zz matched 1000");
        else
            $display("FAIL: result=%d (expected 2)", result);

        value = 4'b1011;
        #1 if (result == 8'd2)
            $display("PASS: casex pattern 10zz matched 1011");
        else
            $display("FAIL: result=%d (expected 2)", result);

        // Z in value should match x pattern
        value = 4'bzz00;
        #1 if (result == 8'd3)
            $display("PASS: casex value zz00 matched xx00");
        else
            $display("FAIL: result=%d (expected 3)", result);

        // X in value should match z pattern
        value = 4'b10xx;
        #1 if (result == 8'd2)
            $display("PASS: casex value 10xx matched 10zz");
        else
            $display("FAIL: result=%d (expected 2)", result);

        $finish;
    end
endmodule
