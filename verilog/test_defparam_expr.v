// EXPECT=PASS
// Test defparam with expressions
module configurable #(
    parameter BASE = 10,
    parameter MULTIPLIER = 2
) (
    output [15:0] value
);
    assign value = BASE * MULTIPLIER;
endmodule

module test_defparam_expr;
    wire [15:0] out1, out2, out3;

    configurable u1 (.value(out1));
    configurable u2 (.value(out2));
    configurable u3 (.value(out3));

    // Default: BASE=10, MULTIPLIER=2, result=20

    // Override with constant
    defparam u2.BASE = 15;
    // Result should be 15*2 = 30

    // Override both parameters
    defparam u3.BASE = 8;
    defparam u3.MULTIPLIER = 4;
    // Result should be 8*4 = 32

    initial begin
        #1 begin
            if (out1 == 16'd20)
                $display("PASS: Default params 10*2=20");
            else
                $display("FAIL: out1=%d (expected 20)", out1);

            if (out2 == 16'd30)
                $display("PASS: Defparam BASE=15, 15*2=30");
            else
                $display("FAIL: out2=%d (expected 30)", out2);

            if (out3 == 16'd32)
                $display("PASS: Defparam both params 8*4=32");
            else
                $display("FAIL: out3=%d (expected 32)", out3);
        end

        $finish;
    end
endmodule
