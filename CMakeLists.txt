cmake_minimum_required(VERSION 3.20)

project(metalfpga LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(METALFPGA_SOURCES
  src/frontend/ast.cc
  src/frontend/verilog_parser.cc
  src/core/elaboration.cc
  src/ir/ir.cc
  src/codegen/msl_codegen.cc
  src/codegen/host_codegen.mm
  src/runtime/metal_runtime.mm
  src/utils/diagnostics.cc
)

set(METALFPGA_HEADERS
  src/frontend/ast.hh
  src/frontend/verilog_parser.hh
  src/ir/ir.hh
  src/codegen/msl_codegen.hh
  src/codegen/host_codegen.hh
  src/runtime/metal_runtime.hh
  src/utils/diagnostics.hh
)

set(METALFPGA_MSL
  src/msl/demo.metal
)

add_library(metalfpga STATIC
  ${METALFPGA_SOURCES}
  ${METALFPGA_HEADERS}
  ${METALFPGA_MSL}
)

set_source_files_properties(${METALFPGA_MSL} PROPERTIES HEADER_FILE_ONLY ON)

target_include_directories(metalfpga PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(metalfpga PUBLIC cxx_std_17)

add_executable(metalfpga_cli
  src/main.mm
)

target_link_libraries(metalfpga_cli PRIVATE metalfpga)
