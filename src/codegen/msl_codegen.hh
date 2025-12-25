#pragma once

#include <string>

#include "frontend/ast.hh"

namespace gpga {

std::string EmitMSLStub(const Module& module, bool four_state = false);

}  // namespace gpga
