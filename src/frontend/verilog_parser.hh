#pragma once

#include <string>

#include "frontend/ast.hh"
#include "utils/diagnostics.hh"

namespace gpga {

struct ParseOptions {
  bool allow_empty = false;
  bool enable_4state = false;
  bool strict_1364 = false;
};

bool ParseVerilogFile(const std::string& path, Program* out_program,
                      Diagnostics* diagnostics,
                      const ParseOptions& options = {});

}  // namespace gpga
