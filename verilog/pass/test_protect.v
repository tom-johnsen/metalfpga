// EXPECT=PASS
// Test: `protect and `endprotect directives
// Feature: IP protection (encryption)
// Expected: May pass - directives might be ignored

module test_protect;
  `protect
  // This section would be encrypted in some tools
  wire [7:0] secret = 8'hAB;
  `endprotect

  wire [7:0] public = 8'hCD;
endmodule
