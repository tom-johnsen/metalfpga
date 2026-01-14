/**
 * @file elaboration.hh
 * @brief Elaboration entry points and output data.
 */
#pragma once

#include <string>
#include <unordered_map>

#include "frontend/ast.hh"
#include "utils/diagnostics.hh"

namespace gpga {

/// Result of elaborating a program into a single design.
struct ElaboratedDesign {
  Module top; ///< Top-level module after elaboration.
  // Flat path to hierarchical path mapping for debug and diagnostics.
  std::unordered_map<std::string, std::string> flat_to_hier;
};

/**
 * @brief Elaborate a program using its default top-level module.
 *
 * @param program Input program AST.
 * @param out_design Output elaborated design.
 * @param diagnostics Diagnostics sink for errors and warnings.
 * @param enable_4state Enable 4-state semantics for elaboration.
 * @param verbose_warnings Emit detailed warning messages.
 * @return True on success.
 */
bool Elaborate(const Program& program, ElaboratedDesign* out_design,
               Diagnostics* diagnostics, bool enable_4state = false,
               bool verbose_warnings = false);
/**
 * @brief Elaborate a program with an explicit top-level module name.
 *
 * @param program Input program AST.
 * @param top_name Name of the top-level module to elaborate.
 * @param out_design Output elaborated design.
 * @param diagnostics Diagnostics sink for errors and warnings.
 * @param enable_4state Enable 4-state semantics for elaboration.
 * @param verbose_warnings Emit detailed warning messages.
 * @return True on success.
 */
bool Elaborate(const Program& program, const std::string& top_name,
               ElaboratedDesign* out_design, Diagnostics* diagnostics,
               bool enable_4state = false, bool verbose_warnings = false);

}  // namespace gpga
