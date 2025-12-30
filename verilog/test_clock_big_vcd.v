// EXPECT=PASS
`timescale 1ns/1ps

module tb;
  reg clk;
  reg rst;
  reg enable;
  reg [7:0] counter;
  reg [7:0] accum;
  reg [3:0] lfsr;
  reg [15:0] shreg;

  wire [7:0] mix = counter + {4'b0, lfsr};
  wire pulse = (counter[5:0] == 6'h3f);

  initial begin
    $dumpfile("test_clock_big_vcd.vcd");
    $dumpvars(0, tb);
  end

  initial begin
    clk = 1'b0;
    rst = 1'b1;
    enable = 1'b0;
    counter = 8'h00;
    accum = 8'h00;
    lfsr = 4'h1;
    shreg = 16'h0001;

    #25 rst = 1'b0;
    repeat (400) @(posedge clk);
    $finish;
  end

  always #5 clk = ~clk;

  always @(posedge clk) begin
    if (rst) begin
      counter <= 8'h00;
      accum <= 8'h00;
      lfsr <= 4'h1;
      shreg <= 16'h0001;
      enable <= 1'b0;
    end else begin
      counter <= counter + 8'h01;
      lfsr <= {lfsr[2:0], lfsr[3] ^ lfsr[2]};
      shreg <= {shreg[14:0], shreg[15] ^ shreg[13]};
      if (counter[2:0] == 3'b111) begin
        enable <= ~enable;
      end
      if (enable) begin
        accum <= accum + mix;
      end else if (pulse) begin
        accum <= accum ^ mix;
      end else begin
        accum <= accum - 8'h01;
      end
    end
  end
endmodule
