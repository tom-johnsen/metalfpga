// EXPECT=PASS
// Test delta-cycle ordering between active and NBA regions
module test_delta_cycle_ordering;
    reg [7:0] x, y, z;
    reg trigger;
    integer step;

    initial begin
        x = 0;
        y = 0;
        z = 0;
        trigger = 0;
        step = 0;

        #1 trigger = 1;

        #1 begin
            // After one delta cycle with trigger=1:
            // Active region: x=10 (blocking)
            // NBA region: y=20, z=30
            if (x == 8'd10 && y == 8'd20 && z == 8'd30)
                $display("PASS: First delta cycle completed correctly");
            else
                $display("FAIL: x=%d y=%d z=%d (expected 10,20,30)", x, y, z);
        end

        #1 trigger = 0;
        #1 trigger = 1;

        #1 begin
            // Second trigger:
            // x was 10, now 40 (blocking, reads old y=20 and z=30)
            // y=100, z=110 (NBA, read old x=10)
            if (x == 8'd40 && y == 8'd100 && z == 8'd110)
                $display("PASS: Second delta cycle used old values for NBA");
            else
                $display("FAIL: x=%d y=%d z=%d (expected 40,100,110)", x, y, z);
        end

        $finish;
    end

    always @(posedge trigger) begin
        if (step == 0) begin
            // First execution
            x = 8'd10;      // Blocking: immediate
            y <= 8'd20;     // NBA: scheduled
            z <= 8'd30;     // NBA: scheduled
            step = 1;
        end else begin
            // Second execution
            x = y + z;      // Blocking: 20 + 30 = 50 (but old values!)
            y <= x * 10;    // NBA: old x (10) * 10 = 100
            z <= x + 100;   // NBA: old x (10) + 100 = 110
        end
    end
endmodule
