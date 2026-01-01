// EXPECT=PASS
// Test: $writememb with wide memory elements (>64 bits)
module test_system_writememb_wide;
  reg [127:0] mem [0:3];

  initial begin
    mem[0] = 128'h0123456789abcdef0011223344556677;
    mem[1] = 128'h8899aabbccddeeff1020304050607080;
    mem[2] = 128'h0000000000000000ffffffffffffffff;
    mem[3] = 128'hdeadbeefdeadbeefcafebabecafebabe;

    $writememb("wide.bin", mem);
    $writememb("wide_partial.bin", mem, 1, 2);
    $finish;
  end
endmodule
