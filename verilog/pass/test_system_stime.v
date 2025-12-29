// EXPECT=PASS
// Test: $stime system function
// Feature: 32-bit simulation time value

module test_system_stime;
  integer t0;
  integer t1;

  initial begin
    t0 = $stime;
    #1;
    t1 = $stime;
  end
endmodule
