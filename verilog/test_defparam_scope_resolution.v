// EXPECT=PASS
// Test defparam scope resolution with similar names
module config #(parameter VALUE = 10) (
    output [7:0] out
);
    assign out = VALUE;
endmodule

module block_a (
    output [7:0] result
);
    config u_cfg (.out(result));
endmodule

module block_b (
    output [7:0] result
);
    config u_cfg (.out(result));
endmodule

module test_defparam_scope_resolution;
    wire [7:0] out_a, out_b;

    block_a u_a (.result(out_a));
    block_b u_b (.result(out_b));

    // Override same module name in different scopes
    defparam u_a.u_cfg.VALUE = 25;
    defparam u_b.u_cfg.VALUE = 75;

    initial begin
        #1 begin
            if (out_a == 8'd25)
                $display("PASS: Defparam scope resolution block_a");
            else
                $display("FAIL: out_a=%d (expected 25)", out_a);

            if (out_b == 8'd75)
                $display("PASS: Defparam scope resolution block_b");
            else
                $display("FAIL: out_b=%d (expected 75)", out_b);
        end

        $finish;
    end
endmodule
