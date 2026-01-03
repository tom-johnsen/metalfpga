#!/bin/zsh
set -euo pipefail

cmake --build /Users/tom/cpp/metalfpga/build --config Debug --target all --

./build/metalfpga_cli deprecated/verilog/test_clock_big_vcd.v \
  --auto \
  --4state \
  --emit-msl artifacts/profile/test_clock_big_vcd.msl \
  --emit-host artifacts/profile/test_clock_big_vcd_host.cc

clang++ -std=c++17 \
  -I./src \
  -I./include \
  artifacts/profile/test_clock_big_vcd_host.cc \
  build/libmetalfpga.a \
  -framework Metal \
  -framework Foundation \
  -o artifacts/profile/test_clock_big_vcd_host
