// EXPECT=PASS
// Delayed NBA should advance time even if no other time waits exist.
module test_nba_delay_only;
    reg [7:0] a;

    initial begin
        a = 8'd0;
        a <= #5 8'd7;
    end

    initial begin
        @(a);
        if (a == 8'd7)
            $display("PASS: delayed NBA advanced time");
        else
            $display("FAIL: a=%d (expected 7)", a);
        $finish;
    end
endmodule
