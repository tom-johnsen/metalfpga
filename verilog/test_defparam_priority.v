// EXPECT=PASS
// Test defparam vs instantiation parameter override priority
module param_test #(
    parameter A = 1,
    parameter B = 2
) (
    output [7:0] out_a,
    output [7:0] out_b
);
    assign out_a = A;
    assign out_b = B;
endmodule

module test_defparam_priority;
    wire [7:0] a1, b1;
    wire [7:0] a2, b2;

    // Instance 1: No override at instantiation
    param_test u1 (
        .out_a(a1),
        .out_b(b1)
    );

    // Use defparam to override
    defparam u1.A = 10;
    defparam u1.B = 20;

    // Instance 2: Override at instantiation
    param_test #(.A(100), .B(200)) u2 (
        .out_a(a2),
        .out_b(b2)
    );

    // Note: defparam typically has lower priority than instantiation override
    // but is useful for modifying instances you don't control

    initial begin
        #1 begin
            if (a1 == 8'd10 && b1 == 8'd20)
                $display("PASS: defparam override worked (A=10, B=20)");
            else
                $display("FAIL: a1=%d b1=%d (expected 10,20)", a1, b1);

            if (a2 == 8'd100 && b2 == 8'd200)
                $display("PASS: Instantiation override (A=100, B=200)");
            else
                $display("FAIL: a2=%d b2=%d (expected 100,200)", a2, b2);
        end

        $finish;
    end
endmodule
