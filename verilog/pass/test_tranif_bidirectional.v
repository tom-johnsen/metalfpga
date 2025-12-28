// EXPECT=PASS
// Test tranif bidirectional pass gates
module test_tranif_bidirectional;
    reg ctrl;
    reg drive_a, drive_b;
    reg enable_a, enable_b;
    wire net_a, net_b;

    // Bidirectional pass gate
    tranif1 (net_a, net_b, ctrl);

    // Drivers on both sides (tri-state)
    bufif1 (net_a, drive_a, enable_a);
    bufif1 (net_b, drive_b, enable_b);

    initial begin
        ctrl = 1'b0;
        drive_a = 1'b0;
        drive_b = 1'b0;
        enable_a = 1'b0;
        enable_b = 1'b0;

        // Test 1: Gate off, drive A
        #1 begin
            drive_a = 1'b1;
            enable_a = 1'b1;
        end

        #1 begin
            if (net_a === 1'b1 && net_b === 1'bz)
                $display("PASS: Gate off - no propagation");
            else
                $display("FAIL: net_a=%b net_b=%b (expected 1,z)", net_a, net_b);
        end

        // Test 2: Turn gate on - A should propagate to B
        #1 ctrl = 1'b1;

        #1 begin
            if (net_a === 1'b1 && net_b === 1'b1)
                $display("PASS: Gate on - A propagates to B");
            else
                $display("FAIL: net_a=%b net_b=%b (expected 1,1)", net_a, net_b);
        end

        // Test 3: Disable A, drive B - should propagate to A
        #1 begin
            enable_a = 1'b0;
            drive_b = 1'b0;
            enable_b = 1'b1;
        end

        #1 begin
            if (net_a === 1'b0 && net_b === 1'b0)
                $display("PASS: Gate on - B propagates to A");
            else
                $display("FAIL: net_a=%b net_b=%b (expected 0,0)", net_a, net_b);
        end

        // Test 4: Both drive same value
        #1 begin
            drive_a = 1'b1;
            enable_a = 1'b1;
            drive_b = 1'b1;
        end

        #1 begin
            if (net_a === 1'b1 && net_b === 1'b1)
                $display("PASS: Both sides drive 1");
            else
                $display("FAIL: net_a=%b net_b=%b", net_a, net_b);
        end

        // Test 5: Conflicting drives (should resolve based on strength)
        #1 drive_a = 1'b0;  // A drives 0, B drives 1

        #1 begin
            // Result depends on drive strength resolution
            $display("INFO: Conflict resolution - net_a=%b net_b=%b", net_a, net_b);
        end

        $finish;
    end
endmodule
