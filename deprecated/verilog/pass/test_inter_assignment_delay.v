// EXPECT=PASS
// Test inter-assignment delays (#delay before statement)
module test_inter_assignment_delay;
    reg [7:0] a, b, c;

    initial begin
        a = 0;
        b = 0;
        c = 0;

        // Inter-assignment delays: wait THEN assign
        #5 a = 8'd10;    // Time 5: a = 10
        #3 b = 8'd20;    // Time 8: b = 20
        #2 c = 8'd30;    // Time 10: c = 30

        #1 begin
            // Time 11: check all values
            if (a == 8'd10 && b == 8'd20 && c == 8'd30)
                $display("PASS: Inter-assignment delays sequential");
            else
                $display("FAIL: a=%d b=%d c=%d (expected 10,20,30)", a, b, c);
        end

        // Contrast with intra-assignment delay
        a = #5 8'd100;   // Evaluate RHS now (100), assign at time 16

        // This executes immediately (time 11)
        b = a;           // b = old a (10), not 100

        #6 begin
            // Time 17: a should now be 100
            if (a == 8'd100 && b == 8'd10)
                $display("PASS: Intra-assignment delay evaluated at RHS time");
            else
                $display("FAIL: a=%d b=%d (expected 100,10)", a, b);
        end

        $finish;
    end
endmodule
