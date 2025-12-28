// EXPECT=PASS
// Test wired-AND (wand) and wired-OR (wor) net types
module test_wired_logic;
    reg a, b, c;
    wand net_and;  // Wired-AND: result is AND of all drivers
    wor net_or;    // Wired-OR: result is OR of all drivers

    // Multiple drivers on wand
    buf (net_and, a);
    buf (net_and, b);

    // Multiple drivers on wor
    buf (net_or, a);
    buf (net_or, c);

    initial begin
        a = 1'b1;
        b = 1'b1;
        c = 1'b0;

        #1 begin
            // wand: 1 & 1 = 1
            if (net_and === 1'b1)
                $display("PASS: wand with both 1 = 1");
            else
                $display("FAIL: net_and=%b (expected 1)", net_and);

            // wor: 1 | 0 = 1
            if (net_or === 1'b1)
                $display("PASS: wor with 1,0 = 1");
            else
                $display("FAIL: net_or=%b (expected 1)", net_or);
        end

        // One driver goes to 0
        #1 b = 1'b0;

        #1 begin
            // wand: 1 & 0 = 0
            if (net_and === 1'b0)
                $display("PASS: wand with 1,0 = 0");
            else
                $display("FAIL: net_and=%b (expected 0)", net_and);
        end

        // Both wor drivers to 0
        #1 begin
            a = 1'b0;
            c = 1'b0;
        end

        #1 begin
            // wand: 0 & 0 = 0
            if (net_and === 1'b0)
                $display("PASS: wand with both 0 = 0");
            else
                $display("FAIL: net_and=%b (expected 0)", net_and);

            // wor: 0 | 0 = 0
            if (net_or === 1'b0)
                $display("PASS: wor with both 0 = 0");
            else
                $display("FAIL: net_or=%b (expected 0)", net_or);
        end

        $finish;
    end
endmodule
