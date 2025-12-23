#pragma once

#include <string>

#include "frontend/ast.hh"

namespace gpga {

std::string EmitHostStub(const Module& module);

}  // namespace gpga
