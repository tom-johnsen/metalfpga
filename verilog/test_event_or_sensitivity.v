// EXPECT=PASS
// Test OR'd event sensitivity (@(a or b or c))
module test_event_or_sensitivity;
    reg a, b, c;
    reg [7:0] trigger_count;

    initial begin
        a = 0;
        b = 0;
        c = 0;
        trigger_count = 0;

        #1 a = 1;  // Should trigger (count = 1)
        #1 b = 1;  // Should trigger (count = 2)
        #1 c = 1;  // Should trigger (count = 3)
        #1 a = 0;  // Should trigger (count = 4)
        #1 b = 0;  // Should trigger (count = 5)

        // Change multiple in same delta - only one trigger
        #1 begin
            a = 1;
            c = 0;
        end

        #1 begin
            if (trigger_count == 8'd6)
                $display("PASS: OR sensitivity triggered 6 times");
            else
                $display("FAIL: trigger_count=%d (expected 6)", trigger_count);
        end

        $finish;
    end

    always @(a or b or c) begin
        trigger_count = trigger_count + 1;
    end
endmodule
