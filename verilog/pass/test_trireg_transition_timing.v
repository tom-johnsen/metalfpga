// EXPECT=PASS
// Test trireg charge/discharge transition timing
module test_trireg_transition_timing;
    reg driver;
    reg enable;
    trireg (medium) capacitor;

    bufif1 (capacitor, driver, enable);

    initial begin
        driver = 1'b0;
        enable = 1'b1;

        // Start at 0
        #1 begin
            if (capacitor === 1'b0)
                $display("PASS: Initial state 0");
            else
                $display("FAIL: capacitor=%b (expected 0)", capacitor);
        end

        // Transition 0 -> 1
        #1 driver = 1'b1;

        #1 begin
            if (capacitor === 1'b1)
                $display("PASS: Charged 0->1 in 1 time unit");
            else
                $display("FAIL: capacitor=%b after charge (expected 1)", capacitor);
        end

        // Disconnect during charge
        #1 begin
            driver = 1'b1;
            enable = 1'b0;
        end

        #1 begin
            if (capacitor === 1'b1)
                $display("PASS: Holds charge after disconnect");
            else
                $display("FAIL: capacitor=%b (expected 1)", capacitor);
        end

        // Reconnect and discharge
        #1 begin
            driver = 1'b0;
            enable = 1'b1;
        end

        #1 begin
            if (capacitor === 1'b0)
                $display("PASS: Discharged 1->0 in 1 time unit");
            else
                $display("FAIL: capacitor=%b (expected 0)", capacitor);
        end

        // Rapid toggling
        #1 driver = 1'b1;
        #1 driver = 1'b0;
        #1 driver = 1'b1;
        #1 driver = 1'b0;

        #1 begin
            if (capacitor === 1'b0)
                $display("PASS: Follows rapid transitions");
            else
                $display("FAIL: capacitor=%b after toggle (expected 0)", capacitor);
        end

        $finish;
    end
endmodule
