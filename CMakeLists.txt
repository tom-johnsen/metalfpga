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
  #src/msl/demo.metal
)

add_library(metalfpga STATIC
  ${METALFPGA_SOURCES}
  ${METALFPGA_HEADERS}
  ${METALFPGA_MSL}
)

set_source_files_properties(${METALFPGA_MSL} PROPERTIES HEADER_FILE_ONLY ON)

target_include_directories(metalfpga PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(metalfpga PUBLIC cxx_std_17)

add_executable(metalfpga_cli
  src/main.mm
)

target_link_libraries(metalfpga_cli PRIVATE metalfpga)

add_executable(metalfpga_smoke
  src/tools/metal_smoke.mm
)

target_link_libraries(metalfpga_smoke PRIVATE metalfpga)

set(CRLIBM_REF_SOURCES
  thirdparty/crlibm/crlibm_private.c
  thirdparty/crlibm/triple-double.c
  thirdparty/crlibm/exp-td.c
  thirdparty/crlibm/exp-td-standalone.c
  thirdparty/crlibm/expm1-standalone.c
  thirdparty/crlibm/expm1.c
  thirdparty/crlibm/log-td.c
  thirdparty/crlibm/log1p.c
  thirdparty/crlibm/log10-td.c
  thirdparty/crlibm/log2-td.c
  thirdparty/crlibm/rem_pio2_accurate.c
  thirdparty/crlibm/trigo_fast.c
  thirdparty/crlibm/trigo_accurate.c
  thirdparty/crlibm/trigpi.c
  thirdparty/crlibm/asincos.c
  thirdparty/crlibm/atan_fast.c
  thirdparty/crlibm/atan_accurate.c
  thirdparty/crlibm/csh_fast.c
  thirdparty/crlibm/pow.c
  thirdparty/crlibm/scs_lib/scs_private.c
  thirdparty/crlibm/scs_lib/addition_scs.c
  thirdparty/crlibm/scs_lib/division_scs.c
  thirdparty/crlibm/scs_lib/print_scs.c
  thirdparty/crlibm/scs_lib/double2scs.c
  thirdparty/crlibm/scs_lib/zero_scs.c
  thirdparty/crlibm/scs_lib/multiplication_scs.c
  thirdparty/crlibm/scs_lib/scs2double.c
)

add_library(crlibm_ref STATIC EXCLUDE_FROM_ALL
  ${CRLIBM_REF_SOURCES}
)

target_include_directories(crlibm_ref PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/crlibm
)

target_compile_definitions(crlibm_ref PRIVATE
  HAVE_CONFIG_H=1
  SCS_NB_WORDS=8
  SCS_NB_BITS=30
)

add_executable(metalfpga_crlibm_compare
  src/tools/crlibm_compare.cc
)

target_include_directories(metalfpga_crlibm_compare PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(metalfpga_crlibm_compare PRIVATE crlibm_ref)

add_custom_target(metalfpga_tools ALL
  DEPENDS metalfpga_cli metalfpga_smoke metalfpga_crlibm_compare
)

if(APPLE)
  target_link_libraries(metalfpga PUBLIC "-framework Metal" "-framework Foundation")
endif()
