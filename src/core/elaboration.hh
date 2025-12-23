#pragma once

#include <string>
#include <unordered_map>

#include "frontend/ast.hh"
#include "utils/diagnostics.hh"

namespace gpga {

struct ElaboratedDesign {
  Module top;
  std::unordered_map<std::string, std::string> flat_to_hier;
};

bool Elaborate(const Program& program, ElaboratedDesign* out_design,
               Diagnostics* diagnostics);
bool Elaborate(const Program& program, const std::string& top_name,
               ElaboratedDesign* out_design, Diagnostics* diagnostics);

}  // namespace gpga
