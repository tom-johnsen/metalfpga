// EXPECT=PASS
module ParameterArithmeticExample;

  // Simple parameter declarations
  parameter DATA_WIDTH = 8;          // Data bus width
  parameter NUM_REGS = 16;           // Number of registers in an array
  parameter CLOCK_FREQ_MHZ = 100;    // Clock frequency in MHz

  // Parameters defined using arithmetic operations on other parameters and constants
  // Example 1: Subtracting a constant from a parameter
  parameter ADDR_WIDTH = $clog2(NUM_REGS); // Address width needed for NUM_REGS
                                         // $clog2 is a system function that returns
                                         // the ceiling of log base 2

  // Example 2: Arithmetic involving multiple parameters and a constant
  parameter TOTAL_BUS_SIZE = DATA_WIDTH * 2; // Total size of two data buses

  // Example 3: Calculation of a related timing parameter
  parameter CLOCK_PERIOD_NS = 1000 / CLOCK_FREQ_MHZ; // Clock period in nanoseconds

  // Example 4: A mask derived from a width parameter
  parameter BYTE_SIZE = 8;
  parameter BYTE_MASK = BYTE_SIZE - 1; // Example directly from sources

  // Example 5: Average calculation using other parameters
  parameter REAL_VAL_R = 5.7; // Declares 'r' as a real parameter
  parameter INT_VAL_F = 9;    // Declares 'f' as an integer parameter
  parameter AVERAGE_DELAY = (REAL_VAL_R + INT_VAL_F) / 2; // Uses addition and division

  initial begin
    $display("--- Parameter Arithmetic Example ---");
    $display("DATA_WIDTH      = %0d", DATA_WIDTH);        // Expected: 8
    $display("NUM_REGS        = %0d", NUM_REGS);         // Expected: 16
    $display("CLOCK_FREQ_MHZ  = %0d", CLOCK_FREQ_MHZ); // Expected: 100

    $display("ADDR_WIDTH      = %0d", ADDR_WIDTH);     // NUM_REGS=16, log2(16)=4. Expected: 4
    $display("TOTAL_BUS_SIZE  = %0d", TOTAL_BUS_SIZE); // DATA_WIDTH*2 = 8*2 = 16. Expected: 16
    $display("CLOCK_PERIOD_NS = %0d", CLOCK_PERIOD_NS); // 1000/100 = 10. Expected: 10
    $display("BYTE_MASK       = %0d", BYTE_MASK);      // BYTE_SIZE-1 = 8-1 = 7. Expected: 7
    $display("AVERAGE_DELAY   = %0f", AVERAGE_DELAY);  // (5.7 + 9) / 2 = 14.7 / 2 = 7.35. Expected: 7.35
  end

endmodule