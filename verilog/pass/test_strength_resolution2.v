// EXPECT=PASS
// Test strength-based resolution of multiple drivers
module test_strength_resolution;
    wire net;
    reg drive_strong, drive_pull, drive_weak;

    // Multiple drivers with different strengths
    buf (strong1, strong0) strong_driver (net, drive_strong);
    buf (pull1, pull0) pull_driver (net, drive_pull);
    buf (weak1, weak0) weak_driver (net, drive_weak);

    initial begin
        drive_strong = 1'b0;
        drive_pull = 1'b0;
        drive_weak = 1'b0;

        #1 begin
            // All drive 0 - should be 0
            if (net === 1'b0)
                $display("PASS: All drivers agree on 0");
            else
                $display("FAIL: net=%b (expected 0)", net);
        end

        // Weak tries to pull to 1, others stay 0
        #1 drive_weak = 1'b1;

        #1 begin
            // Strong and pull win over weak - should be 0
            if (net === 1'b0)
                $display("PASS: Strong/pull overpower weak");
            else
                $display("FAIL: net=%b (expected 0)", net);
        end

        // Pull tries to pull to 1, strong stays 0
        #1 drive_pull = 1'b1;

        #1 begin
            // Strong wins over pull - should be 0
            if (net === 1'b0)
                $display("PASS: Strong overpowers pull");
            else
                $display("FAIL: net=%b (expected 0)", net);
        end

        // Strong also goes to 1
        #1 drive_strong = 1'b1;

        #1 begin
            // All agree on 1 - should be 1
            if (net === 1'b1)
                $display("PASS: All drivers agree on 1");
            else
                $display("FAIL: net=%b (expected 1)", net);
        end

        // Strong to 0, pull and weak to 1
        #1 drive_strong = 1'b0;

        #1 begin
            // Strong wins - should be 0
            if (net === 1'b0)
                $display("PASS: Strong 0 wins over pull/weak 1");
            else
                $display("FAIL: net=%b (expected 0)", net);
        end

        $finish;
    end
endmodule
