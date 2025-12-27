// EXPECT=PASS
// Test signed division and modulo operations
module test_signed_division(
    input signed [7:0] dividend,
    input signed [7:0] divisor,
    output wire signed [7:0] quotient,
    output wire signed [7:0] remainder,
    output wire signed [7:0] div_pos_pos,
    output wire signed [7:0] div_pos_neg,
    output wire signed [7:0] div_neg_pos,
    output wire signed [7:0] div_neg_neg,
    output wire signed [7:0] mod_pos_pos,
    output wire signed [7:0] mod_pos_neg,
    output wire signed [7:0] mod_neg_pos,
    output wire signed [7:0] mod_neg_neg
);
    // General division and modulo
    assign quotient = dividend / divisor;
    assign remainder = dividend % divisor;

    // Constant test cases for division
    assign div_pos_pos = 8'sh14 / 8'sh05;  // 20 / 5 = 4
    assign div_pos_neg = 8'sh14 / 8'shFB;  // 20 / -5 = -4
    assign div_neg_pos = 8'shEC / 8'sh05;  // -20 / 5 = -4
    assign div_neg_neg = 8'shEC / 8'shFB;  // -20 / -5 = 4

    // Constant test cases for modulo
    assign mod_pos_pos = 8'sh17 % 8'sh05;  // 23 % 5 = 3
    assign mod_pos_neg = 8'sh17 % 8'shFB;  // 23 % -5 = 3
    assign mod_neg_pos = 8'shE9 % 8'sh05;  // -23 % 5 = -3
    assign mod_neg_neg = 8'shE9 % 8'shFB;  // -23 % -5 = -3
endmodule
