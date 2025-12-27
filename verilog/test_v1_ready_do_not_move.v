// EXPECT=PASS
// test_v1_ready_do_not_move.v
//
// The official v1.0 readiness test for metalfpga.
//
// This is the minimal design that proves the entire pipeline works end-to-end:
// - Verilog parsing and elaboration
// - MSL codegen
// - Host emitter
// - GPU kernel execution
// - Service record infrastructure ($display, $dumpvars)
// - VCD waveform output
//
// If this compiles, runs on GPU, increments correctly, and dumps a valid VCD:
// **That's v1.0.**
//
// DO NOT MOVE OR MODIFY THIS FILE.

`timescale 1ns/1ps

module counter(
    input wire clk,
    input wire rst,
    output reg [7:0] count
);
    // Simple 8-bit counter with synchronous reset
    always @(posedge clk or posedge rst) begin
        if (rst)
            count <= 8'd0;
        else
            count <= count + 8'd1;
    end

    // Test infrastructure: display count every cycle
    always @(posedge clk) begin
        $display("Time: %t | Count: %d (0x%h)", $time, count, count);
    end

    // VCD dump for waveform verification
    initial begin
        $dumpfile("counter_v1.vcd");
        $dumpvars(0, counter);
    end
endmodule

// Testbench for standalone validation
module tb_counter;
    reg clk;
    reg rst;
    wire [7:0] count;

    // Instantiate counter
    counter dut (
        .clk(clk),
        .rst(rst),
        .count(count)
    );

    // Clock generation: 10ns period (100 MHz)
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    // Test sequence
    initial begin
        $display("=== metalfpga v1.0 Readiness Test ===");
        $display("Design: 8-bit counter with reset");

        // Reset pulse
        rst = 1;
        #20;
        rst = 0;

        // Run for 300ns (30 clock cycles)
        #300;

        // Verify count reached at least 25
        if (count >= 8'd25) begin
            $display("PASS: Counter incremented correctly (count = %d)", count);
        end else begin
            $display("FAIL: Counter did not increment as expected (count = %d)", count);
        end

        $display("=== Test Complete ===");
        $finish;
    end
endmodule
