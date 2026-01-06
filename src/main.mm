#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "codegen/host_codegen.hh"
#include "codegen/msl_codegen.hh"
#include "core/elaboration.hh"
#include "core/scheduler_vm.hh"
#include "frontend/verilog_parser.hh"
#include "gpga_sched.h"
#include "runtime/metal_runtime.hh"
#include "utils/msl_naming.hh"
#include "utils/diagnostics.hh"

namespace {

constexpr const char* kMetalFpgaVersion = "dev";
volatile sig_atomic_t g_halt_request = 0;

void HandleHaltSignal(int signal) { g_halt_request = signal; }

void InstallHaltSignalHandlers() {
  std::signal(SIGINT, HandleHaltSignal);
  std::signal(SIGTERM, HandleHaltSignal);
}

void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " <input.v> [<more.v> ...] [--emit-msl <path>] [--emit-host <path>]"
            << " [--emit-flat <path>] [--dump-flat] [--top <module>]"
            << " [--4state] [--sched-vm] [--auto] [--strict-1364]"
            << " [--sdf <path>] [--version]"
            << " [--verbose]"
            << " [--run] [--count N] [--service-capacity N]"
            << " [--max-steps N] [--max-proc-steps N]"
            << " [--dispatch-timeout-ms N]"
            << " [--run-verbose]"
            << " [--source-bindings]"
            << " [--vcd-dir <path>] [--vcd-steps N]"
            << " [+ARG[=VALUE] ...]\n";
}

void PrintVersion() { std::cout << "metalfpga " << kMetalFpgaVersion << "\n"; }

bool WriteFile(const std::string& path, const std::string& content,
               gpga::Diagnostics* diagnostics) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to open output file",
                     gpga::SourceLocation{path});
    return false;
  }
  out << content;
  if (!out) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to write output file",
                     gpga::SourceLocation{path});
    return false;
  }
  return true;
}

std::string ToLowerAscii(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  for (unsigned char ch : input) {
    out.push_back(static_cast<char>(std::tolower(ch)));
  }
  return out;
}

std::string DirLabel(gpga::PortDir dir) {
  switch (dir) {
    case gpga::PortDir::kInput:
      return "input";
    case gpga::PortDir::kOutput:
      return "output";
    case gpga::PortDir::kInout:
      return "inout";
  }
  return "unknown";
}

const gpga::Port* FindPort(const gpga::Module& module,
                           const std::string& name) {
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return &port;
    }
  }
  return nullptr;
}

bool IsOutputPort(const gpga::Module& module, const std::string& name) {
  const gpga::Port* port = FindPort(module, name);
  return port && (port->dir == gpga::PortDir::kOutput ||
                  port->dir == gpga::PortDir::kInout);
}

struct SdfToken {
  std::string text;
  int line = 0;
  int column = 0;
};

struct SdfNode {
  bool is_atom = false;
  std::string text;
  std::vector<SdfNode> children;
  int line = 0;
  int column = 0;
};

struct SdfTimingCheck {
  std::string name;
  std::string edge;
  std::string signal;
  std::string condition;
  bool has_cond = false;
  struct Value {
    bool valid = false;
    bool is_real = false;
    double real_value = 0.0;
    int64_t int_value = 0;
  };
  struct Limit {
    Value min;
    Value typ;
    Value max;
    bool HasAny() const { return min.valid || typ.valid || max.valid; }
  };
  std::vector<Limit> limits;
  int line = 0;
  int column = 0;
};

bool TokenizeSdf(const std::string& content, std::vector<SdfToken>* tokens,
                 std::string* error, int* error_line, int* error_column) {
  tokens->clear();
  int line = 1;
  int column = 1;
  size_t i = 0;
  auto set_error = [&](const std::string& message) {
    if (error) {
      *error = message;
    }
    if (error_line) {
      *error_line = line;
    }
    if (error_column) {
      *error_column = column;
    }
  };
  auto push_token = [&](const std::string& text, int tok_line,
                        int tok_column) {
    tokens->push_back(SdfToken{text, tok_line, tok_column});
  };
  while (i < content.size()) {
    unsigned char ch = static_cast<unsigned char>(content[i]);
    if (std::isspace(ch)) {
      if (content[i] == '\n') {
        ++line;
        column = 1;
      } else {
        ++column;
      }
      ++i;
      continue;
    }
    if (content[i] == '/' && i + 1 < content.size()) {
      if (content[i + 1] == '/') {
        i += 2;
        column += 2;
        while (i < content.size() && content[i] != '\n') {
          ++i;
          ++column;
        }
        continue;
      }
      if (content[i + 1] == '*') {
        i += 2;
        column += 2;
        bool closed = false;
        while (i + 1 < content.size()) {
          if (content[i] == '\n') {
            ++line;
            column = 1;
            ++i;
            continue;
          }
          if (content[i] == '*' && content[i + 1] == '/') {
            i += 2;
            column += 2;
            closed = true;
            break;
          }
          ++i;
          ++column;
        }
        if (!closed) {
          set_error("unterminated block comment");
          return false;
        }
        continue;
      }
    }
    if (content[i] == '(' || content[i] == ')') {
      int tok_line = line;
      int tok_column = column;
      push_token(std::string(1, content[i]), tok_line, tok_column);
      ++i;
      ++column;
      continue;
    }
    if (content[i] == '"') {
      int tok_line = line;
      int tok_column = column;
      ++i;
      ++column;
      std::string value;
      bool closed = false;
      while (i < content.size()) {
        char c = content[i];
        if (c == '"') {
          closed = true;
          ++i;
          ++column;
          break;
        }
        if (c == '\\' && i + 1 < content.size()) {
          value.push_back(content[i + 1]);
          i += 2;
          column += 2;
          continue;
        }
        if (c == '\n') {
          ++line;
          column = 1;
          value.push_back(c);
          ++i;
          continue;
        }
        value.push_back(c);
        ++i;
        ++column;
      }
      if (!closed) {
        set_error("unterminated string literal");
        return false;
      }
      push_token(value, tok_line, tok_column);
      continue;
    }
    int tok_line = line;
    int tok_column = column;
    size_t start = i;
    while (i < content.size()) {
      char c = content[i];
      if (std::isspace(static_cast<unsigned char>(c)) || c == '(' ||
          c == ')') {
        break;
      }
      ++i;
      ++column;
    }
    if (i == start) {
      set_error("unexpected character");
      return false;
    }
    push_token(content.substr(start, i - start), tok_line, tok_column);
  }
  return true;
}

bool ParseSdfNode(const std::vector<SdfToken>& tokens, size_t* index,
                  SdfNode* out, std::string* error, int* error_line,
                  int* error_column) {
  if (*index >= tokens.size()) {
    if (error) {
      *error = "unexpected end of file";
    }
    return false;
  }
  const SdfToken& token = tokens[*index];
  if (token.text == "(") {
    out->is_atom = false;
    out->line = token.line;
    out->column = token.column;
    out->children.clear();
    ++(*index);
    while (*index < tokens.size() && tokens[*index].text != ")") {
      SdfNode child;
      if (!ParseSdfNode(tokens, index, &child, error, error_line,
                        error_column)) {
        return false;
      }
      out->children.push_back(std::move(child));
    }
    if (*index >= tokens.size()) {
      if (error) {
        *error = "missing ')' in SDF list";
      }
      if (error_line) {
        *error_line = token.line;
      }
      if (error_column) {
        *error_column = token.column;
      }
      return false;
    }
    ++(*index);
    return true;
  }
  if (token.text == ")") {
    if (error) {
      *error = "unexpected ')'";
    }
    if (error_line) {
      *error_line = token.line;
    }
    if (error_column) {
      *error_column = token.column;
    }
    return false;
  }
  out->is_atom = true;
  out->text = token.text;
  out->line = token.line;
  out->column = token.column;
  ++(*index);
  return true;
}

bool ParseSdfTokens(const std::vector<SdfToken>& tokens,
                    std::vector<SdfNode>* nodes, std::string* error,
                    int* error_line, int* error_column) {
  nodes->clear();
  size_t index = 0;
  while (index < tokens.size()) {
    SdfNode node;
    if (!ParseSdfNode(tokens, &index, &node, error, error_line,
                      error_column)) {
      return false;
    }
    nodes->push_back(std::move(node));
  }
  return true;
}

void AppendSdfNodeText(const SdfNode& node, std::string* out,
                       bool include_parens) {
  if (node.is_atom) {
    out->append(node.text);
    return;
  }
  if (include_parens) {
    out->push_back('(');
  }
  for (const auto& child : node.children) {
    AppendSdfNodeText(child, out, true);
  }
  if (include_parens) {
    out->push_back(')');
  }
}

std::string NormalizeSdfExpr(const SdfNode& node) {
  std::string out;
  AppendSdfNodeText(node, &out, false);
  return ToLowerAscii(out);
}

std::string NormalizeSdfNodeList(const std::vector<SdfNode>& nodes) {
  SdfNode wrapper;
  wrapper.is_atom = false;
  wrapper.children = nodes;
  return NormalizeSdfExpr(wrapper);
}

bool IsTimingCheckName(const std::string& name) {
  static const std::unordered_set<std::string> names = {
      "setup", "hold", "setuphold", "recovery", "removal",
      "recrem", "skew", "period", "width", "pulsewidth", "nochange"};
  return names.count(name) != 0u;
}

bool ParseSdfNumber(const std::string& text, SdfTimingCheck::Value* out) {
  if (!out) {
    return false;
  }
  *out = {};
  if (text.empty() || text == "*") {
    return false;
  }
  bool has_float = false;
  for (char ch : text) {
    if (ch == '.' || ch == 'e' || ch == 'E') {
      has_float = true;
      break;
    }
  }
  char* end = nullptr;
  if (has_float) {
    double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || (end && *end != '\0')) {
      return false;
    }
    out->valid = true;
    out->is_real = true;
    out->real_value = value;
    return true;
  }
  long long value = std::strtoll(text.c_str(), &end, 10);
  if (end == text.c_str() || (end && *end != '\0')) {
    double real_value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || (end && *end != '\0')) {
      return false;
    }
    out->valid = true;
    out->is_real = true;
    out->real_value = real_value;
    return true;
  }
  out->valid = true;
  out->is_real = false;
  out->int_value = static_cast<int64_t>(value);
  return true;
}

SdfTimingCheck::Limit ParseSdfLimitToken(const std::string& text) {
  SdfTimingCheck::Limit limit;
  if (text.empty()) {
    return limit;
  }
  std::vector<std::string> parts;
  size_t start = 0;
  for (size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == ':') {
      parts.push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  auto parse_value = [&](const std::string& token,
                         SdfTimingCheck::Value* out) {
    if (!token.empty()) {
      ParseSdfNumber(token, out);
    }
  };
  if (parts.size() == 1) {
    parse_value(parts[0], &limit.min);
    limit.typ = limit.min;
    limit.max = limit.min;
  } else if (parts.size() == 2) {
    parse_value(parts[0], &limit.min);
    parse_value(parts[1], &limit.max);
  } else {
    parse_value(parts[0], &limit.min);
    parse_value(parts[1], &limit.typ);
    parse_value(parts[2], &limit.max);
  }
  return limit;
}

void AppendSdfValueTokens(const SdfNode& node,
                          std::vector<std::string>* out) {
  if (!out) {
    return;
  }
  if (node.is_atom) {
    out->push_back(ToLowerAscii(node.text));
    return;
  }
  if (!node.children.empty() && node.children[0].is_atom) {
    std::string head = ToLowerAscii(node.children[0].text);
    if (head == "cond" || head == "posedge" || head == "negedge") {
      return;
    }
  }
  for (const auto& child : node.children) {
    if (child.is_atom) {
      out->push_back(ToLowerAscii(child.text));
    } else {
      std::string normalized = NormalizeSdfExpr(child);
      if (!normalized.empty()) {
        out->push_back(normalized);
      }
    }
  }
}

size_t SdfEventCountForCheck(const std::string& name) {
  if (name == "period" || name == "width" || name == "pulsewidth") {
    return 1u;
  }
  return 2u;
}

size_t SdfRefEventIndexForCheck(const std::string& name,
                                size_t event_count) {
  if (event_count == 0u) {
    return 0u;
  }
  if ((name == "setup" || name == "recovery" || name == "removal" ||
       name == "recrem") &&
      event_count > 1u) {
    return 1u;
  }
  return 0u;
}

void ExtractEventFromSdfNode(const SdfNode& node, std::string* edge,
                             std::string* signal) {
  edge->clear();
  signal->clear();
  if (node.is_atom) {
    *signal = ToLowerAscii(node.text);
    return;
  }
  if (node.children.empty()) {
    return;
  }
  if (node.children[0].is_atom) {
    std::string first = ToLowerAscii(node.children[0].text);
    if (first == "posedge" || first == "negedge") {
      *edge = first;
      if (node.children.size() > 1) {
        *signal = NormalizeSdfNodeList(
            std::vector<SdfNode>(node.children.begin() + 1,
                                 node.children.end()));
      }
      return;
    }
  }
  *signal = NormalizeSdfNodeList(node.children);
}

bool ParseSdfTimingCheck(const SdfNode& node, SdfTimingCheck* out) {
  if (node.is_atom || node.children.empty()) {
    return false;
  }
  if (!node.children[0].is_atom) {
    return false;
  }
  std::string name = ToLowerAscii(node.children[0].text);
  if (!IsTimingCheckName(name)) {
    return false;
  }
  size_t event_needed = SdfEventCountForCheck(name);
  std::vector<const SdfNode*> event_nodes;
  size_t value_start = 1;
  while (value_start < node.children.size() &&
         event_nodes.size() < event_needed) {
    event_nodes.push_back(&node.children[value_start]);
    ++value_start;
  }
  if (event_nodes.empty()) {
    return false;
  }
  size_t ref_index = SdfRefEventIndexForCheck(name, event_nodes.size());
  const SdfNode* event_node = event_nodes[ref_index];
  out->name = name;
  out->edge.clear();
  out->signal.clear();
  out->condition.clear();
  out->has_cond = false;
  out->limits.clear();
  out->line = node.line;
  out->column = node.column;
  if (!event_node->children.empty() && event_node->children[0].is_atom) {
    std::string head = ToLowerAscii(event_node->children[0].text);
    if (head == "cond") {
      out->has_cond = true;
      if (event_node->children.size() >= 3) {
        out->condition = NormalizeSdfExpr(event_node->children[1]);
        ExtractEventFromSdfNode(event_node->children.back(), &out->edge,
                                &out->signal);
        return !out->signal.empty();
      }
      return false;
    }
  }
  ExtractEventFromSdfNode(*event_node, &out->edge, &out->signal);
  std::vector<std::string> value_tokens;
  for (size_t i = value_start; i < node.children.size(); ++i) {
    AppendSdfValueTokens(node.children[i], &value_tokens);
  }
  for (const auto& token : value_tokens) {
    SdfTimingCheck::Limit limit = ParseSdfLimitToken(token);
    if (limit.HasAny()) {
      out->limits.push_back(std::move(limit));
    }
  }
  return !out->signal.empty();
}

void CollectSdfTimingChecks(const SdfNode& node,
                            std::vector<SdfTimingCheck>* out) {
  SdfTimingCheck timing;
  if (ParseSdfTimingCheck(node, &timing)) {
    out->push_back(std::move(timing));
  }
  if (!node.is_atom) {
    for (const auto& child : node.children) {
      CollectSdfTimingChecks(child, out);
    }
  }
}

bool LoadSdfTimingChecks(const std::string& path,
                         std::vector<SdfTimingCheck>* checks,
                         gpga::Diagnostics* diagnostics) {
  checks->clear();
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to open SDF file",
                     gpga::SourceLocation{path});
    return false;
  }
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  std::vector<SdfToken> tokens;
  std::string error;
  int error_line = 0;
  int error_column = 0;
  if (!TokenizeSdf(content, &tokens, &error, &error_line, &error_column)) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to tokenize SDF: " + error,
                     gpga::SourceLocation{path, error_line, error_column});
    return false;
  }
  std::vector<SdfNode> nodes;
  if (!ParseSdfTokens(tokens, &nodes, &error, &error_line, &error_column)) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to parse SDF: " + error,
                     gpga::SourceLocation{path, error_line, error_column});
    return false;
  }
  for (const auto& node : nodes) {
    CollectSdfTimingChecks(node, checks);
  }
  return true;
}

uint64_t DoubleToBits(double value) {
  uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "double size mismatch");
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::unique_ptr<gpga::Expr> MakeSdfRealExpr(double value) {
  uint64_t bits = DoubleToBits(value);
  auto expr = std::make_unique<gpga::Expr>();
  expr->kind = gpga::ExprKind::kNumber;
  expr->number = bits;
  expr->value_bits = bits;
  expr->has_width = true;
  expr->number_width = 64;
  expr->is_real_literal = true;
  return expr;
}

std::unique_ptr<gpga::Expr> MakeSdfIntegerExpr(int64_t value) {
  auto expr = std::make_unique<gpga::Expr>();
  expr->kind = gpga::ExprKind::kNumber;
  expr->is_signed = value < 0;
  expr->has_width = true;
  expr->number_width = 64;
  uint64_t bits = static_cast<uint64_t>(value);
  expr->number = bits;
  expr->value_bits = bits;
  return expr;
}

std::unique_ptr<gpga::Expr> MakeSdfValueExpr(
    const SdfTimingCheck::Value& value) {
  if (!value.valid) {
    return nullptr;
  }
  if (value.is_real) {
    return MakeSdfRealExpr(value.real_value);
  }
  return MakeSdfIntegerExpr(value.int_value);
}

void ClearTimingLimit(gpga::TimingCheckLimit* limit) {
  if (!limit) {
    return;
  }
  limit->min.reset();
  limit->typ.reset();
  limit->max.reset();
}

void ApplySdfLimit(const SdfTimingCheck::Limit& src,
                   gpga::TimingCheckLimit* dest) {
  if (!dest || !src.HasAny()) {
    return;
  }
  ClearTimingLimit(dest);
  dest->min = MakeSdfValueExpr(src.min);
  dest->typ = MakeSdfValueExpr(src.typ);
  dest->max = MakeSdfValueExpr(src.max);
}

size_t TimingCheckLimitCount(const gpga::TimingCheck& check) {
  switch (check.kind) {
    case gpga::TimingCheckKind::kSetupHold:
    case gpga::TimingCheckKind::kRecRem:
    case gpga::TimingCheckKind::kFullSkew:
    case gpga::TimingCheckKind::kPulseWidth:
    case gpga::TimingCheckKind::kNoChange:
      return 2u;
    default:
      return 1u;
  }
}

std::string TimingCheckKey(const std::string& name, const std::string& edge,
                           const std::string& signal,
                           const std::string& condition) {
  return name + "|" + edge + "|" + signal + "|" + condition;
}

void ApplySdfTimingChecks(const std::string& path,
                          const std::vector<SdfTimingCheck>& sdf_checks,
                          gpga::Program* program,
                          gpga::Diagnostics* diagnostics) {
  std::unordered_map<std::string, std::vector<gpga::TimingCheck*>>
      checks_by_key;
  std::vector<std::pair<std::string, gpga::TimingCheck*>> check_keys;
  for (auto& module : program->modules) {
    for (auto& check : module.timing_checks) {
      std::string key = TimingCheckKey(check.name, check.edge, check.signal,
                                       check.condition);
      checks_by_key[key].push_back(&check);
      check_keys.emplace_back(module.name, &check);
    }
  }
  const char* verbose_env = std::getenv("METALFPGA_SDF_VERBOSE");
  bool verbose = verbose_env && *verbose_env != '\0';
  std::unordered_set<std::string> matched_keys;
  for (const auto& sdf : sdf_checks) {
    std::string key = TimingCheckKey(sdf.name, sdf.edge, sdf.signal,
                                     sdf.condition);
    auto it = checks_by_key.find(key);
    if (it != checks_by_key.end()) {
      matched_keys.insert(key);
      if (!sdf.limits.empty()) {
        for (gpga::TimingCheck* check : it->second) {
          size_t limit_count = TimingCheckLimitCount(*check);
          if (limit_count > 0u) {
            ApplySdfLimit(sdf.limits[0], &check->limit);
          }
          if (limit_count > 1u && sdf.limits.size() > 1u) {
            ApplySdfLimit(sdf.limits[1], &check->limit2);
          }
        }
      }
      if (verbose) {
        std::cerr << "SDF match: " << sdf.name;
        if (!sdf.edge.empty()) {
          std::cerr << " " << sdf.edge;
        }
        if (!sdf.signal.empty()) {
          std::cerr << " " << sdf.signal;
        }
        if (!sdf.condition.empty()) {
          std::cerr << " &&& " << sdf.condition;
        }
        std::cerr << "\n";
      }
      continue;
    }
    if (sdf.has_cond) {
      diagnostics->Add(gpga::Severity::kWarning,
                       "SDF COND did not match any timing check",
                       gpga::SourceLocation{path, sdf.line, sdf.column});
    } else if (verbose) {
      std::cerr << "SDF no match: " << sdf.name;
      if (!sdf.edge.empty()) {
        std::cerr << " " << sdf.edge;
      }
      if (!sdf.signal.empty()) {
        std::cerr << " " << sdf.signal;
      }
      std::cerr << "\n";
    }
  }
  if (verbose) {
    for (const auto& entry : check_keys) {
      const gpga::TimingCheck* check = entry.second;
      const std::string key = TimingCheckKey(check->name, check->edge,
                                             check->signal, check->condition);
      if (matched_keys.count(key) != 0u) {
        continue;
      }
      std::cerr << "SDF unannotated: " << entry.first << "." << check->name;
      if (!check->edge.empty()) {
        std::cerr << " " << check->edge;
      }
      if (!check->signal.empty()) {
        std::cerr << " " << check->signal;
      }
      if (!check->condition.empty()) {
        std::cerr << " &&& " << check->condition;
      }
      std::cerr << "\n";
    }
  }
}

struct SysTaskInfo {
  bool has_tasks = false;
  bool has_dumpvars = false;
  size_t max_args = 0;
  std::vector<std::string> string_table;
  std::unordered_map<std::string, size_t> string_ids;
};

bool IsSystemTask(const std::string& name) {
  return !name.empty() && name[0] == '$';
}

bool IdentAsString(const std::string& name) {
  return name == "$dumpvars" || name == "$readmemh" || name == "$readmemb" ||
         name == "$writememh" || name == "$writememb" ||
         name == "$printtimescale";
}

std::vector<char> ExtractFormatSpecs(const std::string& format) {
  std::vector<char> specs;
  for (size_t i = 0; i < format.size(); ++i) {
    if (format[i] != '%') {
      continue;
    }
    if (i + 1 < format.size() && format[i + 1] == '%') {
      ++i;
      continue;
    }
    size_t j = i + 1;
    if (j < format.size() && format[j] == '0') {
      ++j;
    }
    while (j < format.size() &&
           std::isdigit(static_cast<unsigned char>(format[j]))) {
      ++j;
    }
    if (j < format.size() && format[j] == '.') {
      ++j;
      while (j < format.size() &&
             std::isdigit(static_cast<unsigned char>(format[j]))) {
        ++j;
      }
    }
    if (j >= format.size()) {
      break;
    }
    char spec = format[j];
    if (spec >= 'A' && spec <= 'Z') {
      spec = static_cast<char>(spec - 'A' + 'a');
    }
    specs.push_back(spec);
    i = j;
  }
  return specs;
}

bool IsFileSystemFunctionName(const std::string& name) {
  return name == "$fopen" || name == "$fclose" || name == "$fgetc" ||
         name == "$fgets" || name == "$feof" || name == "$fscanf" ||
         name == "$sscanf" || name == "$ftell" || name == "$fseek" ||
         name == "$ferror" || name == "$ungetc" || name == "$fread" ||
         name == "$test$plusargs" || name == "$value$plusargs";
}

void AddString(SysTaskInfo* info, const std::string& value) {
  if (!info) {
    return;
  }
  auto it = info->string_ids.find(value);
  if (it != info->string_ids.end()) {
    return;
  }
  info->string_ids[value] = info->string_table.size();
  info->string_table.push_back(value);
}

void CollectSystemFunctionExpr(const gpga::Expr& expr, SysTaskInfo* info) {
  if (!info) {
    return;
  }
  if (expr.kind == gpga::ExprKind::kCall) {
    if (IsFileSystemFunctionName(expr.ident)) {
      info->has_tasks = true;
      info->max_args = std::max(info->max_args, expr.call_args.size());
      for (size_t i = 0; i < expr.call_args.size(); ++i) {
        const auto* arg = expr.call_args[i].get();
        if (!arg) {
          continue;
        }
        if (arg->kind == gpga::ExprKind::kString) {
          AddString(info, arg->string_value);
          continue;
        }
        if (arg->kind == gpga::ExprKind::kIdentifier) {
          bool treat_ident = false;
          if (expr.ident == "$fgets") {
            treat_ident = (i == 0);
          } else if (expr.ident == "$fread") {
            treat_ident = (i == 0);
          } else if (expr.ident == "$fscanf" || expr.ident == "$sscanf") {
            treat_ident = (i >= 2);
            if (expr.ident == "$sscanf" && i == 0) {
              treat_ident = true;
            }
          } else if (expr.ident == "$fopen") {
            treat_ident = true;
          } else if (expr.ident == "$test$plusargs" ||
                     expr.ident == "$value$plusargs") {
            treat_ident = true;
          }
          if (treat_ident) {
            AddString(info, arg->ident);
          }
        }
      }
    }
    for (const auto& arg : expr.call_args) {
      if (arg) {
        CollectSystemFunctionExpr(*arg, info);
      }
    }
    return;
  }
  if (expr.kind == gpga::ExprKind::kUnary && expr.operand) {
    CollectSystemFunctionExpr(*expr.operand, info);
    return;
  }
  if (expr.kind == gpga::ExprKind::kBinary) {
    if (expr.lhs) {
      CollectSystemFunctionExpr(*expr.lhs, info);
    }
    if (expr.rhs) {
      CollectSystemFunctionExpr(*expr.rhs, info);
    }
    return;
  }
  if (expr.kind == gpga::ExprKind::kTernary) {
    if (expr.condition) {
      CollectSystemFunctionExpr(*expr.condition, info);
    }
    if (expr.then_expr) {
      CollectSystemFunctionExpr(*expr.then_expr, info);
    }
    if (expr.else_expr) {
      CollectSystemFunctionExpr(*expr.else_expr, info);
    }
    return;
  }
  if (expr.kind == gpga::ExprKind::kSelect) {
    if (expr.base) {
      CollectSystemFunctionExpr(*expr.base, info);
    }
    if (expr.msb_expr) {
      CollectSystemFunctionExpr(*expr.msb_expr, info);
    }
    if (expr.lsb_expr) {
      CollectSystemFunctionExpr(*expr.lsb_expr, info);
    }
    return;
  }
  if (expr.kind == gpga::ExprKind::kIndex) {
    if (expr.base) {
      CollectSystemFunctionExpr(*expr.base, info);
    }
    if (expr.index) {
      CollectSystemFunctionExpr(*expr.index, info);
    }
    return;
  }
  if (expr.kind == gpga::ExprKind::kConcat) {
    for (const auto& element : expr.elements) {
      if (element) {
        CollectSystemFunctionExpr(*element, info);
      }
    }
    if (expr.repeat_expr) {
      CollectSystemFunctionExpr(*expr.repeat_expr, info);
    }
    return;
  }
}

void CollectReadSignalsExpr(const gpga::Expr& expr,
                            std::unordered_set<std::string>* out) {
  if (!out) {
    return;
  }
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      out->insert(expr.ident);
      return;
    case gpga::ExprKind::kUnary:
      if (expr.operand) {
        CollectReadSignalsExpr(*expr.operand, out);
      }
      return;
    case gpga::ExprKind::kBinary:
      if (expr.lhs) {
        CollectReadSignalsExpr(*expr.lhs, out);
      }
      if (expr.rhs) {
        CollectReadSignalsExpr(*expr.rhs, out);
      }
      return;
    case gpga::ExprKind::kTernary:
      if (expr.condition) {
        CollectReadSignalsExpr(*expr.condition, out);
      }
      if (expr.then_expr) {
        CollectReadSignalsExpr(*expr.then_expr, out);
      }
      if (expr.else_expr) {
        CollectReadSignalsExpr(*expr.else_expr, out);
      }
      return;
    case gpga::ExprKind::kSelect:
      if (expr.base) {
        CollectReadSignalsExpr(*expr.base, out);
      }
      if (expr.msb_expr) {
        CollectReadSignalsExpr(*expr.msb_expr, out);
      }
      if (expr.lsb_expr) {
        CollectReadSignalsExpr(*expr.lsb_expr, out);
      }
      return;
    case gpga::ExprKind::kIndex:
      if (expr.base) {
        CollectReadSignalsExpr(*expr.base, out);
      }
      if (expr.index) {
        CollectReadSignalsExpr(*expr.index, out);
      }
      return;
    case gpga::ExprKind::kCall:
      for (const auto& arg : expr.call_args) {
        if (arg) {
          CollectReadSignalsExpr(*arg, out);
        }
      }
      return;
    case gpga::ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        if (element) {
          CollectReadSignalsExpr(*element, out);
        }
      }
      if (expr.repeat_expr) {
        CollectReadSignalsExpr(*expr.repeat_expr, out);
      }
      return;
    default:
      return;
  }
}

void CollectReadSignals(const gpga::Statement& stmt,
                        std::unordered_set<std::string>* out) {
  if (!out) {
    return;
  }
  if (stmt.kind == gpga::StatementKind::kAssign ||
      stmt.kind == gpga::StatementKind::kForce ||
      stmt.kind == gpga::StatementKind::kRelease) {
    if (stmt.assign.rhs) {
      CollectReadSignalsExpr(*stmt.assign.rhs, out);
    }
    if (stmt.assign.lhs_index) {
      CollectReadSignalsExpr(*stmt.assign.lhs_index, out);
    }
    for (const auto& index : stmt.assign.lhs_indices) {
      if (index) {
        CollectReadSignalsExpr(*index, out);
      }
    }
    if (stmt.assign.lhs_msb_expr) {
      CollectReadSignalsExpr(*stmt.assign.lhs_msb_expr, out);
    }
    if (stmt.assign.lhs_lsb_expr) {
      CollectReadSignalsExpr(*stmt.assign.lhs_lsb_expr, out);
    }
    if (stmt.assign.delay) {
      CollectReadSignalsExpr(*stmt.assign.delay, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kIf) {
    if (stmt.condition) {
      CollectReadSignalsExpr(*stmt.condition, out);
    }
    for (const auto& inner : stmt.then_branch) {
      CollectReadSignals(inner, out);
    }
    for (const auto& inner : stmt.else_branch) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kCase) {
    if (stmt.case_expr) {
      CollectReadSignalsExpr(*stmt.case_expr, out);
    }
    for (const auto& item : stmt.case_items) {
      for (const auto& label : item.labels) {
        if (label) {
          CollectReadSignalsExpr(*label, out);
        }
      }
      for (const auto& inner : item.body) {
        CollectReadSignals(inner, out);
      }
    }
    for (const auto& inner : stmt.default_branch) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFor) {
    if (stmt.for_init_rhs) {
      CollectReadSignalsExpr(*stmt.for_init_rhs, out);
    }
    if (stmt.for_condition) {
      CollectReadSignalsExpr(*stmt.for_condition, out);
    }
    if (stmt.for_step_rhs) {
      CollectReadSignalsExpr(*stmt.for_step_rhs, out);
    }
    for (const auto& inner : stmt.for_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWhile) {
    if (stmt.while_condition) {
      CollectReadSignalsExpr(*stmt.while_condition, out);
    }
    for (const auto& inner : stmt.while_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kRepeat) {
    if (stmt.repeat_count) {
      CollectReadSignalsExpr(*stmt.repeat_count, out);
    }
    for (const auto& inner : stmt.repeat_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kDelay) {
    if (stmt.delay) {
      CollectReadSignalsExpr(*stmt.delay, out);
    }
    for (const auto& inner : stmt.delay_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kEventControl) {
    if (stmt.event_expr) {
      CollectReadSignalsExpr(*stmt.event_expr, out);
    }
    for (const auto& item : stmt.event_items) {
      if (item.expr) {
        CollectReadSignalsExpr(*item.expr, out);
      }
    }
    for (const auto& inner : stmt.event_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWait) {
    if (stmt.wait_condition) {
      CollectReadSignalsExpr(*stmt.wait_condition, out);
    }
    for (const auto& inner : stmt.wait_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      CollectReadSignals(inner, out);
    }
  }
  if (stmt.kind == gpga::StatementKind::kTaskCall) {
    for (const auto& arg : stmt.task_args) {
      if (arg) {
        CollectReadSignalsExpr(*arg, out);
      }
    }
    return;
  }
}

void CollectSystemFunctionInfo(const gpga::Statement& stmt, SysTaskInfo* info) {
  if (!info) {
    return;
  }
  if (stmt.kind == gpga::StatementKind::kAssign) {
    if (stmt.assign.rhs) {
      CollectSystemFunctionExpr(*stmt.assign.rhs, info);
    }
    if (stmt.assign.lhs_index) {
      CollectSystemFunctionExpr(*stmt.assign.lhs_index, info);
    }
    for (const auto& index : stmt.assign.lhs_indices) {
      if (index) {
        CollectSystemFunctionExpr(*index, info);
      }
    }
    if (stmt.assign.lhs_msb_expr) {
      CollectSystemFunctionExpr(*stmt.assign.lhs_msb_expr, info);
    }
    if (stmt.assign.lhs_lsb_expr) {
      CollectSystemFunctionExpr(*stmt.assign.lhs_lsb_expr, info);
    }
    if (stmt.assign.delay) {
      CollectSystemFunctionExpr(*stmt.assign.delay, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kIf) {
    if (stmt.condition) {
      CollectSystemFunctionExpr(*stmt.condition, info);
    }
    for (const auto& inner : stmt.then_branch) {
      CollectSystemFunctionInfo(inner, info);
    }
    for (const auto& inner : stmt.else_branch) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kCase) {
    if (stmt.case_expr) {
      CollectSystemFunctionExpr(*stmt.case_expr, info);
    }
    for (const auto& item : stmt.case_items) {
      for (const auto& label : item.labels) {
        if (label) {
          CollectSystemFunctionExpr(*label, info);
        }
      }
      for (const auto& inner : item.body) {
        CollectSystemFunctionInfo(inner, info);
      }
    }
    for (const auto& inner : stmt.default_branch) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFor) {
    if (stmt.for_init_rhs) {
      CollectSystemFunctionExpr(*stmt.for_init_rhs, info);
    }
    if (stmt.for_condition) {
      CollectSystemFunctionExpr(*stmt.for_condition, info);
    }
    if (stmt.for_step_rhs) {
      CollectSystemFunctionExpr(*stmt.for_step_rhs, info);
    }
    for (const auto& inner : stmt.for_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWhile) {
    if (stmt.while_condition) {
      CollectSystemFunctionExpr(*stmt.while_condition, info);
    }
    for (const auto& inner : stmt.while_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kRepeat) {
    if (stmt.repeat_count) {
      CollectSystemFunctionExpr(*stmt.repeat_count, info);
    }
    for (const auto& inner : stmt.repeat_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kDelay) {
    if (stmt.delay) {
      CollectSystemFunctionExpr(*stmt.delay, info);
    }
    for (const auto& inner : stmt.delay_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kEventControl) {
    if (stmt.event_expr) {
      CollectSystemFunctionExpr(*stmt.event_expr, info);
    }
    for (const auto& item : stmt.event_items) {
      if (item.expr) {
        CollectSystemFunctionExpr(*item.expr, info);
      }
    }
    for (const auto& inner : stmt.event_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWait) {
    if (stmt.wait_condition) {
      CollectSystemFunctionExpr(*stmt.wait_condition, info);
    }
    for (const auto& inner : stmt.wait_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      CollectSystemFunctionInfo(inner, info);
    }
  }
}

void CollectTasks(const gpga::Statement& stmt, SysTaskInfo* info) {
  if (!info) {
    return;
  }
  if (stmt.kind == gpga::StatementKind::kTaskCall &&
      IsSystemTask(stmt.task_name)) {
    info->has_tasks = true;
    if (stmt.task_name == "$dumpvars") {
      info->has_dumpvars = true;
    }
    info->max_args = std::max(info->max_args, stmt.task_args.size());
    size_t format_arg_start = 0;
    if (stmt.task_name == "$fdisplay" || stmt.task_name == "$fwrite") {
      format_arg_start = 1;
    } else if (stmt.task_name == "$sformat") {
      format_arg_start = 1;
    }
    std::vector<char> format_specs;
    bool has_format_specs =
        stmt.task_args.size() > format_arg_start &&
        stmt.task_args[format_arg_start] &&
        stmt.task_args[format_arg_start]->kind == gpga::ExprKind::kString;
    if (has_format_specs) {
      format_specs =
          ExtractFormatSpecs(stmt.task_args[format_arg_start]->string_value);
    }
    size_t format_arg_index = 0;
    if (stmt.task_name == "$sformat" && !stmt.task_args.empty() &&
        stmt.task_args[0] &&
        stmt.task_args[0]->kind == gpga::ExprKind::kIdentifier) {
      AddString(info, stmt.task_args[0]->ident);
    }
    bool treat_ident = IdentAsString(stmt.task_name);
    for (size_t i = 0; i < stmt.task_args.size(); ++i) {
      const auto& arg = stmt.task_args[i];
      if (!arg) {
        continue;
      }
      bool is_format_literal = has_format_specs && i == format_arg_start &&
                               arg->kind == gpga::ExprKind::kString;
      if (arg->kind == gpga::ExprKind::kString) {
        AddString(info, arg->string_value);
      } else if (treat_ident && arg->kind == gpga::ExprKind::kIdentifier) {
        AddString(info, arg->ident);
      } else if (has_format_specs && !is_format_literal &&
                 format_arg_index < format_specs.size() &&
                 format_specs[format_arg_index] == 's' &&
                 arg->kind == gpga::ExprKind::kIdentifier) {
        AddString(info, arg->ident);
      }
      if (has_format_specs && !is_format_literal) {
        ++format_arg_index;
      }
    }
  }
  if (stmt.kind == gpga::StatementKind::kIf) {
    for (const auto& inner : stmt.then_branch) {
      CollectTasks(inner, info);
    }
    for (const auto& inner : stmt.else_branch) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kCase) {
    for (const auto& item : stmt.case_items) {
      for (const auto& inner : item.body) {
        CollectTasks(inner, info);
      }
    }
    for (const auto& inner : stmt.default_branch) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFor) {
    for (const auto& inner : stmt.for_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWhile) {
    for (const auto& inner : stmt.while_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kRepeat) {
    for (const auto& inner : stmt.repeat_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kDelay) {
    for (const auto& inner : stmt.delay_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kEventControl) {
    for (const auto& inner : stmt.event_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWait) {
    for (const auto& inner : stmt.wait_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      CollectTasks(inner, info);
    }
  }
}

gpga::ServiceStringTable BuildStringTable(const gpga::Module& module) {
  SysTaskInfo info;
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectTasks(stmt, &info);
      CollectSystemFunctionInfo(stmt, &info);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      CollectTasks(stmt, &info);
      CollectSystemFunctionInfo(stmt, &info);
    }
  }
  gpga::ServiceStringTable table;
  table.entries = std::move(info.string_table);
  return table;
}

bool ModuleUsesDumpvars(const gpga::Module& module) {
  SysTaskInfo info;
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectTasks(stmt, &info);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      CollectTasks(stmt, &info);
    }
  }
  return info.has_dumpvars;
}

gpga::ModuleInfo BuildModuleInfo(const gpga::Module& module,
                                 bool four_state) {
  gpga::ModuleInfo info;
  info.name = module.name;
  info.four_state = four_state;
  std::unordered_map<std::string, gpga::SignalInfo> signals;
  for (const auto& port : module.ports) {
    gpga::SignalInfo sig;
    sig.name = port.name;
    sig.width = port.width;
    sig.is_real = port.is_real;
    signals[port.name] = sig;
  }
  for (const auto& net : module.nets) {
    auto it = signals.find(net.name);
    if (it == signals.end()) {
      gpga::SignalInfo sig;
      sig.name = net.name;
      sig.width = net.width;
      sig.array_size = net.array_size;
      sig.is_real = net.is_real;
      sig.is_trireg = net.type == gpga::NetType::kTrireg;
      signals[net.name] = sig;
    } else {
      it->second.width = std::max(it->second.width,
                                  static_cast<uint32_t>(net.width));
      if (net.array_size > 0) {
        it->second.array_size = net.array_size;
      }
      if (net.is_real) {
        it->second.is_real = true;
      }
      if (net.type == gpga::NetType::kTrireg) {
        it->second.is_trireg = true;
      }
    }
  }
  info.signals.reserve(signals.size());
  for (auto& entry : signals) {
    info.signals.push_back(std::move(entry.second));
  }
  return info;
}

struct FileHandleEntry {
  std::FILE* file = nullptr;
  std::string path;
};

struct FileTable {
  uint32_t next_handle = 1u;
  std::unordered_map<uint32_t, FileHandleEntry> handles;
};

const gpga::SignalInfo* FindSignalInfo(const gpga::ModuleInfo& module,
                                       const std::string& name) {
  for (const auto& sig : module.signals) {
    if (sig.name == name) {
      return &sig;
    }
  }
  return nullptr;
}

std::string MslSignalName(const std::string& name) {
  return gpga::MslMangleIdentifier(name);
}

const gpga::MetalBuffer* FindBuffer(
    const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
    const std::string& base, const char* suffix) {
  auto it = buffers.find(base + suffix);
  if (it != buffers.end()) {
    return &it->second;
  }
  it = buffers.find(base);
  if (it != buffers.end()) {
    return &it->second;
  }
  return nullptr;
}

gpga::MetalBuffer* FindBufferMutable(
    std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
    const std::string& base, const char* suffix) {
  if (!buffers) {
    return nullptr;
  }
  auto it = buffers->find(base + suffix);
  if (it != buffers->end()) {
    return &it->second;
  }
  it = buffers->find(base);
  if (it != buffers->end()) {
    return &it->second;
  }
  return nullptr;
}

bool InitSchedulerVmBuffers(
    std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
    const gpga::SchedulerConstants& sched, uint32_t instance_count,
    const gpga::SchedulerVmLayout* layout, std::string* error) {
  if (!sched.vm_enabled) {
    return true;
  }
  if (!buffers) {
    if (error) {
      *error = "missing scheduler buffers for VM initialization";
    }
    return false;
  }
  constexpr uint32_t kMinWordsPerProc = gpga::kSchedulerVmWordsPerProc;
  const uint32_t proc_count = sched.proc_count;
  const uint32_t vm_words =
      layout ? static_cast<uint32_t>(layout->bytecode.size())
             : sched.vm_bytecode_words;
  const uint32_t layout_proc_count = layout ? layout->proc_count : proc_count;
  const uint32_t layout_words_per_proc =
      layout ? layout->words_per_proc : 0u;
  if (proc_count == 0u || vm_words == 0u) {
    if (error) {
      *error = "scheduler VM enabled without bytecode sizing";
    }
    return false;
  }
  if (sched.vm_cond_count == 0u) {
    if (error) {
      *error = "scheduler VM enabled without cond sizing";
    }
    return false;
  }
  if (layout && layout_proc_count != proc_count) {
    if (error) {
      *error = "scheduler VM layout proc count mismatch";
    }
    return false;
  }
  if (vm_words < proc_count * kMinWordsPerProc) {
    if (error) {
      *error = "scheduler VM bytecode buffer too small for proc count";
    }
    return false;
  }
  if (vm_words % proc_count != 0u) {
    if (error) {
      *error = "scheduler VM bytecode words not divisible by proc count";
    }
    return false;
  }
  const uint32_t words_per_proc = vm_words / proc_count;
  if (words_per_proc < kMinWordsPerProc) {
    if (error) {
      *error = "scheduler VM words per proc below minimum";
    }
    return false;
  }
  if (layout && layout_words_per_proc != 0u &&
      layout_words_per_proc != words_per_proc) {
    if (error) {
      *error = "scheduler VM layout words-per-proc mismatch";
    }
    return false;
  }
  auto* bytecode_buf = FindBufferMutable(buffers, "sched_vm_bytecode", "");
  auto* offset_buf =
      FindBufferMutable(buffers, "sched_vm_proc_bytecode_offset", "");
  auto* length_buf =
      FindBufferMutable(buffers, "sched_vm_proc_bytecode_length", "");
  auto* cond_val_buf = FindBufferMutable(buffers, "sched_vm_cond_val", "");
  auto* cond_xz_buf = FindBufferMutable(buffers, "sched_vm_cond_xz", "");
  auto* cond_entry_buf =
      FindBufferMutable(buffers, "sched_vm_cond_entry", "");
  auto* signal_entry_buf =
      FindBufferMutable(buffers, "sched_vm_signal_entry", "");
  auto* ip_buf = FindBufferMutable(buffers, "sched_vm_ip", "");
  auto* call_sp_buf = FindBufferMutable(buffers, "sched_vm_call_sp", "");
  auto* call_frame_buf = FindBufferMutable(buffers, "sched_vm_call_frame", "");
  auto* case_header_buf =
      FindBufferMutable(buffers, "sched_vm_case_header", "");
  auto* case_entry_buf =
      FindBufferMutable(buffers, "sched_vm_case_entry", "");
  auto* case_words_buf =
      FindBufferMutable(buffers, "sched_vm_case_words", "");
  auto* expr_buf = FindBufferMutable(buffers, "sched_vm_expr", "");
  auto* expr_imm_buf = FindBufferMutable(buffers, "sched_vm_expr_imm", "");
  auto* assign_entry_buf =
      FindBufferMutable(buffers, "sched_vm_assign_entry", "");
  auto* delay_assign_buf =
      FindBufferMutable(buffers, "sched_vm_delay_assign_entry", "");
  auto* force_entry_buf =
      FindBufferMutable(buffers, "sched_vm_force_entry", "");
  auto* release_entry_buf =
      FindBufferMutable(buffers, "sched_vm_release_entry", "");
  auto* service_entry_buf =
      FindBufferMutable(buffers, "sched_vm_service_entry", "");
  auto* service_arg_buf =
      FindBufferMutable(buffers, "sched_vm_service_arg", "");
  auto* service_ret_assign_buf =
      FindBufferMutable(buffers, "sched_vm_service_ret_assign_entry", "");
  if (!bytecode_buf || !offset_buf || !length_buf || !cond_val_buf ||
      !cond_xz_buf || !cond_entry_buf || !signal_entry_buf || !ip_buf ||
      !call_sp_buf || !call_frame_buf || !case_header_buf ||
      !case_entry_buf || !case_words_buf || !expr_buf || !expr_imm_buf ||
      !assign_entry_buf || !delay_assign_buf || !force_entry_buf ||
      !release_entry_buf || !service_entry_buf || !service_arg_buf ||
      !service_ret_assign_buf) {
    if (error) {
      *error = "missing scheduler VM buffers";
    }
    return false;
  }
  if (!bytecode_buf->contents() || !offset_buf->contents() ||
      !length_buf->contents() || !cond_val_buf->contents() ||
      !cond_xz_buf->contents() || !cond_entry_buf->contents() ||
      !signal_entry_buf->contents() || !ip_buf->contents() ||
      !call_sp_buf->contents() || !call_frame_buf->contents() ||
      !case_header_buf->contents() || !case_entry_buf->contents() ||
      !case_words_buf->contents() || !expr_buf->contents() ||
      !expr_imm_buf->contents() || !assign_entry_buf->contents() ||
      !delay_assign_buf->contents() || !force_entry_buf->contents() ||
      !release_entry_buf->contents() || !service_entry_buf->contents() ||
      !service_arg_buf->contents() || !service_ret_assign_buf->contents()) {
    if (error) {
      *error = "scheduler VM buffers are not CPU-visible";
    }
    return false;
  }
  const size_t bytecode_words_needed =
      static_cast<size_t>(instance_count) * vm_words;
  if (bytecode_buf->length() < bytecode_words_needed * sizeof(uint32_t)) {
    if (error) {
      *error = "scheduler VM bytecode buffer length mismatch";
    }
    return false;
  }
  const size_t proc_entries =
      static_cast<size_t>(instance_count) * proc_count;
  if (offset_buf->length() < proc_entries * sizeof(uint32_t) ||
      length_buf->length() < proc_entries * sizeof(uint32_t) ||
      cond_val_buf->length() <
          proc_entries * sched.vm_cond_count * sizeof(uint32_t) ||
      cond_xz_buf->length() <
          proc_entries * sched.vm_cond_count * sizeof(uint32_t) ||
      ip_buf->length() < proc_entries * sizeof(uint32_t) ||
      call_sp_buf->length() < proc_entries * sizeof(uint32_t)) {
    if (error) {
      *error = "scheduler VM proc buffer length mismatch";
    }
    return false;
  }
  const size_t case_header_count =
      layout ? layout->case_headers.size()
             : static_cast<size_t>(sched.vm_case_header_count);
  const size_t case_entry_count =
      layout ? layout->case_entries.size()
             : static_cast<size_t>(sched.vm_case_entry_count);
  const size_t case_word_count =
      layout ? layout->case_words.size()
             : static_cast<size_t>(sched.vm_case_word_count);
  const size_t cond_entry_count =
      layout ? layout->cond_entries.size()
             : static_cast<size_t>(sched.vm_cond_count);
  const size_t signal_entry_count =
      layout ? layout->signal_entries.size()
             : static_cast<size_t>(sched.vm_signal_count);
  const size_t expr_word_count =
      layout ? layout->expr_table.words.size()
             : static_cast<size_t>(sched.vm_expr_word_count);
  const size_t expr_imm_word_count =
      layout ? layout->expr_table.imm_words.size()
             : static_cast<size_t>(sched.vm_expr_imm_word_count);
  const size_t assign_entry_count =
      layout ? layout->assign_entries.size()
             : static_cast<size_t>(sched.vm_assign_count);
  const size_t delay_assign_entry_count =
      layout ? layout->delay_assign_entries.size()
             : static_cast<size_t>(sched.delay_count);
  const size_t force_entry_count =
      layout ? layout->force_entries.size()
             : static_cast<size_t>(sched.vm_force_count);
  const size_t release_entry_count =
      layout ? layout->release_entries.size()
             : static_cast<size_t>(sched.vm_release_count);
  const size_t service_entry_count =
      layout ? layout->service_entries.size()
             : static_cast<size_t>(sched.vm_service_call_count);
  const size_t service_arg_count =
      layout ? layout->service_args.size()
             : static_cast<size_t>(sched.vm_service_arg_count);
  const size_t service_ret_entry_count =
      layout ? layout->service_ret_entries.size()
             : static_cast<size_t>(sched.vm_service_assign_count);
  if (case_header_count > 0u &&
      case_header_buf->length() <
          case_header_count * sizeof(GpgaSchedVmCaseHeader)) {
    if (error) {
      *error = "scheduler VM case header buffer length mismatch";
    }
    return false;
  }
  if (case_entry_count > 0u &&
      case_entry_buf->length() <
          case_entry_count * sizeof(GpgaSchedVmCaseEntry)) {
    if (error) {
      *error = "scheduler VM case entry buffer length mismatch";
    }
    return false;
  }
  if (case_word_count > 0u &&
      case_words_buf->length() < case_word_count * sizeof(uint64_t)) {
    if (error) {
      *error = "scheduler VM case word buffer length mismatch";
    }
    return false;
  }
  if (cond_entry_count > 0u &&
      cond_entry_buf->length() <
          cond_entry_count * sizeof(GpgaSchedVmCondEntry)) {
    if (error) {
      *error = "scheduler VM cond entry buffer length mismatch";
    }
    return false;
  }
  if (signal_entry_count > 0u &&
      signal_entry_buf->length() <
          signal_entry_count * sizeof(GpgaSchedVmSignalEntry)) {
    if (error) {
      *error = "scheduler VM signal entry buffer length mismatch";
    }
    return false;
  }
  if (expr_word_count > 0u &&
      expr_buf->length() < expr_word_count * sizeof(uint32_t)) {
    if (error) {
      *error = "scheduler VM expr buffer length mismatch";
    }
    return false;
  }
  if (expr_imm_word_count > 0u &&
      expr_imm_buf->length() < expr_imm_word_count * sizeof(uint32_t)) {
    if (error) {
      *error = "scheduler VM expr imm buffer length mismatch";
    }
    return false;
  }
  if (assign_entry_count > 0u &&
      assign_entry_buf->length() <
          assign_entry_count * sizeof(GpgaSchedVmAssignEntry)) {
    if (error) {
      *error = "scheduler VM assign entry buffer length mismatch";
    }
    return false;
  }
  if (delay_assign_entry_count > 0u &&
      delay_assign_buf->length() <
          delay_assign_entry_count * sizeof(GpgaSchedVmDelayAssignEntry)) {
    if (error) {
      *error = "scheduler VM delay assign buffer length mismatch";
    }
    return false;
  }
  if (force_entry_count > 0u &&
      force_entry_buf->length() <
          force_entry_count * sizeof(GpgaSchedVmForceEntry)) {
    if (error) {
      *error = "scheduler VM force entry buffer length mismatch";
    }
    return false;
  }
  if (release_entry_count > 0u &&
      release_entry_buf->length() <
          release_entry_count * sizeof(GpgaSchedVmReleaseEntry)) {
    if (error) {
      *error = "scheduler VM release entry buffer length mismatch";
    }
    return false;
  }
  if (service_entry_count > 0u &&
      service_entry_buf->length() <
          service_entry_count * sizeof(GpgaSchedVmServiceEntry)) {
    if (error) {
      *error = "scheduler VM service entry buffer length mismatch";
    }
    return false;
  }
  if (service_arg_count > 0u &&
      service_arg_buf->length() <
          service_arg_count * sizeof(GpgaSchedVmServiceArg)) {
    if (error) {
      *error = "scheduler VM service arg buffer length mismatch";
    }
    return false;
  }
  if (service_ret_entry_count > 0u &&
      service_ret_assign_buf->length() <
          service_ret_entry_count * sizeof(GpgaSchedVmServiceRetAssignEntry)) {
    if (error) {
      *error = "scheduler VM service ret-assign buffer length mismatch";
    }
    return false;
  }

  std::memset(bytecode_buf->contents(), 0, bytecode_buf->length());
  std::memset(cond_val_buf->contents(), 0, cond_val_buf->length());
  std::memset(cond_xz_buf->contents(), 0, cond_xz_buf->length());
  std::memset(cond_entry_buf->contents(), 0, cond_entry_buf->length());
  std::memset(signal_entry_buf->contents(), 0, signal_entry_buf->length());
  std::memset(ip_buf->contents(), 0, ip_buf->length());
  std::memset(call_sp_buf->contents(), 0, call_sp_buf->length());
  std::memset(call_frame_buf->contents(), 0, call_frame_buf->length());
  std::memset(case_header_buf->contents(), 0, case_header_buf->length());
  std::memset(case_entry_buf->contents(), 0, case_entry_buf->length());
  std::memset(case_words_buf->contents(), 0, case_words_buf->length());
  std::memset(expr_buf->contents(), 0, expr_buf->length());
  std::memset(expr_imm_buf->contents(), 0, expr_imm_buf->length());
  std::memset(assign_entry_buf->contents(), 0, assign_entry_buf->length());
  std::memset(delay_assign_buf->contents(), 0, delay_assign_buf->length());
  std::memset(force_entry_buf->contents(), 0, force_entry_buf->length());
  std::memset(release_entry_buf->contents(), 0, release_entry_buf->length());
  std::memset(service_entry_buf->contents(), 0, service_entry_buf->length());
  std::memset(service_arg_buf->contents(), 0, service_arg_buf->length());
  std::memset(service_ret_assign_buf->contents(), 0,
              service_ret_assign_buf->length());

  auto* bytecode = static_cast<uint32_t*>(bytecode_buf->contents());
  auto* offsets = static_cast<uint32_t*>(offset_buf->contents());
  auto* lengths = static_cast<uint32_t*>(length_buf->contents());
  const uint32_t* layout_bytecode =
      layout ? layout->bytecode.data() : nullptr;
  const uint32_t* layout_offsets =
      layout ? layout->proc_offsets.data() : nullptr;
  const uint32_t* layout_lengths =
      layout ? layout->proc_lengths.data() : nullptr;
  const bool has_layout =
      layout && !layout->bytecode.empty() && layout_offsets &&
      layout_lengths;
  for (uint32_t gid = 0u; gid < instance_count; ++gid) {
    const size_t vm_base = static_cast<size_t>(gid) * vm_words;
    const size_t proc_base = static_cast<size_t>(gid) * proc_count;
    if (has_layout) {
      std::memcpy(bytecode + vm_base, layout_bytecode,
                  sizeof(uint32_t) * layout->bytecode.size());
    }
    for (uint32_t pid = 0u; pid < proc_count; ++pid) {
      const size_t bc_index = vm_base + (pid * words_per_proc);
      const uint32_t offset = static_cast<uint32_t>(bc_index);
      if (has_layout) {
        offsets[proc_base + pid] =
            static_cast<uint32_t>(vm_base + layout_offsets[pid]);
        lengths[proc_base + pid] = layout_lengths[pid];
        continue;
      }
      offsets[proc_base + pid] = offset;
      lengths[proc_base + pid] = kMinWordsPerProc;
      bytecode[bc_index] = gpga::MakeSchedulerVmInstr(
          gpga::SchedulerVmOp::kCallGroup);
      bytecode[bc_index + 1u] =
          gpga::MakeSchedulerVmInstr(gpga::SchedulerVmOp::kDone);
    }
  }
  if (layout) {
    if (!layout->case_headers.empty()) {
      auto* headers =
          static_cast<GpgaSchedVmCaseHeader*>(case_header_buf->contents());
      for (size_t i = 0; i < layout->case_headers.size(); ++i) {
        const gpga::SchedulerVmCaseHeader& src = layout->case_headers[i];
        headers[i].kind = src.kind;
        headers[i].strategy = src.strategy;
        headers[i].width = src.width;
        headers[i].entry_count = src.entry_count;
        headers[i].entry_offset = src.entry_offset;
        headers[i].expr_offset = src.expr_offset;
        headers[i].default_target = src.default_target;
      }
    }
    if (!layout->case_entries.empty()) {
      auto* entries =
          static_cast<GpgaSchedVmCaseEntry*>(case_entry_buf->contents());
      for (size_t i = 0; i < layout->case_entries.size(); ++i) {
        const gpga::SchedulerVmCaseEntry& src = layout->case_entries[i];
        entries[i].want_offset = src.want_offset;
        entries[i].care_offset = src.care_offset;
        entries[i].target = src.target;
      }
    }
    if (!layout->case_words.empty()) {
      std::memcpy(case_words_buf->contents(), layout->case_words.data(),
                  sizeof(uint64_t) * layout->case_words.size());
    }
    if (!layout->cond_entries.empty()) {
      auto* entries =
          static_cast<GpgaSchedVmCondEntry*>(cond_entry_buf->contents());
      for (size_t i = 0; i < layout->cond_entries.size(); ++i) {
        const gpga::SchedulerVmCondEntry& src = layout->cond_entries[i];
        entries[i].kind = src.kind;
        entries[i].val = src.val;
        entries[i].xz = src.xz;
        entries[i].expr_offset = src.expr_offset;
      }
    }
    if (!layout->signal_entries.empty()) {
      std::vector<uint64_t> slot_offsets(layout->packed_slots.size(), 0u);
      uint64_t offset = 0u;
      for (size_t i = 0; i < layout->packed_slots.size(); ++i) {
        offset = (offset + 7u) & ~static_cast<uint64_t>(7u);
        slot_offsets[i] = offset;
        const gpga::SchedulerVmPackedSlot& slot = layout->packed_slots[i];
        const uint64_t array_size =
            static_cast<uint64_t>(std::max<uint32_t>(1u, slot.array_size));
        const uint64_t word_size =
            static_cast<uint64_t>(std::max<uint32_t>(1u, slot.word_size));
        offset += static_cast<uint64_t>(instance_count) * array_size * word_size;
      }
      auto* entries =
          static_cast<GpgaSchedVmSignalEntry*>(signal_entry_buf->contents());
      for (size_t i = 0; i < layout->signal_entries.size(); ++i) {
        const gpga::SchedulerVmSignalEntry& src = layout->signal_entries[i];
        if (src.val_slot < slot_offsets.size()) {
          entries[i].val_offset =
              static_cast<uint32_t>(slot_offsets[src.val_slot]);
        } else {
          entries[i].val_offset = 0u;
        }
        if (src.xz_slot < slot_offsets.size()) {
          entries[i].xz_offset =
              static_cast<uint32_t>(slot_offsets[src.xz_slot]);
        } else {
          entries[i].xz_offset = 0u;
        }
        entries[i].width = src.width;
        entries[i].array_size = src.array_size;
        entries[i].flags = src.flags;
      }
    }
    if (!layout->expr_table.words.empty()) {
      std::memcpy(expr_buf->contents(), layout->expr_table.words.data(),
                  sizeof(uint32_t) * layout->expr_table.words.size());
    }
    if (!layout->expr_table.imm_words.empty()) {
      std::memcpy(expr_imm_buf->contents(),
                  layout->expr_table.imm_words.data(),
                  sizeof(uint32_t) * layout->expr_table.imm_words.size());
    }
    if (!layout->assign_entries.empty()) {
      auto* entries = static_cast<GpgaSchedVmAssignEntry*>(
          assign_entry_buf->contents());
      for (size_t i = 0; i < layout->assign_entries.size(); ++i) {
        const gpga::SchedulerVmAssignEntry& src = layout->assign_entries[i];
        entries[i].flags = src.flags;
        entries[i].signal_id = src.signal_id;
        entries[i].rhs_expr = src.rhs_expr;
      }
    }
    if (!layout->delay_assign_entries.empty()) {
      auto* entries = static_cast<GpgaSchedVmDelayAssignEntry*>(
          delay_assign_buf->contents());
      for (size_t i = 0; i < layout->delay_assign_entries.size(); ++i) {
        const gpga::SchedulerVmDelayAssignEntry& src =
            layout->delay_assign_entries[i];
        entries[i].flags = src.flags;
        entries[i].signal_id = src.signal_id;
        entries[i].rhs_expr = src.rhs_expr;
        entries[i].delay_expr = src.delay_expr;
        entries[i].idx_expr = src.idx_expr;
        entries[i].width = src.width;
        entries[i].base_width = src.base_width;
        entries[i].range_lsb = src.range_lsb;
        entries[i].array_size = src.array_size;
        entries[i].pulse_reject_expr = src.pulse_reject_expr;
        entries[i].pulse_error_expr = src.pulse_error_expr;
      }
    }
    if (!layout->force_entries.empty()) {
      auto* entries = static_cast<GpgaSchedVmForceEntry*>(
          force_entry_buf->contents());
      for (size_t i = 0; i < layout->force_entries.size(); ++i) {
        const gpga::SchedulerVmForceEntry& src = layout->force_entries[i];
        entries[i].flags = src.flags;
        entries[i].signal_id = src.signal_id;
        entries[i].rhs_expr = src.rhs_expr;
        entries[i].force_id = src.force_id;
        entries[i].force_slot = src.force_slot;
        entries[i].passign_slot = src.passign_slot;
      }
    }
    if (!layout->release_entries.empty()) {
      auto* entries = static_cast<GpgaSchedVmReleaseEntry*>(
          release_entry_buf->contents());
      for (size_t i = 0; i < layout->release_entries.size(); ++i) {
        const gpga::SchedulerVmReleaseEntry& src = layout->release_entries[i];
        entries[i].flags = src.flags;
        entries[i].signal_id = src.signal_id;
        entries[i].force_slot = src.force_slot;
        entries[i].passign_slot = src.passign_slot;
      }
    }
    if (!layout->service_entries.empty()) {
      auto* entries = static_cast<GpgaSchedVmServiceEntry*>(
          service_entry_buf->contents());
      for (size_t i = 0; i < layout->service_entries.size(); ++i) {
        const gpga::SchedulerVmServiceEntry& src = layout->service_entries[i];
        entries[i].kind = src.kind;
        entries[i].format_id = src.format_id;
        entries[i].arg_offset = src.arg_offset;
        entries[i].arg_count = src.arg_count;
        entries[i].flags = src.flags;
        entries[i].aux = src.aux;
      }
    }
    if (!layout->service_args.empty()) {
      auto* entries = static_cast<GpgaSchedVmServiceArg*>(
          service_arg_buf->contents());
      for (size_t i = 0; i < layout->service_args.size(); ++i) {
        const gpga::SchedulerVmServiceArg& src = layout->service_args[i];
        entries[i].kind = src.kind;
        entries[i].width = src.width;
        entries[i].payload = src.payload;
        entries[i].flags = src.flags;
      }
    }
    if (!layout->service_ret_entries.empty()) {
      auto* entries = static_cast<GpgaSchedVmServiceRetAssignEntry*>(
          service_ret_assign_buf->contents());
      for (size_t i = 0; i < layout->service_ret_entries.size(); ++i) {
        const gpga::SchedulerVmServiceRetAssignEntry& src =
            layout->service_ret_entries[i];
        entries[i].flags = src.flags;
        entries[i].signal_id = src.signal_id;
        entries[i].width = src.width;
        entries[i].force_slot = src.force_slot;
        entries[i].passign_slot = src.passign_slot;
        entries[i].reserved = src.reserved;
      }
    }
  }
  return true;
}

uint32_t SignalBitWidth(const gpga::SignalInfo& sig) {
  uint32_t width = sig.is_real ? 64u : sig.width;
  return width == 0u ? 1u : width;
}

size_t SignalWordSize(const gpga::SignalInfo& sig) {
  return (SignalBitWidth(sig) > 32u) ? sizeof(uint64_t) : sizeof(uint32_t);
}

size_t SignalWordCount(const gpga::SignalInfo& sig) {
  uint32_t width = SignalBitWidth(sig);
  if (width <= 64u) {
    return 1u;
  }
  return (width + 63u) / 64u;
}

size_t SignalElementSize(const gpga::SignalInfo& sig) {
  return SignalWordSize(sig) * SignalWordCount(sig);
}

size_t SignalElementCount(const gpga::SignalInfo& sig,
                          const gpga::MetalBuffer& buffer) {
  size_t word_size = SignalWordSize(sig);
  size_t word_count = SignalWordCount(sig);
  size_t stride = word_size * word_count;
  if (stride == 0) {
    return 0;
  }
  return buffer.length() / stride;
}

bool ReadSignalWordFromBuffer(const gpga::SignalInfo& sig, uint32_t gid,
                              uint64_t array_index,
                              const gpga::MetalBuffer& buffer,
                              size_t word_index, uint64_t* value_out) {
  if (!value_out || !buffer.contents()) {
    return false;
  }
  size_t word_count = SignalWordCount(sig);
  if (word_index >= word_count) {
    return false;
  }
  size_t array_size = sig.array_size > 0 ? sig.array_size : 1u;
  if (array_index >= array_size) {
    return false;
  }
  size_t element_count = SignalElementCount(sig, buffer);
  if (element_count == 0) {
    return false;
  }
  size_t element_index =
      static_cast<size_t>(gid) * array_size + static_cast<size_t>(array_index);
  if (element_index >= element_count) {
    return false;
  }
  size_t word_size = SignalWordSize(sig);
  size_t offset = (element_index * word_count + word_index) * word_size;
  if (buffer.length() < offset + word_size) {
    return false;
  }
  uint64_t value = 0;
  if (word_size == sizeof(uint64_t)) {
    std::memcpy(&value, static_cast<const uint8_t*>(buffer.contents()) + offset,
                sizeof(uint64_t));
  } else {
    uint32_t tmp = 0;
    std::memcpy(&tmp, static_cast<const uint8_t*>(buffer.contents()) + offset,
                sizeof(uint32_t));
    value = tmp;
  }
  *value_out = value;
  return true;
}

bool ReadPackedSignalWordFromBuffer(const gpga::SignalInfo& sig, uint32_t gid,
                                    uint64_t array_index,
                                    const gpga::MetalBuffer& buffer,
                                    size_t base_offset, size_t word_index,
                                    uint64_t* value_out) {
  if (!value_out || !buffer.contents()) {
    return false;
  }
  size_t word_count = SignalWordCount(sig);
  if (word_index >= word_count) {
    return false;
  }
  size_t array_size = sig.array_size > 0 ? sig.array_size : 1u;
  if (array_index >= array_size) {
    return false;
  }
  size_t word_size = SignalWordSize(sig);
  size_t element_size = SignalElementSize(sig);
  size_t element_index =
      static_cast<size_t>(gid) * array_size + static_cast<size_t>(array_index);
  size_t offset =
      base_offset + (element_index * element_size) + (word_index * word_size);
  if (buffer.length() < offset + word_size) {
    return false;
  }
  uint64_t value = 0;
  if (word_size == sizeof(uint64_t)) {
    std::memcpy(&value, static_cast<const uint8_t*>(buffer.contents()) + offset,
                sizeof(uint64_t));
  } else {
    uint32_t tmp = 0;
    std::memcpy(&tmp, static_cast<const uint8_t*>(buffer.contents()) + offset,
                sizeof(uint32_t));
    value = tmp;
  }
  *value_out = value;
  return true;
}

bool ReadSignalWord(const gpga::SignalInfo& sig, uint32_t gid,
                    uint64_t array_index,
                    const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
                    size_t word_index, uint64_t* value_out) {
  const gpga::MetalBuffer* val_buf =
      FindBuffer(buffers, MslSignalName(sig.name), "_val");
  if (!val_buf) {
    return false;
  }
  return ReadSignalWordFromBuffer(sig, gid, array_index, *val_buf, word_index,
                                  value_out);
}

bool WriteSignalWord(const gpga::SignalInfo& sig, uint32_t gid,
                     uint64_t array_index, size_t word_index, uint64_t value,
                     uint64_t xz, bool four_state,
                     std::unordered_map<std::string, gpga::MetalBuffer>* buffers) {
  gpga::MetalBuffer* val_buf =
      FindBufferMutable(buffers, MslSignalName(sig.name), "_val");
  if (!val_buf || !val_buf->contents()) {
    return false;
  }
  gpga::MetalBuffer* xz_buf = nullptr;
  if (four_state) {
    xz_buf = FindBufferMutable(buffers, MslSignalName(sig.name), "_xz");
  }
  size_t word_count = SignalWordCount(sig);
  if (word_index >= word_count) {
    return false;
  }
  size_t array_size = sig.array_size > 0 ? sig.array_size : 1u;
  if (array_index >= array_size) {
    return false;
  }
  size_t element_count = SignalElementCount(sig, *val_buf);
  if (element_count == 0) {
    return false;
  }
  size_t element_index =
      static_cast<size_t>(gid) * array_size + static_cast<size_t>(array_index);
  if (element_index >= element_count) {
    return false;
  }
  size_t word_size = SignalWordSize(sig);
  size_t offset = (element_index * word_count + word_index) * word_size;
  if (val_buf->length() < offset + word_size) {
    return false;
  }
  if (word_size == sizeof(uint64_t)) {
    std::memcpy(static_cast<uint8_t*>(val_buf->contents()) + offset, &value,
                sizeof(uint64_t));
    if (four_state && xz_buf && xz_buf->contents() &&
        xz_buf->length() >= offset + word_size) {
      std::memcpy(static_cast<uint8_t*>(xz_buf->contents()) + offset, &xz,
                  sizeof(uint64_t));
    }
  } else {
    uint32_t tmp = static_cast<uint32_t>(value);
    std::memcpy(static_cast<uint8_t*>(val_buf->contents()) + offset, &tmp,
                sizeof(uint32_t));
    if (four_state && xz_buf && xz_buf->contents() &&
        xz_buf->length() >= offset + word_size) {
      uint32_t tmp_xz = static_cast<uint32_t>(xz);
      std::memcpy(static_cast<uint8_t*>(xz_buf->contents()) + offset, &tmp_xz,
                  sizeof(uint32_t));
    }
  }
  return true;
}

bool ReadSignalValue(const gpga::SignalInfo& sig, uint32_t gid,
                     const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
                     uint64_t* value_out) {
  return ReadSignalWord(sig, gid, 0u, buffers, 0u, value_out);
}

bool WriteSignalValue(const gpga::SignalInfo& sig, uint32_t gid, uint64_t value,
                      uint64_t xz, bool four_state,
                      std::unordered_map<std::string, gpga::MetalBuffer>* buffers) {
  size_t word_count = SignalWordCount(sig);
  if (!WriteSignalWord(sig, gid, 0u, 0u, value, xz, four_state, buffers)) {
    return false;
  }
  for (size_t i = 1; i < word_count; ++i) {
    if (!WriteSignalWord(sig, gid, 0u, i, 0ull, 0ull, four_state, buffers)) {
      return false;
    }
  }
  return true;
}

bool WriteSignalValueAtIndex(const gpga::SignalInfo& sig, uint32_t gid,
                             uint64_t index, uint64_t value, uint64_t xz,
                             bool four_state,
                             std::unordered_map<std::string, gpga::MetalBuffer>* buffers) {
  size_t word_count = SignalWordCount(sig);
  if (!WriteSignalWord(sig, gid, index, 0u, value, xz, four_state, buffers)) {
    return false;
  }
  for (size_t i = 1; i < word_count; ++i) {
    if (!WriteSignalWord(sig, gid, index, i, 0ull, 0ull, four_state, buffers)) {
      return false;
    }
  }
  return true;
}

uint64_t PackStringBits(const std::string& value, size_t max_bytes) {
  uint64_t bits = 0;
  size_t count = std::min(max_bytes, sizeof(uint64_t));
  if (value.size() < count) {
    count = value.size();
  }
  size_t start = value.size() > count ? (value.size() - count) : 0;
  for (size_t i = 0; i < count; ++i) {
    size_t pos = count - 1u - i;
    bits |= (static_cast<uint64_t>(
                 static_cast<unsigned char>(value[start + i])) << (8u * pos));
  }
  return bits;
}

std::string UnpackStringBits(uint64_t bits, size_t max_bytes) {
  std::string out;
  out.reserve(max_bytes);
  size_t start = 0;
  for (; start < max_bytes; ++start) {
    size_t pos = max_bytes - 1u - start;
    char c = static_cast<char>((bits >> (8u * pos)) & 0xFFu);
    if (c != '\0') {
      break;
    }
  }
  for (size_t i = start; i < max_bytes; ++i) {
    size_t pos = max_bytes - 1u - i;
    char c = static_cast<char>((bits >> (8u * pos)) & 0xFFu);
    if (c == '\0') {
      break;
    }
    out.push_back(c);
  }
  return out;
}

std::vector<uint64_t> PackStringWords(const std::string& value,
                                      size_t max_bytes) {
  size_t word_count = std::max<size_t>(1u, (max_bytes + 7u) / 8u);
  size_t byte_count = word_count * 8u;
  std::vector<uint8_t> bytes(byte_count, 0u);
  size_t usable_start = byte_count > max_bytes ? byte_count - max_bytes : 0u;
  size_t count = std::min(value.size(), max_bytes);
  size_t src_start = value.size() > count ? (value.size() - count) : 0u;
  size_t dest_start = usable_start + (max_bytes - count);
  for (size_t i = 0; i < count; ++i) {
    bytes[dest_start + i] =
        static_cast<uint8_t>(value[src_start + i]);
  }
  std::vector<uint64_t> words(word_count, 0ull);
  for (size_t word_index = 0; word_index < word_count; ++word_index) {
    size_t byte_base = byte_count - (word_index + 1u) * 8u;
    uint64_t word = 0;
    for (size_t b = 0; b < 8u; ++b) {
      word |=
          static_cast<uint64_t>(bytes[byte_base + b]) << (8u * (7u - b));
    }
    words[word_index] = word;
  }
  return words;
}

std::string UnpackStringWords(const std::vector<uint64_t>& words,
                              size_t max_bytes) {
  if (words.empty() || max_bytes == 0u) {
    return {};
  }
  size_t word_count = words.size();
  size_t byte_count = word_count * 8u;
  std::vector<uint8_t> bytes(byte_count, 0u);
  for (size_t word_index = 0; word_index < word_count; ++word_index) {
    size_t byte_base = byte_count - (word_index + 1u) * 8u;
    uint64_t word = words[word_index];
    for (size_t b = 0; b < 8u; ++b) {
      bytes[byte_base + b] =
          static_cast<uint8_t>((word >> (8u * (7u - b))) & 0xFFu);
    }
  }
  size_t usable_start = byte_count > max_bytes ? byte_count - max_bytes : 0u;
  size_t start = usable_start;
  for (; start < byte_count; ++start) {
    if (bytes[start] != 0u) {
      break;
    }
  }
  std::string out;
  out.reserve(byte_count - start);
  for (size_t i = start; i < byte_count; ++i) {
    if (bytes[i] == 0u) {
      break;
    }
    out.push_back(static_cast<char>(bytes[i]));
  }
  return out;
}

std::string ReadSignalString(const gpga::SignalInfo& sig, uint32_t gid,
                             const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
  size_t max_bytes = (SignalBitWidth(sig) + 7u) / 8u;
  size_t word_count = SignalWordCount(sig);
  std::vector<uint64_t> words(word_count, 0ull);
  for (size_t i = 0; i < word_count; ++i) {
    if (!ReadSignalWord(sig, gid, 0u, buffers, i, &words[i])) {
      return {};
    }
  }
  return UnpackStringWords(words, max_bytes);
}

struct FormatSpec {
  char spec = 0;
  bool suppress = false;
};

std::vector<FormatSpec> ParseFormatSpecs(const std::string& format) {
  std::vector<FormatSpec> specs;
  for (size_t i = 0; i < format.size(); ++i) {
    if (format[i] != '%') {
      continue;
    }
    if (i + 1 < format.size() && format[i + 1] == '%') {
      ++i;
      continue;
    }
    bool suppress = false;
    size_t j = i + 1;
    while (j < format.size()) {
      char c = format[j];
      if (c == '*') {
        suppress = true;
        ++j;
        continue;
      }
      if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' ||
          c == '#' || c == '.') {
        ++j;
        continue;
      }
      FormatSpec spec;
      spec.spec =
          static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      spec.suppress = suppress;
      specs.push_back(spec);
      i = j;
      break;
    }
  }
  return specs;
}

bool ReadTokenFromFile(std::FILE* file, std::string* token, bool* saw_eof) {
  if (saw_eof) {
    *saw_eof = false;
  }
  if (!file || !token) {
    return false;
  }
  token->clear();
  int c = 0;
  do {
    c = std::fgetc(file);
    if (c == EOF) {
      if (saw_eof) {
        *saw_eof = true;
      }
      return false;
    }
  } while (std::isspace(static_cast<unsigned char>(c)));
  while (c != EOF && !std::isspace(static_cast<unsigned char>(c))) {
    token->push_back(static_cast<char>(c));
    c = std::fgetc(file);
  }
  return !token->empty();
}

bool ReadTokenFromString(const std::string& input, size_t* pos,
                         std::string* token) {
  if (!pos || !token) {
    return false;
  }
  token->clear();
  size_t i = *pos;
  while (i < input.size() &&
         std::isspace(static_cast<unsigned char>(input[i]))) {
    ++i;
  }
  if (i >= input.size()) {
    *pos = i;
    return false;
  }
  while (i < input.size() &&
         !std::isspace(static_cast<unsigned char>(input[i]))) {
    token->push_back(input[i]);
    ++i;
  }
  *pos = i;
  return !token->empty();
}

std::string PlusargPrefix(const std::string& format) {
  size_t pos = format.find('%');
  if (pos == std::string::npos) {
    return format;
  }
  return format.substr(0, pos);
}

bool FindPlusargMatch(const std::vector<std::string>& plusargs,
                      const std::string& prefix, std::string* value_out) {
  for (const auto& arg : plusargs) {
    if (arg.rfind(prefix, 0) == 0) {
      if (value_out) {
        *value_out = arg.substr(prefix.size());
      }
      return true;
    }
  }
  return false;
}

bool ParseTokenValue(const std::string& token, char spec, uint64_t* out_value,
                     std::string* out_string) {
  if (spec == 's') {
    if (out_string) {
      *out_string = token;
    }
    if (out_value) {
      *out_value = 0;
    }
    return true;
  }
  if (spec == 'c') {
    if (out_value) {
      unsigned char ch =
          token.empty() ? 0u : static_cast<unsigned char>(token[0]);
      *out_value = static_cast<uint64_t>(ch);
    }
    return true;
  }
  if (spec == 'f' || spec == 'e' || spec == 'g') {
    char* end = nullptr;
    double val = std::strtod(token.c_str(), &end);
    if (end == token.c_str()) {
      return false;
    }
    if (out_value) {
      uint64_t bits = 0;
      std::memcpy(&bits, &val, sizeof(bits));
      *out_value = bits;
    }
    return true;
  }
  int base = 10;
  if (spec == 'h' || spec == 'x') {
    base = 16;
  } else if (spec == 'o') {
    base = 8;
  } else if (spec == 'b') {
    base = 2;
  }
  char* end = nullptr;
  if (spec == 'd' || spec == 'i') {
    int parse_base = (spec == 'i') ? 0 : base;
    long long val = std::strtoll(token.c_str(), &end, parse_base);
    if (end == token.c_str()) {
      return false;
    }
    if (out_value) {
      *out_value = static_cast<uint64_t>(val);
    }
    return true;
  }
  unsigned long long val = std::strtoull(token.c_str(), &end, base);
  if (end == token.c_str()) {
    return false;
  }
  if (out_value) {
    *out_value = static_cast<uint64_t>(val);
  }
  return true;
}

bool ParseTokenWords(const std::string& token_in, char spec, uint32_t width,
                     std::vector<uint64_t>* out_words,
                     std::string* out_string) {
  if (!out_words) {
    return false;
  }
  out_words->clear();
  if (spec == 's') {
    if (out_string) {
      *out_string = token_in;
    }
    return true;
  }
  if (width == 0u) {
    width = 1u;
  }
  size_t word_count = (width + 63u) / 64u;
  out_words->assign(word_count, 0ull);
  if (spec == 'c') {
    unsigned char ch =
        token_in.empty() ? 0u : static_cast<unsigned char>(token_in[0]);
    (*out_words)[0] = static_cast<uint64_t>(ch);
  } else if (spec == 'f' || spec == 'e' || spec == 'g') {
    char* end = nullptr;
    double val = std::strtod(token_in.c_str(), &end);
    if (end == token_in.c_str()) {
      return false;
    }
    uint64_t bits = 0;
    std::memcpy(&bits, &val, sizeof(bits));
    (*out_words)[0] = bits;
  } else {
    std::string token;
    token.reserve(token_in.size());
    for (char c : token_in) {
      if (c != '_') {
        token.push_back(c);
      }
    }
    if (token.empty()) {
      return false;
    }
    bool allow_sign = (spec == 'd' || spec == 'i');
    bool negative = false;
    if (!token.empty() && (token[0] == '+' || token[0] == '-')) {
      if (!allow_sign) {
        return false;
      }
      negative = (token[0] == '-');
      token.erase(token.begin());
    }
    int base = 10;
    if (spec == 'h' || spec == 'x') {
      base = 16;
    } else if (spec == 'o') {
      base = 8;
    } else if (spec == 'b') {
      base = 2;
    } else if (spec == 'i') {
      if (token.size() >= 2 && token[0] == '0' &&
          (token[1] == 'x' || token[1] == 'X')) {
        base = 16;
        token = token.substr(2);
      } else if (token.size() >= 2 && token[0] == '0' &&
                 (token[1] == 'b' || token[1] == 'B')) {
        base = 2;
        token = token.substr(2);
      } else if (token.size() > 1 && token[0] == '0') {
        base = 8;
        token = token.substr(1);
      }
    } else {
      if (base == 16 && token.size() >= 2 && token[0] == '0' &&
          (token[1] == 'x' || token[1] == 'X')) {
        token = token.substr(2);
      } else if (base == 2 && token.size() >= 2 && token[0] == '0' &&
                 (token[1] == 'b' || token[1] == 'B')) {
        token = token.substr(2);
      }
    }
    if (token.empty()) {
      return false;
    }
    for (char c : token) {
      int digit = -1;
      if (c >= '0' && c <= '9') {
        digit = c - '0';
      } else if (c >= 'a' && c <= 'f') {
        digit = 10 + (c - 'a');
      } else if (c >= 'A' && c <= 'F') {
        digit = 10 + (c - 'A');
      }
      if (digit < 0 || digit >= base) {
        return false;
      }
      uint64_t carry = static_cast<uint64_t>(digit);
      for (size_t i = 0; i < out_words->size(); ++i) {
        unsigned __int128 prod =
            static_cast<unsigned __int128>((*out_words)[i]) *
                static_cast<unsigned __int128>(base) +
            carry;
        (*out_words)[i] = static_cast<uint64_t>(prod);
        carry = static_cast<uint64_t>(prod >> 64);
      }
    }
    if (negative) {
      for (auto& word : *out_words) {
        word = ~word;
      }
      uint64_t carry = 1u;
      for (size_t i = 0; i < out_words->size(); ++i) {
        unsigned __int128 sum =
            static_cast<unsigned __int128>((*out_words)[i]) + carry;
        (*out_words)[i] = static_cast<uint64_t>(sum);
        carry = static_cast<uint64_t>(sum >> 64);
      }
    }
  }
  uint32_t tail_bits = width % 64u;
  if (tail_bits != 0u) {
    uint64_t mask = (1ull << tail_bits) - 1ull;
    (*out_words)[out_words->size() - 1u] &= mask;
  }
  return true;
}

uint32_t ReadU32(const uint8_t* base, size_t offset) {
  uint32_t value = 0;
  std::memcpy(&value, base + offset, sizeof(value));
  return value;
}

uint64_t ReadU64(const uint8_t* base, size_t offset) {
  uint64_t value = 0;
  std::memcpy(&value, base + offset, sizeof(value));
  return value;
}

std::string ResolveString(const gpga::ServiceStringTable& strings,
                          uint32_t id) {
  if (id >= strings.entries.size()) {
    return "<invalid_string_id>";
  }
  return strings.entries[id];
}

uint64_t MaskForWidth(uint32_t width) {
  if (width >= 64u) {
    return 0xFFFFFFFFFFFFFFFFull;
  }
  if (width == 0u) {
    return 0ull;
  }
  return (1ull << width) - 1ull;
}

int64_t SignExtend(uint64_t value, uint32_t width) {
  if (width == 0u || width >= 64u) {
    return static_cast<int64_t>(value);
  }
  uint64_t mask = MaskForWidth(width);
  uint64_t sign_bit = 1ull << (width - 1u);
  uint64_t masked = value & mask;
  if ((masked & sign_bit) != 0ull) {
    return static_cast<int64_t>(masked | ~mask);
  }
  return static_cast<int64_t>(masked);
}

struct TimeFormatState {
  bool active = false;
  int units = 0;
  int precision = 0;
  std::string suffix;
  int min_width = 0;
};

TimeFormatState g_time_format;
int g_timescale_exp = -9;

std::string TrimString(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1u]))) {
    --end;
  }
  return value.substr(start, end - start);
}

int ParseTimescaleExponent(const std::string& timescale) {
  std::string token = TrimString(timescale);
  size_t slash = token.find('/');
  if (slash != std::string::npos) {
    token = TrimString(token.substr(0, slash));
  }
  if (token.empty()) {
    return -9;
  }
  size_t pos = 0;
  while (pos < token.size() &&
         std::isdigit(static_cast<unsigned char>(token[pos]))) {
    ++pos;
  }
  int magnitude = 1;
  if (pos > 0) {
    magnitude = std::atoi(token.substr(0, pos).c_str());
  }
  std::string unit = TrimString(token.substr(pos));
  for (auto& ch : unit) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  int unit_exp = -9;
  if (unit == "s") {
    unit_exp = 0;
  } else if (unit == "ms") {
    unit_exp = -3;
  } else if (unit == "us") {
    unit_exp = -6;
  } else if (unit == "ns") {
    unit_exp = -9;
  } else if (unit == "ps") {
    unit_exp = -12;
  } else if (unit == "fs") {
    unit_exp = -15;
  }
  int mag_exp = 0;
  if (magnitude == 10) {
    mag_exp = 1;
  } else if (magnitude == 100) {
    mag_exp = 2;
  }
  return unit_exp + mag_exp;
}

std::string FormatBits(uint64_t value, uint64_t xz, uint32_t width, int base,
                       bool has_xz) {
  if (width == 0u) {
    width = 1u;
  }
  if (width > 64u) {
    width = 64u;
  }
  uint64_t mask = MaskForWidth(width);
  value &= mask;
  xz &= mask;

  int group = 1;
  if (base == 16) {
    group = 4;
  } else if (base == 8) {
    group = 3;
  }
  int digits = static_cast<int>((width + group - 1u) / group);
  std::string out;
  out.reserve(static_cast<size_t>(digits));
  for (int i = digits - 1; i >= 0; --i) {
    int shift = i * group;
    uint64_t group_mask = ((1ull << group) - 1ull) << shift;
    if (has_xz && (xz & group_mask) != 0ull) {
      out.push_back('x');
      continue;
    }
    uint64_t digit = (value >> shift) & ((1ull << group) - 1ull);
    if (base == 16) {
      out.push_back("0123456789abcdef"[digit & 0xF]);
    } else if (base == 8) {
      out.push_back("01234567"[digit & 0x7]);
    } else {
      out.push_back((digit & 1ull) ? '1' : '0');
    }
  }
  return out;
}

bool WideHasXz(const std::vector<uint64_t>& words) {
  for (uint64_t word : words) {
    if (word != 0u) {
      return true;
    }
  }
  return false;
}

bool WideBit(const std::vector<uint64_t>& words, uint32_t bit) {
  size_t word_index = bit / 64u;
  if (word_index >= words.size()) {
    return false;
  }
  uint32_t shift = bit % 64u;
  return ((words[word_index] >> shift) & 1ull) != 0ull;
}

std::vector<uint64_t> MaskWideWords(std::vector<uint64_t> words,
                                    uint32_t width) {
  if (width == 0u || words.empty()) {
    return words;
  }
  uint32_t word_count = (width + 63u) / 64u;
  if (words.size() > word_count) {
    words.resize(word_count);
  }
  uint32_t rem = width % 64u;
  if (rem != 0u && !words.empty()) {
    uint64_t mask = (1ull << rem) - 1ull;
    words.back() &= mask;
  }
  return words;
}

std::string FormatWideBits(const std::vector<uint64_t>& value_words,
                           const std::vector<uint64_t>& xz_words,
                           uint32_t width, int base, bool has_xz) {
  if (width == 0u) {
    width = 1u;
  }
  int group = 1;
  if (base == 16) {
    group = 4;
  } else if (base == 8) {
    group = 3;
  }
  int digits = static_cast<int>((width + group - 1u) / group);
  std::string out;
  out.reserve(static_cast<size_t>(digits));
  for (int i = digits - 1; i >= 0; --i) {
    int shift = i * group;
    bool group_xz = false;
    uint64_t digit = 0;
    for (int bit = 0; bit < group; ++bit) {
      int bit_index = shift + bit;
      if (bit_index >= static_cast<int>(width)) {
        continue;
      }
      if (has_xz && WideBit(xz_words, static_cast<uint32_t>(bit_index))) {
        group_xz = true;
      }
      if (WideBit(value_words, static_cast<uint32_t>(bit_index))) {
        digit |= (1ull << bit);
      }
    }
    if (has_xz && group_xz) {
      out.push_back('x');
      continue;
    }
    if (base == 16) {
      out.push_back("0123456789abcdef"[digit & 0xF]);
    } else if (base == 8) {
      out.push_back("01234567"[digit & 0x7]);
    } else {
      out.push_back((digit & 1ull) ? '1' : '0');
    }
  }
  return out;
}

std::string FormatWideUnsigned(std::vector<uint64_t> words, uint32_t width) {
  words = MaskWideWords(std::move(words), width);
  while (!words.empty() && words.back() == 0u) {
    words.pop_back();
  }
  if (words.empty()) {
    return "0";
  }
  std::string out;
  while (!words.empty()) {
    unsigned __int128 rem = 0;
    for (size_t i = words.size(); i-- > 0;) {
      unsigned __int128 cur = (rem << 64) | words[i];
      words[i] = static_cast<uint64_t>(cur / 10u);
      rem = cur % 10u;
    }
    out.push_back(static_cast<char>('0' + static_cast<uint32_t>(rem)));
    while (!words.empty() && words.back() == 0u) {
      words.pop_back();
    }
  }
  std::reverse(out.begin(), out.end());
  return out;
}

std::string FormatWideSigned(std::vector<uint64_t> words, uint32_t width) {
  words = MaskWideWords(std::move(words), width);
  if (width == 0u || words.empty()) {
    return "0";
  }
  bool sign = WideBit(words, width - 1u);
  if (!sign) {
    return FormatWideUnsigned(words, width);
  }
  for (auto& word : words) {
    word = ~word;
  }
  words = MaskWideWords(std::move(words), width);
  uint64_t carry = 1u;
  for (auto& word : words) {
    uint64_t prev = word;
    word += carry;
    carry = (word < prev) ? 1u : 0u;
  }
  return "-" + FormatWideUnsigned(words, width);
}

std::string ApplyPadding(std::string text, int width, bool zero_pad) {
  if (width <= 0 || static_cast<int>(text.size()) >= width) {
    return text;
  }
  char pad_char = zero_pad ? '0' : ' ';
  int pad_len = width - static_cast<int>(text.size());
  if (zero_pad && !text.empty() && text[0] == '-') {
    return "-" + std::string(pad_len, pad_char) + text.substr(1);
  }
  return std::string(pad_len, pad_char) + text;
}

std::string FormatNumeric(const gpga::ServiceArgView& arg, char spec,
                          bool has_xz) {
  if (arg.kind == gpga::ServiceArgKind::kWide && !arg.wide_value.empty()) {
    const std::vector<uint64_t>& val = arg.wide_value;
    const std::vector<uint64_t>& xz = arg.wide_xz;
    if (has_xz && WideHasXz(xz) &&
        (spec == 'd' || spec == 'u' || spec == 't')) {
      return "x";
    }
    if (spec == 'b') {
      return FormatWideBits(val, xz, arg.width, 2, has_xz);
    }
    if (spec == 'o') {
      return FormatWideBits(val, xz, arg.width, 8, has_xz);
    }
    if (spec == 'h' || spec == 'x') {
      return FormatWideBits(val, xz, arg.width, 16, has_xz);
    }
    if (spec == 'u' || spec == 't') {
      return FormatWideUnsigned(val, arg.width);
    }
    return FormatWideSigned(val, arg.width);
  }
  if (has_xz && arg.xz != 0u &&
      (spec == 'd' || spec == 'u' || spec == 't')) {
    return "x";
  }
  uint32_t width = arg.width;
  if (spec == 'b') {
    return FormatBits(arg.value, arg.xz, width, 2, has_xz);
  }
  if (spec == 'o') {
    return FormatBits(arg.value, arg.xz, width, 8, has_xz);
  }
  if (spec == 'h' || spec == 'x') {
    return FormatBits(arg.value, arg.xz, width, 16, has_xz);
  }
  if (spec == 't') {
    if (!g_time_format.active) {
      return std::to_string(arg.value);
    }
    if (has_xz && arg.xz != 0u) {
      return "x";
    }
    double scaled = static_cast<double>(arg.value) *
                    std::pow(10.0,
                             static_cast<double>(g_timescale_exp -
                                                 g_time_format.units));
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(g_time_format.precision) << scaled;
    std::string text = oss.str();
    text = ApplyPadding(text, g_time_format.min_width, false);
    return text + g_time_format.suffix;
  }
  if (spec == 'u') {
    uint64_t mask = MaskForWidth(width);
    return std::to_string(arg.value & mask);
  }
int64_t signed_value = SignExtend(arg.value, width);
return std::to_string(signed_value);
}

struct PackedSignalOffsets {
  size_t val_offset = 0u;
  size_t xz_offset = 0u;
  size_t elem_size = 0u;
  size_t array_size = 1u;
  bool has_val = false;
  bool has_xz = false;
};

struct PackedStateLayout {
  std::unordered_map<std::string, PackedSignalOffsets> offsets;

  const PackedSignalOffsets* Find(const std::string& name) const {
    auto it = offsets.find(name);
    if (it == offsets.end()) {
      return nullptr;
    }
    return &it->second;
  }
};

size_t Align8(size_t value) {
  return (value + 7u) & ~static_cast<size_t>(7u);
}

PackedStateLayout BuildPackedStateLayout(const gpga::Module& module,
                                         const gpga::ModuleInfo& info,
                                         bool four_state,
                                         uint32_t count) {
  PackedStateLayout layout;
  std::unordered_set<std::string> scheduled_reads;
  for (const auto& block : module.always_blocks) {
    if (block.edge == gpga::EdgeKind::kCombinational) {
      continue;
    }
    if (block.edge == gpga::EdgeKind::kPosedge ||
        block.edge == gpga::EdgeKind::kNegedge) {
      if (!block.clock.empty()) {
        scheduled_reads.insert(block.clock);
      }
    }
    for (const auto& stmt : block.statements) {
      CollectReadSignals(stmt, &scheduled_reads);
    }
  }
  std::unordered_set<std::string> port_names;
  port_names.reserve(module.ports.size());
  for (const auto& port : module.ports) {
    port_names.insert(port.name);
  }
  std::vector<std::string> reg_names;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      continue;
    }
    if (port_names.count(net.name) > 0 || net.type == gpga::NetType::kTrireg) {
      continue;
    }
    if (net.type == gpga::NetType::kReg ||
        scheduled_reads.count(net.name) > 0) {
      reg_names.push_back(net.name);
    }
  }
  std::vector<const gpga::Net*> trireg_nets;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      continue;
    }
    if (net.type == gpga::NetType::kTrireg &&
        !IsOutputPort(module, net.name)) {
      trireg_nets.push_back(&net);
    }
  }
  std::vector<const gpga::Net*> array_nets;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      array_nets.push_back(&net);
    }
  }
  size_t offset = 0;
  auto add_segment = [&](const std::string& base_name, bool is_xz) {
    const gpga::SignalInfo* sig = FindSignalInfo(info, base_name);
    if (!sig) {
      return;
    }
    size_t array_size = sig->array_size > 0 ? sig->array_size : 1u;
    size_t elem_size = SignalElementSize(*sig);
    offset = Align8(offset);
    PackedSignalOffsets& entry = layout.offsets[base_name];
    entry.elem_size = elem_size;
    entry.array_size = array_size;
    if (is_xz) {
      entry.has_xz = true;
      entry.xz_offset = offset;
    } else {
      entry.has_val = true;
      entry.val_offset = offset;
    }
    offset += static_cast<size_t>(count) * array_size * elem_size;
  };
  auto add_decay = [&]() {
    offset = Align8(offset);
    offset += static_cast<size_t>(count) * sizeof(uint64_t);
  };
  for (const auto& port : module.ports) {
    add_segment(port.name, false);
    if (four_state) {
      add_segment(port.name, true);
    }
  }
  for (const auto& reg : reg_names) {
    add_segment(reg, false);
    if (four_state) {
      add_segment(reg, true);
    }
  }
  for (const auto* reg : trireg_nets) {
    add_segment(reg->name, false);
    if (four_state) {
      add_segment(reg->name, true);
    }
    add_decay();
  }
  for (const auto* net : array_nets) {
    add_segment(net->name, false);
    if (four_state) {
      add_segment(net->name, true);
    }
  }
  return layout;
}

std::string FormatReal(const gpga::ServiceArgView& arg, char spec,
                       int precision, bool has_xz) {
  if (has_xz && arg.xz != 0u) {
    return "x";
  }
  double value = 0.0;
  if (arg.kind == gpga::ServiceArgKind::kReal) {
    uint64_t bits = arg.value;
    std::memcpy(&value, &bits, sizeof(value));
  } else {
    int64_t signed_value = SignExtend(arg.value, arg.width);
    value = static_cast<double>(signed_value);
  }
  std::ostringstream oss;
  if (spec == 'f') {
    oss << std::fixed;
  } else if (spec == 'e') {
    oss << std::scientific;
  }
  if (precision >= 0) {
    oss << std::setprecision(precision);
  }
  oss << value;
  return oss.str();
}

std::string FormatArg(const gpga::ServiceArgView& arg, char spec, int precision,
                      const gpga::ServiceStringTable& strings, bool has_xz) {
  if (arg.kind == gpga::ServiceArgKind::kString ||
      arg.kind == gpga::ServiceArgKind::kIdent) {
    return ResolveString(strings, static_cast<uint32_t>(arg.value));
  }
  if (spec == 's') {
    return FormatNumeric(arg, 'd', has_xz);
  }
  if (spec == 'f' || spec == 'e' || spec == 'g') {
    return FormatReal(arg, spec, precision, has_xz);
  }
  return FormatNumeric(arg, spec, has_xz);
}

std::string FormatWithSpec(const std::string& fmt,
                           const std::vector<gpga::ServiceArgView>& args,
                           size_t start_index,
                           const gpga::ServiceStringTable& strings,
                           bool has_xz,
                           const gpga::ModuleInfo* module,
                           const std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
                           uint32_t gid) {
  std::ostringstream oss;
  size_t arg_index = start_index;
  for (size_t i = 0; i < fmt.size(); ++i) {
    char c = fmt[i];
    if (c != '%') {
      oss << c;
      continue;
    }
    if (i + 1 < fmt.size() && fmt[i + 1] == '%') {
      oss << '%';
      ++i;
      continue;
    }
    bool zero_pad = false;
    int width = 0;
    int precision = -1;
    size_t j = i + 1;
    if (j < fmt.size() && fmt[j] == '0') {
      zero_pad = true;
      ++j;
    }
    while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') {
      width = (width * 10) + (fmt[j] - '0');
      ++j;
    }
    if (j < fmt.size() && fmt[j] == '.') {
      ++j;
      precision = 0;
      while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') {
        precision = (precision * 10) + (fmt[j] - '0');
        ++j;
      }
    }
    if (j >= fmt.size()) {
      break;
    }
    char spec = fmt[j];
    if (spec >= 'A' && spec <= 'Z') {
      spec = static_cast<char>(spec - 'A' + 'a');
    }
    i = j;
    if (arg_index >= args.size()) {
      oss << ApplyPadding("<missing>", width, false);
      continue;
    }
    std::string text;
    if (spec == 's') {
      const auto& arg = args[arg_index];
      if (arg.kind == gpga::ServiceArgKind::kIdent && module && buffers) {
        std::string name =
            ResolveString(strings, static_cast<uint32_t>(arg.value));
        const gpga::SignalInfo* sig = FindSignalInfo(*module, name);
        if (sig) {
          text = ReadSignalString(*sig, gid, *buffers);
        } else {
          text = name;
        }
      } else if (arg.kind == gpga::ServiceArgKind::kString ||
                 arg.kind == gpga::ServiceArgKind::kIdent) {
        text = ResolveString(strings, static_cast<uint32_t>(arg.value));
      } else if (arg.kind == gpga::ServiceArgKind::kWide &&
                 !arg.wide_value.empty()) {
        size_t max_bytes = (arg.width + 7u) / 8u;
        text = UnpackStringWords(arg.wide_value, max_bytes);
      } else {
        size_t max_bytes = (arg.width + 7u) / 8u;
        max_bytes = std::min(max_bytes, sizeof(uint64_t));
        text = UnpackStringBits(arg.value, max_bytes);
      }
    } else {
      text = FormatArg(args[arg_index], spec, precision, strings, has_xz);
    }
    ++arg_index;
    oss << ApplyPadding(std::move(text), width, zero_pad);
  }
  return oss.str();
}

std::string FormatDefaultArgs(const std::vector<gpga::ServiceArgView>& args,
                              const gpga::ServiceStringTable& strings,
                              bool has_xz) {
  std::ostringstream oss;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      oss << " ";
    }
    const auto& arg = args[i];
    if (arg.kind == gpga::ServiceArgKind::kString ||
        arg.kind == gpga::ServiceArgKind::kIdent) {
      oss << ResolveString(strings, static_cast<uint32_t>(arg.value));
    } else if (arg.kind == gpga::ServiceArgKind::kReal) {
      oss << FormatReal(arg, 'g', -1, has_xz);
    } else {
      oss << FormatNumeric(arg, 'd', has_xz);
    }
  }
  return oss.str();
}

struct DecodedServiceRecord {
  gpga::ServiceKind kind = gpga::ServiceKind::kDisplay;
  uint32_t pid = 0;
  uint32_t format_id = 0xFFFFFFFFu;
  std::vector<gpga::ServiceArgView> args;
};

void DecodeServiceRecords(const void* records, uint32_t record_count,
                          uint32_t max_args, uint32_t wide_words, bool has_xz,
                          std::vector<DecodedServiceRecord>* out) {
  if (!out) {
    return;
  }
  out->clear();
  if (!records || record_count == 0 || max_args == 0) {
    return;
  }
  const auto* base = static_cast<const uint8_t*>(records);
  const size_t stride =
      gpga::ServiceRecordStride(max_args, wide_words, has_xz);
  out->reserve(record_count);
  for (uint32_t i = 0; i < record_count; ++i) {
    const uint8_t* rec = base + (stride * i);
    DecodedServiceRecord record;
    uint32_t kind_raw = ReadU32(rec, 0);
    record.kind = static_cast<gpga::ServiceKind>(kind_raw);
    record.pid = ReadU32(rec, sizeof(uint32_t));
    record.format_id = ReadU32(rec, sizeof(uint32_t) * 2u);
    uint32_t arg_count = ReadU32(rec, sizeof(uint32_t) * 3u);
    if (arg_count > max_args) {
      arg_count = max_args;
    }

    size_t offset = sizeof(uint32_t) * 4u;
    const size_t arg_kind_offset = offset;
    const size_t arg_width_offset =
        arg_kind_offset + sizeof(uint32_t) * max_args;
    const size_t arg_val_offset =
        arg_width_offset + sizeof(uint32_t) * max_args;
    const size_t arg_xz_offset = arg_val_offset + sizeof(uint64_t) * max_args;
    const size_t arg_wide_val_offset =
        arg_xz_offset + (has_xz ? sizeof(uint64_t) * max_args : 0u);
    const size_t arg_wide_xz_offset =
        arg_wide_val_offset +
        sizeof(uint64_t) * max_args * static_cast<size_t>(wide_words);

    record.args.reserve(arg_count);
    for (uint32_t a = 0; a < arg_count; ++a) {
      gpga::ServiceArgView arg;
      arg.kind = static_cast<gpga::ServiceArgKind>(
          ReadU32(rec, arg_kind_offset + sizeof(uint32_t) * a));
      arg.width = ReadU32(rec, arg_width_offset + sizeof(uint32_t) * a);
      arg.value = ReadU64(rec, arg_val_offset + sizeof(uint64_t) * a);
      if (has_xz) {
        arg.xz = ReadU64(rec, arg_xz_offset + sizeof(uint64_t) * a);
      }
      if (arg.kind == gpga::ServiceArgKind::kWide && wide_words > 0u) {
        uint32_t word_count = (arg.width + 63u) / 64u;
        size_t base =
            arg_wide_val_offset +
            sizeof(uint64_t) * (static_cast<size_t>(a) * wide_words);
        arg.wide_value.resize(word_count, 0ull);
        for (uint32_t w = 0; w < word_count; ++w) {
          arg.wide_value[w] =
              ReadU64(rec, base + sizeof(uint64_t) * w);
        }
        if (has_xz) {
          size_t xz_base =
              arg_wide_xz_offset +
              sizeof(uint64_t) * (static_cast<size_t>(a) * wide_words);
          arg.wide_xz.resize(word_count, 0ull);
          for (uint32_t w = 0; w < word_count; ++w) {
            arg.wide_xz[w] =
                ReadU64(rec, xz_base + sizeof(uint64_t) * w);
          }
        }
      }
      record.args.push_back(arg);
    }
    out->push_back(std::move(record));
  }
}

std::string VcdIdForIndex(size_t index) {
  const int base = 94;
  const int first = 33;
  std::string id;
  size_t value = index;
  do {
    int digit = static_cast<int>(value % base);
    id.push_back(static_cast<char>(first + digit));
    value /= base;
  } while (value > 0);
  return id;
}

struct VcdSignal {
  std::string name;
  std::string id;
  std::string base_name;
  uint32_t width = 1;
  uint32_t word_count = 1;
  uint32_t array_size = 1;
  uint32_t array_index = 0;
  uint32_t instance_index = 0;
  bool is_real = false;
  uint64_t last_val = 0;
  uint64_t last_xz = 0;
  std::vector<uint64_t> last_val_words;
  std::vector<uint64_t> last_xz_words;
  bool has_value = false;
};

class VcdWriter {
 public:
  void SetPackedLayout(const PackedStateLayout* layout) {
    packed_layout_ = layout;
  }

  bool Start(const std::string& filename, const std::string& output_dir,
             const gpga::ModuleInfo& module,
             const std::vector<std::string>& filter, uint32_t depth,
             bool four_state,
             const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
             const std::string& timescale,
             uint32_t instance_count,
             const std::unordered_map<std::string, std::string>* flat_to_hier,
             std::string* error) {
    if (active_) {
      return true;
    }
    std::string path = filename.empty() ? "dump.vcd" : filename;
    if (!output_dir.empty()) {
      std::filesystem::path base(output_dir);
      std::filesystem::path out_path(path);
      if (!out_path.is_absolute()) {
        out_path = base / out_path;
      }
      std::error_code ec;
      std::filesystem::create_directories(out_path.parent_path(), ec);
      if (ec) {
        if (error) {
          *error = "failed to create VCD directory: " +
                   out_path.parent_path().string();
        }
        return false;
      }
      path = out_path.string();
    }
    out_.open(path, std::ios::out | std::ios::trunc);
    if (!out_) {
      if (error) {
        *error = "failed to open VCD file: " + path;
      }
      return false;
    }
    four_state_ = four_state;
    timescale_ = timescale.empty() ? "1ns" : timescale;
    dumping_ = true;
    bool dump_all = filter.empty();
    for (const auto& name : filter) {
      if (name == module.name) {
        dump_all = true;
        break;
      }
    }
    BuildSignals(module, filter, dump_all, depth, instance_count, flat_to_hier);
    WriteHeader(module.name);
    EmitInitialValues(buffers);
    active_ = true;
    return true;
  }

  void Update(uint64_t time,
              const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
    if (!dumping_) {
      return;
    }
    EmitSnapshot(time, buffers, false);
  }

  void FinalSnapshot(
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
    if (!active_) {
      return;
    }
    if (!dumping_) {
      return;
    }
    uint64_t time = has_time_ ? last_time_ : 0;
    if (!last_time_had_values_) {
      EmitSnapshot(time, buffers, true);
    }
  }

  void ForceSnapshot(
      uint64_t time,
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
    if (!active_ || !dumping_) {
      return;
    }
    EmitSnapshot(time, buffers, true);
  }

  void SetDumping(bool enabled) { dumping_ = enabled; }

  void SetDumpLimit(uint64_t limit) {
    dump_limit_ = limit;
    CheckDumpLimit();
  }

  void Flush() {
    if (out_) {
      out_.flush();
    }
  }

  void Close() {
    if (out_) {
      out_.flush();
      out_.close();
    }
    active_ = false;
  }

  bool active() const { return active_; }

 private:
  void BuildSignals(const gpga::ModuleInfo& module,
                    const std::vector<std::string>& filter, bool dump_all,
                    uint32_t depth_limit, uint32_t instance_count,
                    const std::unordered_map<std::string, std::string>* flat_to_hier) {
    std::unordered_set<std::string> wanted(filter.begin(), filter.end());
    signals_.clear();
    size_t index = 0;
    for (const auto& sig : module.signals) {
      std::string display_base = sig.name;
      if (flat_to_hier) {
        auto it = flat_to_hier->find(sig.name);
        if (it != flat_to_hier->end() && !it->second.empty()) {
          display_base = it->second;
        }
      }
      std::string display_rel = StripModulePrefix(display_base, module.name);
      if (display_rel.empty()) {
        display_rel = display_base;
      }
      uint32_t array_size = sig.array_size > 0 ? sig.array_size : 1u;
      uint32_t inst_count = std::max<uint32_t>(1u, instance_count);
      for (uint32_t inst = 0; inst < inst_count; ++inst) {
        std::string display_name = display_rel;
        if (inst_count > 1u) {
          display_name =
              "inst" + std::to_string(inst) + "." + display_rel;
        }
        for (uint32_t i = 0; i < array_size; ++i) {
          bool include = dump_all || wanted.empty();
          if (!include) {
            include =
                MatchesFilterName(wanted, module.name, display_name, i) ||
                MatchesFilterName(wanted, module.name, display_rel, i) ||
                MatchesFilterName(wanted, module.name, sig.name, i);
          }
          if (!include) {
            continue;
          }
          if (depth_limit > 0 && !PassesDepth(display_rel, depth_limit)) {
            continue;
          }
          VcdSignal entry;
          entry.base_name = sig.name;
          entry.array_size = array_size;
          entry.array_index = i;
          entry.instance_index = inst;
          entry.is_real = sig.is_real;
          entry.width = sig.is_real ? 64u : std::max<uint32_t>(1u, sig.width);
          entry.word_count = entry.width <= 64u ? 1u :
              static_cast<uint32_t>((entry.width + 63u) / 64u);
          entry.id = VcdIdForIndex(index++);
          if (array_size > 1u) {
            entry.name = display_name + "[" + std::to_string(i) + "]";
          } else {
            entry.name = display_name;
          }
          signals_.push_back(std::move(entry));
        }
      }
    }
  }

  bool PassesDepth(const std::string& name, uint32_t depth_limit) const {
    if (depth_limit == 0u) {
      return true;
    }
    std::vector<std::string> scope;
    std::string leaf;
    SplitHierName(name, &scope, &leaf);
    uint32_t depth = scope.empty() ? 1u
                                   : static_cast<uint32_t>(scope.size() + 1u);
    return depth <= depth_limit;
  }

  bool MatchesFilterName(const std::unordered_set<std::string>& wanted,
                         const std::string& module_name,
                         const std::string& name, uint32_t index) const {
    if (wanted.empty()) {
      return true;
    }
    const std::string indexed = name + "[" + std::to_string(index) + "]";
    for (const auto& raw : wanted) {
      std::string filter = raw;
      if (!module_name.empty() && StartsWith(filter, module_name + ".")) {
        filter = filter.substr(module_name.size() + 1);
      } else if (!module_name.empty() &&
                 StartsWith(filter, module_name + "__")) {
        filter = filter.substr(module_name.size() + 2);
      }
      if (filter == name || filter == indexed) {
        return true;
      }
      if (StartsWith(name, filter)) {
        if (name.size() == filter.size()) {
          return true;
        }
        char next = name[filter.size()];
        if (next == '.') {
          return true;
        }
        if (next == '_' && filter.size() + 1 < name.size() &&
            name[filter.size() + 1] == '_') {
          return true;
        }
      }
    }
    return false;
  }

  bool StartsWith(const std::string& value, const std::string& prefix) const {
    return value.size() >= prefix.size() &&
           value.compare(0, prefix.size(), prefix) == 0;
  }

  std::string StripModulePrefix(const std::string& name,
                                const std::string& module_name) const {
    if (module_name.empty()) {
      return name;
    }
    const std::string dot_prefix = module_name + ".";
    const std::string flat_prefix = module_name + "__";
    if (StartsWith(name, dot_prefix)) {
      return name.substr(dot_prefix.size());
    }
    if (StartsWith(name, flat_prefix)) {
      return name.substr(flat_prefix.size());
    }
    return name;
  }

  void SplitHierName(const std::string& name, std::vector<std::string>* scope,
                     std::string* leaf) const {
    if (scope) {
      scope->clear();
    }
    if (leaf) {
      *leaf = name;
    }
    if (!scope || !leaf) {
      return;
    }
    std::vector<std::string> parts;
    std::string current;
    int bracket_depth = 0;
    for (size_t i = 0; i < name.size(); ++i) {
      char c = name[i];
      if (c == '[') {
        bracket_depth++;
        current.push_back(c);
        continue;
      }
      if (c == ']') {
        if (bracket_depth > 0) {
          bracket_depth--;
        }
        current.push_back(c);
        continue;
      }
      if (bracket_depth == 0 && c == '.') {
        if (!current.empty()) {
          parts.push_back(current);
          current.clear();
        }
        continue;
      }
      if (bracket_depth == 0 && c == '_' && i + 1 < name.size() &&
          name[i + 1] == '_') {
        if (!current.empty()) {
          parts.push_back(current);
          current.clear();
        }
        i++;
        continue;
      }
      current.push_back(c);
    }
    if (!current.empty()) {
      parts.push_back(current);
    }
    if (parts.empty()) {
      return;
    }
    if (parts.size() == 1) {
      *leaf = parts[0];
      return;
    }
    scope->assign(parts.begin(), parts.end() - 1);
    *leaf = parts.back();
  }

  void WriteHeader(const std::string& module_name) {
    out_ << "$date\n  today\n$end\n";
    out_ << "$version\n  metalfpga\n$end\n";
    out_ << "$timescale " << timescale_ << " $end\n";
    out_ << "$scope module " << module_name << " $end\n";
    struct ScopedSignal {
      std::vector<std::string> scope;
      std::string leaf;
      const VcdSignal* signal = nullptr;
    };
    std::vector<ScopedSignal> ordered;
    ordered.reserve(signals_.size());
    for (const auto& sig : signals_) {
      ScopedSignal entry;
      entry.signal = &sig;
      SplitHierName(sig.name, &entry.scope, &entry.leaf);
      if (entry.leaf.empty()) {
        entry.leaf = sig.name;
      }
      ordered.push_back(std::move(entry));
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const ScopedSignal& a, const ScopedSignal& b) {
                if (a.scope != b.scope) {
                  return std::lexicographical_compare(
                      a.scope.begin(), a.scope.end(), b.scope.begin(),
                      b.scope.end());
                }
                return a.leaf < b.leaf;
              });
    std::vector<std::string> current;
    for (const auto& entry : ordered) {
      size_t common = 0;
      while (common < current.size() && common < entry.scope.size() &&
             current[common] == entry.scope[common]) {
        common++;
      }
      for (size_t i = current.size(); i > common; --i) {
        out_ << "$upscope $end\n";
      }
      for (size_t i = common; i < entry.scope.size(); ++i) {
        out_ << "$scope module " << entry.scope[i] << " $end\n";
      }
      current = entry.scope;
      const auto& sig = *entry.signal;
      if (sig.is_real) {
        out_ << "$var real 64 " << sig.id << " " << entry.leaf << " $end\n";
      } else {
        out_ << "$var wire " << sig.width << " " << sig.id << " "
             << entry.leaf << " $end\n";
      }
    }
    for (size_t i = current.size(); i > 0; --i) {
      out_ << "$upscope $end\n";
    }
    out_ << "$upscope $end\n";
    out_ << "$enddefinitions $end\n";
    CheckDumpLimit();
  }

  void EmitInitialValues(
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
    out_ << "#0\n";
    for (auto& sig : signals_) {
      if (sig.word_count <= 1u) {
        uint64_t val = 0;
        uint64_t xz = 0;
        if (!ReadSignal(sig, buffers, &val, &xz)) {
          continue;
        }
        sig.last_val = val;
        sig.last_xz = xz;
        sig.has_value = true;
        EmitValue(sig, val, xz);
        continue;
      }
      std::vector<uint64_t> val_words;
      std::vector<uint64_t> xz_words;
      if (!ReadSignalWords(sig, buffers, &val_words, &xz_words)) {
        continue;
      }
      sig.last_val_words = val_words;
      sig.last_xz_words = xz_words;
      sig.has_value = true;
      EmitValueWords(sig, val_words, xz_words);
    }
    last_time_ = 0;
    has_time_ = true;
    last_time_had_values_ = true;
    CheckDumpLimit();
  }

  bool ReadSignal(const VcdSignal& sig,
                  const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
                  uint64_t* val, uint64_t* xz) const {
    if (!val || !xz) {
      return false;
    }
    const gpga::MetalBuffer* val_buf =
        FindBuffer(buffers, MslSignalName(sig.base_name), "_val");
    const gpga::MetalBuffer* xz_buf = nullptr;
    const gpga::MetalBuffer* packed_buf = nullptr;
    const PackedSignalOffsets* packed = nullptr;
    if (four_state_) {
      xz_buf = FindBuffer(buffers, MslSignalName(sig.base_name), "_xz");
    }
    if (!val_buf && packed_layout_) {
      packed = packed_layout_->Find(sig.base_name);
      if (packed) {
        packed_buf = FindBuffer(buffers, "gpga_state", "");
      }
    }
    if (!val_buf && (!packed || !packed_buf || !packed->has_val)) {
      return false;
    }
    gpga::SignalInfo info;
    info.name = sig.base_name;
    info.width = sig.width;
    info.array_size = sig.array_size;
    info.is_real = sig.is_real;
    uint64_t val_word = 0;
    if (packed) {
      if (!ReadPackedSignalWordFromBuffer(
              info, sig.instance_index, sig.array_index, *packed_buf,
              packed->val_offset, 0u, &val_word)) {
        return false;
      }
    } else {
      if (!ReadSignalWordFromBuffer(info, sig.instance_index, sig.array_index,
                                    *val_buf, 0u, &val_word)) {
        return false;
      }
    }
    *val = val_word;
    if (four_state_) {
      if (packed && packed_buf && packed->has_xz) {
        return ReadPackedSignalWordFromBuffer(
            info, sig.instance_index, sig.array_index, *packed_buf,
            packed->xz_offset, 0u, xz);
      }
      if (xz_buf && xz_buf->contents() &&
          ReadSignalWordFromBuffer(info, sig.instance_index, sig.array_index,
                                   *xz_buf, 0u, xz)) {
        return true;
      }
      *xz = 0;
    } else {
      *xz = 0;
    }
    return true;
  }

  bool ReadSignalWords(
      const VcdSignal& sig,
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
      std::vector<uint64_t>* val_words,
      std::vector<uint64_t>* xz_words) const {
    if (!val_words || !xz_words) {
      return false;
    }
    const gpga::MetalBuffer* val_buf =
        FindBuffer(buffers, MslSignalName(sig.base_name), "_val");
    const gpga::MetalBuffer* xz_buf = nullptr;
    const gpga::MetalBuffer* packed_buf = nullptr;
    const PackedSignalOffsets* packed = nullptr;
    if (four_state_) {
      xz_buf = FindBuffer(buffers, MslSignalName(sig.base_name), "_xz");
    }
    if (!val_buf && packed_layout_) {
      packed = packed_layout_->Find(sig.base_name);
      if (packed) {
        packed_buf = FindBuffer(buffers, "gpga_state", "");
      }
    }
    if (!val_buf && (!packed || !packed_buf || !packed->has_val)) {
      return false;
    }
    gpga::SignalInfo info;
    info.name = sig.base_name;
    info.width = sig.width;
    info.array_size = sig.array_size;
    info.is_real = sig.is_real;
    val_words->assign(sig.word_count, 0ull);
    xz_words->assign(sig.word_count, 0ull);
    for (uint32_t word = 0; word < sig.word_count; ++word) {
      if (packed) {
        if (!ReadPackedSignalWordFromBuffer(
                info, sig.instance_index, sig.array_index, *packed_buf,
                packed->val_offset, word, &(*val_words)[word])) {
          return false;
        }
      } else {
        if (!ReadSignalWordFromBuffer(info, sig.instance_index, sig.array_index,
                                      *val_buf, word, &(*val_words)[word])) {
          return false;
        }
      }
      if (four_state_) {
        if (packed && packed_buf && packed->has_xz) {
          if (!ReadPackedSignalWordFromBuffer(
                  info, sig.instance_index, sig.array_index, *packed_buf,
                  packed->xz_offset, word, &(*xz_words)[word])) {
            return false;
          }
          continue;
        }
        if (xz_buf && xz_buf->contents()) {
          if (!ReadSignalWordFromBuffer(
                  info, sig.instance_index, sig.array_index, *xz_buf, word,
                  &(*xz_words)[word])) {
            return false;
          }
        }
      }
    }
    return true;
  }

  const gpga::MetalBuffer* FindBuffer(
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
      const std::string& base, const char* suffix) const {
    auto it = buffers.find(base + suffix);
    if (it != buffers.end()) {
      return &it->second;
    }
    it = buffers.find(base);
    if (it != buffers.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void EmitValue(const VcdSignal& sig, uint64_t val, uint64_t xz) {
    if (sig.is_real) {
      if (xz != 0) {
        out_ << "rnan " << sig.id << "\n";
        return;
      }
      double real_val = 0.0;
      std::memcpy(&real_val, &val, sizeof(real_val));
      out_ << "r" << real_val << " " << sig.id << "\n";
      return;
    }
    std::string bits;
    bits.reserve(sig.width);
    for (int i = static_cast<int>(sig.width) - 1; i >= 0; --i) {
      uint64_t mask = 1ull << static_cast<uint32_t>(i);
      bool bit_xz = (xz & mask) != 0ull;
      bool bit_val = (val & mask) != 0ull;
      if (bit_xz) {
        bits.push_back(bit_val ? 'x' : 'z');
      } else {
        bits.push_back(bit_val ? '1' : '0');
      }
    }
    if (sig.width == 1u) {
      out_ << bits[0] << sig.id << "\n";
    } else {
      out_ << "b" << bits << " " << sig.id << "\n";
    }
  }

  void EmitValueWords(const VcdSignal& sig,
                      const std::vector<uint64_t>& val_words,
                      const std::vector<uint64_t>& xz_words) {
    if (sig.is_real) {
      EmitValue(sig, val_words.empty() ? 0ull : val_words[0],
                xz_words.empty() ? 0ull : xz_words[0]);
      return;
    }
    std::string bits;
    bits.reserve(sig.width);
    for (int i = static_cast<int>(sig.width) - 1; i >= 0; --i) {
      uint32_t bit_index = static_cast<uint32_t>(i);
      uint32_t word_index = bit_index / 64u;
      uint32_t word_bit = bit_index % 64u;
      uint64_t mask = 1ull << word_bit;
      bool bit_xz = word_index < xz_words.size() &&
                    (xz_words[word_index] & mask) != 0ull;
      bool bit_val = word_index < val_words.size() &&
                     (val_words[word_index] & mask) != 0ull;
      if (bit_xz) {
        bits.push_back(bit_val ? 'x' : 'z');
      } else {
        bits.push_back(bit_val ? '1' : '0');
      }
    }
    if (sig.width == 1u) {
      out_ << bits[0] << sig.id << "\n";
    } else {
      out_ << "b" << bits << " " << sig.id << "\n";
    }
  }

  void EmitSnapshot(
      uint64_t time,
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
      bool force_values) {
    if (!active_) {
      return;
    }
    if (dump_limit_ != 0u && out_) {
      std::streampos pos = out_.tellp();
      if (pos >= 0 &&
          static_cast<uint64_t>(pos) >= dump_limit_) {
        dumping_ = false;
        return;
      }
    }
    if (!has_time_ || time != last_time_) {
      out_ << "#" << time << "\n";
      last_time_ = time;
      has_time_ = true;
      last_time_had_values_ = false;
    }
    for (auto& sig : signals_) {
      if (sig.word_count <= 1u) {
        uint64_t val = 0;
        uint64_t xz = 0;
        if (!ReadSignal(sig, buffers, &val, &xz)) {
          continue;
        }
        if (!force_values && sig.has_value && sig.last_val == val &&
            sig.last_xz == xz) {
          continue;
        }
        sig.last_val = val;
        sig.last_xz = xz;
        sig.has_value = true;
        EmitValue(sig, val, xz);
        last_time_had_values_ = true;
        continue;
      }
      std::vector<uint64_t> val_words;
      std::vector<uint64_t> xz_words;
      if (!ReadSignalWords(sig, buffers, &val_words, &xz_words)) {
        continue;
      }
      if (!force_values && sig.has_value &&
          sig.last_val_words == val_words &&
          sig.last_xz_words == xz_words) {
        continue;
      }
      sig.last_val_words = val_words;
      sig.last_xz_words = xz_words;
      sig.has_value = true;
      EmitValueWords(sig, val_words, xz_words);
      last_time_had_values_ = true;
    }
    CheckDumpLimit();
  }

  void CheckDumpLimit() {
    if (!dumping_ || dump_limit_ == 0u || !out_) {
      return;
    }
    std::streampos pos = out_.tellp();
    if (pos < 0) {
      return;
    }
    uint64_t written = static_cast<uint64_t>(pos);
    if (written >= dump_limit_) {
      dumping_ = false;
    }
  }

  bool active_ = false;
  bool four_state_ = false;
  bool has_time_ = false;
  bool last_time_had_values_ = false;
  bool dumping_ = true;
  uint64_t last_time_ = 0;
  uint64_t dump_limit_ = 0;
  std::string timescale_ = "1ns";
  const PackedStateLayout* packed_layout_ = nullptr;
  std::ofstream out_;
  std::vector<VcdSignal> signals_;
};

std::string StripComments(const std::string& line) {
  size_t pos = line.find("//");
  std::string out = (pos == std::string::npos) ? line : line.substr(0, pos);
  pos = out.find('#');
  if (pos != std::string::npos) {
    out = out.substr(0, pos);
  }
  return out;
}

std::string NormalizeToken(const std::string& token) {
  std::string out;
  out.reserve(token.size());
  for (char c : token) {
    if (c != '_') {
      out.push_back(c);
    }
  }
  return out;
}

bool ParseUnsigned(const std::string& token, int base, uint64_t* out) {
  if (!out) {
    return false;
  }
  try {
    size_t pos = 0;
    uint64_t value = std::stoull(token, &pos, base);
    if (pos != token.size()) {
      return false;
    }
    *out = value;
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseMemValue(const std::string& token_in, bool is_hex, uint32_t width,
                   uint64_t* val, uint64_t* xz) {
  if (!val || !xz) {
    return false;
  }
  std::string token = NormalizeToken(token_in);
  if (token.size() >= 2 && token[0] == '0' &&
      (token[1] == 'x' || token[1] == 'X')) {
    is_hex = true;
    token = token.substr(2);
  } else if (token.size() >= 2 && token[0] == '0' &&
             (token[1] == 'b' || token[1] == 'B')) {
    is_hex = false;
    token = token.substr(2);
  }
  uint64_t out_val = 0;
  uint64_t out_xz = 0;
  uint32_t max_bits = std::min<uint32_t>(width, 64u);
  if (is_hex) {
    int bit_pos = 0;
    for (int i = static_cast<int>(token.size()) - 1; i >= 0 && bit_pos < static_cast<int>(max_bits); --i) {
      char c = token[static_cast<size_t>(i)];
      uint8_t nibble = 0;
      bool is_x = false;
      bool is_z = false;
      if (c >= '0' && c <= '9') {
        nibble = static_cast<uint8_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        nibble = static_cast<uint8_t>(10 + (c - 'a'));
      } else if (c >= 'A' && c <= 'F') {
        nibble = static_cast<uint8_t>(10 + (c - 'A'));
      } else if (c == 'x' || c == 'X') {
        is_x = true;
      } else if (c == 'z' || c == 'Z') {
        is_z = true;
      } else {
        return false;
      }
      for (int b = 0; b < 4 && bit_pos < static_cast<int>(max_bits); ++b, ++bit_pos) {
        uint64_t mask = 1ull << static_cast<uint32_t>(bit_pos);
        if (is_x || is_z) {
          out_xz |= mask;
          if (is_x) {
            out_val |= mask;
          }
        } else if (nibble & (1u << b)) {
          out_val |= mask;
        }
      }
    }
  } else {
    int bit_pos = 0;
    for (int i = static_cast<int>(token.size()) - 1; i >= 0 && bit_pos < static_cast<int>(max_bits); --i, ++bit_pos) {
      char c = token[static_cast<size_t>(i)];
      uint64_t mask = 1ull << static_cast<uint32_t>(bit_pos);
      if (c == '1') {
        out_val |= mask;
      } else if (c == '0') {
        continue;
      } else if (c == 'x' || c == 'X') {
        out_xz |= mask;
        out_val |= mask;
      } else if (c == 'z' || c == 'Z') {
        out_xz |= mask;
      } else {
        return false;
      }
    }
  }
  *val = out_val;
  *xz = out_xz;
  return true;
}

bool ParseMemValueWords(const std::string& token_in, bool is_hex,
                        uint32_t width, std::vector<uint64_t>* val_words,
                        std::vector<uint64_t>* xz_words) {
  if (!val_words || !xz_words) {
    return false;
  }
  if (width == 0u) {
    width = 1u;
  }
  const uint32_t word_count = (width + 63u) / 64u;
  val_words->assign(word_count, 0ull);
  xz_words->assign(word_count, 0ull);
  std::string token = NormalizeToken(token_in);
  if (token.size() >= 2 && token[0] == '0' &&
      (token[1] == 'x' || token[1] == 'X')) {
    is_hex = true;
    token = token.substr(2);
  } else if (token.size() >= 2 && token[0] == '0' &&
             (token[1] == 'b' || token[1] == 'B')) {
    is_hex = false;
    token = token.substr(2);
  }
  auto set_bit = [&](std::vector<uint64_t>* words, uint32_t bit) {
    size_t word_index = bit / 64u;
    if (word_index >= words->size()) {
      return;
    }
    uint32_t word_bit = bit % 64u;
    (*words)[word_index] |= (1ull << word_bit);
  };
  uint32_t bit_pos = 0;
  if (is_hex) {
    for (int i = static_cast<int>(token.size()) - 1;
         i >= 0 && bit_pos < width; --i) {
      char c = token[static_cast<size_t>(i)];
      uint8_t nibble = 0;
      bool is_x = false;
      bool is_z = false;
      if (c >= '0' && c <= '9') {
        nibble = static_cast<uint8_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        nibble = static_cast<uint8_t>(10 + (c - 'a'));
      } else if (c >= 'A' && c <= 'F') {
        nibble = static_cast<uint8_t>(10 + (c - 'A'));
      } else if (c == 'x' || c == 'X') {
        is_x = true;
      } else if (c == 'z' || c == 'Z') {
        is_z = true;
      } else {
        return false;
      }
      for (int b = 0; b < 4 && bit_pos < width; ++b, ++bit_pos) {
        if (is_x || is_z) {
          set_bit(xz_words, bit_pos);
          if (is_x) {
            set_bit(val_words, bit_pos);
          }
        } else if (nibble & (1u << b)) {
          set_bit(val_words, bit_pos);
        }
      }
    }
  } else {
    for (int i = static_cast<int>(token.size()) - 1;
         i >= 0 && bit_pos < width; --i, ++bit_pos) {
      char c = token[static_cast<size_t>(i)];
      if (c == '1') {
        set_bit(val_words, bit_pos);
      } else if (c == '0') {
        continue;
      } else if (c == 'x' || c == 'X') {
        set_bit(xz_words, bit_pos);
        set_bit(val_words, bit_pos);
      } else if (c == 'z' || c == 'Z') {
        set_bit(xz_words, bit_pos);
      } else {
        return false;
      }
    }
  }
  return true;
}

bool ApplyReadmem(const std::string& filename, bool is_hex,
                  const gpga::SignalInfo& signal, bool four_state,
                  std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
                  uint32_t instance_count, uint64_t start, uint64_t end,
                  std::string* error) {
  if (!buffers) {
    return false;
  }
  std::ifstream in(filename);
  if (!in) {
    if (error) {
      *error = "failed to open readmem file: " + filename;
    }
    return false;
  }
  const std::string base = MslSignalName(signal.name);
  auto it_val = buffers->find(base + "_val");
  if (it_val == buffers->end()) {
    it_val = buffers->find(base);
  }
  if (it_val == buffers->end()) {
    if (error) {
      *error = "readmem target buffer not found: " + signal.name;
    }
    return false;
  }
  uint32_t width = signal.is_real ? 64u : signal.width;
  if (width == 0u) {
    width = 1u;
  }
  uint32_t array_size = signal.array_size > 0 ? signal.array_size : 1u;
  if (end == std::numeric_limits<uint64_t>::max()) {
    end = (array_size > 0) ? static_cast<uint64_t>(array_size - 1u) : 0u;
  }
  if (start > end) {
    std::swap(start, end);
  }
  uint64_t address = start;
  std::string line;
  while (std::getline(in, line)) {
    line = StripComments(line);
    std::stringstream ss(line);
    std::string token;
    while (ss >> token) {
      token = NormalizeToken(token);
      if (token.empty()) {
        continue;
      }
      if (token[0] == '@') {
        uint64_t addr = 0;
        std::string addr_token = token.substr(1);
        if (!ParseUnsigned(addr_token, is_hex ? 16 : 2, &addr)) {
          if (error) {
            *error = "invalid readmem address: " + token;
          }
          return false;
        }
        address = addr;
        continue;
      }
      if (address > end) {
        return true;
      }
      if (address >= array_size) {
        address++;
        continue;
      }
      if (width <= 64u) {
        uint64_t val = 0;
        uint64_t xz = 0;
        if (!ParseMemValue(token, is_hex, width, &val, &xz)) {
          if (error) {
            *error = "invalid readmem value: " + token;
          }
          return false;
        }
        for (uint32_t gid = 0; gid < instance_count; ++gid) {
          WriteSignalValueAtIndex(signal, gid, address, val, xz, four_state,
                                  buffers);
        }
      } else {
        std::vector<uint64_t> val_words;
        std::vector<uint64_t> xz_words;
        if (!ParseMemValueWords(token, is_hex, width, &val_words,
                                &xz_words)) {
          if (error) {
            *error = "invalid readmem value: " + token;
          }
          return false;
        }
        for (uint32_t gid = 0; gid < instance_count; ++gid) {
          size_t word_count = SignalWordCount(signal);
          for (size_t word = 0; word < word_count; ++word) {
            uint64_t val = (word < val_words.size()) ? val_words[word] : 0ull;
            uint64_t xz = (word < xz_words.size()) ? xz_words[word] : 0ull;
            WriteSignalWord(signal, gid, address, word, val, xz, four_state,
                            buffers);
          }
        }
      }
      address++;
    }
  }
  return true;
}

std::string FormatMemWords(const std::vector<uint64_t>& val_words,
                           const std::vector<uint64_t>& xz_words,
                           uint32_t width, bool is_hex, bool four_state) {
  if (width == 0u) {
    width = 1u;
  }
  auto get_bit = [&](const std::vector<uint64_t>& words,
                     uint32_t bit) -> bool {
    size_t word_index = bit / 64u;
    if (word_index >= words.size()) {
      return false;
    }
    uint32_t word_bit = bit % 64u;
    return (words[word_index] & (1ull << word_bit)) != 0ull;
  };
  if (!is_hex) {
    std::string out;
    out.reserve(width);
    for (int bit = static_cast<int>(width) - 1; bit >= 0; --bit) {
      uint32_t bit_index = static_cast<uint32_t>(bit);
      bool bit_xz = four_state && get_bit(xz_words, bit_index);
      bool bit_val = get_bit(val_words, bit_index);
      if (!bit_xz) {
        out.push_back(bit_val ? '1' : '0');
      } else {
        out.push_back(bit_val ? 'x' : 'z');
      }
    }
    return out;
  }
  uint32_t digits = (width + 3u) / 4u;
  std::string out;
  out.reserve(digits);
  for (uint32_t d = 0; d < digits; ++d) {
    int start_bit = static_cast<int>(width) - 1 - static_cast<int>(d * 4u);
    uint32_t nibble_val = 0u;
    uint32_t nibble_xz = 0u;
    uint32_t nibble_mask = 0u;
    for (int b = 0; b < 4; ++b) {
      int bit_index = start_bit - b;
      if (bit_index < 0) {
        continue;
      }
      uint32_t nibble_bit = 1u << (3 - b);
      nibble_mask |= nibble_bit;
      uint32_t bit_u = static_cast<uint32_t>(bit_index);
      if (get_bit(val_words, bit_u)) {
        nibble_val |= nibble_bit;
      }
      if (four_state && get_bit(xz_words, bit_u)) {
        nibble_xz |= nibble_bit;
      }
    }
    if (four_state && nibble_xz != 0u) {
      if (nibble_xz == nibble_mask && nibble_val == 0u) {
        out.push_back('z');
      } else {
        out.push_back('x');
      }
    } else {
      static const char kDigits[] = "0123456789abcdef";
      out.push_back(kDigits[nibble_val & 0xFu]);
    }
  }
  return out;
}

std::string FormatMemWord(uint64_t val, uint64_t xz, uint32_t width,
                          bool is_hex, bool four_state) {
  if (width == 0u) {
    width = 1u;
  }
  if (width > 64u) {
    width = 64u;
  }
  if (!is_hex) {
    std::string out;
    out.reserve(width);
    for (int bit = static_cast<int>(width) - 1; bit >= 0; --bit) {
      uint64_t mask = 1ull << static_cast<uint32_t>(bit);
      bool has_xz = four_state && ((xz & mask) != 0u);
      if (!has_xz) {
        out.push_back((val & mask) ? '1' : '0');
      } else {
        out.push_back((val & mask) ? 'x' : 'z');
      }
    }
    return out;
  }
  uint32_t digits = (width + 3u) / 4u;
  std::string out;
  out.reserve(digits);
  for (uint32_t d = 0; d < digits; ++d) {
    int start_bit = static_cast<int>(width) - 1 - static_cast<int>(d * 4u);
    uint32_t nibble_val = 0u;
    uint32_t nibble_xz = 0u;
    uint32_t nibble_mask = 0u;
    for (int b = 0; b < 4; ++b) {
      int bit_index = start_bit - b;
      if (bit_index < 0) {
        continue;
      }
      uint32_t nibble_bit = 1u << (3 - b);
      nibble_mask |= nibble_bit;
      uint64_t mask = 1ull << static_cast<uint32_t>(bit_index);
      if (val & mask) {
        nibble_val |= nibble_bit;
      }
      if (xz & mask) {
        nibble_xz |= nibble_bit;
      }
    }
    if (four_state && nibble_xz != 0u) {
      if (nibble_xz == nibble_mask && nibble_val == 0u) {
        out.push_back('z');
      } else {
        out.push_back('x');
      }
    } else {
      static const char kDigits[] = "0123456789abcdef";
      out.push_back(kDigits[nibble_val & 0xFu]);
    }
  }
  return out;
}

bool ApplyWritemem(const std::string& filename, bool is_hex,
                   const gpga::SignalInfo& signal, bool four_state,
                   std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
                   uint64_t start, uint64_t end, std::string* error) {
  if (!buffers) {
    return false;
  }
  std::ofstream out(filename, std::ios::out | std::ios::trunc);
  if (!out) {
    if (error) {
      *error = "failed to open writemem file: " + filename;
    }
    return false;
  }
  const std::string base = MslSignalName(signal.name);
  auto it_val = buffers->find(base + "_val");
  if (it_val == buffers->end()) {
    it_val = buffers->find(base);
  }
  if (it_val == buffers->end()) {
    if (error) {
      *error = "writemem target buffer not found: " + signal.name;
    }
    return false;
  }
  gpga::MetalBuffer* val_buf = &it_val->second;
  gpga::MetalBuffer* xz_buf = nullptr;
  if (four_state) {
    auto it_xz = buffers->find(base + "_xz");
    if (it_xz != buffers->end()) {
      xz_buf = &it_xz->second;
    }
  }
  uint32_t width = signal.is_real ? 64u : signal.width;
  if (width == 0u) {
    width = 1u;
  }
  uint32_t array_size = signal.array_size > 0 ? signal.array_size : 1u;
  if (end == std::numeric_limits<uint64_t>::max()) {
    end = (array_size > 0) ? static_cast<uint64_t>(array_size - 1u) : 0u;
  }
  if (start > end) {
    std::swap(start, end);
  }
  if (array_size == 0 || start >= array_size) {
    return true;
  }
  uint64_t limit_end = std::min<uint64_t>(end, array_size - 1u);
  for (uint64_t addr = start; addr <= limit_end; ++addr) {
    if (width <= 64u) {
      uint64_t val = 0;
      uint64_t xz = 0;
      if (!ReadSignalWordFromBuffer(signal, 0u, addr, *val_buf, 0u, &val)) {
        break;
      }
      if (four_state && xz_buf) {
        ReadSignalWordFromBuffer(signal, 0u, addr, *xz_buf, 0u, &xz);
      }
      out << FormatMemWord(val, xz, width, is_hex, four_state) << "\n";
    } else {
      const size_t word_count = SignalWordCount(signal);
      std::vector<uint64_t> val_words(word_count, 0ull);
      std::vector<uint64_t> xz_words(word_count, 0ull);
      bool ok = true;
      for (size_t word = 0; word < word_count; ++word) {
        if (!ReadSignalWordFromBuffer(signal, 0u, addr, *val_buf, word,
                                      &val_words[word])) {
          ok = false;
          break;
        }
        if (four_state && xz_buf) {
          ReadSignalWordFromBuffer(signal, 0u, addr, *xz_buf, word,
                                   &xz_words[word]);
        }
      }
      if (!ok) {
        break;
      }
      out << FormatMemWords(val_words, xz_words, width, is_hex, four_state)
          << "\n";
    }
    if (!out) {
      if (error) {
        *error = "failed to write writemem file: " + filename;
      }
      return false;
    }
  }
  return true;
}

bool HandleServiceRecords(
    const std::vector<DecodedServiceRecord>& records,
    const gpga::ServiceStringTable& strings, const gpga::ModuleInfo& module,
    const std::string& vcd_dir,
    const std::unordered_map<std::string, std::string>* flat_to_hier,
    const std::string& timescale,
    bool four_state, uint32_t instance_count, uint32_t gid,
    uint32_t proc_count, FileTable* files,
    const std::vector<std::string>& plusargs,
    std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
    VcdWriter* vcd, gpga::ServiceDrainResult* result,
    std::string* dumpfile, std::string* error) {
  if (!buffers || !vcd || !dumpfile || !files) {
    return false;
  }
  if (result) {
    result->saw_finish = false;
    result->saw_stop = false;
    result->saw_error = false;
  }
  g_timescale_exp = ParseTimescaleExponent(timescale);
  auto current_time = [&]() -> uint64_t {
    auto time_it = buffers->find("sched_time");
    if (time_it != buffers->end() && time_it->second.contents()) {
      uint64_t time = 0;
      std::memcpy(&time, time_it->second.contents(), sizeof(time));
      return time;
    }
    return 0;
  };
  constexpr uint32_t kWaitNone = 0u;
  constexpr uint32_t kWaitService = 7u;
  constexpr uint32_t kProcReady = 0u;
  gpga::MetalBuffer* wait_kind_buf =
      FindBufferMutable(buffers, "sched_wait_kind", "");
  gpga::MetalBuffer* wait_time_buf =
      FindBufferMutable(buffers, "sched_wait_time", "");
  gpga::MetalBuffer* state_buf =
      FindBufferMutable(buffers, "sched_state", "");
  auto* wait_kind_ptr =
      wait_kind_buf ? static_cast<uint32_t*>(wait_kind_buf->contents()) : nullptr;
  auto* wait_time_ptr =
      wait_time_buf ? static_cast<uint64_t*>(wait_time_buf->contents()) : nullptr;
  auto* state_ptr =
      state_buf ? static_cast<uint32_t*>(state_buf->contents()) : nullptr;
  auto resume_service = [&](uint32_t pid, uint64_t value) {
    if (!wait_kind_ptr || !wait_time_ptr || !state_ptr) {
      return;
    }
    size_t idx = static_cast<size_t>(gid) * proc_count + pid;
    if (wait_kind_buf &&
        wait_kind_buf->length() < (idx + 1u) * sizeof(uint32_t)) {
      return;
    }
    if (wait_time_buf &&
        wait_time_buf->length() < (idx + 1u) * sizeof(uint64_t)) {
      return;
    }
    if (state_buf &&
        state_buf->length() < (idx + 1u) * sizeof(uint32_t)) {
      return;
    }
    if (wait_kind_ptr[idx] != kWaitService) {
      return;
    }
    wait_time_ptr[idx] = value;
    wait_kind_ptr[idx] = kWaitNone;
    state_ptr[idx] = kProcReady;
  };
  auto resolve_string_or_ident = [&](const gpga::ServiceArgView& arg) -> std::string {
    if (arg.kind == gpga::ServiceArgKind::kString ||
        arg.kind == gpga::ServiceArgKind::kIdent) {
      std::string value =
          ResolveString(strings, static_cast<uint32_t>(arg.value));
      if (arg.kind == gpga::ServiceArgKind::kIdent) {
        const gpga::SignalInfo* sig = FindSignalInfo(module, value);
        if (sig) {
          return ReadSignalString(*sig, gid, *buffers);
        }
      }
      return value;
    }
    return {};
  };
  auto write_output_arg =
      [&](const gpga::ServiceArgView& arg, uint64_t value,
          const std::string& str_value,
          const std::vector<uint64_t>* wide_words) -> bool {
    if (arg.kind != gpga::ServiceArgKind::kIdent) {
      return false;
    }
    std::string name = ResolveString(strings, static_cast<uint32_t>(arg.value));
    const gpga::SignalInfo* sig = FindSignalInfo(module, name);
    if (!sig) {
      return false;
    }
    if (!str_value.empty()) {
      size_t max_bytes = (SignalBitWidth(*sig) + 7u) / 8u;
      std::vector<uint64_t> words = PackStringWords(str_value, max_bytes);
      for (size_t i = 0; i < words.size(); ++i) {
        if (!WriteSignalWord(*sig, gid, 0u, i, words[i], 0ull, four_state,
                             buffers)) {
          return false;
        }
      }
      return true;
    }
    uint32_t width = arg.width > 0 ? arg.width : sig->width;
    if (width > 64u && wide_words && !wide_words->empty()) {
      size_t word_count = SignalWordCount(*sig);
      for (size_t i = 0; i < word_count; ++i) {
        uint64_t word = (i < wide_words->size()) ? (*wide_words)[i] : 0ull;
        if (!WriteSignalWord(*sig, gid, 0u, i, word, 0ull, four_state,
                             buffers)) {
          return false;
        }
      }
      return true;
    }
    uint64_t masked = value & MaskForWidth(width);
    return WriteSignalValue(*sig, gid, masked, 0ull, four_state, buffers);
  };
  std::unordered_map<uint32_t, size_t> last_record_for_pid;
  last_record_for_pid.reserve(records.size());
  for (size_t i = 0; i < records.size(); ++i) {
    last_record_for_pid[records[i].pid] = i;
  }
  auto should_resume = [&](const DecodedServiceRecord& rec,
                           size_t record_index) -> bool {
    auto it = last_record_for_pid.find(rec.pid);
    return it != last_record_for_pid.end() && it->second == record_index;
  };
  auto resume_if_waiting = [&](const DecodedServiceRecord& rec,
                               size_t record_index, uint64_t value) -> void {
    if (!should_resume(rec, record_index)) {
      return;
    }
    resume_service(rec.pid, value);
  };
  for (size_t record_index = 0; record_index < records.size();
       ++record_index) {
    const auto& rec = records[record_index];
    switch (rec.kind) {
      case gpga::ServiceKind::kDumpfile: {
        *dumpfile = ResolveString(strings, rec.format_id);
        std::cout << "$dumpfile \"" << *dumpfile << "\" (pid=" << rec.pid
                  << ")\n";
        break;
      }
      case gpga::ServiceKind::kDumpvars: {
        std::vector<std::string> targets;
        size_t start = 0;
        uint32_t depth = 0;
        if (!rec.args.empty() &&
            rec.args[0].kind == gpga::ServiceArgKind::kValue) {
          depth = static_cast<uint32_t>(rec.args[0].value);
          start = 1;
        }
        for (size_t i = start; i < rec.args.size(); ++i) {
          const auto& arg = rec.args[i];
          if (arg.kind == gpga::ServiceArgKind::kString ||
              arg.kind == gpga::ServiceArgKind::kIdent) {
            targets.push_back(ResolveString(strings,
                                            static_cast<uint32_t>(arg.value)));
          }
        }
        if (!vcd->Start(*dumpfile, vcd_dir, module, targets, depth, four_state,
                        *buffers, timescale, instance_count, flat_to_hier,
                        error)) {
          return false;
        }
        std::cout << "$dumpvars (pid=" << rec.pid << ")";
        for (const auto& arg : rec.args) {
          std::cout << " ";
          if (arg.kind == gpga::ServiceArgKind::kString ||
              arg.kind == gpga::ServiceArgKind::kIdent) {
            std::cout
                << ResolveString(strings, static_cast<uint32_t>(arg.value));
          } else {
            std::cout << FormatNumeric(arg, 'h', four_state);
          }
        }
        std::cout << "\n";
        break;
      }
      case gpga::ServiceKind::kShowcancelled: {
        std::cout << "$showcancelled (pid=" << rec.pid << ")";
        if (!rec.args.empty()) {
          std::cout << " delay_id=" << FormatNumeric(rec.args[0], 'h',
                                                     four_state);
        }
        if (rec.args.size() > 1) {
          std::cout << " index=" << FormatNumeric(rec.args[1], 'h', four_state);
        }
        if (rec.args.size() > 2) {
          std::cout << " index_xz="
                    << FormatNumeric(rec.args[2], 'h', four_state);
        }
        if (rec.args.size() > 3) {
          std::cout << " time=" << FormatNumeric(rec.args[3], 'd', four_state);
        }
        std::cout << "\n";
        break;
      }
      case gpga::ServiceKind::kFinish: {
        if (result) {
          result->saw_finish = true;
        }
        std::cout << "$finish (pid=" << rec.pid << ")\n";
        break;
      }
      case gpga::ServiceKind::kStop: {
        if (result) {
          result->saw_stop = true;
        }
        std::cout << "$stop (pid=" << rec.pid << ")\n";
        break;
      }
      case gpga::ServiceKind::kDisplay:
      case gpga::ServiceKind::kWrite:
      case gpga::ServiceKind::kMonitor:
      case gpga::ServiceKind::kStrobe: {
        std::string fmt = (rec.format_id != 0xFFFFFFFFu)
                              ? ResolveString(strings, rec.format_id)
                              : "";
        size_t start_index = 0;
        if (!fmt.empty() && !rec.args.empty() &&
            rec.args.front().kind == gpga::ServiceArgKind::kString &&
            rec.args.front().value ==
                static_cast<uint64_t>(rec.format_id)) {
          start_index = 1;
        }
        std::string line =
            fmt.empty() ? FormatDefaultArgs(rec.args, strings, four_state)
                        : FormatWithSpec(fmt, rec.args, start_index, strings,
                                         four_state, &module, buffers, gid);
        std::cout << line;
        if (rec.kind != gpga::ServiceKind::kWrite) {
          std::cout << "\n";
        }
        break;
      }
      case gpga::ServiceKind::kSformat: {
        if (rec.args.empty() ||
            rec.args.front().kind != gpga::ServiceArgKind::kIdent) {
          break;
        }
        std::vector<gpga::ServiceArgView> fmt_args;
        if (rec.args.size() > 1) {
          fmt_args.assign(rec.args.begin() + 1, rec.args.end());
        }
        std::string fmt = (rec.format_id != 0xFFFFFFFFu)
                              ? ResolveString(strings, rec.format_id)
                              : "";
        size_t start_index = 0;
        if (!fmt.empty() && !fmt_args.empty() &&
            fmt_args.front().kind == gpga::ServiceArgKind::kString &&
            fmt_args.front().value ==
                static_cast<uint64_t>(rec.format_id)) {
          start_index = 1;
        }
        std::string line =
            fmt.empty() ? FormatDefaultArgs(fmt_args, strings, four_state)
                        : FormatWithSpec(fmt, fmt_args, start_index, strings,
                                         four_state, &module, buffers, gid);
        write_output_arg(rec.args.front(), 0, line, nullptr);
        break;
      }
      case gpga::ServiceKind::kTimeformat: {
        if (rec.args.size() < 4) {
          break;
        }
        int units = 0;
        int precision = 0;
        int min_width = 0;
        std::string suffix;
        if (rec.args[0].kind == gpga::ServiceArgKind::kValue) {
          units =
              static_cast<int>(SignExtend(rec.args[0].value, rec.args[0].width));
        }
        if (rec.args[1].kind == gpga::ServiceArgKind::kValue) {
          precision =
              static_cast<int>(SignExtend(rec.args[1].value, rec.args[1].width));
        }
        suffix = resolve_string_or_ident(rec.args[2]);
        if (rec.args[3].kind == gpga::ServiceArgKind::kValue) {
          min_width =
              static_cast<int>(SignExtend(rec.args[3].value, rec.args[3].width));
        }
        if (precision < 0) {
          precision = 0;
        }
        if (min_width < 0) {
          min_width = 0;
        }
        g_time_format.active = true;
        g_time_format.units = units;
        g_time_format.precision = precision;
        g_time_format.suffix = std::move(suffix);
        g_time_format.min_width = min_width;
        break;
      }
      case gpga::ServiceKind::kPrinttimescale: {
        std::string target = module.name;
        if (!rec.args.empty() &&
            (rec.args.front().kind == gpga::ServiceArgKind::kString ||
             rec.args.front().kind == gpga::ServiceArgKind::kIdent)) {
          target = ResolveString(strings,
                                 static_cast<uint32_t>(rec.args.front().value));
        }
        std::string display_timescale = TrimString(timescale);
        if (display_timescale.empty()) {
          display_timescale = "1ns";
        }
        std::cout << "Time scale of " << target << " is " << display_timescale
                  << "\n";
        break;
      }
      case gpga::ServiceKind::kTestPlusargs: {
        std::string pattern =
            (rec.format_id != 0xFFFFFFFFu)
                ? ResolveString(strings, rec.format_id)
                : std::string();
        if (pattern.empty() && !rec.args.empty()) {
          pattern = resolve_string_or_ident(rec.args.front());
        }
        bool found = (!pattern.empty() &&
                      FindPlusargMatch(plusargs, pattern, nullptr));
        resume_if_waiting(rec, record_index, found ? 1u : 0u);
        break;
      }
      case gpga::ServiceKind::kValuePlusargs: {
        std::string format =
            (rec.format_id != 0xFFFFFFFFu)
                ? ResolveString(strings, rec.format_id)
                : std::string();
        if (format.empty() && !rec.args.empty()) {
          format = resolve_string_or_ident(rec.args.front());
        }
        if (format.empty()) {
          resume_if_waiting(rec, record_index, 0u);
          break;
        }
        std::string prefix = PlusargPrefix(format);
        std::string payload;
        if (!FindPlusargMatch(plusargs, prefix, &payload)) {
          resume_if_waiting(rec, record_index, 0u);
          break;
        }
        std::vector<FormatSpec> specs = ParseFormatSpecs(format);
        if (specs.empty()) {
          resume_if_waiting(rec, record_index, 0u);
          break;
        }
        size_t arg_index = 0;
        size_t pos = 0;
        int assigned = 0;
        for (const auto& spec : specs) {
          std::string token;
          if (!ReadTokenFromString(payload, &pos, &token)) {
            break;
          }
          if (spec.suppress) {
            continue;
          }
          while (arg_index < rec.args.size() &&
                 rec.args[arg_index].kind != gpga::ServiceArgKind::kIdent) {
            ++arg_index;
          }
          if (arg_index >= rec.args.size()) {
            break;
          }
          uint32_t width = rec.args[arg_index].width;
          if (width == 0u) {
            width = 64u;
          }
          std::vector<uint64_t> words;
          std::string str_value;
          if (!ParseTokenWords(token, spec.spec, width, &words, &str_value)) {
            break;
          }
          uint64_t value = words.empty() ? 0ull : words[0];
          const std::vector<uint64_t>* wide_words =
              (width > 64u) ? &words : nullptr;
          write_output_arg(rec.args[arg_index], value, str_value, wide_words);
          ++assigned;
          ++arg_index;
        }
        resume_if_waiting(rec, record_index, assigned > 0 ? 1u : 0u);
        break;
      }
      case gpga::ServiceKind::kFdisplay:
      case gpga::ServiceKind::kFwrite: {
        if (rec.args.empty() ||
            rec.args.front().kind != gpga::ServiceArgKind::kValue) {
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
        auto file_it = files->handles.find(fd);
        if (file_it == files->handles.end() || !file_it->second.file) {
          break;
        }
        std::vector<gpga::ServiceArgView> fmt_args;
        if (rec.args.size() > 1) {
          fmt_args.assign(rec.args.begin() + 1, rec.args.end());
        }
        std::string fmt = (rec.format_id != 0xFFFFFFFFu)
                              ? ResolveString(strings, rec.format_id)
                              : "";
        size_t start_index = 0;
        if (!fmt.empty() && !fmt_args.empty() &&
            fmt_args.front().kind == gpga::ServiceArgKind::kString &&
            fmt_args.front().value ==
                static_cast<uint64_t>(rec.format_id)) {
          start_index = 1;
        }
        std::string line =
            fmt.empty() ? FormatDefaultArgs(fmt_args, strings, four_state)
                        : FormatWithSpec(fmt, fmt_args, start_index, strings,
                                         four_state, &module, buffers, gid);
        std::fwrite(line.data(), 1, line.size(), file_it->second.file);
        if (rec.kind == gpga::ServiceKind::kFdisplay) {
          std::fputc('\n', file_it->second.file);
        }
        std::fflush(file_it->second.file);
        break;
      }
      case gpga::ServiceKind::kFopen: {
        std::string path;
        std::string mode = "r";
        if (!rec.args.empty()) {
          path = resolve_string_or_ident(rec.args[0]);
        }
        if (rec.args.size() > 1) {
          std::string arg_mode = resolve_string_or_ident(rec.args[1]);
          if (!arg_mode.empty()) {
            mode = arg_mode;
          }
        }
        uint64_t handle = 0;
        if (!path.empty()) {
          std::FILE* file = std::fopen(path.c_str(), mode.c_str());
          if (file) {
            uint32_t id = files->next_handle++;
            files->handles[id] = FileHandleEntry{file, path};
            handle = id;
          }
        }
        resume_if_waiting(rec, record_index, handle);
        break;
      }
      case gpga::ServiceKind::kFclose: {
        if (rec.args.empty() ||
            rec.args.front().kind != gpga::ServiceArgKind::kValue) {
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
        auto file_it = files->handles.find(fd);
        if (file_it != files->handles.end()) {
          if (file_it->second.file) {
            std::fclose(file_it->second.file);
          }
          files->handles.erase(file_it);
        }
        resume_if_waiting(rec, record_index, 0);
        break;
      }
      case gpga::ServiceKind::kFflush: {
        if (rec.args.empty()) {
          std::fflush(nullptr);
          break;
        }
        if (rec.args.front().kind != gpga::ServiceArgKind::kValue) {
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
        auto file_it = files->handles.find(fd);
        if (file_it != files->handles.end() && file_it->second.file) {
          std::fflush(file_it->second.file);
        }
        break;
      }
      case gpga::ServiceKind::kRewind: {
        if (rec.args.empty() ||
            rec.args.front().kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
        auto file_it = files->handles.find(fd);
        if (file_it != files->handles.end() && file_it->second.file) {
          std::rewind(file_it->second.file);
        }
        resume_if_waiting(rec, record_index, 0);
        break;
      }
      case gpga::ServiceKind::kFgetc: {
        if (rec.args.empty() ||
            rec.args.front().kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 0xFFFFFFFFull);
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
        auto file_it = files->handles.find(fd);
        uint64_t value = 0xFFFFFFFFull;
        if (file_it != files->handles.end() && file_it->second.file) {
          int ch = std::fgetc(file_it->second.file);
          if (ch != EOF) {
            value = static_cast<uint32_t>(static_cast<unsigned char>(ch));
          }
        }
        resume_if_waiting(rec, record_index, value);
        break;
      }
      case gpga::ServiceKind::kFgets: {
        if (rec.args.size() < 2 ||
            rec.args[0].kind != gpga::ServiceArgKind::kIdent ||
            rec.args[1].kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        std::string target =
            ResolveString(strings, static_cast<uint32_t>(rec.args[0].value));
        const gpga::SignalInfo* sig = FindSignalInfo(module, target);
        uint32_t fd = static_cast<uint32_t>(rec.args[1].value);
        auto file_it = files->handles.find(fd);
        if (!sig || file_it == files->handles.end() || !file_it->second.file) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        size_t max_bytes = (sig->width + 7u) / 8u;
        if (max_bytes == 0) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        std::vector<char> buffer(max_bytes + 1, '\0');
        char* result = std::fgets(buffer.data(),
                                  static_cast<int>(buffer.size()),
                                  file_it->second.file);
        if (!result) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        std::string line(buffer.data());
        write_output_arg(rec.args[0], 0, line, nullptr);
        resume_if_waiting(rec, record_index, static_cast<uint64_t>(line.size()));
        break;
      }
      case gpga::ServiceKind::kFeof: {
        if (rec.args.empty() ||
            rec.args.front().kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 1);
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
        auto file_it = files->handles.find(fd);
        uint64_t value = 1;
        if (file_it != files->handles.end() && file_it->second.file) {
          value = std::feof(file_it->second.file) ? 1u : 0u;
        }
        resume_if_waiting(rec, record_index, value);
        break;
      }
      case gpga::ServiceKind::kFerror: {
        if (rec.args.empty() ||
            rec.args.front().kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 1);
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
        auto file_it = files->handles.find(fd);
        uint64_t value = 1;
        if (file_it != files->handles.end() && file_it->second.file) {
          value = std::ferror(file_it->second.file) ? 1u : 0u;
        }
        resume_if_waiting(rec, record_index, value);
        break;
      }
      case gpga::ServiceKind::kFseek: {
        if (rec.args.size() < 3 ||
            rec.args[0].kind != gpga::ServiceArgKind::kValue ||
            rec.args[1].kind != gpga::ServiceArgKind::kValue ||
            rec.args[2].kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 1);
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args[0].value);
        int64_t offset = SignExtend(rec.args[1].value, rec.args[1].width);
        int origin = static_cast<int>(rec.args[2].value);
        auto file_it = files->handles.find(fd);
        uint64_t value = 1;
        if (file_it != files->handles.end() && file_it->second.file) {
          int rc = std::fseek(file_it->second.file, static_cast<long>(offset),
                              origin);
          value = (rc == 0) ? 0u : 1u;
        }
        resume_if_waiting(rec, record_index, value);
        break;
      }
      case gpga::ServiceKind::kFungetc: {
        if (rec.args.size() < 2 ||
            rec.args[0].kind != gpga::ServiceArgKind::kValue ||
            rec.args[1].kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 0xFFFFFFFFull);
          break;
        }
        uint64_t ch = rec.args[0].value & 0xFFu;
        uint32_t fd = static_cast<uint32_t>(rec.args[1].value);
        auto file_it = files->handles.find(fd);
        uint64_t value = 0xFFFFFFFFull;
        if (file_it != files->handles.end() && file_it->second.file) {
          int rc = std::ungetc(static_cast<unsigned char>(ch),
                               file_it->second.file);
          if (rc != EOF) {
            value = static_cast<uint32_t>(static_cast<unsigned char>(rc));
          }
        }
        resume_if_waiting(rec, record_index, value);
        break;
      }
      case gpga::ServiceKind::kFread: {
        if (rec.args.size() < 2 ||
            rec.args[0].kind != gpga::ServiceArgKind::kIdent ||
            rec.args[1].kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        std::string target =
            ResolveString(strings, static_cast<uint32_t>(rec.args[0].value));
        const gpga::SignalInfo* sig = FindSignalInfo(module, target);
        if (!sig) {
          if (error) {
            *error = "fread target not found: " + target;
          }
          return false;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args[1].value);
        auto file_it = files->handles.find(fd);
        if (file_it == files->handles.end() || !file_it->second.file) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        uint32_t width = sig->is_real ? 64u : sig->width;
        if (width == 0u) {
          width = 1u;
        }
        size_t elem_bytes = (width + 7u) / 8u;
        size_t word_count = SignalWordCount(*sig);
        if (elem_bytes == 0) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        auto read_element =
            [&](std::vector<uint64_t>* out_val_words,
                std::vector<uint64_t>* out_xz_words) -> size_t {
          if (!out_val_words || !out_xz_words) {
            return 0;
          }
          std::vector<unsigned char> buffer(elem_bytes, 0);
          size_t bytes_read =
              std::fread(buffer.data(), 1, elem_bytes, file_it->second.file);
          if (bytes_read == 0) {
            return 0;
          }
          out_val_words->assign(word_count, 0ull);
          out_xz_words->assign(word_count, 0ull);
          size_t start_pos = elem_bytes - bytes_read;
          for (size_t i = 0; i < bytes_read; ++i) {
            size_t pos = start_pos + (bytes_read - 1u - i);
            uint32_t bit_pos = static_cast<uint32_t>(pos * 8u);
            uint32_t word_index = bit_pos / 64u;
            uint32_t word_bit = bit_pos % 64u;
            uint64_t byte_val = static_cast<uint64_t>(buffer[i]);
            if (word_index < out_val_words->size()) {
              (*out_val_words)[word_index] |= (byte_val << word_bit);
            }
            if (word_bit > 56u && word_index + 1u < out_val_words->size()) {
              (*out_val_words)[word_index + 1u] |=
                  (byte_val >> (64u - word_bit));
            }
          }
          if (four_state && bytes_read < elem_bytes) {
            for (size_t pos = 0; pos < start_pos; ++pos) {
              uint32_t bit_pos = static_cast<uint32_t>(pos * 8u);
              uint32_t word_index = bit_pos / 64u;
              uint32_t word_bit = bit_pos % 64u;
              uint64_t byte_mask = 0xFFull;
              if (word_index < out_xz_words->size()) {
                (*out_xz_words)[word_index] |= (byte_mask << word_bit);
              }
              if (word_bit > 56u && word_index + 1u < out_xz_words->size()) {
                (*out_xz_words)[word_index + 1u] |=
                    (byte_mask >> (64u - word_bit));
              }
            }
          }
          uint32_t tail_bits = width % 64u;
          if (tail_bits != 0u && !out_val_words->empty()) {
            uint64_t mask = (1ull << tail_bits) - 1ull;
            (*out_val_words)[out_val_words->size() - 1u] &= mask;
            (*out_xz_words)[out_xz_words->size() - 1u] &= mask;
          }
          return bytes_read;
        };

        size_t array_size = sig->array_size > 0 ? sig->array_size : 1u;
        if (array_size <= 1u) {
          std::vector<uint64_t> val_words;
          std::vector<uint64_t> xz_words;
          size_t bytes_read = read_element(&val_words, &xz_words);
          if (bytes_read == 0) {
            resume_if_waiting(rec, record_index, 0);
            break;
          }
          for (size_t w = 0; w < word_count; ++w) {
            uint64_t word = w < val_words.size() ? val_words[w] : 0ull;
            uint64_t xz_word = w < xz_words.size() ? xz_words[w] : 0ull;
            WriteSignalWord(*sig, gid, 0u, w, word, xz_word, four_state,
                            buffers);
          }
          resume_if_waiting(rec, record_index, static_cast<uint64_t>(bytes_read));
          break;
        }

        int64_t start = 0;
        int64_t count = -1;
        if (rec.args.size() >= 3 &&
            rec.args[2].kind == gpga::ServiceArgKind::kValue) {
          start = SignExtend(rec.args[2].value, rec.args[2].width);
        }
        if (rec.args.size() >= 4 &&
            rec.args[3].kind == gpga::ServiceArgKind::kValue) {
          count = SignExtend(rec.args[3].value, rec.args[3].width);
        }
        if (start < 0 || count == 0) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        uint64_t start_index = static_cast<uint64_t>(start);
        if (start_index >= array_size) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        uint64_t max_elements = array_size - start_index;
        if (count > 0) {
          max_elements = std::min<uint64_t>(max_elements, count);
        }
        uint64_t total_read = 0;
        for (uint64_t i = 0; i < max_elements; ++i) {
          std::vector<uint64_t> val_words;
          std::vector<uint64_t> xz_words;
          size_t bytes_read = read_element(&val_words, &xz_words);
          if (bytes_read == 0) {
            break;
          }
          for (size_t w = 0; w < word_count; ++w) {
            uint64_t word = w < val_words.size() ? val_words[w] : 0ull;
            uint64_t xz_word = w < xz_words.size() ? xz_words[w] : 0ull;
            WriteSignalWord(*sig, gid, start_index + i, w, word, xz_word,
                            four_state, buffers);
          }
          total_read += bytes_read;
          if (bytes_read < elem_bytes) {
            break;
          }
        }
        resume_if_waiting(rec, record_index, total_read);
        break;
      }
      case gpga::ServiceKind::kFtell: {
        if (rec.args.empty() ||
            rec.args.front().kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 0xFFFFFFFFFFFFFFFFull);
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
        auto file_it = files->handles.find(fd);
        uint64_t value = 0xFFFFFFFFFFFFFFFFull;
        if (file_it != files->handles.end() && file_it->second.file) {
          long pos = std::ftell(file_it->second.file);
          if (pos >= 0) {
            value = static_cast<uint64_t>(pos);
          }
        }
        resume_if_waiting(rec, record_index, value);
        break;
      }
      case gpga::ServiceKind::kFscanf: {
        if (rec.args.size() < 2 ||
            rec.args[0].kind != gpga::ServiceArgKind::kValue) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        uint32_t fd = static_cast<uint32_t>(rec.args[0].value);
        auto file_it = files->handles.find(fd);
        if (file_it == files->handles.end() || !file_it->second.file) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        std::string fmt = (rec.format_id != 0xFFFFFFFFu)
                              ? ResolveString(strings, rec.format_id)
                              : "";
        std::vector<FormatSpec> specs = ParseFormatSpecs(fmt);
        int assigned = 0;
        bool eof_before = false;
        size_t arg_index = 1;
        for (size_t spec_index = 0; spec_index < specs.size(); ++spec_index) {
          const FormatSpec& spec = specs[spec_index];
          std::string token;
          bool token_eof = false;
          if (!ReadTokenFromFile(file_it->second.file, &token, &token_eof)) {
            eof_before = token_eof && (assigned == 0);
            break;
          }
          if (spec.suppress) {
            std::vector<uint64_t> ignored_words;
            std::string ignored_str;
            if (!ParseTokenWords(token, spec.spec, 64u, &ignored_words,
                                 &ignored_str)) {
              break;
            }
            continue;
          }
          while (arg_index < rec.args.size() &&
                 rec.args[arg_index].kind != gpga::ServiceArgKind::kIdent) {
            ++arg_index;
          }
          if (arg_index >= rec.args.size()) {
            break;
          }
          uint32_t target_width = rec.args[arg_index].width;
          if (target_width == 0u) {
            target_width = 64u;
          }
          std::vector<uint64_t> words;
          std::string str_value;
          if (!ParseTokenWords(token, spec.spec, target_width, &words,
                               &str_value)) {
            break;
          }
          uint64_t value = words.empty() ? 0ull : words[0];
          const std::vector<uint64_t>* wide_words =
              (target_width > 64u) ? &words : nullptr;
          write_output_arg(rec.args[arg_index], value, str_value, wide_words);
          assigned++;
          ++arg_index;
        }
        if (specs.empty()) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        if (eof_before) {
          resume_if_waiting(rec, record_index, 0xFFFFFFFFull);
        } else {
          resume_if_waiting(rec, record_index, static_cast<uint64_t>(assigned));
        }
        break;
      }
      case gpga::ServiceKind::kSscanf: {
        if (rec.args.size() < 2) {
          resume_if_waiting(rec, record_index, 0);
          break;
        }
        std::string input = resolve_string_or_ident(rec.args[0]);
        std::string fmt = (rec.format_id != 0xFFFFFFFFu)
                              ? ResolveString(strings, rec.format_id)
                              : "";
        std::vector<FormatSpec> specs = ParseFormatSpecs(fmt);
        size_t pos = 0;
        int assigned = 0;
        size_t arg_index = 1;
        for (size_t spec_index = 0; spec_index < specs.size(); ++spec_index) {
          const FormatSpec& spec = specs[spec_index];
          std::string token;
          if (!ReadTokenFromString(input, &pos, &token)) {
            resume_if_waiting(rec, record_index, static_cast<uint64_t>(assigned));
            break;
          }
          if (spec.suppress) {
            std::vector<uint64_t> ignored_words;
            std::string ignored_str;
            if (!ParseTokenWords(token, spec.spec, 64u, &ignored_words,
                                 &ignored_str)) {
              resume_if_waiting(rec, record_index,
                                static_cast<uint64_t>(assigned));
              break;
            }
            continue;
          }
          while (arg_index < rec.args.size() &&
                 rec.args[arg_index].kind != gpga::ServiceArgKind::kIdent) {
            ++arg_index;
          }
          if (arg_index >= rec.args.size()) {
            break;
          }
          uint32_t target_width = rec.args[arg_index].width;
          if (target_width == 0u) {
            target_width = 64u;
          }
          std::vector<uint64_t> words;
          std::string str_value;
          if (!ParseTokenWords(token, spec.spec, target_width, &words,
                               &str_value)) {
            resume_if_waiting(rec, record_index,
                              static_cast<uint64_t>(assigned));
            break;
          }
          uint64_t value = words.empty() ? 0ull : words[0];
          const std::vector<uint64_t>* wide_words =
              (target_width > 64u) ? &words : nullptr;
          write_output_arg(rec.args[arg_index], value, str_value, wide_words);
          assigned++;
          ++arg_index;
        }
        resume_if_waiting(rec, record_index, static_cast<uint64_t>(assigned));
        break;
      }
      case gpga::ServiceKind::kDumpoff: {
        vcd->SetDumping(false);
        std::cout << "$dumpoff (pid=" << rec.pid << ")\n";
        break;
      }
      case gpga::ServiceKind::kDumpon: {
        vcd->SetDumping(true);
        vcd->ForceSnapshot(current_time(), *buffers);
        std::cout << "$dumpon (pid=" << rec.pid << ")\n";
        break;
      }
      case gpga::ServiceKind::kDumpflush: {
        vcd->Flush();
        std::cout << "$dumpflush (pid=" << rec.pid << ")\n";
        break;
      }
      case gpga::ServiceKind::kDumpall: {
        vcd->ForceSnapshot(current_time(), *buffers);
        std::cout << "$dumpall (pid=" << rec.pid << ")\n";
        break;
      }
      case gpga::ServiceKind::kDumplimit: {
        uint64_t limit = 0;
        if (!rec.args.empty() &&
            rec.args[0].kind == gpga::ServiceArgKind::kValue) {
          limit = rec.args[0].value;
        }
        vcd->SetDumpLimit(limit);
        std::cout << "$dumplimit (pid=" << rec.pid << ")";
        if (!rec.args.empty()) {
          std::cout << " " << FormatNumeric(rec.args.front(), 'h', four_state);
        }
        std::cout << "\n";
        break;
      }
      case gpga::ServiceKind::kReadmemh:
      case gpga::ServiceKind::kReadmemb: {
        std::string label =
            (rec.kind == gpga::ServiceKind::kReadmemh) ? "$readmemh"
                                                       : "$readmemb";
        std::string filename = ResolveString(strings, rec.format_id);
        std::string target;
        uint64_t start = 0;
        uint64_t end = std::numeric_limits<uint64_t>::max();
        bool seen_target = false;
        bool seen_start = false;
        for (const auto& arg : rec.args) {
          if (arg.kind == gpga::ServiceArgKind::kIdent ||
              arg.kind == gpga::ServiceArgKind::kString) {
            if (!seen_target &&
                arg.kind == gpga::ServiceArgKind::kString &&
                arg.value == static_cast<uint64_t>(rec.format_id)) {
              continue;
            }
            if (!seen_target) {
              target =
                  ResolveString(strings, static_cast<uint32_t>(arg.value));
              seen_target = true;
              continue;
            }
          }
          if (seen_target && arg.kind == gpga::ServiceArgKind::kValue) {
            if (!seen_start) {
              start = arg.value;
              seen_start = true;
            } else if (end == std::numeric_limits<uint64_t>::max()) {
              end = arg.value;
            }
          }
        }
        if (target.empty()) {
          continue;
        }
        auto it = std::find_if(module.signals.begin(), module.signals.end(),
                               [&](const gpga::SignalInfo& sig) {
                                 return sig.name == target;
                               });
        if (it == module.signals.end()) {
          if (error) {
            *error = "readmem target not found: " + target;
          }
          return false;
        }
        bool is_hex = rec.kind == gpga::ServiceKind::kReadmemh;
        if (!ApplyReadmem(filename, is_hex, *it, four_state, buffers,
                          instance_count, start, end, error)) {
          return false;
        }
        std::cout << label << " \"" << filename << "\" (pid=" << rec.pid << ")";
        for (const auto& arg : rec.args) {
          std::cout << " ";
          if (arg.kind == gpga::ServiceArgKind::kString ||
              arg.kind == gpga::ServiceArgKind::kIdent) {
            std::cout
                << ResolveString(strings, static_cast<uint32_t>(arg.value));
          } else {
            std::cout << FormatNumeric(arg, 'h', four_state);
          }
        }
        std::cout << "\n";
        break;
      }
      case gpga::ServiceKind::kWritememh:
      case gpga::ServiceKind::kWritememb: {
        std::string label =
            (rec.kind == gpga::ServiceKind::kWritememh) ? "$writememh"
                                                        : "$writememb";
        std::string filename = ResolveString(strings, rec.format_id);
        std::string target;
        uint64_t start = 0;
        uint64_t end = std::numeric_limits<uint64_t>::max();
        bool seen_target = false;
        bool seen_start = false;
        for (const auto& arg : rec.args) {
          if (arg.kind == gpga::ServiceArgKind::kIdent ||
              arg.kind == gpga::ServiceArgKind::kString) {
            if (!seen_target &&
                arg.kind == gpga::ServiceArgKind::kString &&
                arg.value == static_cast<uint64_t>(rec.format_id)) {
              continue;
            }
            if (!seen_target) {
              target =
                  ResolveString(strings, static_cast<uint32_t>(arg.value));
              seen_target = true;
              continue;
            }
          }
          if (seen_target && arg.kind == gpga::ServiceArgKind::kValue) {
            if (!seen_start) {
              start = arg.value;
              seen_start = true;
            } else if (end == std::numeric_limits<uint64_t>::max()) {
              end = arg.value;
            }
          }
        }
        if (target.empty()) {
          continue;
        }
        auto it = std::find_if(module.signals.begin(), module.signals.end(),
                               [&](const gpga::SignalInfo& sig) {
                                 return sig.name == target;
                               });
        if (it == module.signals.end()) {
          if (error) {
            *error = "writemem target not found: " + target;
          }
          return false;
        }
        bool is_hex = rec.kind == gpga::ServiceKind::kWritememh;
        if (!ApplyWritemem(filename, is_hex, *it, four_state, buffers, start,
                           end, error)) {
          return false;
        }
        std::cout << label << " \"" << filename << "\" (pid=" << rec.pid << ")";
        for (const auto& arg : rec.args) {
          std::cout << " ";
          if (arg.kind == gpga::ServiceArgKind::kString ||
              arg.kind == gpga::ServiceArgKind::kIdent) {
            std::cout
                << ResolveString(strings, static_cast<uint32_t>(arg.value));
          } else {
            std::cout << FormatNumeric(arg, 'h', four_state);
          }
        }
        std::cout << "\n";
        break;
      }
      default:
        if (result) {
          result->saw_error = true;
        }
        std::cout << "unknown service kind " << static_cast<uint32_t>(rec.kind)
                  << " (pid=" << rec.pid << ")\n";
        break;
    }
  }
  return true;
}

bool HasKernel(const std::string& msl, const std::string& name) {
  return msl.find("kernel void " + name) != std::string::npos;
}

void MergeSpecs(std::unordered_map<std::string, size_t>* lengths,
                const std::vector<gpga::BufferSpec>& specs) {
  if (!lengths) {
    return;
  }
  for (const auto& spec : specs) {
    size_t& current = (*lengths)[spec.name];
    current = std::max(current, spec.length);
  }
}

bool BuildBindings(const gpga::MetalKernel& kernel,
                   const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
                   std::vector<gpga::MetalBufferBinding>* bindings,
                   std::string* error) {
  if (!bindings) {
    return false;
  }
  bindings->clear();
  for (const auto& entry : kernel.BufferIndices()) {
    auto it = buffers.find(entry.first);
    if (it == buffers.end()) {
      if (error) {
        *error = "missing buffer for " + entry.first;
      }
      return false;
    }
    bindings->push_back({entry.second, &it->second, 0});
  }
  return true;
}

bool BuildSchedulerVmArgBuffer(
    gpga::MetalRuntime* runtime, const gpga::MetalKernel& kernel,
    std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
    std::string* error) {
  if (!runtime || !buffers) {
    if (error) {
      *error = "scheduler VM argument buffer requires runtime and buffers";
    }
    return false;
  }
  if (!kernel.HasBuffer("sched_vm_args")) {
    return true;
  }
  constexpr uint32_t kVmArgIdBytecode = 0u;
  constexpr uint32_t kVmArgIdProcOffset = 1u;
  constexpr uint32_t kVmArgIdProcLength = 2u;
  constexpr uint32_t kVmArgIdCondVal = 3u;
  constexpr uint32_t kVmArgIdCondXz = 4u;
  constexpr uint32_t kVmArgIdCondEntry = 5u;
  constexpr uint32_t kVmArgIdSignalEntry = 6u;
  constexpr uint32_t kVmArgIdIp = 7u;
  constexpr uint32_t kVmArgIdCallSp = 8u;
  constexpr uint32_t kVmArgIdCallFrame = 9u;
  constexpr uint32_t kVmArgIdCaseHeader = 10u;
  constexpr uint32_t kVmArgIdCaseEntry = 11u;
  constexpr uint32_t kVmArgIdCaseWords = 12u;
  constexpr uint32_t kVmArgIdExpr = 13u;
  constexpr uint32_t kVmArgIdExprImm = 14u;
  constexpr uint32_t kVmArgIdDelayAssignEntry = 15u;
  constexpr uint32_t kVmArgIdAssignEntry = 16u;
  constexpr uint32_t kVmArgIdForceEntry = 17u;
  constexpr uint32_t kVmArgIdReleaseEntry = 18u;
  constexpr uint32_t kVmArgIdServiceEntry = 19u;
  constexpr uint32_t kVmArgIdServiceArg = 20u;
  constexpr uint32_t kVmArgIdServiceRetAssignEntry = 21u;
  std::vector<gpga::MetalBufferBinding> bindings;
  bindings.reserve(19);
  auto add_binding = [&](const char* name, uint32_t index) -> bool {
    auto it = buffers->find(name);
    if (it == buffers->end()) {
      if (error) {
        *error = std::string("missing buffer for scheduler VM arg: ") + name;
      }
      return false;
    }
    bindings.push_back({index, &it->second, 0});
    return true;
  };
  if (!add_binding("sched_vm_bytecode", kVmArgIdBytecode) ||
      !add_binding("sched_vm_proc_bytecode_offset", kVmArgIdProcOffset) ||
      !add_binding("sched_vm_proc_bytecode_length", kVmArgIdProcLength) ||
      !add_binding("sched_vm_cond_val", kVmArgIdCondVal) ||
      !add_binding("sched_vm_cond_xz", kVmArgIdCondXz) ||
      !add_binding("sched_vm_cond_entry", kVmArgIdCondEntry) ||
      !add_binding("sched_vm_signal_entry", kVmArgIdSignalEntry) ||
      !add_binding("sched_vm_ip", kVmArgIdIp) ||
      !add_binding("sched_vm_call_sp", kVmArgIdCallSp) ||
      !add_binding("sched_vm_call_frame", kVmArgIdCallFrame) ||
      !add_binding("sched_vm_case_header", kVmArgIdCaseHeader) ||
      !add_binding("sched_vm_case_entry", kVmArgIdCaseEntry) ||
      !add_binding("sched_vm_case_words", kVmArgIdCaseWords) ||
      !add_binding("sched_vm_expr", kVmArgIdExpr) ||
      !add_binding("sched_vm_expr_imm", kVmArgIdExprImm) ||
      !add_binding("sched_vm_delay_assign_entry",
                   kVmArgIdDelayAssignEntry) ||
      !add_binding("sched_vm_assign_entry", kVmArgIdAssignEntry) ||
      !add_binding("sched_vm_force_entry", kVmArgIdForceEntry) ||
      !add_binding("sched_vm_release_entry", kVmArgIdReleaseEntry) ||
      !add_binding("sched_vm_service_entry", kVmArgIdServiceEntry) ||
      !add_binding("sched_vm_service_arg", kVmArgIdServiceArg) ||
      !add_binding("sched_vm_service_ret_assign_entry",
                   kVmArgIdServiceRetAssignEntry)) {
    return false;
  }
  const uint32_t arg_index = kernel.BufferIndex("sched_vm_args");
  gpga::MetalBuffer arg_buffer;
  if (!runtime->EncodeArgumentBuffer(kernel, arg_index, bindings, &arg_buffer,
                                     error)) {
    return false;
  }
  (*buffers)["sched_vm_args"] = std::move(arg_buffer);
  return true;
}

void SwapNextBuffers(std::unordered_map<std::string, gpga::MetalBuffer>* buffers) {
  if (!buffers) {
    return;
  }
  std::vector<std::pair<std::string, std::string>> swaps;
  for (const auto& entry : *buffers) {
    const std::string& name = entry.first;
    if (name.size() > 9 && name.compare(name.size() - 9, 9, "_next_val") == 0) {
      swaps.emplace_back(name.substr(0, name.size() - 9) + "_val", name);
    } else if (name.size() > 8 &&
               name.compare(name.size() - 8, 8, "_next_xz") == 0) {
      swaps.emplace_back(name.substr(0, name.size() - 8) + "_xz", name);
    } else if (name.size() > 5 &&
               name.compare(name.size() - 5, 5, "_next") == 0) {
      swaps.emplace_back(name.substr(0, name.size() - 5), name);
    }
  }
  for (const auto& pair : swaps) {
    auto it_a = buffers->find(pair.first);
    auto it_b = buffers->find(pair.second);
    if (it_a != buffers->end() && it_b != buffers->end()) {
      std::swap(it_a->second, it_b->second);
    }
  }
}

bool RunMetal(const gpga::Module& module, const std::string& msl,
              const std::unordered_map<std::string, std::string>& flat_to_hier,
              bool enable_4state, uint32_t count, uint32_t service_capacity,
              uint32_t max_steps, uint32_t max_proc_steps,
              uint32_t dispatch_timeout_ms,
              bool run_verbose,
              bool source_bindings,
              const std::string& vcd_dir, uint32_t vcd_steps,
              const std::vector<std::string>& plusargs,
              std::string* error) {
  gpga::MetalRuntime runtime;
  runtime.SetPreferSourceBindings(source_bindings);
  if (run_verbose) {
    std::cerr << "Compiling Metal source (" << msl.size() << " bytes)...\n";
  }
  auto compile_start = std::chrono::steady_clock::now();
  if (!runtime.CompileSource(msl, {"include"}, error)) {
    return false;
  }
  if (run_verbose) {
    auto compile_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - compile_start);
    std::cerr << "Compile finished in " << compile_ms.count() << " ms\n";
  }

  const std::string base = "gpga_" + gpga::MslMangleIdentifier(module.name);
  const bool has_sched = HasKernel(msl, base + "_sched_step");
  const bool has_init = HasKernel(msl, base + "_init");
  const bool has_tick = HasKernel(msl, base + "_tick");
  {
    std::vector<std::string> precompile;
    if (has_sched) {
      precompile.push_back(base + "_sched_step");
    } else {
      precompile.push_back(base);
      if (has_init) {
        precompile.push_back(base + "_init");
      }
      if (has_tick) {
        precompile.push_back(base + "_tick");
      }
    }
    if (!runtime.PrecompileKernels(precompile, error)) {
      return false;
    }
  }

  gpga::SchedulerConstants sched;
  gpga::ParseSchedulerConstants(msl, &sched, error);
  gpga::SchedulerVmLayout vm_layout;
  const gpga::SchedulerVmLayout* vm_layout_ptr = nullptr;
  if (sched.vm_enabled) {
    if (!gpga::BuildSchedulerVmLayoutFromModule(
            module, &vm_layout, error, enable_4state)) {
      return false;
    }
    if (!vm_layout.bytecode.empty()) {
      sched.vm_bytecode_words =
          static_cast<uint32_t>(vm_layout.bytecode.size());
      sched.vm_case_header_count =
          static_cast<uint32_t>(vm_layout.case_headers.size());
      sched.vm_case_entry_count =
          static_cast<uint32_t>(vm_layout.case_entries.size());
      sched.vm_case_word_count =
          static_cast<uint32_t>(vm_layout.case_words.size());
      sched.vm_assign_count =
          static_cast<uint32_t>(vm_layout.assign_entries.size());
      sched.vm_service_call_count =
          static_cast<uint32_t>(vm_layout.service_entries.size());
      sched.vm_service_assign_count =
          static_cast<uint32_t>(vm_layout.service_ret_entries.size());
      sched.vm_service_arg_count =
          static_cast<uint32_t>(vm_layout.service_args.size());
      sched.vm_expr_word_count =
          static_cast<uint32_t>(vm_layout.expr_table.words.size());
      sched.vm_expr_imm_word_count =
          static_cast<uint32_t>(vm_layout.expr_table.imm_words.size());
      vm_layout_ptr = &vm_layout;
    }
    if (run_verbose) {
      uint32_t callgroup_procs = 0u;
      uint32_t empty_procs = 0u;
      const size_t proc_count = std::min(vm_layout.proc_offsets.size(),
                                         vm_layout.proc_lengths.size());
      for (size_t pid = 0; pid < proc_count; ++pid) {
        const uint32_t len = vm_layout.proc_lengths[pid];
        if (len == 0u) {
          empty_procs += 1u;
          continue;
        }
        const size_t offset = vm_layout.proc_offsets[pid];
        if (offset >= vm_layout.bytecode.size()) {
          callgroup_procs += 1u;
          continue;
        }
        if (gpga::DecodeSchedulerVmOp(vm_layout.bytecode[offset]) ==
            gpga::SchedulerVmOp::kCallGroup) {
          callgroup_procs += 1u;
        }
      }
      if (callgroup_procs == 0u) {
        std::cerr << "sched-vm: no CallGroup fallbacks; A-full helpers skipped\n";
      } else {
        std::cerr << "sched-vm: CallGroup fallbacks in " << callgroup_procs
                  << " of " << proc_count << " procs";
        if (empty_procs > 0u) {
          std::cerr << " (" << empty_procs << " empty bytecode)";
        }
        std::cerr << "\n";
      }
    }
  }

  gpga::ModuleInfo info = BuildModuleInfo(module, enable_4state);

  gpga::MetalKernel comb_kernel;
  gpga::MetalKernel init_kernel;
  gpga::MetalKernel tick_kernel;
  gpga::MetalKernel sched_kernel;

  if (has_sched) {
    if (run_verbose) {
      std::cerr << "Creating scheduler kernel...\n";
    }
    if (!runtime.CreateKernel(base + "_sched_step", &sched_kernel, error)) {
      return false;
    }
  } else {
    if (run_verbose) {
      std::cerr << "Creating combinational kernel...\n";
    }
    if (!runtime.CreateKernel(base, &comb_kernel, error)) {
      return false;
    }
    if (has_init) {
      if (run_verbose) {
        std::cerr << "Creating init kernel...\n";
      }
      if (!runtime.CreateKernel(base + "_init", &init_kernel, error)) {
        return false;
      }
    }
    if (has_tick) {
      if (run_verbose) {
        std::cerr << "Creating tick kernel...\n";
      }
      if (!runtime.CreateKernel(base + "_tick", &tick_kernel, error)) {
        return false;
      }
    }
  }

  std::unordered_map<std::string, size_t> buffer_lengths;
  std::vector<gpga::BufferSpec> specs;
  if (has_sched) {
    if (!gpga::BuildBufferSpecs(info, sched_kernel, sched, count,
                                service_capacity, &specs, error)) {
      return false;
    }
    MergeSpecs(&buffer_lengths, specs);
  } else {
    if (!gpga::BuildBufferSpecs(info, comb_kernel, sched, count,
                                service_capacity, &specs, error)) {
      return false;
    }
    MergeSpecs(&buffer_lengths, specs);
    if (has_init) {
      if (!gpga::BuildBufferSpecs(info, init_kernel, sched, count,
                                  service_capacity, &specs, error)) {
        return false;
      }
      MergeSpecs(&buffer_lengths, specs);
    }
    if (has_tick) {
      if (!gpga::BuildBufferSpecs(info, tick_kernel, sched, count,
                                  service_capacity, &specs, error)) {
        return false;
      }
      MergeSpecs(&buffer_lengths, specs);
    }
  }

  std::unordered_map<std::string, gpga::MetalBuffer> buffers;
  for (const auto& entry : buffer_lengths) {
    gpga::MetalBuffer buffer = runtime.CreateBuffer(entry.second, nullptr);
    if (buffer.contents()) {
      std::memset(buffer.contents(), 0, buffer.length());
    }
    buffers.emplace(entry.first, std::move(buffer));
  }
  uint32_t* sched_halt_mode = nullptr;
  auto halt_it = buffers.find("sched_halt_mode");
  if (halt_it != buffers.end() && halt_it->second.contents()) {
    sched_halt_mode =
        static_cast<uint32_t*>(halt_it->second.contents());
    for (uint32_t gid = 0; gid < count; ++gid) {
      sched_halt_mode[gid] = GPGA_SCHED_HALT_NONE;
    }
  }

  PackedStateLayout packed_layout;
  bool has_packed_layout = false;
  if (buffers.find("gpga_state") != buffers.end()) {
    packed_layout = BuildPackedStateLayout(module, info, enable_4state, count);
    has_packed_layout = true;
  }

  const bool has_dumpvars = ModuleUsesDumpvars(module);
  const uint32_t vcd_step_budget = (vcd_steps > 0u) ? vcd_steps : 1u;
  uint32_t effective_max_steps = max_steps;
  if (has_dumpvars) {
    effective_max_steps = 1u;
  }

  auto params_it = buffers.find("params");
  if (params_it != buffers.end() && params_it->second.contents()) {
    auto* params =
        static_cast<gpga::GpgaParams*>(params_it->second.contents());
    params->count = count;
  }
  auto sched_it = buffers.find("sched");
  gpga::GpgaSchedParams* sched_params = nullptr;
  if (sched_it != buffers.end() && sched_it->second.contents()) {
    sched_params =
        static_cast<gpga::GpgaSchedParams*>(sched_it->second.contents());
    sched_params->count = count;
    sched_params->max_steps = effective_max_steps;
    sched_params->max_proc_steps = max_proc_steps;
    sched_params->service_capacity = service_capacity;
  }

  if (!InitSchedulerVmBuffers(&buffers, sched, count, vm_layout_ptr, error)) {
    return false;
  }
  if (has_sched && sched.vm_enabled) {
    if (!BuildSchedulerVmArgBuffer(&runtime, sched_kernel, &buffers, error)) {
      return false;
    }
  }

  gpga::ServiceStringTable strings = BuildStringTable(module);
  VcdWriter vcd;
  if (has_packed_layout) {
    vcd.SetPackedLayout(&packed_layout);
  }
  std::string dumpfile;
  std::vector<FileTable> file_tables(count);

  if (has_sched) {
    if (sched_halt_mode) {
      InstallHaltSignalHandlers();
    }
    std::vector<gpga::MetalBufferBinding> bindings;
    if (!BuildBindings(sched_kernel, buffers, &bindings, error)) {
      return false;
    }
    const uint32_t kStatusFinished = 2u;
    const uint32_t kStatusError = 3u;
    const uint32_t kStatusStopped = 4u;
    const uint32_t kStatusIdle = 1u;
    const uint32_t max_iters = 100000u;
    for (uint32_t iter = 0; iter < max_iters; ++iter) {
      if (sched_params && has_dumpvars) {
        sched_params->max_steps = vcd.active() ? vcd_step_budget : 1u;
      }
      if (sched_halt_mode && g_halt_request != 0) {
        for (uint32_t gid = 0; gid < count; ++gid) {
          sched_halt_mode[gid] = GPGA_SCHED_HALT_STOP;
        }
        g_halt_request = 0;
      }
      if (run_verbose && (iter == 0u || (iter % 1000u) == 0u)) {
        std::cerr << "Dispatch iter " << iter << "\n";
      }
      auto dispatch_start = std::chrono::steady_clock::now();
      if (!runtime.Dispatch(sched_kernel, bindings, count, error,
                            dispatch_timeout_ms)) {
        return false;
      }
      if (run_verbose && (iter == 0u || (iter % 1000u) == 0u)) {
        auto dispatch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - dispatch_start);
        std::cerr << "Dispatch iter " << iter << " took "
                  << dispatch_ms.count() << " ms\n";
      }
      bool saw_finish = false;
      if (sched_kernel.HasBuffer("sched_service") &&
          sched_kernel.HasBuffer("sched_service_count")) {
        auto* counts =
            static_cast<uint32_t*>(buffers["sched_service_count"].contents());
        auto* records =
            static_cast<uint8_t*>(buffers["sched_service"].contents());
        if (counts && records) {
          size_t stride =
              gpga::ServiceRecordStride(
                  std::max<uint32_t>(1u, sched.service_max_args),
                  sched.service_wide_words, enable_4state);
          for (uint32_t gid = 0; gid < count; ++gid) {
            uint32_t used = counts[gid];
            if (used > service_capacity) {
              std::cerr << "warning: service record overflow (gid=" << gid
                        << ", used=" << used << ", capacity="
                        << service_capacity << ")\n";
              used = service_capacity;
            }
            if (used == 0u) {
              continue;
            }
            const uint8_t* rec_base =
                records + (gid * service_capacity * stride);
            std::vector<DecodedServiceRecord> decoded;
            DecodeServiceRecords(rec_base, used,
                                 std::max<uint32_t>(1u,
                                                    sched.service_max_args),
                                 sched.service_wide_words, enable_4state,
                                 &decoded);
            gpga::ServiceDrainResult result;
            if (!HandleServiceRecords(decoded, strings, info, vcd_dir,
                                      &flat_to_hier, module.timescale,
                                      enable_4state, count, gid,
                                      sched.proc_count, &file_tables[gid],
                                      plusargs, &buffers, &vcd, &result,
                                      &dumpfile, error)) {
              return false;
            }
            if (result.saw_finish || result.saw_stop || result.saw_error) {
              saw_finish = true;
            }
            counts[gid] = 0u;
          }
        }
      }
      if (vcd.active()) {
        auto time_it = buffers.find("sched_time");
        if (time_it != buffers.end() && time_it->second.contents()) {
          uint64_t time = 0;
          std::memcpy(&time, time_it->second.contents(), sizeof(time));
          vcd.Update(time, buffers);
        } else {
          vcd.Update(static_cast<uint64_t>(iter), buffers);
        }
      }
      auto* status =
          static_cast<uint32_t*>(buffers["sched_status"].contents());
      if (!status) {
        break;
      }
      const uint32_t status_val = status[0];
      const bool should_stop = status_val == kStatusFinished ||
          status_val == kStatusStopped || status_val == kStatusError ||
          saw_finish;
      if (status_val == kStatusIdle && has_dumpvars && !should_stop) {
        continue;
      }
      if (should_stop || status_val == kStatusIdle) {
        vcd.FinalSnapshot(buffers);
        vcd.Close();
        break;
      }
    }
  } else {
    std::vector<gpga::MetalBufferBinding> bindings;
    if (!BuildBindings(comb_kernel, buffers, &bindings, error)) {
      return false;
    }
    std::vector<gpga::MetalBufferBinding> init_bindings;
    std::vector<gpga::MetalBufferBinding> tick_bindings;
    std::vector<gpga::MetalDispatch> dispatches;
    if (has_init) {
      if (!BuildBindings(init_kernel, buffers, &init_bindings, error)) {
        return false;
      }
      dispatches.push_back(gpga::MetalDispatch{&init_kernel, &init_bindings});
    }
    dispatches.push_back(gpga::MetalDispatch{&comb_kernel, &bindings});
    if (has_tick) {
      if (!BuildBindings(tick_kernel, buffers, &tick_bindings, error)) {
        return false;
      }
      dispatches.push_back(gpga::MetalDispatch{&tick_kernel, &tick_bindings});
    }
    if (!runtime.DispatchBatch(dispatches, count, error,
                               dispatch_timeout_ms)) {
      return false;
    }
    if (has_tick) {
      SwapNextBuffers(&buffers);
    }
  }
  for (auto& table : file_tables) {
    for (auto& entry : table.handles) {
      if (entry.second.file) {
        std::fclose(entry.second.file);
      }
    }
    table.handles.clear();
  }
  return true;
}

int SignalWidth(const gpga::Module& module, const std::string& name) {
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return port.width;
    }
  }
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return net.width;
    }
  }
  return 32;
}

bool SignalSigned(const gpga::Module& module, const std::string& name) {
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return port.is_signed;
    }
  }
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return net.is_signed;
    }
  }
  return false;
}

bool IsArrayNet(const gpga::Module& module, const std::string& name,
                int* element_width) {
  for (const auto& net : module.nets) {
    if (net.name == name &&
        (net.array_size > 0 || !net.array_dims.empty())) {
      if (element_width) {
        *element_width = net.width;
      }
      return true;
    }
  }
  return false;
}

int MinimalWidth(uint64_t value) {
  if (value == 0) {
    return 1;
  }
  int width = 0;
  while (value > 0) {
    value >>= 1;
    ++width;
  }
  return width;
}

int ExprWidth(const gpga::Expr& expr, const gpga::Module& module) {
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      return SignalWidth(module, expr.ident);
    case gpga::ExprKind::kNumber:
      if (expr.has_width && expr.number_width > 0) {
        return expr.number_width;
      }
      return std::max(32, MinimalWidth(expr.number));
    case gpga::ExprKind::kString:
      return std::max<int>(
          1, static_cast<int>(expr.string_value.size() * 8));
    case gpga::ExprKind::kUnary:
      if (expr.unary_op == '!' || expr.unary_op == '&' ||
          expr.unary_op == '|' || expr.unary_op == '^') {
        return 1;
      }
      if (expr.unary_op == 'C') {
        return 32;
      }
      return expr.operand ? ExprWidth(*expr.operand, module) : 32;
    case gpga::ExprKind::kBinary:
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G' ||
          expr.op == 'A' || expr.op == 'O') {
        return 1;
      }
      if (expr.op == 'l' || expr.op == 'r' || expr.op == 'R') {
        return expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      }
      if (expr.op == 'p') {
        return expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      }
      return std::max(expr.lhs ? ExprWidth(*expr.lhs, module) : 32,
                      expr.rhs ? ExprWidth(*expr.rhs, module) : 32);
    case gpga::ExprKind::kTernary:
      return std::max(expr.then_expr ? ExprWidth(*expr.then_expr, module) : 32,
                      expr.else_expr ? ExprWidth(*expr.else_expr, module) : 32);
    case gpga::ExprKind::kSelect: {
      if (expr.indexed_range && expr.indexed_width > 0) {
        return expr.indexed_width;
      }
      int lo = std::min(expr.msb, expr.lsb);
      int hi = std::max(expr.msb, expr.lsb);
      return hi - lo + 1;
    }
    case gpga::ExprKind::kIndex: {
      if (expr.base && expr.base->kind == gpga::ExprKind::kIdentifier) {
        int element_width = 0;
        if (IsArrayNet(module, expr.base->ident, &element_width)) {
          return element_width;
        }
      }
      return 1;
    }
    case gpga::ExprKind::kCall:
      if (expr.ident == "$time") {
        return 64;
      }
      if (expr.ident == "$realtobits") {
        return 64;
      }
      return 32;
    case gpga::ExprKind::kConcat: {
      int total = 0;
      for (const auto& element : expr.elements) {
        total += ExprWidth(*element, module);
      }
      int repeats = std::max(0, expr.repeat);
      return total * repeats;
    }
  }
  return 32;
}

bool ExprSigned(const gpga::Expr& expr, const gpga::Module& module) {
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      return SignalSigned(module, expr.ident);
    case gpga::ExprKind::kNumber:
      return expr.is_signed || !expr.has_base;
    case gpga::ExprKind::kString:
      return false;
    case gpga::ExprKind::kUnary:
      if (expr.unary_op == 'S') {
        return true;
      }
      if (expr.unary_op == 'U') {
        return false;
      }
      if (expr.unary_op == 'C') {
        return false;
      }
      if (expr.unary_op == '!' || expr.unary_op == '&' ||
          expr.unary_op == '|' || expr.unary_op == '^') {
        return false;
      }
      if (expr.unary_op == '-' && expr.operand &&
          expr.operand->kind == gpga::ExprKind::kNumber) {
        return true;
      }
      return expr.operand ? ExprSigned(*expr.operand, module) : false;
    case gpga::ExprKind::kBinary: {
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G' ||
          expr.op == 'A' || expr.op == 'O') {
        return false;
      }
      if (expr.op == 'l' || expr.op == 'r' || expr.op == 'R') {
        return expr.lhs ? ExprSigned(*expr.lhs, module) : false;
      }
      bool lhs_signed = expr.lhs ? ExprSigned(*expr.lhs, module) : false;
      bool rhs_signed = expr.rhs ? ExprSigned(*expr.rhs, module) : false;
      return lhs_signed && rhs_signed;
    }
    case gpga::ExprKind::kTernary: {
      bool t_signed =
          expr.then_expr ? ExprSigned(*expr.then_expr, module) : false;
      bool e_signed =
          expr.else_expr ? ExprSigned(*expr.else_expr, module) : false;
      return t_signed && e_signed;
    }
    case gpga::ExprKind::kSelect:
    case gpga::ExprKind::kIndex:
    case gpga::ExprKind::kConcat:
      return false;
    case gpga::ExprKind::kCall:
      return false;
  }
  return false;
}

bool IsAllOnesExpr(const gpga::Expr& expr, const gpga::Module& module,
                   int* width_out) {
  switch (expr.kind) {
    case gpga::ExprKind::kNumber: {
      if (expr.x_bits != 0 || expr.z_bits != 0) {
        return false;
      }
      int width = expr.has_width && expr.number_width > 0
                      ? expr.number_width
                      : MinimalWidth(expr.number);
      if (width_out) {
        *width_out = width;
      }
      if (width <= 0) {
        return false;
      }
      if (width > 64) {
        return false;
      }
      uint64_t mask = (width == 64)
                          ? std::numeric_limits<uint64_t>::max()
                          : ((1ULL << width) - 1ULL);
      return expr.number == mask;
    }
    case gpga::ExprKind::kString:
      return false;
    case gpga::ExprKind::kConcat: {
      int base_width = 0;
      for (const auto& element : expr.elements) {
        int element_width = 0;
        if (!IsAllOnesExpr(*element, module, &element_width)) {
          return false;
        }
        base_width += element_width;
      }
      if (base_width <= 0) {
        return false;
      }
      int repeats = std::max(0, expr.repeat);
      if (repeats == 0) {
        return false;
      }
      int total = base_width * repeats;
      if (width_out) {
        *width_out = total;
      }
      return true;
    }
    default:
      return false;
  }
}

const gpga::Expr* SimplifyAssignMask(const gpga::Expr& expr,
                                     const gpga::Module& module,
                                     int lhs_width) {
  if (expr.kind != gpga::ExprKind::kBinary || expr.op != '&') {
    return nullptr;
  }
  if (!expr.lhs || !expr.rhs) {
    return nullptr;
  }
  int lhs_expr_width = ExprWidth(*expr.lhs, module);
  int rhs_expr_width = ExprWidth(*expr.rhs, module);
  int target = std::max(lhs_expr_width, rhs_expr_width);
  if (lhs_expr_width == lhs_width &&
      IsAllOnesExpr(*expr.rhs, module, nullptr) &&
      ExprWidth(*expr.rhs, module) == target) {
    return expr.lhs.get();
  }
  if (rhs_expr_width == lhs_width &&
      IsAllOnesExpr(*expr.lhs, module, nullptr) &&
      ExprWidth(*expr.lhs, module) == target) {
    return expr.rhs.get();
  }
  return nullptr;
}

bool IsCompareOp(char op) {
  return op == 'E' || op == 'N' || op == '<' || op == '>' || op == 'L' ||
         op == 'G';
}

bool IsShiftOp(char op) { return op == 'l' || op == 'r' || op == 'R'; }

bool IsLogicalOp(char op) { return op == 'A' || op == 'O'; }

std::string ExprToString(const gpga::Expr& expr, const gpga::Module& module) {
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      return expr.ident;
    case gpga::ExprKind::kNumber: {
      const char* digits = "0123456789ABCDEF";
      if (expr.has_base) {
        if (expr.x_bits != 0 || expr.z_bits != 0) {
          int width = expr.has_width && expr.number_width > 0
                          ? expr.number_width
                          : MinimalWidth(expr.number);
          int bits_per_digit = 1;
          switch (expr.base_char) {
            case 'b':
              bits_per_digit = 1;
              break;
            case 'o':
              bits_per_digit = 3;
              break;
            case 'h':
              bits_per_digit = 4;
              break;
            default:
              bits_per_digit = 1;
              break;
          }
          int digit_count =
              std::max(1, (width + bits_per_digit - 1) / bits_per_digit);
          std::string repr;
          repr.reserve(static_cast<size_t>(digit_count));
          for (int i = 0; i < digit_count; ++i) {
            int shift = (digit_count - 1 - i) * bits_per_digit;
            uint64_t mask_bits =
                (bits_per_digit >= 64) ? 0xFFFFFFFFFFFFFFFFull
                                       : ((1ull << bits_per_digit) - 1ull);
            uint64_t x =
                (shift >= 64) ? 0 : (expr.x_bits >> shift) & mask_bits;
            uint64_t z =
                (shift >= 64) ? 0 : (expr.z_bits >> shift) & mask_bits;
            if (x != 0) {
              repr.push_back('x');
              continue;
            }
            if (z != 0) {
              repr.push_back('z');
              continue;
            }
            uint64_t val = (shift >= 64)
                               ? 0
                               : (expr.value_bits >> shift) & mask_bits;
            repr.push_back(digits[static_cast<int>(val)]);
          }
          std::string prefix;
          if (expr.has_width && expr.number_width > 0) {
            prefix = std::to_string(expr.number_width);
          }
          std::string sign = expr.is_signed ? "s" : "";
          return prefix + "'" + sign + std::string(1, expr.base_char) + repr;
        }
        uint64_t value = expr.number;
        int base = 10;
        switch (expr.base_char) {
          case 'b':
            base = 2;
            break;
          case 'o':
            base = 8;
            break;
          case 'd':
            base = 10;
            break;
          case 'h':
            base = 16;
            break;
          default:
            base = 10;
            break;
        }
        std::string repr;
        if (value == 0) {
          repr = "0";
        } else {
          while (value > 0) {
            int digit = static_cast<int>(value % base);
            repr.insert(repr.begin(), digits[digit]);
            value /= static_cast<uint64_t>(base);
          }
        }
        std::string prefix;
        if (expr.has_width && expr.number_width > 0) {
          prefix = std::to_string(expr.number_width);
        }
        std::string sign = expr.is_signed ? "s" : "";
        return prefix + "'" + sign + std::string(1, expr.base_char) + repr;
      }
      if (expr.has_width && expr.number_width > 0) {
        return std::to_string(expr.number_width) + "'d" +
               std::to_string(expr.number);
      }
      return std::to_string(expr.number);
    }
    case gpga::ExprKind::kString:
      return "\"" + expr.string_value + "\"";
    case gpga::ExprKind::kUnary: {
      std::string operand =
          expr.operand ? ExprToString(*expr.operand, module) : "0";
      if (expr.unary_op == 'S') {
        return "$signed(" + operand + ")";
      }
      if (expr.unary_op == 'U') {
        return "$unsigned(" + operand + ")";
      }
      if (expr.unary_op == 'C') {
        return "$clog2(" + operand + ")";
      }
      return std::string(1, expr.unary_op) + operand;
    }
    case gpga::ExprKind::kBinary:
      {
        int lhs_width = expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
        int rhs_width = expr.rhs ? ExprWidth(*expr.rhs, module) : 32;
        int target = std::max(lhs_width, rhs_width);
        bool signed_op = expr.lhs && expr.rhs &&
                         ExprSigned(*expr.lhs, module) &&
                         ExprSigned(*expr.rhs, module);
        std::string lhs = expr.lhs ? ExprToString(*expr.lhs, module) : "0";
        std::string rhs = expr.rhs ? ExprToString(*expr.rhs, module) : "0";
        if (!IsShiftOp(expr.op) && !IsLogicalOp(expr.op)) {
          if (lhs_width < target) {
            lhs = (signed_op ? "sext(" : "zext(") + lhs + ", " +
                  std::to_string(target) + ")";
          }
          if (rhs_width < target) {
            rhs = (signed_op ? "sext(" : "zext(") + rhs + ", " +
                  std::to_string(target) + ")";
          }
        }
        if (expr.op == 'E') {
          return "(" + lhs + " == " + rhs + ")";
        }
        if (expr.op == 'N') {
          return "(" + lhs + " != " + rhs + ")";
        }
        if (expr.op == 'C') {
          return "(" + lhs + " === " + rhs + ")";
        }
        if (expr.op == 'c') {
          return "(" + lhs + " !== " + rhs + ")";
        }
        if (expr.op == 'W') {
          return "(" + lhs + " ==? " + rhs + ")";
        }
        if (expr.op == 'w') {
          return "(" + lhs + " !=? " + rhs + ")";
        }
        if (expr.op == 'L') {
          return "(" + lhs + " <= " + rhs + ")";
        }
        if (expr.op == 'G') {
          return "(" + lhs + " >= " + rhs + ")";
        }
        if (expr.op == 'l') {
          return "(" + lhs + " << " + rhs + ")";
        }
        if (expr.op == 'r') {
          return "(" + lhs + " >> " + rhs + ")";
        }
        if (expr.op == 'R') {
          return "(" + lhs + " >>> " + rhs + ")";
        }
        if (expr.op == 'A') {
          return "(" + lhs + " && " + rhs + ")";
        }
        if (expr.op == 'O') {
          return "(" + lhs + " || " + rhs + ")";
        }
        return "(" + lhs + " " + expr.op + " " + rhs + ")";
      }
    case gpga::ExprKind::kTernary: {
      std::string cond =
          expr.condition ? ExprToString(*expr.condition, module) : "0";
      std::string then_expr =
          expr.then_expr ? ExprToString(*expr.then_expr, module) : "0";
      std::string else_expr =
          expr.else_expr ? ExprToString(*expr.else_expr, module) : "0";
      return "(" + cond + " ? " + then_expr + " : " + else_expr + ")";
    }
    case gpga::ExprKind::kSelect: {
      std::string base = ExprToString(*expr.base, module);
      if (expr.indexed_range && expr.indexed_width > 0) {
        const gpga::Expr* start =
            expr.indexed_desc ? expr.msb_expr.get() : expr.lsb_expr.get();
        std::string start_expr =
            start ? ExprToString(*start, module)
                  : std::to_string(expr.indexed_desc ? expr.msb : expr.lsb);
        std::string op = expr.indexed_desc ? "-:" : "+:";
        return base + "[" + start_expr + " " + op + " " +
               std::to_string(expr.indexed_width) + "]";
      }
      if (expr.has_range) {
        return base + "[" + std::to_string(expr.msb) + ":" +
               std::to_string(expr.lsb) + "]";
      }
      return base + "[" + std::to_string(expr.msb) + "]";
    }
    case gpga::ExprKind::kIndex: {
      std::string base = ExprToString(*expr.base, module);
      std::string index = expr.index ? ExprToString(*expr.index, module) : "0";
      return base + "[" + index + "]";
    }
    case gpga::ExprKind::kCall: {
      std::string out = expr.ident + "(";
      for (size_t i = 0; i < expr.call_args.size(); ++i) {
        if (i > 0) {
          out += ", ";
        }
        out += ExprToString(*expr.call_args[i], module);
      }
      out += ")";
      return out;
    }
    case gpga::ExprKind::kConcat: {
      std::string inner;
      for (size_t i = 0; i < expr.elements.size(); ++i) {
        if (i > 0) {
          inner += ", ";
        }
        inner += ExprToString(*expr.elements[i], module);
      }
      if (expr.repeat > 1) {
        return "{" + std::to_string(expr.repeat) + "{" + inner + "}}";
      }
      return "{" + inner + "}";
    }
  }
  return "<expr>";
}

void DumpStatement(const gpga::Statement& stmt, const gpga::Module& module,
                   int indent, std::ostream& os) {
  std::string pad(static_cast<size_t>(indent), ' ');
  if (stmt.kind == gpga::StatementKind::kAssign) {
    if (stmt.assign.rhs) {
      std::string lhs = stmt.assign.lhs;
      if (stmt.assign.lhs_index) {
        lhs += "[" + ExprToString(*stmt.assign.lhs_index, module) + "]";
      }
      os << pad << lhs
         << (stmt.assign.nonblocking ? " <= " : " = ")
         << (stmt.assign.delay ? "#" + ExprToString(*stmt.assign.delay, module) + " "
                               : "")
         << ExprToString(*stmt.assign.rhs, module) << ";\n";
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kIf) {
    const gpga::Statement* current = &stmt;
    bool first = true;
    while (current) {
      std::string cond = current->condition
                             ? ExprToString(*current->condition, module)
                             : "0";
      if (first) {
        os << pad << "if (" << cond << ") {\n";
        first = false;
      } else {
        os << pad << "} else if (" << cond << ") {\n";
      }
      for (const auto& inner : current->then_branch) {
        DumpStatement(inner, module, indent + 2, os);
      }
      if (current->else_branch.empty()) {
        os << pad << "}\n";
        break;
      }
      if (current->else_branch.size() == 1 &&
          current->else_branch[0].kind == gpga::StatementKind::kIf) {
        current = &current->else_branch[0];
        continue;
      }
      os << pad << "} else {\n";
      for (const auto& inner : current->else_branch) {
        DumpStatement(inner, module, indent + 2, os);
      }
      os << pad << "}\n";
      break;
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kCase) {
    std::string expr = stmt.case_expr ? ExprToString(*stmt.case_expr, module)
                                      : "0";
    const char* case_name = "case";
    if (stmt.case_kind == gpga::CaseKind::kCaseZ) {
      case_name = "casez";
    } else if (stmt.case_kind == gpga::CaseKind::kCaseX) {
      case_name = "casex";
    }
    os << pad << case_name << " (" << expr << ")\n";
    for (const auto& item : stmt.case_items) {
      std::string labels;
      for (size_t i = 0; i < item.labels.size(); ++i) {
        if (i > 0) {
          labels += ", ";
        }
        labels += ExprToString(*item.labels[i], module);
      }
      os << pad << "  " << labels << ":\n";
      for (const auto& inner : item.body) {
        DumpStatement(inner, module, indent + 4, os);
      }
    }
    if (!stmt.default_branch.empty()) {
      os << pad << "  default:\n";
      for (const auto& inner : stmt.default_branch) {
        DumpStatement(inner, module, indent + 4, os);
      }
    }
    os << pad << "endcase\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kBlock) {
    os << pad << "begin";
    if (!stmt.block_label.empty()) {
      os << " : " << stmt.block_label;
    }
    os << "\n";
    for (const auto& inner : stmt.block) {
      DumpStatement(inner, module, indent + 2, os);
    }
    os << pad << "end";
    if (!stmt.block_label.empty()) {
      os << " : " << stmt.block_label;
    }
    os << "\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kDelay) {
    std::string delay =
        stmt.delay ? ExprToString(*stmt.delay, module) : "0";
    os << pad << "#" << delay;
    if (stmt.delay_body.empty()) {
      os << ";\n";
      return;
    }
    os << "\n";
    for (const auto& inner : stmt.delay_body) {
      DumpStatement(inner, module, indent + 2, os);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kEventControl) {
    if (!stmt.event_items.empty()) {
      os << pad << "@(";
      for (size_t i = 0; i < stmt.event_items.size(); ++i) {
        if (i > 0) {
          os << ", ";
        }
        if (stmt.event_items[i].edge == gpga::EventEdgeKind::kPosedge) {
          os << "posedge ";
        } else if (stmt.event_items[i].edge == gpga::EventEdgeKind::kNegedge) {
          os << "negedge ";
        }
        if (stmt.event_items[i].expr) {
          os << ExprToString(*stmt.event_items[i].expr, module);
        } else {
          os << "/*missing*/";
        }
      }
      os << ")";
    } else if (!stmt.event_expr) {
      os << pad << "@*";
    } else {
      std::string expr = ExprToString(*stmt.event_expr, module);
      os << pad << "@(";
      if (stmt.event_edge == gpga::EventEdgeKind::kPosedge) {
        os << "posedge ";
      } else if (stmt.event_edge == gpga::EventEdgeKind::kNegedge) {
        os << "negedge ";
      }
      os << expr << ")";
    }
    if (stmt.event_body.empty()) {
      os << ";\n";
      return;
    }
    os << "\n";
    for (const auto& inner : stmt.event_body) {
      DumpStatement(inner, module, indent + 2, os);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kEventTrigger) {
    os << pad << "-> " << stmt.trigger_target << ";\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWait) {
    std::string expr =
        stmt.wait_condition ? ExprToString(*stmt.wait_condition, module) : "0";
    os << pad << "wait (" << expr << ")";
    if (stmt.wait_body.empty()) {
      os << ";\n";
      return;
    }
    os << "\n";
    for (const auto& inner : stmt.wait_body) {
      DumpStatement(inner, module, indent + 2, os);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kForever) {
    os << pad << "forever\n";
    for (const auto& inner : stmt.forever_body) {
      DumpStatement(inner, module, indent + 2, os);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFork) {
    os << pad << "fork";
    if (!stmt.block_label.empty()) {
      os << " : " << stmt.block_label;
    }
    os << "\n";
    for (const auto& inner : stmt.fork_branches) {
      DumpStatement(inner, module, indent + 2, os);
    }
    os << pad << "join\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kDisable) {
    os << pad << "disable " << stmt.disable_target << ";\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kTaskCall) {
    os << pad << stmt.task_name << "(";
    for (size_t i = 0; i < stmt.task_args.size(); ++i) {
      if (i > 0) {
        os << ", ";
      }
      os << ExprToString(*stmt.task_args[i], module);
    }
    os << ");\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kForce) {
    if (stmt.assign.rhs) {
      std::string lhs =
          stmt.force_target.empty() ? stmt.assign.lhs : stmt.force_target;
      if (stmt.assign.lhs_index) {
        lhs += "[" + ExprToString(*stmt.assign.lhs_index, module) + "]";
      }
      if (stmt.is_procedural) {
        os << pad << "assign " << lhs << " = "
           << ExprToString(*stmt.assign.rhs, module) << ";\n";
      } else {
        os << pad << "force " << lhs << " = "
           << ExprToString(*stmt.assign.rhs, module) << ";\n";
      }
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kRelease) {
    std::string lhs =
        stmt.release_target.empty() ? stmt.assign.lhs : stmt.release_target;
    if (stmt.assign.lhs_index) {
      lhs += "[" + ExprToString(*stmt.assign.lhs_index, module) + "]";
    }
    if (stmt.is_procedural) {
      os << pad << "deassign " << lhs << ";\n";
    } else {
      os << pad << "release " << lhs << ";\n";
    }
    return;
  }
}

void DumpFlat(const gpga::ElaboratedDesign& design, std::ostream& os) {
  const auto& top = design.top;
  os << "Top: " << top.name << "\n";
  if (!top.parameters.empty()) {
    os << "Parameters:\n";
    for (const auto& param : top.parameters) {
      if (param.value) {
        os << "  - " << param.name << " = "
           << ExprToString(*param.value, top) << "\n";
      } else {
        os << "  - " << param.name << "\n";
      }
    }
  }
  os << "Ports:\n";
  for (const auto& port : top.ports) {
    os << "  - " << DirLabel(port.dir);
    if (port.is_signed) {
      os << " signed";
    }
    os << " " << port.name << " [" << port.width << "]\n";
  }
  os << "Nets:\n";
  for (const auto& net : top.nets) {
    const char* type = "wire";
    switch (net.type) {
      case gpga::NetType::kWire:
        type = "wire";
        break;
      case gpga::NetType::kReg:
        type = "reg";
        break;
      case gpga::NetType::kWand:
        type = "wand";
        break;
      case gpga::NetType::kWor:
        type = "wor";
        break;
      case gpga::NetType::kTri0:
        type = "tri0";
        break;
      case gpga::NetType::kTri1:
        type = "tri1";
        break;
      case gpga::NetType::kTriand:
        type = "triand";
        break;
      case gpga::NetType::kTrior:
        type = "trior";
        break;
      case gpga::NetType::kTrireg:
        type = "trireg";
        break;
      case gpga::NetType::kSupply0:
        type = "supply0";
        break;
      case gpga::NetType::kSupply1:
        type = "supply1";
        break;
    }
    os << "  - " << type;
    if (net.is_signed) {
      os << " signed";
    }
    os << " " << net.name << " [" << net.width << "]";
    if (net.array_size > 0) {
      os << " [" << net.array_size << "]";
    }
    os << "\n";
  }
  if (!top.events.empty()) {
    os << "Events:\n";
    for (const auto& evt : top.events) {
      os << "  - event " << evt.name << "\n";
    }
  }
  auto format_assign = [&](const gpga::Assign& assign) -> std::string {
    int lhs_width = SignalWidth(top, assign.lhs);
    std::string lhs = assign.lhs;
    if (assign.lhs_has_range) {
      int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
      int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
      lhs_width = hi - lo + 1;
      if (assign.lhs_msb == assign.lhs_lsb) {
        lhs += "[" + std::to_string(assign.lhs_msb) + "]";
      } else {
        lhs += "[" + std::to_string(assign.lhs_msb) + ":" +
               std::to_string(assign.lhs_lsb) + "]";
      }
    }
    const gpga::Expr* rhs_expr = assign.rhs.get();
    if (const gpga::Expr* simplified =
            SimplifyAssignMask(*assign.rhs, top, lhs_width)) {
      rhs_expr = simplified;
    }
    int rhs_width = ExprWidth(*rhs_expr, top);
    std::string rhs = ExprToString(*rhs_expr, top);
    if (rhs_width < lhs_width) {
      rhs = (ExprSigned(*rhs_expr, top) ? "sext(" : "zext(") + rhs + ", " +
            std::to_string(lhs_width) + ")";
    } else if (rhs_width > lhs_width) {
      rhs = "trunc(" + rhs + ", " + std::to_string(lhs_width) + ")";
    }
    return lhs + " = " + rhs;
  };
  auto format_proc_assign = [&](const gpga::Statement& stmt) -> std::string {
    std::string lhs = stmt.assign.lhs;
    for (const auto& idx : stmt.assign.lhs_indices) {
      if (idx) {
        lhs += "[" + ExprToString(*idx, top) + "]";
      }
    }
    if (stmt.assign.lhs_index) {
      lhs += "[" + ExprToString(*stmt.assign.lhs_index, top) + "]";
    } else if (stmt.assign.lhs_has_range) {
      if (stmt.assign.lhs_msb == stmt.assign.lhs_lsb) {
        lhs += "[" + std::to_string(stmt.assign.lhs_msb) + "]";
      } else {
        lhs += "[" + std::to_string(stmt.assign.lhs_msb) + ":" +
               std::to_string(stmt.assign.lhs_lsb) + "]";
      }
    }
    std::string rhs =
        stmt.assign.rhs ? ExprToString(*stmt.assign.rhs, top) : "<null>";
    std::string delay;
    if (stmt.assign.delay) {
      delay = "#" + ExprToString(*stmt.assign.delay, top) + " ";
    }
    return "procedural " + lhs + " = " + delay + rhs;
  };
  std::vector<const gpga::Assign*> explicit_assigns;
  std::vector<const gpga::Assign*> derived_assigns;
  std::vector<const gpga::Assign*> implicit_assigns;
  std::vector<const gpga::Assign*> inlined_assigns;
  for (const auto& assign : top.assigns) {
    if (!assign.rhs) {
      continue;
    }
    if (assign.is_derived) {
      derived_assigns.push_back(&assign);
    } else if (assign.origin_depth == 0 && !assign.is_implicit) {
      explicit_assigns.push_back(&assign);
    } else if (assign.is_implicit) {
      implicit_assigns.push_back(&assign);
    } else {
      inlined_assigns.push_back(&assign);
    }
  }
  std::vector<std::string> procedural_assigns;
  std::function<void(const gpga::Statement&)> collect_proc;
  collect_proc = [&](const gpga::Statement& stmt) {
    if (stmt.kind == gpga::StatementKind::kForce && stmt.is_procedural) {
      if (stmt.assign.rhs) {
        procedural_assigns.push_back(format_proc_assign(stmt));
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kIf) {
      for (const auto& inner : stmt.then_branch) {
        collect_proc(inner);
      }
      for (const auto& inner : stmt.else_branch) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kBlock) {
      for (const auto& inner : stmt.block) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kCase) {
      for (const auto& item : stmt.case_items) {
        for (const auto& inner : item.body) {
          collect_proc(inner);
        }
      }
      for (const auto& inner : stmt.default_branch) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kFor) {
      for (const auto& inner : stmt.for_body) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kWhile) {
      for (const auto& inner : stmt.while_body) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kRepeat) {
      for (const auto& inner : stmt.repeat_body) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kDelay) {
      for (const auto& inner : stmt.delay_body) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kEventControl) {
      for (const auto& inner : stmt.event_body) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kWait) {
      for (const auto& inner : stmt.wait_body) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kForever) {
      for (const auto& inner : stmt.forever_body) {
        collect_proc(inner);
      }
      return;
    }
    if (stmt.kind == gpga::StatementKind::kFork) {
      for (const auto& inner : stmt.fork_branches) {
        collect_proc(inner);
      }
    }
  };
  for (const auto& block : top.always_blocks) {
    if (block.origin_depth != 0 || block.is_synthesized) {
      continue;
    }
    for (const auto& stmt : block.statements) {
      collect_proc(stmt);
    }
  }
  os << "Assigns:\n";
  for (const auto* assign : explicit_assigns) {
    os << "  - " << format_assign(*assign) << "\n";
  }
  for (const auto& proc : procedural_assigns) {
    os << "  - " << proc << "\n";
  }
  if (!derived_assigns.empty()) {
    os << "Derived assigns:\n";
    for (const auto* assign : derived_assigns) {
      os << "  * " << format_assign(*assign) << "\n";
    }
  }
  if (!inlined_assigns.empty()) {
    os << "Inlined assigns:\n";
    for (const auto* assign : inlined_assigns) {
      os << "  * " << format_assign(*assign) << "\n";
    }
  }
  if (!implicit_assigns.empty()) {
    os << "Implicit assigns:\n";
    for (const auto* assign : implicit_assigns) {
      os << "  * " << format_assign(*assign) << "\n";
    }
  }
  os << "Switches:\n";
  for (const auto& sw : top.switches) {
    const char* kind = "tran";
    switch (sw.kind) {
      case gpga::SwitchKind::kTran:
        kind = "tran";
        break;
      case gpga::SwitchKind::kTranif1:
        kind = "tranif1";
        break;
      case gpga::SwitchKind::kTranif0:
        kind = "tranif0";
        break;
      case gpga::SwitchKind::kCmos:
        kind = "cmos";
        break;
    }
    os << "  - " << kind << " " << sw.a << " <-> " << sw.b;
    if (sw.control) {
      os << " (" << ExprToString(*sw.control, top) << ")";
    }
    if (sw.control_n) {
      os << " / (" << ExprToString(*sw.control_n, top) << ")";
    }
    os << "\n";
  }
  auto emit_block_header = [&](const gpga::AlwaysBlock& block,
                               const std::string& bullet) {
    if (block.edge == gpga::EdgeKind::kCombinational) {
      if (!block.sensitivity.empty() && block.sensitivity != "*") {
        os << bullet << "always @(" << block.sensitivity << ")\n";
      } else {
        os << bullet << "always @*\n";
      }
      return;
    }
    if (block.edge == gpga::EdgeKind::kInitial) {
      os << bullet << "initial\n";
      return;
    }
    if (!block.sensitivity.empty()) {
      os << bullet << "always @(" << block.sensitivity << ")\n";
    } else {
      os << bullet << "always @("
         << (block.edge == gpga::EdgeKind::kPosedge ? "posedge " : "negedge ")
         << block.clock << ")\n";
    }
  };
  auto emit_block = [&](const gpga::AlwaysBlock& block,
                        const std::string& bullet, bool split_decl_init) {
    if (split_decl_init && block.is_decl_init &&
        block.statements.size() > 1) {
      for (const auto& stmt : block.statements) {
        emit_block_header(block, bullet);
        DumpStatement(stmt, top, 4, os);
      }
      return;
    }
    emit_block_header(block, bullet);
    for (const auto& stmt : block.statements) {
      DumpStatement(stmt, top, 4, os);
    }
  };
  os << "Always blocks:\n";
  std::vector<const gpga::AlwaysBlock*> explicit_blocks;
  std::vector<const gpga::AlwaysBlock*> inlined_blocks;
  std::vector<const gpga::AlwaysBlock*> synthesized_blocks;
  for (const auto& block : top.always_blocks) {
    if (block.is_synthesized) {
      synthesized_blocks.push_back(&block);
    } else if (block.origin_depth != 0) {
      inlined_blocks.push_back(&block);
    } else {
      explicit_blocks.push_back(&block);
    }
  }
  for (const auto* block : explicit_blocks) {
    emit_block(*block, "  - ", true);
  }
  if (!inlined_blocks.empty()) {
    os << "Inlined always blocks:\n";
    for (const auto* block : inlined_blocks) {
      emit_block(*block, "  * ", false);
    }
  }
  if (!synthesized_blocks.empty()) {
    os << "Synthesized always blocks:\n";
    for (const auto* block : synthesized_blocks) {
      emit_block(*block, "  * ", false);
    }
  }
  os << "Flat name map:\n";
  if (design.flat_to_hier.empty()) {
    os << "  - __top__ -> " << top.name << "\n";
  } else {
    for (const auto& entry : design.flat_to_hier) {
      os << "  - " << entry.first << " -> " << entry.second << "\n";
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 2;
  }

  std::vector<std::string> input_paths;
  std::string msl_out;
  std::string host_out;
  std::string flat_out;
  std::string top_name;
  std::string sdf_path;
  bool dump_flat = false;
  bool enable_4state = false;
  bool sched_vm = false;
  bool auto_discover = false;
  bool strict_1364 = false;
  bool verbose_warnings = false;
  bool run = false;
  bool run_verbose = false;
  bool run_source_bindings = false;
  uint32_t run_count = 1u;
  uint32_t run_service_capacity = 32u;
  uint32_t run_max_steps = 1024u;
  uint32_t run_max_proc_steps = 64u;
  uint32_t run_dispatch_timeout_ms = 0u;
  std::string vcd_dir;
  uint32_t vcd_steps = 0u;
  std::vector<std::string> plusargs;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--emit-msl") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      msl_out = argv[++i];
    } else if (arg == "--emit-flat") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      flat_out = argv[++i];
    } else if (arg == "--dump-flat") {
      dump_flat = true;
    } else if (arg == "--top") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      top_name = argv[++i];
    } else if (arg == "--emit-host") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      host_out = argv[++i];
    } else if (arg == "--sdf") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      sdf_path = argv[++i];
    } else if (arg == "--4state") {
      enable_4state = true;
    } else if (arg == "--sched-vm") {
      sched_vm = true;
    } else if (arg == "--auto") {
      auto_discover = true;
    } else if (arg == "--strict-1364") {
      strict_1364 = true;
    } else if (arg == "--verbose" || arg == "-v") {
      verbose_warnings = true;
    } else if (arg == "--version") {
      PrintVersion();
      return 0;
    } else if (arg == "--run") {
      run = true;
    } else if (arg == "--run-verbose") {
      run_verbose = true;
    } else if (arg == "--source-bindings") {
      run_source_bindings = true;
    } else if (arg == "--count") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_count = static_cast<uint32_t>(std::stoul(argv[++i]));
      if (run_count == 0u) {
        run_count = 1u;
      }
    } else if (arg == "--service-capacity") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_service_capacity = static_cast<uint32_t>(std::stoul(argv[++i]));
      if (run_service_capacity == 0u) {
        run_service_capacity = 1u;
      }
    } else if (arg == "--max-steps") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_max_steps = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--max-proc-steps") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_max_proc_steps = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--dispatch-timeout-ms") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_dispatch_timeout_ms =
          static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--vcd-dir" || arg == "--vcr-dir") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      vcd_dir = argv[++i];
    } else if (arg == "--vcd-steps" || arg == "--vcr-steps") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      vcd_steps = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (!arg.empty() && arg[0] == '+') {
      plusargs.push_back(arg.substr(1));
    } else if (!arg.empty() && arg[0] == '-') {
      PrintUsage(argv[0]);
      return 2;
    } else {
      input_paths.push_back(arg);
    }
  }

  if (input_paths.empty()) {
    PrintUsage(argv[0]);
    return 2;
  }

  gpga::Diagnostics diagnostics;
  gpga::Program program;
  program.modules.clear();
  gpga::ParseOptions parse_options;
  parse_options.enable_4state = enable_4state;
  parse_options.strict_1364 = strict_1364;
  std::unordered_set<size_t> explicit_module_indices;
  std::unordered_set<std::string> active_module_names;
  bool have_active_modules = false;

  struct ParseItem {
    std::string path;
    bool explicit_input = false;
  };

  std::vector<ParseItem> parse_queue;
  parse_queue.reserve(input_paths.size());
  std::unordered_map<std::string, size_t> seen_paths;
  auto add_path = [&](const std::string& path, bool explicit_input) {
    std::error_code ec;
    std::filesystem::path fs_path(path);
    std::filesystem::path normalized =
        std::filesystem::weakly_canonical(fs_path, ec);
    std::string key = ec ? fs_path.lexically_normal().string()
                         : normalized.string();
    auto it = seen_paths.find(key);
    if (it == seen_paths.end()) {
      seen_paths[key] = parse_queue.size();
      parse_queue.push_back(ParseItem{path, explicit_input});
      return;
    }
    if (explicit_input) {
      parse_queue[it->second].explicit_input = true;
    }
  };
  for (const auto& path : input_paths) {
    add_path(path, true);
  }
  if (auto_discover) {
    for (const auto& path : input_paths) {
      std::error_code ec;
      std::filesystem::path root = std::filesystem::path(path).parent_path();
      if (root.empty()) {
        root = ".";
      }
      std::vector<std::string> discovered;
      for (auto it =
               std::filesystem::recursive_directory_iterator(root, ec);
           it != std::filesystem::recursive_directory_iterator();
           it.increment(ec)) {
        if (ec) {
          break;
        }
        if (!it->is_regular_file()) {
          continue;
        }
        if (it->path().extension() == ".v") {
          discovered.push_back(it->path().string());
        }
      }
      std::sort(discovered.begin(), discovered.end());
      for (const auto& candidate : discovered) {
        add_path(candidate, false);
      }
    }
  }

  for (const auto& item : parse_queue) {
    if (item.explicit_input) {
      size_t before_count = program.modules.size();
      if (!gpga::ParseVerilogFile(item.path, &program, &diagnostics,
                                  parse_options)) {
        diagnostics.RenderTo(std::cerr);
        return 1;
      }
      if (diagnostics.HasErrors()) {
        diagnostics.RenderTo(std::cerr);
        return 1;
      }
      size_t after_count = program.modules.size();
      for (size_t i = before_count; i < after_count; ++i) {
        explicit_module_indices.insert(i);
      }
      continue;
    }
    gpga::Program temp_program;
    gpga::Diagnostics temp_diag;
    if (!gpga::ParseVerilogFile(item.path, &temp_program, &temp_diag,
                                parse_options)) {
      continue;
    }
    if (temp_diag.HasErrors()) {
      continue;
    }
    for (auto& module : temp_program.modules) {
      program.modules.push_back(std::move(module));
    }
  }

  if (auto_discover && !explicit_module_indices.empty()) {
    std::unordered_map<std::string, std::vector<std::string>> graph;
    graph.reserve(program.modules.size());
    for (const auto& module : program.modules) {
      auto& edges = graph[module.name];
      for (const auto& instance : module.instances) {
        edges.push_back(instance.module_name);
      }
    }
    active_module_names.clear();
    active_module_names.reserve(program.modules.size());
    have_active_modules = true;
    std::vector<std::string> stack;
    for (size_t idx : explicit_module_indices) {
      if (idx >= program.modules.size()) {
        continue;
      }
      const std::string& name = program.modules[idx].name;
      if (active_module_names.insert(name).second) {
        stack.push_back(name);
      }
    }
    while (!stack.empty()) {
      std::string name = std::move(stack.back());
      stack.pop_back();
      auto it = graph.find(name);
      if (it == graph.end()) {
        continue;
      }
      for (const auto& child : it->second) {
        if (active_module_names.insert(child).second) {
          stack.push_back(child);
        }
      }
    }
    for (auto& module : program.modules) {
      if (active_module_names.count(module.name) == 0) {
        module.defparams.clear();
      }
    }
  }

  if (!sdf_path.empty()) {
    std::vector<SdfTimingCheck> sdf_checks;
    if (!LoadSdfTimingChecks(sdf_path, &sdf_checks, &diagnostics)) {
      diagnostics.RenderTo(std::cerr);
      return 1;
    }
    ApplySdfTimingChecks(sdf_path, sdf_checks, &program, &diagnostics);
    if (diagnostics.HasErrors()) {
      diagnostics.RenderTo(std::cerr);
      return 1;
    }
  }

  gpga::ElaboratedDesign design;
  bool elaborated = false;
  if (top_name.empty() && auto_discover &&
      !explicit_module_indices.empty()) {
    auto is_active = [&](const std::string& name) -> bool {
      return !have_active_modules || active_module_names.count(name) > 0;
    };
    std::unordered_set<std::string> instantiated;
    for (const auto& module : program.modules) {
      if (!is_active(module.name)) {
        continue;
      }
      for (const auto& instance : module.instances) {
        if (is_active(instance.module_name)) {
          instantiated.insert(instance.module_name);
        }
      }
    }
    std::vector<const gpga::Module*> roots;
    for (size_t i = 0; i < program.modules.size(); ++i) {
      if (explicit_module_indices.count(i) == 0) {
        continue;
      }
      const auto& module = program.modules[i];
      if (!is_active(module.name)) {
        continue;
      }
      if (instantiated.count(module.name) == 0) {
        roots.push_back(&module);
      }
    }
    if (!roots.empty()) {
      auto has_initial = [](const gpga::Module& module) -> bool {
        for (const auto& block : module.always_blocks) {
          if (block.edge == gpga::EdgeKind::kInitial) {
            return true;
          }
        }
        return false;
      };
      auto is_test = [](const std::string& name) -> bool {
        return name.rfind("test_", 0) == 0;
      };
      const gpga::Module* chosen = nullptr;
      for (const auto* module : roots) {
        if (has_initial(*module) && is_test(module->name)) {
          chosen = module;
          break;
        }
      }
      if (!chosen) {
        for (const auto* module : roots) {
          if (has_initial(*module)) {
            chosen = module;
            break;
          }
        }
      }
      if (!chosen) {
        for (const auto* module : roots) {
          if (is_test(module->name)) {
            chosen = module;
            break;
          }
        }
      }
      if (!chosen) {
        chosen = roots.front();
      }
      if (roots.size() > 1) {
        diagnostics.Add(gpga::Severity::kWarning,
                        "multiple top-level modules found; using '" +
                            chosen->name +
                            "' (use --top <name> to override)");
      }
      top_name = chosen->name;
    }
  }
  if (!top_name.empty()) {
    elaborated =
        gpga::Elaborate(program, top_name, &design, &diagnostics,
                        enable_4state, verbose_warnings);
  } else {
    elaborated =
        gpga::Elaborate(program, &design, &diagnostics, enable_4state,
                        verbose_warnings);
  }
  if (!elaborated || diagnostics.HasErrors()) {
    diagnostics.RenderTo(std::cerr);
    return 1;
  }
  if (!diagnostics.Items().empty()) {
    diagnostics.RenderTo(std::cerr);
  }

  std::string msl;
  if (!msl_out.empty() || run) {
    gpga::MslEmitOptions msl_options;
    msl_options.four_state = enable_4state;
    msl_options.sched_vm = sched_vm;
    msl = gpga::EmitMSLStub(design.top, msl_options);
    if (!msl_out.empty()) {
      if (!WriteFile(msl_out, msl, &diagnostics)) {
        diagnostics.RenderTo(std::cerr);
        return 1;
      }
    }
  }

  if (!host_out.empty()) {
    std::string host = gpga::EmitHostStub(design.top);
    if (!WriteFile(host_out, host, &diagnostics)) {
      diagnostics.RenderTo(std::cerr);
      return 1;
    }
  }

  if (run) {
    std::string error;
    if (!RunMetal(design.top, msl, design.flat_to_hier, enable_4state,
                  run_count,
                  run_service_capacity, run_max_steps, run_max_proc_steps,
                  run_dispatch_timeout_ms, run_verbose, run_source_bindings,
                  vcd_dir, vcd_steps, plusargs, &error)) {
      std::cerr << "Run failed: " << error << "\n";
      return 1;
    }
  }

  if (msl_out.empty() && host_out.empty() && !run) {
    std::cout << "Elaborated top module '" << design.top.name
              << "'. Use --emit-msl/--emit-host to write stubs.\n";
  }
  if (dump_flat || !flat_out.empty()) {
    std::ostringstream os;
    DumpFlat(design, os);
    const std::string flat_text = os.str();
    if (dump_flat) {
      std::cout << flat_text;
    }
    if (!flat_out.empty()) {
      if (!WriteFile(flat_out, flat_text, &diagnostics)) {
        diagnostics.RenderTo(std::cerr);
        return 1;
      }
    }
  }

  return 0;
}
