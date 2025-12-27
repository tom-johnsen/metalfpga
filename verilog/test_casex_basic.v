// EXPECT=PASS
// Test basic casex with X/Z/? don't-care matching
module test_casex_basic;
    reg [3:0] value;
    reg [7:0] result;

    always @(*) begin
        casex (value)
            4'b1xxx: result = 8'd1;  // Match 1xxx (1000-1111)
            4'b01xx: result = 8'd2;  // Match 01xx (0100-0111)
            4'b001x: result = 8'd3;  // Match 001x (0010-0011)
            4'b0001: result = 8'd4;  // Match 0001
            default: result = 8'd0;
        endcase
    end

    initial begin
        value = 4'b1010;
        #1 if (result == 8'd1)
            $display("PASS: casex matched 1010 with 1xxx");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 4'b0101;
        #1 if (result == 8'd2)
            $display("PASS: casex matched 0101 with 01xx");
        else
            $display("FAIL: result=%d (expected 2)", result);

        value = 4'b0010;
        #1 if (result == 8'd3)
            $display("PASS: casex matched 0010 with 001x");
        else
            $display("FAIL: result=%d (expected 3)", result);

        value = 4'b0001;
        #1 if (result == 8'd4)
            $display("PASS: casex matched 0001 exactly");
        else
            $display("FAIL: result=%d (expected 4)", result);

        $finish;
    end
endmodule
