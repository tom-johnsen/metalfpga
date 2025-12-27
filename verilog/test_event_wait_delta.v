// EXPECT=PASS
// Test event waiting and delta-cycle semantics
module test_event_wait_delta;
    reg [7:0] value;
    event data_ready;
    event processing_done;

    initial begin
        value = 0;

        // Producer: set value and trigger event
        #10 begin
            value = 8'd42;
            -> data_ready;
        end

        #20 begin
            if (value == 8'd100)
                $display("PASS: Event communication worked");
            else
                $display("FAIL: value=%d (expected 100)", value);
        end

        $finish;
    end

    // Consumer: wait for event, process data
    initial begin
        @(data_ready);  // Wait for producer

        if (value == 8'd42)
            $display("PASS: Received value 42");
        else
            $display("FAIL: Received value=%d (expected 42)", value);

        // Process: multiply by 2 and add 16
        value = value * 2 + 16;  // 42*2 + 16 = 100

        -> processing_done;
    end
endmodule
