// EXPECT=PASS
// Test generate using parameter to determine loop count
module parameterized_inverter #(parameter WIDTH = 8) (
    input [WIDTH-1:0] in,
    output [WIDTH-1:0] out
);
    genvar i;
    generate
        for (i = 0; i < WIDTH; i = i + 1) begin : inv_gen
            not (out[i], in[i]);
        end
    endgenerate
endmodule

module test_generate_param_width;
    reg [3:0] data4;
    reg [7:0] data8;
    wire [3:0] out4;
    wire [7:0] out8;

    parameterized_inverter #(.WIDTH(4)) u_inv4 (
        .in(data4),
        .out(out4)
    );

    parameterized_inverter #(.WIDTH(8)) u_inv8 (
        .in(data8),
        .out(out8)
    );

    initial begin
        data4 = 4'b1010;
        data8 = 8'b11001010;

        #1 begin
            if (out4 == 4'b0101)
                $display("PASS: Generate loop count from WIDTH=4 parameter");
            else
                $display("FAIL: out4=%b (expected 0101)", out4);

            if (out8 == 8'b00110101)
                $display("PASS: Generate loop count from WIDTH=8 parameter");
            else
                $display("FAIL: out8=%b (expected 00110101)", out8);
        end

        $finish;
    end
endmodule
