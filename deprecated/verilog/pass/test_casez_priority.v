// EXPECT=PASS
// Test casez priority - first matching case wins
module test_casez_priority;
    reg [3:0] value;
    reg [7:0] result;

    always @(*) begin
        casez (value)
            4'b1???: result = 8'd1;  // Broader pattern first
            4'b11??: result = 8'd2;  // More specific pattern second
            4'b111?: result = 8'd3;  // Even more specific
            4'b1111: result = 8'd4;  // Most specific
            default: result = 8'd0;
        endcase
    end

    initial begin
        // Should match first case (1???) even though later cases also match
        value = 4'b1111;
        #1 if (result == 8'd1)
            $display("PASS: casez priority - first match wins (1111 -> 1???)");
        else
            $display("FAIL: result=%d (expected 1, first match)", result);

        value = 4'b1000;
        #1 if (result == 8'd1)
            $display("PASS: casez priority - 1000 matches 1???");
        else
            $display("FAIL: result=%d (expected 1)", result);

        value = 4'b0111;
        #1 if (result == 8'd0)
            $display("PASS: casez no match - goes to default");
        else
            $display("FAIL: result=%d (expected 0)", result);

        $finish;
    end
endmodule
