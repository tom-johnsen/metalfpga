// Resistor Power Dissipation Testbench
// Simulates a resistor being driven by a sine wave voltage source
// Calculates instantaneous power: P = V^2 / R
// Samples at 100x the sine wave frequency

`timescale 1ns/1ps

module resistor #(
    parameter real RESISTANCE = 1000.0  // Ohms
) (
    input wire clk,
    input real voltage_in,      // Voltage in volts
    output real current_out,    // Current in amperes
    output real power_out       // Power in watts
);
    always @(posedge clk) begin
        // Ohm's law: I = V / R
        current_out = voltage_in / RESISTANCE;

        // Power: P = V^2 / R = V * I
        power_out = voltage_in * current_out;
    end
endmodule

module sine_generator #(
    parameter real AMPLITUDE = 10.0,      // Peak amplitude in volts
    parameter real FREQUENCY = 1000.0,    // Frequency in Hz
    parameter integer SAMPLES_PER_CYCLE = 100
) (
    input wire clk,
    input wire reset,
    output real sine_out                   // Voltage output
);
    // Sine wave lookup table (one complete cycle, 100 samples)
    real sine_lut [0:99];
    integer sample_index;
    // Pre-computed sine table scaled to amplitude
    initial begin
        integer i;
        real angle;
        real two_pi;
        two_pi = 2.0 * $acos(-1.0);  // 2π calculated from arccos(-1) = π
        for (i = 0; i < 100; i = i + 1) begin
            angle = (i * two_pi) / 100.0;
            sine_lut[i] = AMPLITUDE * $sin(angle);
        end
    end

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            sample_index = 0;
            sine_out = 0.0;
        end else begin
            sine_out = sine_lut[sample_index];

            if (sample_index == SAMPLES_PER_CYCLE - 1)
                sample_index = 0;
            else
                sample_index = sample_index + 1;
        end
    end
endmodule

module testbench_resistor_power;
    // Parameters
    parameter real SINE_FREQ = 60.0;        // 60 Hz AC mains
    parameter real SINE_AMPLITUDE = 170.0;  // Peak voltage (120V RMS * sqrt(2) ≈ 170V)
    parameter real RESISTANCE = 100.0;      // 100 Ohm resistor
    parameter integer SAMPLES_PER_CYCLE = 100;
    parameter real SAMPLE_FREQ = SINE_FREQ * SAMPLES_PER_CYCLE;  // 6 kHz
    parameter real SAMPLE_PERIOD_NS = 1.0e9 / SAMPLE_FREQ;       // ~166.67 ns

    // Signals (all real)
    reg clk;
    reg reset;
    real voltage;
    real current;
    real power;

    // Instantiate sine generator
    sine_generator #(
        .AMPLITUDE(SINE_AMPLITUDE),
        .FREQUENCY(SINE_FREQ),
        .SAMPLES_PER_CYCLE(SAMPLES_PER_CYCLE)
    ) sine_gen (
        .clk(clk),
        .reset(reset),
        .sine_out(voltage)
    );

    // Instantiate resistor
    resistor #(
        .RESISTANCE(RESISTANCE)
    ) dut (
        .clk(clk),
        .voltage_in(voltage),
        .current_out(current),
        .power_out(power)
    );

    // Clock generation: 100 samples per sine cycle
    initial begin
        clk = 0;
        forever #(SAMPLE_PERIOD_NS/2) clk = ~clk;
    end

    // Simulation control
    initial begin
        // Setup VCD dump
        $dumpfile("resistor_power.vcd");
        $dumpvars(0, testbench_resistor_power.clk,
                     testbench_resistor_power.reset,
                     testbench_resistor_power.voltage,
                     testbench_resistor_power.current,
                     testbench_resistor_power.power);

        // Reset
        reset = 1;
        #100;
        reset = 0;

        // Run for 5 complete sine cycles
        #(5.0 * (1.0e9 / SINE_FREQ));

        // Display summary statistics
        $display("\n=== Resistor Power Dissipation Test ===");
        $display("Resistance: %.2f Ohms", RESISTANCE);
        $display("Input: %.2f Hz sine wave, %.2f V peak", SINE_FREQ, SINE_AMPLITUDE);
        $display("Sample rate: %.2f kHz (%0d samples/cycle)",
                 SAMPLE_FREQ/1000.0, SAMPLES_PER_CYCLE);
        $display("Expected RMS voltage: %.2f V", SINE_AMPLITUDE / $sqrt(2.0));
        $display("Expected RMS current: %.2f A", (SINE_AMPLITUDE / $sqrt(2.0)) / RESISTANCE);
        $display("Expected average power: %.2f W",
                 (SINE_AMPLITUDE * SINE_AMPLITUDE) / (2.0 * RESISTANCE));
        $display("\nVCD file written to: resistor_power.vcd");
        $display("View with: gtkwave resistor_power.vcd\n");

        $finish;
    end

    // Monitor - prints every 10th sample to avoid spam
    integer sample_count = 0;
    always @(posedge clk) begin
        if (!reset) begin
            if (sample_count % 10 == 0) begin
                $display("t=%0t ns  V=%8.3f V  I=%8.6f A  P=%8.3f W",
                         $time, voltage, current, power);
            end
            sample_count = sample_count + 1;
        end
    end
endmodule
