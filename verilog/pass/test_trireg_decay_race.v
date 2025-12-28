// EXPECT=PASS
// Test trireg decay race with reconnection
module test_trireg_decay_race;
    reg driver;
    reg enable;
    trireg (small) fast_decay;

    bufif1 (fast_decay, driver, enable);

    initial begin
        driver = 1'b1;
        enable = 1'b1;

        // Charge
        #1 begin
            if (fast_decay === 1'b1)
                $display("PASS: Charged to 1");
            else
                $display("FAIL: fast_decay=%b", fast_decay);
        end

        // Disconnect and let decay start
        #1 enable = 1'b0;

        #5 begin
            $display("INFO: After 5 time units: fast_decay=%b", fast_decay);
        end

        // Reconnect before full decay
        #1 begin
            driver = 1'b1;
            enable = 1'b1;
        end

        #1 begin
            if (fast_decay === 1'b1)
                $display("PASS: Refreshed charge before decay");
            else
                $display("FAIL: fast_decay=%b (expected 1)", fast_decay);
        end

        // Disconnect again
        #1 enable = 1'b0;

        // Wait for decay
        #20 begin
            $display("INFO: After long decay: fast_decay=%b", fast_decay);

            if (fast_decay === 1'bx || fast_decay === 1'b1)
                $display("PASS: Decay behavior reasonable");
            else
                $display("INFO: Decayed to %b", fast_decay);
        end

        $finish;
    end
endmodule
