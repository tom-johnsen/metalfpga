#include "codegen/host_codegen.hh"

#include <sstream>

namespace gpga {

std::string EmitHostStub(const Module& module) {
  std::ostringstream out;
  out << "// Placeholder host stub emitted by GPGA.\n";
  out << "// Module: " << module.name << "\n";
  for (const auto& port : module.ports) {
    out << "//   port " << port.name << " (width " << port.width << ")\n";
  }
  out << "\n";
  out << "// TODO: wire Metal pipeline state and buffer bindings.\n";
  return out.str();
}

}  // namespace gpga
