// EXPECT=PASS
// Test tri/triand/trior/tri0/tri1 net types
module test_tri_nets;
    reg drive_a, drive_b, enable_a, enable_b;

    tri net_tri;      // Regular tri-state
    triand net_and;   // Tri-state with wired-AND
    trior net_or;     // Tri-state with wired-OR
    tri0 net_0;       // Pull to 0 when floating
    tri1 net_1;       // Pull to 1 when floating

    // Drivers
    bufif1 (net_tri, drive_a, enable_a);
    bufif1 (net_and, drive_a, enable_a);
    bufif1 (net_and, drive_b, enable_b);
    bufif1 (net_or, drive_a, enable_a);
    bufif1 (net_or, drive_b, enable_b);
    bufif1 (net_0, drive_a, enable_a);
    bufif1 (net_1, drive_a, enable_a);

    initial begin
        drive_a = 1'b1;
        drive_b = 1'b1;
        enable_a = 1'b0;
        enable_b = 1'b0;

        #1 begin
            // All disabled - check pull behavior
            if (net_tri === 1'bz)
                $display("PASS: tri floats to Z");
            else
                $display("FAIL: net_tri=%b (expected z)", net_tri);

            if (net_0 === 1'b0)
                $display("PASS: tri0 pulls to 0");
            else
                $display("FAIL: net_0=%b (expected 0)", net_0);

            if (net_1 === 1'b1)
                $display("PASS: tri1 pulls to 1");
            else
                $display("FAIL: net_1=%b (expected 1)", net_1);
        end

        // Enable one driver
        #1 enable_a = 1'b1;

        #1 begin
            if (net_tri === 1'b1)
                $display("PASS: tri passes 1");
            else
                $display("FAIL: net_tri=%b (expected 1)", net_tri);

            if (net_and === 1'b1)
                $display("PASS: triand with one driver = 1");
            else
                $display("FAIL: net_and=%b (expected 1)", net_and);
        end

        // Enable both drivers on triand/trior
        #1 begin
            drive_a = 1'b1;
            drive_b = 1'b0;
            enable_b = 1'b1;
        end

        #1 begin
            // triand: 1 & 0 = 0
            if (net_and === 1'b0)
                $display("PASS: triand resolves 1,0 to 0");
            else
                $display("FAIL: net_and=%b (expected 0)", net_and);

            // trior: 1 | 0 = 1
            if (net_or === 1'b1)
                $display("PASS: trior resolves 1,0 to 1");
            else
                $display("FAIL: net_or=%b (expected 1)", net_or);
        end

        $finish;
    end
endmodule
