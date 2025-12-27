// EXPECT=PASS
// Test trireg with 4-state drive values (X and Z)
module test_trireg_4state_drive;
    reg driver;
    reg enable;
    trireg (medium) storage;

    bufif1 (storage, driver, enable);

    initial begin
        driver = 1'b0;
        enable = 1'b0;

        #1 begin
            // No driver, should be Z or X
            $display("INFO: Initial storage (no driver)=%b", storage);
        end

        // Charge with 1
        #1 begin
            driver = 1'b1;
            enable = 1'b1;
        end

        #1 begin
            if (storage === 1'b1)
                $display("PASS: Charged to 1");
            else
                $display("FAIL: storage=%b (expected 1)", storage);
        end

        // Drive with X
        #1 driver = 1'bx;

        #1 begin
            if (storage === 1'bx)
                $display("PASS: Trireg stores X value");
            else
                $display("INFO: storage=%b after X drive", storage);
        end

        // Disconnect - should hold X or decay
        #1 enable = 1'b0;

        #2 begin
            $display("INFO: After disconnect from X drive: storage=%b", storage);
        end

        // Drive with Z (high-impedance driver)
        #1 begin
            driver = 1'bz;
            enable = 1'b1;
        end

        #1 begin
            // Z driver is like no driver
            $display("INFO: With Z driver: storage=%b", storage);
        end

        // Charge with 0, then drive conflict (X)
        #1 begin
            driver = 1'b0;
            enable = 1'b1;
        end

        #1 begin
            if (storage === 1'b0)
                $display("PASS: Charged to 0");
            else
                $display("INFO: storage=%b", storage);
        end

        // Now enable both 0 and 1 drivers to create conflict
        // (This would need multiple drivers, shown conceptually)
        #1 driver = 1'bx;  // X represents conflict

        #1 begin
            $display("INFO: Conflict/X drive: storage=%b", storage);
        end

        // Disconnect and monitor decay from X
        #1 enable = 1'b0;

        #5 begin
            $display("INFO: After decay from X: storage=%b", storage);
        end

        #10 begin
            $display("INFO: Long decay from X: storage=%b", storage);
        end

        $finish;
    end
endmodule
