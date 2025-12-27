// EXPECT=PASS
// Test generate creating array of module instances
module dff (
    input clk,
    input d,
    output reg q
);
    initial q = 0;
    always @(posedge clk) q <= d;
endmodule

module test_generate_array_inst;
    reg clk;
    reg [7:0] d;
    wire [7:0] q;

    genvar i;
    generate
        for (i = 0; i < 8; i = i + 1) begin : dff_array
            dff u_dff (
                .clk(clk),
                .d(d[i]),
                .q(q[i])
            );
        end
    endgenerate

    initial begin
        clk = 0;
        d = 8'b10101010;

        #5 clk = 1;
        #5 clk = 0;

        #1 begin
            if (q == 8'b10101010)
                $display("PASS: Generated array of 8 D flip-flops");
            else
                $display("FAIL: q=%b (expected 10101010)", q);
        end

        // Change data and clock again
        #1 d = 8'b11001100;
        #5 clk = 1;
        #5 clk = 0;

        #1 begin
            if (q == 8'b11001100)
                $display("PASS: DFF array updated correctly");
            else
                $display("FAIL: q=%b (expected 11001100)", q);
        end

        $finish;
    end
endmodule
