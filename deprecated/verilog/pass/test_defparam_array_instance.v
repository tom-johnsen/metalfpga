// EXPECT=PASS
// Test defparam on array of instances
module shifter #(parameter SHIFT_AMT = 1) (
    input [7:0] in,
    output [7:0] out
);
    assign out = in << SHIFT_AMT;
endmodule

module test_defparam_array_instance;
    reg [7:0] data;
    wire [7:0] out0, out1, out2, out3;

    shifter u_shift[0:3] (
        .in({data, data, data, data}),
        .out({out0, out1, out2, out3})
    );

    // Override each instance with different shift amount
    defparam u_shift[0].SHIFT_AMT = 0;
    defparam u_shift[1].SHIFT_AMT = 1;
    defparam u_shift[2].SHIFT_AMT = 2;
    defparam u_shift[3].SHIFT_AMT = 3;

    initial begin
        data = 8'd5;  // Binary: 00000101

        #1 begin
            if (out0 == 8'd5)  // 5 << 0 = 5
                $display("PASS: Defparam array instance [0] shift=0");
            else
                $display("FAIL: out0=%d (expected 5)", out0);

            if (out1 == 8'd10)  // 5 << 1 = 10
                $display("PASS: Defparam array instance [1] shift=1");
            else
                $display("FAIL: out1=%d (expected 10)", out1);

            if (out2 == 8'd20)  // 5 << 2 = 20
                $display("PASS: Defparam array instance [2] shift=2");
            else
                $display("FAIL: out2=%d (expected 20)", out2);

            if (out3 == 8'd40)  // 5 << 3 = 40
                $display("PASS: Defparam array instance [3] shift=3");
            else
                $display("FAIL: out3=%d (expected 40)", out3);
        end

        $finish;
    end
endmodule
