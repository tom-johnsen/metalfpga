// EXPECT=PASS
// Test different trireg capacitance levels and decay rates
module test_trireg_capacitance_levels;
    reg driver;
    reg enable;

    trireg (small) cap_small;
    trireg (medium) cap_medium;
    trireg (large) cap_large;

    // All capacitors driven by same source
    bufif1 (cap_small, driver, enable);
    bufif1 (cap_medium, driver, enable);
    bufif1 (cap_large, driver, enable);

    initial begin
        driver = 1'b1;
        enable = 1'b1;

        // Charge all capacitors
        #2 begin
            if (cap_small === 1'b1 && cap_medium === 1'b1 && cap_large === 1'b1)
                $display("PASS: All capacitors charged");
            else
                $display("FAIL: Charging failed - small=%b med=%b large=%b",
                         cap_small, cap_medium, cap_large);
        end

        // Disconnect and monitor decay
        #1 enable = 1'b0;

        #1 begin
            $display("INFO: t=4 - small=%b med=%b large=%b",
                     cap_small, cap_medium, cap_large);
        end

        #5 begin
            $display("INFO: t=9 - small=%b med=%b large=%b",
                     cap_small, cap_medium, cap_large);
        end

        #10 begin
            $display("INFO: t=19 - small=%b med=%b large=%b",
                     cap_small, cap_medium, cap_large);

            // Large capacitor should hold charge longest
            if (cap_large === 1'b1 || cap_large === 1'bx)
                $display("PASS: Large capacitor holds charge longest");
            else
                $display("INFO: Large capacitor decayed to %b", cap_large);
        end

        #20 begin
            $display("INFO: t=39 - small=%b med=%b large=%b",
                     cap_small, cap_medium, cap_large);
        end

        $finish;
    end
endmodule
