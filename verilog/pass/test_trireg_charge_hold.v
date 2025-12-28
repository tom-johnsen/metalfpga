// EXPECT=PASS
// Test trireg charge holding when driver disconnects
module test_trireg_charge_hold;
    reg driver;
    reg enable;
    trireg cap;

    bufif1 (cap, driver, enable);

    initial begin
        driver = 1'b0;
        enable = 1'b0;

        // Charge to 1
        #1 begin
            driver = 1'b1;
            enable = 1'b1;
        end

        #1 begin
            if (cap === 1'b1)
                $display("PASS: Trireg charged to 1");
            else
                $display("FAIL: cap=%b (expected 1)", cap);
        end

        // Disconnect driver - should hold charge
        #1 enable = 1'b0;

        #1 begin
            if (cap === 1'b1)
                $display("PASS: Trireg holds 1 when disconnected");
            else
                $display("FAIL: cap=%b after disconnect (expected 1)", cap);
        end

        // Wait longer - should still hold
        #5 begin
            if (cap === 1'b1 || cap === 1'bx)
                $display("PASS: Trireg maintains charge or decays to X");
            else
                $display("FAIL: cap=%b (expected 1 or x)", cap);
        end

        // Discharge to 0
        #1 begin
            driver = 1'b0;
            enable = 1'b1;
        end

        #1 begin
            if (cap === 1'b0)
                $display("PASS: Trireg discharged to 0");
            else
                $display("FAIL: cap=%b (expected 0)", cap);
        end

        // Disconnect again
        #1 enable = 1'b0;

        #1 begin
            if (cap === 1'b0)
                $display("PASS: Trireg holds 0 when disconnected");
            else
                $display("FAIL: cap=%b (expected 0)", cap);
        end

        $finish;
    end
endmodule
