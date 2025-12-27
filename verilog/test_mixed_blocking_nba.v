// EXPECT=PASS
// Test mixed blocking and non-blocking in same always block
module test_mixed_blocking_nba;
    reg [7:0] a, b, c, d;
    reg clk;

    initial begin
        clk = 0;
        a = 1;
        b = 2;
        c = 3;
        d = 4;

        #1 clk = 1;
        #1 begin
            // Blocking assignment to 'a' happens immediately in active region
            // So b = a + 10 should use the NEW value of a (100)
            if (a == 8'd100)
                $display("PASS: Blocking assignment to a executed");
            else
                $display("FAIL: a=%d (expected 100)", a);

            if (b == 8'd110)
                $display("PASS: Blocking read of a got new value");
            else
                $display("FAIL: b=%d (expected 110)", b);

            // Non-blocking assignments happen in NBA region
            // c should get old value of a (1) + 20 = 21
            if (c == 8'd21)
                $display("PASS: Non-blocking read of a got old value");
            else
                $display("FAIL: c=%d (expected 21)", c);

            // d uses new value of b (110) because blocking
            if (d == 8'd115)
                $display("PASS: Non-blocking read of b got new value");
            else
                $display("FAIL: d=%d (expected 115)", d);
        end

        $finish;
    end

    always @(posedge clk) begin
        // Blocking assignments execute immediately in active region
        a = 8'd100;        // a becomes 100 NOW
        b = a + 10;        // b = 100 + 10 = 110 NOW

        // Non-blocking assignments read NOW, schedule for NBA region
        c <= a + 20;       // reads old a (1), c scheduled = 21
        d <= b + 5;        // reads new b (110), d scheduled = 115
    end
endmodule
