// EXPECT=PASS
module tb;
  real x, y, pi;

  initial begin
    pi = $acos(-1.0);
    x = 10000;
    $display("$log10(%0.3f) = %0.3f", x, $log10(x));

    x = 1;
    $display("$ln(%0.3f) = %0.3f", x, $ln(x));

    x = 2;
    $display("$exp(%0.3f) = %0.3f", x, $exp(x));

    x = 25;
    $display("$sqrt(%0.3f) = %0.3f", x, $sqrt(x));

    x = 5;
    y = 3;
    $display("$pow(%0.3f, %0.3f) = %0.3f", x, y, $pow(x, y));

    x = 2.7813;
    $display("$floor(%0.3f) = %0.3f", x, $floor(x));

    x = 7.1111;
    $display("$ceil(%0.3f) = %0.3f", x, $ceil(x));

    x = 30 * pi / 180;   // convert 30 degrees to radians
    $display("$sin(%0.3f) = %0.3f", x, $sin(x));

    x = 90 * pi / 180;
    $display("$cos(%0.3f) = %0.3f", x, $cos(x));

    x = 45 * pi / 180;
    $display("$tan(%0.3f) = %0.3f", x, $tan(x));

    x = 0.5;
    $display("$asin(%0.3f) = %0.3f rad, %0.3f deg", x, $asin(x), $asin(x) * 180 / pi);

    x = 0;
    $display("$acos(%0.3f) = %0.3f rad, %0.3f deg", x, $acos(x), $acos(x) * 180 / pi);

    x = 1;
    $display("$atan(%0.3f) = %0.3f rad, %f deg", x, $atan(x), $atan(x) * 180 / pi);

    $finish;
  end
endmodule
