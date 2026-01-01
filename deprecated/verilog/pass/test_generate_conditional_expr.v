// EXPECT=PASS
// Test generate if with complex conditional expressions
module test_generate_conditional_expr;
    parameter P1 = 5;
    parameter P2 = 10;
    parameter P3 = 3;

    wire [7:0] result1, result2, result3, result4;
    reg [7:0] data;

    // Test arithmetic comparison
    generate
        if (P1 + P2 > 12) begin : arith_true
            assign result1 = data + 8'd1;
        end else begin : arith_false
            assign result1 = data - 8'd1;
        end
    endgenerate

    // Test logical AND
    generate
        if ((P1 > 3) && (P2 < 15)) begin : logical_true
            assign result2 = data * 8'd2;
        end else begin : logical_false
            assign result2 = data / 8'd2;
        end
    endgenerate

    // Test modulo operation
    generate
        if (P2 % P1 == 0) begin : mod_true
            assign result3 = data & 8'hF0;
        end else begin : mod_false
            assign result3 = data | 8'h0F;
        end
    endgenerate

    // Test nested expression
    generate
        if ((P1 * P3) < (P2 + P3)) begin : nested_true
            assign result4 = data ^ 8'hFF;
        end else begin : nested_false
            assign result4 = data;
        end
    endgenerate

    initial begin
        data = 8'd10;

        #1 begin
            // P1+P2 = 15 > 12, should add 1
            if (result1 == 8'd11)
                $display("PASS: Arithmetic comparison in generate if");
            else
                $display("FAIL: result1=%d (expected 11)", result1);

            // (5>3) && (10<15) = true, should multiply by 2
            if (result2 == 8'd20)
                $display("PASS: Logical AND in generate if");
            else
                $display("FAIL: result2=%d (expected 20)", result2);

            // 10%5 = 0, should AND with F0
            if (result3 == 8'h00)
                $display("PASS: Modulo in generate if");
            else
                $display("FAIL: result3=%h (expected 00)", result3);

            // (5*3)=15 < (10+3)=13 is false, should keep data
            if (result4 == 8'd10)
                $display("PASS: Nested expression in generate if");
            else
                $display("FAIL: result4=%d (expected 10)", result4);
        end

        $finish;
    end
endmodule
