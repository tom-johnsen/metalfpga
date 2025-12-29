// EXPECT=PASS
// Test: $writememh with wide elements and partial ranges
module test_system_writememh_wide_partial;
  reg [127:0] mem [0:3];

  initial begin
    mem[0] = 128'h0123456789abcdef0011223344556677;
    mem[1] = 128'h8899aabbccddeeff1020304050607080;
    mem[2] = 128'h0000000000000000ffffffffffffffff;
    mem[3] = 128'hdeadbeefdeadbeefcafebabecafebabe;

    $writememh("wide_full.hex", mem);
    $writememh("wide_partial.hex", mem, 1, 2);
    $finish;
  end
endmodule
