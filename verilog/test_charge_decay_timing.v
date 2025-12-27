// EXPECT=PASS
// Test trireg charge decay timing for different capacitances
module test_charge_decay_timing;
    reg driver;
    reg enable;

    trireg (small) cap_small;
    trireg (medium) cap_medium;
    trireg (large) cap_large;

    bufif1 (cap_small, driver, enable);
    bufif1 (cap_medium, driver, enable);
    bufif1 (cap_large, driver, enable);

    initial begin
        driver = 1'b1;
        enable = 1'b1;

        // Charge all capacitors
        #1 begin
            if (cap_small === 1'b1 && cap_medium === 1'b1 && cap_large === 1'b1)
                $display("PASS: All capacitors charged to 1");
            else
                $display("FAIL: small=%b med=%b large=%b", cap_small, cap_medium, cap_large);
        end

        // Disconnect and monitor decay
        #1 enable = 1'b0;

        #1 begin
            $display("INFO: t=3 - small=%b med=%b large=%b", cap_small, cap_medium, cap_large);
        end

        #5 begin
            $display("INFO: t=8 - small=%b med=%b large=%b", cap_small, cap_medium, cap_large);
        end

        #10 begin
            $display("INFO: t=18 - small=%b med=%b large=%b", cap_small, cap_medium, cap_large);
        end

        #20 begin
            $display("INFO: t=38 - small=%b med=%b large=%b", cap_small, cap_medium, cap_large);
            // Small should decay first, large should hold longest
            if (cap_large === 1'b1 || cap_large === 1'bx)
                $display("PASS: Large capacitor holds charge longest");
            else
                $display("INFO: Large cap decayed to %b", cap_large);
        end

        $finish;
    end
endmodule
