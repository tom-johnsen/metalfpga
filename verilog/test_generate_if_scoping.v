// EXPECT=PASS
// Test generate-if scoping edge cases with conditional block access
module adder_mux #(
    parameter HAS_CARRY = 1,
    parameter HAS_OVERFLOW = 0,
    parameter WIDTH = 8
) (
    input [WIDTH-1:0] a,
    input [WIDTH-1:0] b,
    input cin,
    output [WIDTH-1:0] sum,
    output cout,
    output overflow
);
    generate
        if (HAS_CARRY) begin : with_carry
            // This scope only exists if HAS_CARRY=1
            wire [WIDTH:0] full_sum;
            assign full_sum = a + b + cin;
            assign sum = full_sum[WIDTH-1:0];
            assign cout = full_sum[WIDTH];
        end else begin : no_carry
            // This scope only exists if HAS_CARRY=0
            assign sum = a + b;
            assign cout = 1'b0;
        end

        if (HAS_OVERFLOW) begin : with_overflow
            // Overflow detection logic
            wire sign_a, sign_b, sign_sum;
            assign sign_a = a[WIDTH-1];
            assign sign_b = b[WIDTH-1];
            assign sign_sum = sum[WIDTH-1];
            assign overflow = (sign_a == sign_b) && (sign_a != sign_sum);
        end else begin : no_overflow
            assign overflow = 1'b0;
        end
    endgenerate
endmodule

module test_generate_if_scoping;
    reg [7:0] x, y;
    reg ci;
    wire [7:0] sum1, sum2, sum3;
    wire co1, co2, co3;
    wire ovf1, ovf2, ovf3;

    // Instance with both features
    adder_mux #(.HAS_CARRY(1), .HAS_OVERFLOW(1), .WIDTH(8)) u1 (
        .a(x), .b(y), .cin(ci),
        .sum(sum1), .cout(co1), .overflow(ovf1)
    );

    // Instance with carry but no overflow
    adder_mux #(.HAS_CARRY(1), .HAS_OVERFLOW(0), .WIDTH(8)) u2 (
        .a(x), .b(y), .cin(ci),
        .sum(sum2), .cout(co2), .overflow(ovf2)
    );

    // Instance with neither
    adder_mux #(.HAS_CARRY(0), .HAS_OVERFLOW(0), .WIDTH(8)) u3 (
        .a(x), .b(y), .cin(ci),
        .sum(sum3), .cout(co3), .overflow(ovf3)
    );

    initial begin
        x = 8'd100;
        y = 8'd50;
        ci = 1'b0;

        #1 begin
            if (sum1 == 8'd150 && co1 == 1'b0)
                $display("PASS: Generate-if with_carry scope works");
            else
                $display("FAIL: sum1=%d co1=%b", sum1, co1);

            if (ovf1 == 1'b0)
                $display("PASS: Generate-if with_overflow scope works");
            else
                $display("FAIL: ovf1=%b", ovf1);
        end

        // Test carry
        #1 begin
            x = 8'd200;
            y = 8'd100;
            ci = 1'b1;
        end

        #1 begin
            // 200 + 100 + 1 = 301 = 0x12D, sum=0x2D=45, carry=1
            if (sum1 == 8'd45 && co1 == 1'b1)
                $display("PASS: Carry generation in with_carry scope");
            else
                $display("FAIL: sum1=%d co1=%b (expected 45,1)", sum1, co1);

            if (sum3 == 8'd44 && co3 == 1'b0)
                $display("PASS: No carry in no_carry scope");
            else
                $display("FAIL: sum3=%d co3=%b", sum3, co3);
        end

        // Test overflow detection
        #1 begin
            x = 8'sd127;  // Max positive in signed
            y = 8'sd1;
            ci = 1'b0;
        end

        #1 begin
            // 127 + 1 = 128 = -128 in signed (overflow!)
            if (ovf1 == 1'b1)
                $display("PASS: Overflow detected in with_overflow scope");
            else
                $display("FAIL: ovf1=%b (expected 1)", ovf1);

            if (ovf2 == 1'b0)
                $display("PASS: No overflow in no_overflow scope");
            else
                $display("FAIL: ovf2=%b (expected 0)", ovf2);
        end

        $finish;
    end
endmodule
