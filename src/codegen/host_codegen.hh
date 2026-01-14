/**
 * @file host_codegen.hh
 * @brief Host-side stub code generation entry point.
 */
#pragma once

#include <string>

#include "frontend/ast.hh"

namespace gpga {

/**
 * @brief Emit host-side stub code for a module.
 *
 * @param module Input module.
 * @return Generated host source.
 */
std::string EmitHostStub(const Module& module);

}  // namespace gpga
