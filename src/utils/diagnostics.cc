#include "utils/diagnostics.hh"

#include <ostream>
#include <utility>

namespace gpga {

namespace {

const char* SeverityLabel(Severity severity) {
  switch (severity) {
    case Severity::kNote:
      return "note";
    case Severity::kWarning:
      return "warning";
    case Severity::kError:
      return "error";
  }
  return "error";
}

}  // namespace

void Diagnostics::Add(Severity severity, std::string message,
                      SourceLocation location) {
  items_.push_back(Diagnostic{severity, std::move(message),
                              std::move(location)});
}

bool Diagnostics::HasErrors() const {
  for (const auto& diagnostic : items_) {
    if (diagnostic.severity == Severity::kError) {
      return true;
    }
  }
  return false;
}

int Diagnostics::ErrorCount() const {
  int count = 0;
  for (const auto& diagnostic : items_) {
    if (diagnostic.severity == Severity::kError) {
      ++count;
    }
  }
  return count;
}

void Diagnostics::RenderTo(std::ostream& os) const {
  for (const auto& diagnostic : items_) {
    const auto& loc = diagnostic.location;
    if (!loc.file.empty()) {
      os << loc.file;
      if (loc.line > 0) {
        os << ":" << loc.line;
        if (loc.column > 0) {
          os << ":" << loc.column;
        }
      }
      os << ": ";
    }
    os << SeverityLabel(diagnostic.severity) << ": "
       << diagnostic.message << "\n";
  }
}

}  // namespace gpga
