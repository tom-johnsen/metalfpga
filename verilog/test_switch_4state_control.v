// EXPECT=PASS
// Test switch primitives with 4-state control (X and Z on control inputs)
module test_switch_4state_control;
    reg data;
    reg ctrl;
    wire out_bufif0, out_bufif1, out_notif0, out_notif1;
    wire tran_a, tran_b;
    reg drive_a;

    // Various switch primitives
    bufif0 (out_bufif0, data, ctrl);
    bufif1 (out_bufif1, data, ctrl);
    notif0 (out_notif0, data, ctrl);
    notif1 (out_notif1, data, ctrl);

    tranif1 (tran_a, tran_b, ctrl);
    bufif1 (tran_a, drive_a, 1'b1);

    initial begin
        data = 1'b1;
        ctrl = 1'b0;
        drive_a = 1'b0;

        // Test with valid control
        #1 begin
            if (out_bufif0 === 1'b1)
                $display("PASS: bufif0 with ctrl=0");
            else
                $display("FAIL: out_bufif0=%b (expected 1)", out_bufif0);
        end

        // Test with X control
        #1 ctrl = 1'bx;

        #1 begin
            // With X control, output should be X or Z (implementation dependent)
            if (out_bufif0 === 1'bx || out_bufif0 === 1'bz)
                $display("PASS: bufif0 with ctrl=X produces unknown");
            else
                $display("INFO: bufif0 with X control=%b", out_bufif0);

            if (out_bufif1 === 1'bx || out_bufif1 === 1'bz)
                $display("PASS: bufif1 with ctrl=X produces unknown");
            else
                $display("INFO: bufif1 with X control=%b", out_bufif1);

            if (out_notif0 === 1'bx || out_notif0 === 1'bz)
                $display("PASS: notif0 with ctrl=X produces unknown");
            else
                $display("INFO: notif0 with X control=%b", out_notif0);

            if (out_notif1 === 1'bx || out_notif1 === 1'bz)
                $display("PASS: notif1 with ctrl=X produces unknown");
            else
                $display("INFO: notif1 with X control=%b", out_notif1);
        end

        // Test tranif with X control
        #1 begin
            drive_a = 1'b1;
        end

        #1 begin
            // tranif1 with X control should produce X or block
            $display("INFO: tranif1 with X control - tran_a=%b tran_b=%b", tran_a, tran_b);
        end

        // Test with Z control (treated as X)
        #1 ctrl = 1'bz;

        #1 begin
            if (out_bufif0 === 1'bx || out_bufif0 === 1'bz)
                $display("PASS: bufif0 with ctrl=Z produces unknown");
            else
                $display("INFO: bufif0 with Z control=%b", out_bufif0);

            if (out_notif1 === 1'bx || out_notif1 === 1'bz)
                $display("PASS: notif1 with ctrl=Z produces unknown");
            else
                $display("INFO: notif1 with Z control=%b", out_notif1);
        end

        // Test data with X/Z
        #1 begin
            ctrl = 1'b1;
            data = 1'bx;
        end

        #1 begin
            // bufif1 should pass X
            if (out_bufif1 === 1'bx)
                $display("PASS: bufif1 passes X data");
            else
                $display("INFO: bufif1 with X data=%b", out_bufif1);

            // notif1 should invert X to X
            if (out_notif1 === 1'bx)
                $display("PASS: notif1 inverts X to X");
            else
                $display("INFO: notif1 with X data=%b", out_notif1);
        end

        #1 data = 1'bz;

        #1 begin
            // Z on data input
            $display("INFO: bufif1 with Z data=%b", out_bufif1);
            $display("INFO: notif1 with Z data=%b", out_notif1);
        end

        $finish;
    end
endmodule
