/**
 * @file diagnostics.hh
 * @brief Diagnostic reporting utilities.
 */
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace gpga {

/// Diagnostic severity level.
enum class Severity {
  kNote,    ///< Informational note.
  kWarning, ///< Warning that may affect output.
  kError,   ///< Error that prevents correct output.
};

/// Source location for diagnostics.
struct SourceLocation {
  std::string file; ///< Source file path.
  int line = 0; ///< 1-based line number.
  int column = 0; ///< 1-based column number.
};

/// Single diagnostic message.
struct Diagnostic {
  Severity severity = Severity::kNote; ///< Severity level.
  std::string message; ///< Diagnostic message text.
  SourceLocation location; ///< Source location.
};

/// Collects diagnostics emitted during compilation.
class Diagnostics {
 public:
  /**
   * @brief Add a diagnostic entry.
   *
   * @param severity Severity of the diagnostic.
   * @param message Diagnostic message.
   * @param location Source location (optional).
   */
  void Add(Severity severity, std::string message,
           SourceLocation location = {});
  /**
   * @brief Check whether any errors have been recorded.
   *
   * @return True if at least one error exists.
   */
  bool HasErrors() const;
  /**
   * @brief Count errors in the diagnostic list.
   *
   * @return Number of error diagnostics.
   */
  int ErrorCount() const;
  /// @return All diagnostic items.
  const std::vector<Diagnostic>& Items() const { return items_; }
  /**
   * @brief Render diagnostics to a stream.
   *
   * @param os Output stream to receive formatted messages.
   */
  void RenderTo(std::ostream& os) const;

 private:
  std::vector<Diagnostic> items_;
};

}  // namespace gpga
