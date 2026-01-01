// EXPECT=PASS
// Test notif0/notif1 tri-state inverters
module test_notif_tristate;
    reg data, ctrl;
    wire out0, out1;

    // notif0: inverts when ctrl=0, Z when ctrl=1
    notif0 (out0, data, ctrl);

    // notif1: inverts when ctrl=1, Z when ctrl=0
    notif1 (out1, data, ctrl);

    initial begin
        data = 1'b0;
        ctrl = 1'b0;

        #1 begin
            // ctrl=0: notif0 inverts (1), notif1 high-Z
            if (out0 === 1'b1)
                $display("PASS: notif0 inverts to 1 when ctrl=0");
            else
                $display("FAIL: out0=%b (expected 1)", out0);

            if (out1 === 1'bz)
                $display("PASS: notif1 is Z when ctrl=0");
            else
                $display("FAIL: out1=%b (expected z)", out1);
        end

        #1 data = 1'b1;

        #1 begin
            if (out0 === 1'b0)
                $display("PASS: notif0 inverts to 0 when ctrl=0");
            else
                $display("FAIL: out0=%b (expected 0)", out0);
        end

        #1 ctrl = 1'b1;

        #1 begin
            // ctrl=1: notif0 high-Z, notif1 inverts
            if (out0 === 1'bz)
                $display("PASS: notif0 is Z when ctrl=1");
            else
                $display("FAIL: out0=%b (expected z)", out0);

            if (out1 === 1'b0)
                $display("PASS: notif1 inverts to 0 when ctrl=1");
            else
                $display("FAIL: out1=%b (expected 0)", out1);
        end

        #1 data = 1'b0;

        #1 begin
            if (out1 === 1'b1)
                $display("PASS: notif1 inverts to 1 when ctrl=1");
            else
                $display("FAIL: out1=%b (expected 1)", out1);
        end

        $finish;
    end
endmodule
