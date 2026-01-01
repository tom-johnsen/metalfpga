// EXPECT=PASS
// Test: Wait statement with complex conditions
// Feature: wait() with combinational expressions
// Expected: May fail - complex wait conditions

module test_wait_condition;
  reg [7:0] a, b;
  reg done;

  initial begin
    a = 0;
    b = 0;
    done = 0;

    fork
      begin
        #10 a = 8'h5A;
        #10 b = 8'hA5;
      end

      begin
        wait (a == 8'h5A && b == 8'hA5);  // Wait for both conditions
        done = 1;
      end
    join
  end
endmodule
