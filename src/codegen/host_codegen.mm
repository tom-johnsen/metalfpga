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
  for (const auto& net : module.nets) {
    if (net.array_size <= 0) {
      continue;
    }
    out << "//   array " << net.name << " (element width " << net.width
        << ", length " << net.array_size << ")\n";
    out << "//     buffer size hint: params.count * " << net.array_size
        << " elements\n";
    out << "//     layout: index = gid * " << net.array_size
        << " + element_index\n";
    out << "//     double buffer: " << net.name << " + " << net.name
        << "_next (swap after tick)\n";
  }
  out << "\n";
  out << "// TODO: wire Metal pipeline state and buffer bindings.\n";
  return out.str();
}

}  // namespace gpga
