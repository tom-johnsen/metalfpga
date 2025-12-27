// EXPECT=PASS
// Test NBA with intra-assignment delays - active/NBA region interaction
module test_nba_delayed_assignment;
    reg [7:0] a, b, c;
    reg trigger;

    initial begin
        a = 8'd0;
        b = 8'd0;
        c = 8'd0;
        trigger = 0;

        // Set initial values
        #1 a = 8'd5;
        #1 b = 8'd10;

        // Trigger the always block
        #5 trigger = 1;

        // a increments during delay period
        #1 a = 8'd6;
        #1 a = 8'd7;
        #1 a = 8'd8;  // This is time 9

        #2 begin
            // b should be 5 (captured at trigger time, before increments)
            // c should be 10 (NBA without delay)
            if (b == 8'd5 && c == 8'd10)
                $display("PASS: NBA with delay captured value at trigger time");
            else
                $display("FAIL: b=%d c=%d (expected 5,10)", b, c);
        end

        $finish;
    end

    always @(posedge trigger) begin
        // NBA with delay: evaluates 'a' NOW (5), assigns after delay
        b <= #3 a;
        // Regular NBA: schedules for NBA region
        c <= b;
    end
endmodule
