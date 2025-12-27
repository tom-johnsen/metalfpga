// EXPECT=PASS
// Test CMOS transmission gates
module test_cmos_switches;
    reg data, nctrl, pctrl;
    wire out_cmos, out_rcmos;

    // CMOS: passes when nctrl=1 AND pctrl=0 (complementary controls)
    cmos (out_cmos, data, nctrl, pctrl);

    // RCMOS: resistive CMOS (reduced strength)
    rcmos (out_rcmos, data, nctrl, pctrl);

    initial begin
        data = 1'b0;
        nctrl = 1'b0;
        pctrl = 1'b1;

        #1 begin
            // Both off (nctrl=0, pctrl=1) - should be Z
            if (out_cmos === 1'bz)
                $display("PASS: CMOS off produces Z");
            else
                $display("FAIL: out_cmos=%b (expected z)", out_cmos);
        end

        // Turn on CMOS (nctrl=1, pctrl=0)
        #1 begin
            nctrl = 1'b1;
            pctrl = 1'b0;
        end

        #1 begin
            if (out_cmos === 1'b0)
                $display("PASS: CMOS on passes 0");
            else
                $display("FAIL: out_cmos=%b (expected 0)", out_cmos);
        end

        #1 data = 1'b1;

        #1 begin
            if (out_cmos === 1'b1)
                $display("PASS: CMOS on passes 1");
            else
                $display("FAIL: out_cmos=%b (expected 1)", out_cmos);

            if (out_rcmos === 1'b1)
                $display("PASS: RCMOS on passes 1");
            else
                $display("FAIL: out_rcmos=%b (expected 1)", out_rcmos);
        end

        // Invalid control combination (both on)
        #1 pctrl = 1'b1;

        #1 begin
            // Result depends on implementation
            $display("INFO: CMOS both controls on - out_cmos=%b", out_cmos);
        end

        $finish;
    end
endmodule
