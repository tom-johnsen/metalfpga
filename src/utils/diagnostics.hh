#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace gpga {

enum class Severity {
  kNote,
  kWarning,
  kError,
};

struct SourceLocation {
  std::string file;
  int line = 0;
  int column = 0;
};

struct Diagnostic {
  Severity severity = Severity::kNote;
  std::string message;
  SourceLocation location;
};

class Diagnostics {
 public:
  void Add(Severity severity, std::string message,
           SourceLocation location = {});
  bool HasErrors() const;
  int ErrorCount() const;
  const std::vector<Diagnostic>& Items() const { return items_; }
  void RenderTo(std::ostream& os) const;

 private:
  std::vector<Diagnostic> items_;
};

}  // namespace gpga
