// EXPECT=PASS
// Test bufif0/bufif1 tri-state buffers
module test_bufif_tristate;
    reg data, ctrl;
    wire out0, out1;

    // bufif0: passes when ctrl=0, Z when ctrl=1
    bufif0 (out0, data, ctrl);

    // bufif1: passes when ctrl=1, Z when ctrl=0
    bufif1 (out1, data, ctrl);

    initial begin
        data = 1'b0;
        ctrl = 1'b0;

        #1 begin
            // ctrl=0: bufif0 passes, bufif1 high-Z
            if (out0 === 1'b0)
                $display("PASS: bufif0 passes when ctrl=0");
            else
                $display("FAIL: out0=%b (expected 0)", out0);

            if (out1 === 1'bz)
                $display("PASS: bufif1 is Z when ctrl=0");
            else
                $display("FAIL: out1=%b (expected z)", out1);
        end

        #1 data = 1'b1;

        #1 begin
            if (out0 === 1'b1)
                $display("PASS: bufif0 passes 1 when ctrl=0");
            else
                $display("FAIL: out0=%b (expected 1)", out0);
        end

        #1 ctrl = 1'b1;

        #1 begin
            // ctrl=1: bufif0 high-Z, bufif1 passes
            if (out0 === 1'bz)
                $display("PASS: bufif0 is Z when ctrl=1");
            else
                $display("FAIL: out0=%b (expected z)", out0);

            if (out1 === 1'b1)
                $display("PASS: bufif1 passes 1 when ctrl=1");
            else
                $display("FAIL: out1=%b (expected 1)", out1);
        end

        #1 data = 1'b0;

        #1 begin
            if (out1 === 1'b0)
                $display("PASS: bufif1 passes 0 when ctrl=1");
            else
                $display("FAIL: out1=%b (expected 0)", out1);
        end

        // Test with X control
        #1 ctrl = 1'bx;

        #1 begin
            if (out0 === 1'bx || out0 === 1'bz)
                $display("PASS: bufif0 output unknown with X control");
            else
                $display("FAIL: out0=%b", out0);
        end

        $finish;
    end
endmodule
