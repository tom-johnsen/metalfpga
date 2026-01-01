// EXPECT=PASS
// Test multiple defparam overrides on same instance
module multi_param #(
    parameter WIDTH = 4,
    parameter DEPTH = 2,
    parameter INIT = 0
) (
    output [WIDTH-1:0] value
);
    assign value = INIT;
endmodule

module test_defparam_multiple;
    wire [7:0] out1, out2;

    multi_param u_inst1 (
        .value(out1)
    );

    multi_param u_inst2 (
        .value(out2)
    );

    // Override multiple parameters on first instance
    defparam u_inst1.WIDTH = 8;
    defparam u_inst1.INIT = 8'hAB;

    // Override different parameters on second instance
    defparam u_inst2.WIDTH = 8;
    defparam u_inst2.DEPTH = 4;
    defparam u_inst2.INIT = 8'hCD;

    initial begin
        #1 begin
            if (out1 == 8'hAB)
                $display("PASS: Multiple defparams on inst1 (AB)");
            else
                $display("FAIL: out1=%h (expected AB)", out1);

            if (out2 == 8'hCD)
                $display("PASS: Multiple defparams on inst2 (CD)");
            else
                $display("FAIL: out2=%h (expected CD)", out2);
        end

        $finish;
    end
endmodule
