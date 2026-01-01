// EXPECT=PASS
// Test: Forever loop with disable
// Feature: Breaking out of forever loop

module test_forever_disable;
  reg clk;
  reg [7:0] counter;

  initial begin : main_block
    clk = 0;
    counter = 0;

    fork
      begin : clock_gen
        forever #5 clk = ~clk;
      end

      begin : counter_block
        forever @(posedge clk) begin
          counter = counter + 1;
          if (counter == 10)
            disable clock_gen;  // Stop clock after 10 cycles
        end
      end
    join
  end
endmodule
