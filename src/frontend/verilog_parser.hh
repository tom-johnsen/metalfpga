/**
 * @file verilog_parser.hh
 * @brief Verilog source parser entry points.
 */
#pragma once

#include <string>

#include "frontend/ast.hh"
#include "utils/diagnostics.hh"

namespace gpga {

/// Parser configuration options.
struct ParseOptions {
  bool allow_empty = false; ///< Allow empty input files.
  bool enable_4state = false; ///< Enable 4-state logic parsing.
  bool strict_1364 = false; ///< Enable stricter IEEE-1364 checks.
};

/**
 * @brief Parse a Verilog file into an AST program.
 *
 * @param path Input file path.
 * @param out_program Output program AST.
 * @param diagnostics Diagnostic sink for errors and warnings.
 * @param options Parser options.
 * @return True on successful parse.
 */
bool ParseVerilogFile(const std::string& path, Program* out_program,
                      Diagnostics* diagnostics,
                      const ParseOptions& options = {});

}  // namespace gpga
