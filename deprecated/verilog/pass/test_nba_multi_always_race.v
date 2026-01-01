// EXPECT=PASS
// Test NBA ordering with multiple always blocks and race conditions
module test_nba_multi_always_race;
    reg [7:0] a, b, c;
    reg clk;
    integer phase;

    initial begin
        clk = 0;
        a = 8'd1;
        b = 8'd2;
        c = 8'd3;
        phase = 0;

        // Test 1: Multiple always blocks scheduling NBAs simultaneously
        #1 clk = 1;
        #1 begin
            // All three NBAs from different blocks should execute in NBA region
            // Order within NBA region should be deterministic
            if (a == 8'd10 && b == 8'd20 && c == 8'd30)
                $display("PASS: Multi-always NBA simultaneous update");
            else
                $display("FAIL: a=%d b=%d c=%d (expected 10,20,30)", a, b, c);
        end

        // Test 2: NBA reading values written by blocking in another always
        #1 clk = 0;
        #1 phase = 1;
        #1 clk = 1;
        #1 begin
            // always_1 does blocking write to 'a'
            // always_2 NBA reads 'a' - should see NEW value (blocking happens in active region)
            // always_3 NBA reads 'b' - should see OLD value (NBA hasn't executed yet)
            if (a == 8'd100 && b == 8'd100 && c == 8'd20)
                $display("PASS: NBA reads blocking assignment from other always");
            else
                $display("FAIL: a=%d b=%d c=%d (expected 100,100,20)", a, b, c);
        end

        $finish;
    end

    // Multiple always blocks create race conditions
    always @(posedge clk) begin
        if (phase == 0) begin
            a <= 8'd10;
        end else if (phase == 1) begin
            a = 8'd100;  // Blocking assignment in active region
        end
    end

    always @(posedge clk) begin
        if (phase == 0) begin
            b <= 8'd20;
        end else if (phase == 1) begin
            b <= a;  // NBA reads 'a' - should see blocking write from always_1
        end
    end

    always @(posedge clk) begin
        if (phase == 0) begin
            c <= 8'd30;
        end else if (phase == 1) begin
            c <= b;  // NBA reads old 'b' (20), not the new NBA value
        end
    end
endmodule
