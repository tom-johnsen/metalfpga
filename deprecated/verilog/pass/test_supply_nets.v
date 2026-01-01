// EXPECT=PASS
// Test supply0/supply1 net types (constant power/ground)
module test_supply_nets;
    supply0 gnd;  // Always 0 (ground)
    supply1 vdd;  // Always 1 (power)
    wire out_and, out_or;

    // Use supply nets in logic
    and (out_and, vdd, gnd);  // 1 & 0 = 0
    or (out_or, vdd, gnd);    // 1 | 0 = 1

    initial begin
        #1 begin
            if (gnd === 1'b0)
                $display("PASS: supply0 is 0");
            else
                $display("FAIL: gnd=%b (expected 0)", gnd);

            if (vdd === 1'b1)
                $display("PASS: supply1 is 1");
            else
                $display("FAIL: vdd=%b (expected 1)", vdd);

            if (out_and === 1'b0)
                $display("PASS: vdd AND gnd = 0");
            else
                $display("FAIL: out_and=%b (expected 0)", out_and);

            if (out_or === 1'b1)
                $display("PASS: vdd OR gnd = 1");
            else
                $display("FAIL: out_or=%b (expected 1)", out_or);
        end

        $finish;
    end
endmodule
