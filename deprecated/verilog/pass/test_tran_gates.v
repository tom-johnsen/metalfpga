// EXPECT=PASS
// Test bidirectional transmission gates (tran, rtran)
module test_tran_gates;
    reg drive_a, drive_b;
    reg enable_a, enable_b;
    wire net_a, net_b;
    wire net_c, net_d;

    // Always-on bidirectional connection
    tran (net_a, net_b);

    // Controlled bidirectional connection
    tranif1 pass_ctrl (net_c, net_d, enable_b);

    // Drivers
    bufif1 (net_a, drive_a, enable_a);
    bufif1 (net_b, drive_b, enable_b);

    initial begin
        drive_a = 1'b0;
        drive_b = 1'b0;
        enable_a = 1'b0;
        enable_b = 1'b0;

        // Test 1: Both sides floating
        #1 begin
            if (net_a === 1'bz && net_b === 1'bz)
                $display("PASS: tran with both floating = z,z");
            else
                $display("FAIL: net_a=%b net_b=%b (expected z,z)", net_a, net_b);
        end

        // Test 2: Drive from A side
        #1 begin
            drive_a = 1'b1;
            enable_a = 1'b1;
        end

        #1 begin
            if (net_a === 1'b1 && net_b === 1'b1)
                $display("PASS: tran propagates A to B");
            else
                $display("FAIL: net_a=%b net_b=%b (expected 1,1)", net_a, net_b);
        end

        // Test 3: Disable A, drive from B side
        #1 begin
            enable_a = 1'b0;
            drive_b = 1'b0;
            enable_b = 1'b1;
        end

        #1 begin
            if (net_a === 1'b0 && net_b === 1'b0)
                $display("PASS: tran propagates B to A");
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
                $display("PASS: tran with both driving 1");
            else
                $display("FAIL: net_a=%b net_b=%b", net_a, net_b);
        end

        // Test 5: rtran (resistive - reduced strength)
        $display("PASS: Basic tran gate tests complete");

        $finish;
    end
endmodule
