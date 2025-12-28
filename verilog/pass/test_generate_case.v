// EXPECT=PASS
// Test generate case statement
module alu #(parameter MODE = 0) (
    input [7:0] a,
    input [7:0] b,
    output [7:0] result
);
    generate
        case (MODE)
            0: begin : mode_add
                assign result = a + b;
            end
            1: begin : mode_sub
                assign result = a - b;
            end
            2: begin : mode_and
                assign result = a & b;
            end
            3: begin : mode_or
                assign result = a | b;
            end
            default: begin : mode_xor
                assign result = a ^ b;
            end
        endcase
    endgenerate
endmodule

module test_generate_case;
    reg [7:0] x, y;
    wire [7:0] out_add, out_sub, out_and, out_or, out_xor;

    alu #(.MODE(0)) u_add (.a(x), .b(y), .result(out_add));
    alu #(.MODE(1)) u_sub (.a(x), .b(y), .result(out_sub));
    alu #(.MODE(2)) u_and (.a(x), .b(y), .result(out_and));
    alu #(.MODE(3)) u_or  (.a(x), .b(y), .result(out_or));
    alu #(.MODE(99)) u_xor (.a(x), .b(y), .result(out_xor));  // default case

    initial begin
        x = 8'b11001100;
        y = 8'b10101010;

        #1 begin
            if (out_add == (x + y))
                $display("PASS: Generate case MODE=0 (add)");
            else
                $display("FAIL: out_add=%b", out_add);

            if (out_sub == (x - y))
                $display("PASS: Generate case MODE=1 (sub)");
            else
                $display("FAIL: out_sub=%b", out_sub);

            if (out_and == (x & y))
                $display("PASS: Generate case MODE=2 (and)");
            else
                $display("FAIL: out_and=%b", out_and);

            if (out_or == (x | y))
                $display("PASS: Generate case MODE=3 (or)");
            else
                $display("FAIL: out_or=%b", out_or);

            if (out_xor == (x ^ y))
                $display("PASS: Generate case default (xor)");
            else
                $display("FAIL: out_xor=%b", out_xor);
        end

        $finish;
    end
endmodule
