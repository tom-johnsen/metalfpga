// EXPECT=PASS
// Arithmetic left shift - tests <<< operator
module test_shift_left_arithmetic;
  reg signed [7:0] a;
  reg signed [15:0] wide;
  reg [3:0] shift_amount;
  reg signed [7:0] result_8;
  reg signed [15:0] result_16;

  initial begin
    $display("Testing arithmetic left shift (<<<)");

    // Test 1: Basic shift
    a = 8'sb00000011;  // 3
    shift_amount = 2;
    result_8 = a <<< shift_amount;
    $display("Test 1: %d <<< %d = %d (expected 12)", a, shift_amount, result_8);

    // Test 2: Shift negative number
    a = 8'sb11111100;  // -4
    shift_amount = 1;
    result_8 = a <<< shift_amount;
    $display("Test 2: %d <<< %d = %d (expected -8)", a, shift_amount, result_8);

    // Test 3: Shift by zero
    a = 8'sb00101010;  // 42
    shift_amount = 0;
    result_8 = a <<< shift_amount;
    $display("Test 3: %d <<< %d = %d (expected 42)", a, shift_amount, result_8);

    // Test 4: Large shift (overflow)
    a = 8'sb00001111;  // 15
    shift_amount = 6;
    result_8 = a <<< shift_amount;
    $display("Test 4: %d <<< %d = %d (overflow test)", a, shift_amount, result_8);

    // Test 5: Shift positive max value
    a = 8'sb01111111;  // 127
    shift_amount = 1;
    result_8 = a <<< shift_amount;
    $display("Test 5: %d <<< %d = %d (max positive overflow)", a, shift_amount, result_8);

    // Test 6: Wide value shift
    wide = 16'sb0000000011110000;  // 240
    shift_amount = 4;
    result_16 = wide <<< shift_amount;
    $display("Test 6: %d <<< %d = %d (expected 3840)", wide, shift_amount, result_16);

    // Test 7: Negative wide value
    wide = 16'sb1111111100000001;  // -255
    shift_amount = 2;
    result_16 = wide <<< shift_amount;
    $display("Test 7: %d <<< %d = %d (expected -1020)", wide, shift_amount, result_16);

    // Test 8: Shift by 7 (near width boundary for 8-bit)
    a = 8'sb00000001;  // 1
    shift_amount = 7;
    result_8 = a <<< shift_amount;
    $display("Test 8: %d <<< %d = %d (expected -128)", a, shift_amount, result_8);

    // Test 9: Constant shift expressions
    result_8 = 8'sb00000101 <<< 3;
    $display("Test 9: 5 <<< 3 = %d (expected 40)", result_8);

    // Test 10: Variable shift with sign extension check
    a = 8'sb10101010;  // -86
    shift_amount = 3;
    result_8 = a <<< shift_amount;
    $display("Test 10: %d <<< %d = %d (sign preservation test)", a, shift_amount, result_8);

    $finish;
  end
endmodule
