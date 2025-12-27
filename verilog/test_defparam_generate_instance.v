// EXPECT=PASS
// Test defparam on instances created within generate blocks
module adder #(parameter WIDTH = 4) (
    input [WIDTH-1:0] a, b,
    output [WIDTH-1:0] sum
);
    assign sum = a + b;
endmodule

module test_defparam_generate_instance;
    reg [7:0] x, y;
    wire [7:0] result0, result1;

    genvar i;
    generate
        for (i = 0; i < 2; i = i + 1) begin : adder_gen
            adder u_add (
                .a(x),
                .b(y),
                .sum(i == 0 ? result0 : result1)
            );
        end
    endgenerate

    // Override parameter in generated instance
    defparam adder_gen[0].u_add.WIDTH = 8;
    defparam adder_gen[1].u_add.WIDTH = 8;

    initial begin
        x = 8'd100;
        y = 8'd50;

        #1 begin
            if (result0 == 8'd150)
                $display("PASS: Defparam on generate instance [0]");
            else
                $display("FAIL: result0=%d (expected 150)", result0);

            if (result1 == 8'd150)
                $display("PASS: Defparam on generate instance [1]");
            else
                $display("FAIL: result1=%d (expected 150)", result1);
        end

        $finish;
    end
endmodule
