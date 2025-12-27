// EXPECT=PASS
// Test intra-assignment delays (a = #delay expr)
module test_intra_assignment_delay;
    reg [7:0] counter;
    reg [7:0] snapshot;
    reg trigger;

    initial begin
        counter = 0;
        snapshot = 0;
        trigger = 0;

        #5 trigger = 1;  // Start the always block

        // Counter increments every 1 time unit
        // But snapshot captures value with #3 delay
        #1 counter = 1;
        #1 counter = 2;
        #1 counter = 3;  // This is when snapshot RHS was evaluated
        #1 counter = 4;
        #1 counter = 5;
        #1 counter = 6;  // This is when snapshot gets assigned

        #1 begin
            // snapshot should be 3 (evaluated at time 8, assigned at time 11)
            if (snapshot == 8'd3)
                $display("PASS: Intra-assignment delay captured at evaluation time");
            else
                $display("FAIL: snapshot=%d (expected 3)", snapshot);
        end

        $finish;
    end

    always @(posedge trigger) begin
        // Evaluate counter immediately, but assign after 3 time units
        snapshot = #3 counter;  // counter is 0 at time 5, snapshot assigned at time 8
    end
endmodule
