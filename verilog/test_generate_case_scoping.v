// EXPECT=PASS
// Test generate-case scoping edge cases and variable shadowing
module counter #(parameter MODE = 0, parameter WIDTH = 8) (
    input clk,
    input rst,
    output reg [WIDTH-1:0] count
);
    generate
        case (MODE)
            0: begin : mode_up
                // Local variable in this generate scope
                localparam STEP = 1;
                always @(posedge clk or posedge rst) begin
                    if (rst)
                        count <= 0;
                    else
                        count <= count + STEP;
                end
            end

            1: begin : mode_down
                // Same name, different scope - variable shadowing
                localparam STEP = 1;
                always @(posedge clk or posedge rst) begin
                    if (rst)
                        count <= {WIDTH{1'b1}};  // Max value
                    else
                        count <= count - STEP;
                end
            end

            2: begin : mode_double
                // Different STEP value
                localparam STEP = 2;
                always @(posedge clk or posedge rst) begin
                    if (rst)
                        count <= 0;
                    else
                        count <= count + STEP;
                end
            end

            default: begin : mode_freeze
                // No STEP parameter here
                always @(posedge clk or posedge rst) begin
                    if (rst)
                        count <= 0;
                    // else: freeze (don't update)
                end
            end
        endcase
    endgenerate
endmodule

module test_generate_case_scoping;
    reg clk, rst;
    wire [7:0] cnt_up, cnt_down, cnt_double, cnt_freeze;

    counter #(.MODE(0), .WIDTH(8)) u_up (
        .clk(clk), .rst(rst), .count(cnt_up)
    );

    counter #(.MODE(1), .WIDTH(8)) u_down (
        .clk(clk), .rst(rst), .count(cnt_down)
    );

    counter #(.MODE(2), .WIDTH(8)) u_double (
        .clk(clk), .rst(rst), .count(cnt_double)
    );

    counter #(.MODE(99), .WIDTH(8)) u_freeze (
        .clk(clk), .rst(rst), .count(cnt_freeze)
    );

    initial begin
        clk = 0;
        rst = 1;
        #2 rst = 0;

        // Generate some clocks
        repeat(10) #1 clk = ~clk;

        #1 begin
            // Up counter should have incremented 5 times (STEP=1)
            if (cnt_up == 8'd5)
                $display("PASS: Generate case MODE=0 scoping (up, STEP=1)");
            else
                $display("FAIL: cnt_up=%d (expected 5)", cnt_up);

            // Down counter should have decremented 5 times from 255
            if (cnt_down == 8'd250)
                $display("PASS: Generate case MODE=1 scoping (down, STEP=1)");
            else
                $display("FAIL: cnt_down=%d (expected 250)", cnt_down);

            // Double counter should have incremented by 2 each time
            if (cnt_double == 8'd10)
                $display("PASS: Generate case MODE=2 scoping (STEP=2)");
            else
                $display("FAIL: cnt_double=%d (expected 10)", cnt_double);

            // Freeze counter should stay at 0
            if (cnt_freeze == 8'd0)
                $display("PASS: Generate case default scoping (freeze)");
            else
                $display("FAIL: cnt_freeze=%d (expected 0)", cnt_freeze);
        end

        $finish;
    end
endmodule
