// EXPECT=PASS
// Test generate if with parameters
module adder_or_subtracter #(parameter IS_ADDER = 1) (
    input [7:0] a,
    input [7:0] b,
    output [7:0] result
);
    generate
        if (IS_ADDER) begin : adder_block
            assign result = a + b;
        end else begin : subtracter_block
            assign result = a - b;
        end
    endgenerate
endmodule

module test_generate_if_param;
    reg [7:0] x, y;
    wire [7:0] sum, diff;

    // Instantiate as adder
    adder_or_subtracter #(.IS_ADDER(1)) u_add (
        .a(x),
        .b(y),
        .result(sum)
    );

    // Instantiate as subtracter
    adder_or_subtracter #(.IS_ADDER(0)) u_sub (
        .a(x),
        .b(y),
        .result(diff)
    );

    initial begin
        x = 8'd50;
        y = 8'd30;

        #1 begin
            if (sum == 8'd80)
                $display("PASS: Generate if created adder (50+30=80)");
            else
                $display("FAIL: sum=%d (expected 80)", sum);

            if (diff == 8'd20)
                $display("PASS: Generate if created subtracter (50-30=20)");
            else
                $display("FAIL: diff=%d (expected 20)", diff);
        end

        $finish;
    end
endmodule
