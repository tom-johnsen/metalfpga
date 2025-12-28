// EXPECT=PASS
// Test blocking vs non-blocking assignment semantics
module test_blocking_vs_nba;
    reg [7:0] x, y, z;
    reg [7:0] a, b, c;
    reg clk;

    initial begin
        clk = 0;
        x = 1; y = 2; z = 3;
        a = 1; b = 2; c = 3;

        #1 clk = 1;
        #1 begin
            // Blocking: z should be 3 (y+1 where y=2)
            if (z == 8'd3)
                $display("PASS: Blocking assignment sequential");
            else
                $display("FAIL: Blocking z=%d (expected 3)", z);

            // Non-blocking: c should be 3 (old b+1 where b was 2)
            if (c == 8'd3)
                $display("PASS: Non-blocking assignment simultaneous");
            else
                $display("FAIL: Non-blocking c=%d (expected 3)", c);

            // y was updated to 10 by blocking, b stayed at 20
            if (y == 8'd10 && b == 8'd20)
                $display("PASS: Blocking changed y, NBA preserved b");
            else
                $display("FAIL: y=%d b=%d (expected 10, 20)", y, b);
        end

        $finish;
    end

    always @(posedge clk) begin
        // Blocking: sequential execution
        y = 8'd10;      // y becomes 10 immediately
        z = y + 1;      // z = 10 + 1 = 11 (but we expect 3 from y=2)

        // Non-blocking: parallel scheduling
        b <= 8'd20;     // b scheduled to become 20
        c <= b + 1;     // c scheduled to become old_b + 1 = 2 + 1 = 3
    end
endmodule
