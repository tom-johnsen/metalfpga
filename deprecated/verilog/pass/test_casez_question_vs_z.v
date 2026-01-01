// EXPECT=PASS
// Test that ? and z are equivalent in casez patterns
module test_casez_question_vs_z;
    reg [3:0] value;
    reg [7:0] result_question, result_z;

    // Using ? for don't-care
    always @(*) begin
        casez (value)
            4'b11??: result_question = 8'd1;
            4'b10??: result_question = 8'd2;
            default: result_question = 8'd0;
        endcase
    end

    // Using z for don't-care
    always @(*) begin
        casez (value)
            4'b11zz: result_z = 8'd1;
            4'b10zz: result_z = 8'd2;
            default: result_z = 8'd0;
        endcase
    end

    initial begin
        value = 4'b1100;
        #1 begin
            if (result_question == result_z && result_question == 8'd1)
                $display("PASS: ? and z equivalent for 1100");
            else
                $display("FAIL: question=%d z=%d", result_question, result_z);
        end

        value = 4'b1111;
        #1 begin
            if (result_question == result_z && result_question == 8'd1)
                $display("PASS: ? and z equivalent for 1111");
            else
                $display("FAIL: question=%d z=%d", result_question, result_z);
        end

        value = 4'b1010;
        #1 begin
            if (result_question == result_z && result_question == 8'd2)
                $display("PASS: ? and z equivalent for 1010");
            else
                $display("FAIL: question=%d z=%d", result_question, result_z);
        end

        value = 4'b0000;
        #1 begin
            if (result_question == result_z && result_question == 8'd0)
                $display("PASS: ? and z equivalent for default case");
            else
                $display("FAIL: question=%d z=%d", result_question, result_z);
        end

        $finish;
    end
endmodule
