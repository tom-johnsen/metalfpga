#pragma once

#include <string>

#include "frontend/ast.hh"

namespace gpga {

std::string EmitMSLStub(const Module& module);

}  // namespace gpga
