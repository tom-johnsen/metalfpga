// EXPECT=PASS
// Test trireg charge storage and decay
module test_trireg_decay;
    reg driver;
    reg enable;
    trireg (small) capacitor;  // Small capacitance - faster decay
    trireg (medium) cap_med;   // Medium capacitance
    trireg (large) cap_large;  // Large capacitance - slower decay

    // Driver with tri-state control
    bufif1 (capacitor, driver, enable);
    bufif1 (cap_med, driver, enable);
    bufif1 (cap_large, driver, enable);

    initial begin
        driver = 1'b0;
        enable = 1'b0;

        // Charge the capacitor to 1
        #1 driver = 1'b1;
        #1 enable = 1'b1;
        #1 begin
            if (capacitor === 1'b1)
                $display("PASS: trireg charged to 1");
            else
                $display("FAIL: capacitor=%b (expected 1)", capacitor);
        end

        // Disconnect driver - trireg should hold charge
        #1 enable = 1'b0;
        #1 begin
            if (capacitor === 1'b1)
                $display("PASS: trireg holds charge when disconnected");
            else
                $display("FAIL: capacitor=%b (expected 1)", capacitor);
        end

        // Wait for decay (small cap decays fastest)
        #10 begin
            // Small cap may have decayed to X
            if (capacitor === 1'bx || capacitor === 1'b1)
                $display("PASS: small trireg decay behavior");
            else
                $display("FAIL: capacitor=%b", capacitor);
        end

        // Discharge to 0
        driver = 1'b0;
        enable = 1'b1;
        #1 begin
            if (capacitor === 1'b0)
                $display("PASS: trireg discharged to 0");
            else
                $display("FAIL: capacitor=%b (expected 0)", capacitor);
        end

        $finish;
    end
endmodule
