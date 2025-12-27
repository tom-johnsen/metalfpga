// EXPECT=PASS
// Test basic defparam override
module counter #(parameter WIDTH = 4) (
    input clk,
    output reg [WIDTH-1:0] count
);
    initial count = 0;

    always @(posedge clk) begin
        count <= count + 1;
    end
endmodule

module test_defparam_basic;
    reg clk;
    wire [7:0] count8;
    wire [3:0] count4;

    // Instance with default parameter (WIDTH=4)
    counter u_default (
        .clk(clk),
        .count(count4)
    );

    // Instance with overridden parameter using defparam
    counter u_wide (
        .clk(clk),
        .count(count8)
    );

    defparam u_wide.WIDTH = 8;

    initial begin
        clk = 0;

        // Generate 3 clock cycles
        repeat(3) begin
            #5 clk = 1;
            #5 clk = 0;
        end

        #1 begin
            if (count4 == 4'd3)
                $display("PASS: Default WIDTH=4 counter at 3");
            else
                $display("FAIL: count4=%d (expected 3)", count4);

            if (count8 == 8'd3)
                $display("PASS: Defparam WIDTH=8 counter at 3");
            else
                $display("FAIL: count8=%d (expected 3)", count8);
        end

        $finish;
    end
endmodule
