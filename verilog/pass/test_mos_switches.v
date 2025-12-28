// EXPECT=PASS
// Test NMOS and PMOS switches
module test_mos_switches;
    reg data, nctrl, pctrl;
    wire out_nmos, out_pmos;
    wire out_rnmos, out_rpmos;

    // NMOS: passes when ctrl=1
    nmos (out_nmos, data, nctrl);
    rnmos (out_rnmos, data, nctrl);  // Resistive NMOS

    // PMOS: passes when ctrl=0
    pmos (out_pmos, data, pctrl);
    rpmos (out_rpmos, data, pctrl);  // Resistive PMOS

    initial begin
        data = 1'b0;
        nctrl = 1'b0;
        pctrl = 1'b1;

        #1 begin
            // NMOS off (ctrl=0), PMOS off (ctrl=1) - both Z
            if (out_nmos === 1'bz)
                $display("PASS: NMOS off produces Z");
            else
                $display("FAIL: out_nmos=%b (expected z)", out_nmos);

            if (out_pmos === 1'bz)
                $display("PASS: PMOS off produces Z");
            else
                $display("FAIL: out_pmos=%b (expected z)", out_pmos);
        end

        // Turn on NMOS (ctrl=1), keep PMOS off
        #1 nctrl = 1'b1;

        #1 begin
            if (out_nmos === 1'b0)
                $display("PASS: NMOS on passes 0");
            else
                $display("FAIL: out_nmos=%b (expected 0)", out_nmos);
        end

        #1 data = 1'b1;

        #1 begin
            if (out_nmos === 1'b1)
                $display("PASS: NMOS on passes 1");
            else
                $display("FAIL: out_nmos=%b (expected 1)", out_nmos);
        end

        // Turn on PMOS (ctrl=0), turn off NMOS
        #1 begin
            nctrl = 1'b0;
            pctrl = 1'b0;
            data = 1'b0;
        end

        #1 begin
            if (out_pmos === 1'b0)
                $display("PASS: PMOS on passes 0");
            else
                $display("FAIL: out_pmos=%b (expected 0)", out_pmos);

            if (out_nmos === 1'bz)
                $display("PASS: NMOS off after transition");
            else
                $display("FAIL: out_nmos=%b (expected z)", out_nmos);
        end

        #1 data = 1'b1;

        #1 begin
            if (out_pmos === 1'b1)
                $display("PASS: PMOS on passes 1");
            else
                $display("FAIL: out_pmos=%b (expected 1)", out_pmos);

            if (out_rpmos === 1'b1)
                $display("PASS: RPMOS on passes 1");
            else
                $display("FAIL: out_rpmos=%b (expected 1)", out_rpmos);
        end

        $finish;
    end
endmodule
