// EXPECT=PASS
// Test non-blocking assignment ordering and NBA region semantics
module test_nba_ordering;
    reg [7:0] a, b, c, d;
    reg clk;

    initial begin
        clk = 0;
        a = 8'd10;
        b = 8'd20;
        c = 8'd30;
        d = 8'd40;

        // Test 1: NBA swap - should swap values
        #1 clk = 1;
        #1 begin
            if (a == 8'd20 && b == 8'd10)
                $display("PASS: NBA swap worked");
            else
                $display("FAIL: NBA swap failed - a=%d b=%d", a, b);
        end

        // Test 2: NBA chain - all assignments happen simultaneously
        #1 clk = 0;
        #1 clk = 1;
        #1 begin
            if (a == 8'd30 && b == 8'd40 && c == 8'd10 && d == 8'd20)
                $display("PASS: NBA chain simultaneous");
            else
                $display("FAIL: NBA chain - a=%d b=%d c=%d d=%d", a, b, c, d);
        end

        $finish;
    end

    always @(posedge clk) begin
        // First test: simple swap
        if (a == 8'd10) begin
            a <= b;  // a gets old value of b (20)
            b <= a;  // b gets old value of a (10)
        end
        // Second test: circular chain
        else if (a == 8'd20) begin
            a <= c;  // 30
            b <= d;  // 40
            c <= a;  // old a = 20
            d <= b;  // old b = 10
        end
    end
endmodule
