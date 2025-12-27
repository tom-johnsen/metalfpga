// EXPECT=PASS
// Test basic casez with Z/? don't-care matching
module test_casez_basic;
    reg [3:0] value;
    reg [7:0] result;

    always @(*) begin
        casez (value)
            4'b1???: result = 8'd1;  // Match 1xxx (1000-1111)
            4'b01??: result = 8'd2;  // Match 01xx (0100-0111)
            4'b001?: result = 8'd3;  // Match 001x (0010-0011)
            4'b0001: result = 8'd4;  // Match 0001
            default: result = 8'd0;
        endcase
    end

    initial begin
        value = 4'b1010;
        #1 if (result == 8'd1)
            $display("PASS: casez matched 1010 with 1???");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 4'b1111;
        #1 if (result == 8'd1)
            $display("PASS: casez matched 1111 with 1???");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 4'b0110;
        #1 if (result == 8'd2)
            $display("PASS: casez matched 0110 with 01??");
        else
            $display("FAIL: result=%d (expected 2)", result);

        value = 4'b0011;
        #1 if (result == 8'd3)
            $display("PASS: casez matched 0011 with 001?");
        else
            $display("FAIL: result=%d (expected 3)", result);

        value = 4'b0001;
        #1 if (result == 8'd4)
            $display("PASS: casez matched 0001 exactly");
        else
            $display("FAIL: result=%d (expected 4)", result);

        value = 4'b0000;
        #1 if (result == 8'd0)
            $display("PASS: casez default case");
        else
            $display("FAIL: result=%d (expected 0)", result);

        $finish;
    end
endmodule
