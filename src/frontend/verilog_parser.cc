#include "frontend/verilog_parser.hh"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gpga {

namespace {

enum class TokenKind {
  kIdentifier,
  kNumber,
  kString,
  kSymbol,
  kEnd,
};

struct Token {
  TokenKind kind = TokenKind::kEnd;
  std::string text;
  int line = 1;
  int column = 1;
};

bool IsIdentStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool IsIdentChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::vector<Token> Tokenize(const std::string& text) {
  std::vector<Token> tokens;
  size_t i = 0;
  int line = 1;
  int column = 1;
  auto push = [&](TokenKind kind, std::string value, int token_line,
                  int token_column) {
    Token token;
    token.kind = kind;
    token.text = std::move(value);
    token.line = token_line;
    token.column = token_column;
    tokens.push_back(std::move(token));
  };

  while (i < text.size()) {
    char c = text[i];
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (c == '\n') {
        ++line;
        column = 1;
      } else {
        ++column;
      }
      ++i;
      continue;
    }
    if (c == '/' && i + 1 < text.size()) {
      char next = text[i + 1];
      if (next == '/') {
        i += 2;
        column += 2;
        while (i < text.size() && text[i] != '\n') {
          ++i;
          ++column;
        }
        continue;
      }
      if (next == '*') {
        i += 2;
        column += 2;
        while (i + 1 < text.size()) {
          if (text[i] == '*' && text[i + 1] == '/') {
            i += 2;
            column += 2;
            break;
          }
          if (text[i] == '\n') {
            ++line;
            column = 1;
            ++i;
            continue;
          }
          ++i;
          ++column;
        }
        continue;
      }
    }
    if (c == '(' && i + 1 < text.size() && text[i + 1] == '*') {
      size_t lookahead = i + 2;
      while (lookahead < text.size() &&
             std::isspace(static_cast<unsigned char>(text[lookahead]))) {
        if (text[lookahead] == '\n') {
          break;
        }
        ++lookahead;
      }
      if (lookahead < text.size() && text[lookahead] != ')') {
        i += 2;
        column += 2;
        while (i + 1 < text.size()) {
          if (text[i] == '*' && text[i + 1] == ')') {
            i += 2;
            column += 2;
            break;
          }
          if (text[i] == '\n') {
            ++line;
            column = 1;
            ++i;
            continue;
          }
          ++i;
          ++column;
        }
        continue;
      }
    }
    if (c == '"') {
      int token_line = line;
      int token_column = column;
      ++i;
      ++column;
      std::string value;
      while (i < text.size()) {
        char ch = text[i];
        if (ch == '"') {
          ++i;
          ++column;
          break;
        }
        if (ch == '\\' && i + 1 < text.size()) {
          char esc = text[i + 1];
          switch (esc) {
            case 'n':
              value.push_back('\n');
              break;
            case 't':
              value.push_back('\t');
              break;
            case 'r':
              value.push_back('\r');
              break;
            case '"':
              value.push_back('"');
              break;
            case '\\':
              value.push_back('\\');
              break;
            default:
              value.push_back(esc);
              break;
          }
          i += 2;
          column += 2;
          continue;
        }
        if (ch == '\n') {
          ++line;
          column = 1;
        } else {
          ++column;
        }
        value.push_back(ch);
        ++i;
      }
      push(TokenKind::kString, value, token_line, token_column);
      continue;
    }
    if (IsIdentStart(c)) {
      int token_line = line;
      int token_column = column;
      size_t start = i;
      ++i;
      ++column;
      while (i < text.size() && IsIdentChar(text[i])) {
        ++i;
        ++column;
      }
      push(TokenKind::kIdentifier, text.substr(start, i - start), token_line,
           token_column);
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '.' && i + 1 < text.size() &&
         std::isdigit(static_cast<unsigned char>(text[i + 1])))) {
      int token_line = line;
      int token_column = column;
      size_t start = i;
      bool has_dot = false;
      bool has_exp = false;
      if (c == '.') {
        has_dot = true;
        ++i;
        ++column;
      } else {
        ++i;
        ++column;
        while (i < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[i]))) {
          ++i;
          ++column;
        }
        if (i < text.size() && text[i] == '.') {
          has_dot = true;
          ++i;
          ++column;
        }
      }
      if (has_dot) {
        while (i < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[i]))) {
          ++i;
          ++column;
        }
      }
      if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        has_exp = true;
        ++i;
        ++column;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
          ++i;
          ++column;
        }
        while (i < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[i]))) {
          ++i;
          ++column;
        }
      }
      push(TokenKind::kNumber, text.substr(start, i - start), token_line,
           token_column);
      continue;
    }

    if ((c == '+' || c == '-') && i + 1 < text.size() && text[i + 1] == ':') {
      int token_line = line;
      int token_column = column;
      std::string sym;
      sym.push_back(c);
      sym.push_back(':');
      push(TokenKind::kSymbol, sym, token_line, token_column);
      i += 2;
      column += 2;
      continue;
    }
    if (c == '-' && i + 1 < text.size() && text[i + 1] == '>') {
      int token_line = line;
      int token_column = column;
      push(TokenKind::kSymbol, "->", token_line, token_column);
      i += 2;
      column += 2;
      continue;
    }

    int token_line = line;
    int token_column = column;
    push(TokenKind::kSymbol, std::string(1, c), token_line, token_column);
    ++i;
    ++column;
  }

  tokens.push_back(Token{TokenKind::kEnd, "", line, column});
  return tokens;
}

bool ExpandDefines(const std::string& line,
                   const std::unordered_map<std::string, std::string>& defines,
                   const std::string& path, int line_number,
                   Diagnostics* diagnostics, std::string* out_line) {
  if (!out_line) {
    return false;
  }
  std::string result;
  result.reserve(line.size());
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] != '`') {
      result.push_back(line[i]);
      continue;
    }
    size_t start = i + 1;
    if (start >= line.size() || !IsIdentStart(line[start])) {
      if (diagnostics) {
        diagnostics->Add(Severity::kError,
                         "expected macro name after '`'",
                         SourceLocation{path, line_number,
                                        static_cast<int>(i + 1)});
      }
      return false;
    }
    size_t end = start + 1;
    while (end < line.size() && IsIdentChar(line[end])) {
      ++end;
    }
    std::string name = line.substr(start, end - start);
    auto it = defines.find(name);
    if (it == defines.end()) {
      if (diagnostics) {
        diagnostics->Add(Severity::kError,
                         "undefined macro '" + name + "'",
                         SourceLocation{path, line_number,
                                        static_cast<int>(i + 1)});
      }
      return false;
    }
    result += it->second;
    i = end - 1;
  }
  *out_line = std::move(result);
  return true;
}

struct IfdefState {
  bool parent_active = true;
  bool condition_true = false;
  bool else_seen = false;
  bool active = true;
};

enum class DirectiveKind {
  kDefaultNettype,
  kUnconnectedDrive,
  kNoUnconnectedDrive,
  kResetAll,
  kTimescale,
};

struct DirectiveEvent {
  DirectiveKind kind = DirectiveKind::kDefaultNettype;
  std::string arg;
  int line = 1;
  int column = 1;
};

bool PreprocessVerilogInternal(
    const std::string& input, const std::string& path, Diagnostics* diagnostics,
    std::unordered_map<std::string, std::string>* defines,
    std::string* out_text, int depth,
    std::vector<DirectiveEvent>* directives) {
  if (!out_text || !defines) {
    return false;
  }
  if (depth > 32) {
    diagnostics->Add(Severity::kError,
                     "include depth exceeded",
                     SourceLocation{path});
    return false;
  }
  std::vector<IfdefState> if_stack;
  std::istringstream stream(input);
  std::ostringstream output;
  std::string line;
  int line_number = 1;
  while (std::getline(stream, line)) {
    size_t first = line.find_first_not_of(" \t");
    if (first != std::string::npos && line[first] == '`') {
      size_t pos = first + 1;
      while (pos < line.size() &&
             std::isspace(static_cast<unsigned char>(line[pos]))) {
        ++pos;
      }
      size_t start = pos;
      while (pos < line.size() && IsIdentChar(line[pos])) {
        ++pos;
      }
      std::string directive = line.substr(start, pos - start);
      bool active = if_stack.empty() ? true : if_stack.back().active;
      if (directive == "define") {
        if (active) {
          while (pos < line.size() &&
                 std::isspace(static_cast<unsigned char>(line[pos]))) {
            ++pos;
          }
          size_t name_start = pos;
          if (name_start >= line.size() || !IsIdentStart(line[name_start])) {
            diagnostics->Add(Severity::kError,
                             "expected macro name after `define",
                             SourceLocation{path, line_number,
                                            static_cast<int>(name_start + 1)});
            return false;
          }
          size_t name_end = name_start + 1;
          while (name_end < line.size() && IsIdentChar(line[name_end])) {
            ++name_end;
          }
          std::string name = line.substr(name_start, name_end - name_start);
          size_t value_start = line.find_first_not_of(" \t", name_end);
          std::string value = (value_start == std::string::npos)
                                  ? ""
                                  : line.substr(value_start);
          (*defines)[name] = value;
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "undef") {
        if (active) {
          while (pos < line.size() &&
                 std::isspace(static_cast<unsigned char>(line[pos]))) {
            ++pos;
          }
          size_t name_start = pos;
          if (name_start >= line.size() || !IsIdentStart(line[name_start])) {
            diagnostics->Add(Severity::kError,
                             "expected macro name after `undef",
                             SourceLocation{path, line_number,
                                            static_cast<int>(name_start + 1)});
            return false;
          }
          size_t name_end = name_start + 1;
          while (name_end < line.size() && IsIdentChar(line[name_end])) {
            ++name_end;
          }
          std::string name = line.substr(name_start, name_end - name_start);
          defines->erase(name);
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "ifdef" || directive == "ifndef") {
        while (pos < line.size() &&
               std::isspace(static_cast<unsigned char>(line[pos]))) {
          ++pos;
        }
        size_t name_start = pos;
        if (name_start >= line.size() || !IsIdentStart(line[name_start])) {
          diagnostics->Add(Severity::kError,
                           "expected macro name after `" + directive + "'",
                           SourceLocation{path, line_number,
                                          static_cast<int>(name_start + 1)});
          return false;
        }
        size_t name_end = name_start + 1;
        while (name_end < line.size() && IsIdentChar(line[name_end])) {
          ++name_end;
        }
        std::string name = line.substr(name_start, name_end - name_start);
        bool defined = defines->find(name) != defines->end();
        bool condition_true = (directive == "ifdef") ? defined : !defined;
        IfdefState state;
        state.parent_active = active;
        state.condition_true = condition_true;
        state.active = active && condition_true;
        if_stack.push_back(state);
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "else") {
        if (if_stack.empty()) {
          diagnostics->Add(Severity::kError,
                           "unexpected `else without `ifdef",
                           SourceLocation{path, line_number,
                                          static_cast<int>(first + 1)});
          return false;
        }
        IfdefState& state = if_stack.back();
        if (state.else_seen) {
          diagnostics->Add(Severity::kError,
                           "duplicate `else in conditional block",
                           SourceLocation{path, line_number,
                                          static_cast<int>(first + 1)});
          return false;
        }
        state.else_seen = true;
        state.active = state.parent_active && !state.condition_true;
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "endif") {
        if (if_stack.empty()) {
          diagnostics->Add(Severity::kError,
                           "unexpected `endif without `ifdef",
                           SourceLocation{path, line_number,
                                          static_cast<int>(first + 1)});
          return false;
        }
        if_stack.pop_back();
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "include") {
        if (!active) {
          output << "\n";
          ++line_number;
          continue;
        }
        while (pos < line.size() &&
               std::isspace(static_cast<unsigned char>(line[pos]))) {
          ++pos;
        }
        if (pos >= line.size() ||
            (line[pos] != '"' && line[pos] != '<')) {
          diagnostics->Add(Severity::kError,
                           "expected quoted path after `include",
                           SourceLocation{path, line_number,
                                          static_cast<int>(pos + 1)});
          return false;
        }
        char term = (line[pos] == '"') ? '"' : '>';
        size_t path_start = pos + 1;
        size_t path_end = line.find(term, path_start);
        if (path_end == std::string::npos) {
          diagnostics->Add(Severity::kError,
                           "unterminated `include path",
                           SourceLocation{path, line_number,
                                          static_cast<int>(pos + 1)});
          return false;
        }
        std::string include_raw = line.substr(path_start, path_end - path_start);
        std::filesystem::path include_path(include_raw);
        if (include_path.is_relative()) {
          include_path = std::filesystem::path(path).parent_path() / include_path;
        }
        std::ifstream include_file(include_path);
        if (!include_file) {
          diagnostics->Add(Severity::kError,
                           "failed to open include file",
                           SourceLocation{path, line_number,
                                          static_cast<int>(pos + 1)});
          return false;
        }
        std::ostringstream include_buffer;
        include_buffer << include_file.rdbuf();
        std::string include_text = include_buffer.str();
        std::string included_out;
        if (!PreprocessVerilogInternal(include_text, include_path.string(),
                                       diagnostics, defines, &included_out,
                                       depth + 1, directives)) {
          return false;
        }
        output << included_out;
        if (!included_out.empty() && included_out.back() != '\n') {
          output << "\n";
        }
        ++line_number;
        continue;
      }
      if (directive == "timescale") {
        if (active && directives) {
          size_t arg_pos = line.find_first_not_of(" \t", pos);
          if (arg_pos != std::string::npos) {
            size_t arg_end = arg_pos;
            while (arg_end < line.size() &&
                   !std::isspace(static_cast<unsigned char>(line[arg_end])) &&
                   line[arg_end] != '/') {
              ++arg_end;
            }
            if (arg_end > arg_pos) {
              directives->push_back(DirectiveEvent{
                  DirectiveKind::kTimescale,
                  line.substr(arg_pos, arg_end - arg_pos), line_number,
                  static_cast<int>(first + 1)});
            }
          }
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "celldefine" || directive == "endcelldefine" ||
          directive == "protect" || directive == "endprotect" ||
          directive == "delay_mode_path" || directive == "delay_mode_unit" ||
          directive == "delay_mode_distributed") {
        output << "\n";
        ++line_number;
        continue;
      }
      auto parse_directive_arg = [&](std::string* out) -> bool {
        size_t arg_pos = line.find_first_not_of(" \t", pos);
        if (arg_pos == std::string::npos ||
            !IsIdentStart(line[arg_pos])) {
          diagnostics->Add(Severity::kError,
                           "expected argument after `" + directive + "'",
                           SourceLocation{path, line_number,
                                          static_cast<int>(pos + 1)});
          return false;
        }
        size_t arg_end = arg_pos + 1;
        while (arg_end < line.size() && IsIdentChar(line[arg_end])) {
          ++arg_end;
        }
        *out = line.substr(arg_pos, arg_end - arg_pos);
        return true;
      };
      if (directive == "default_nettype") {
        if (active) {
          std::string arg;
          if (!parse_directive_arg(&arg)) {
            return false;
          }
          if (directives) {
            directives->push_back(DirectiveEvent{
                DirectiveKind::kDefaultNettype, arg, line_number,
                static_cast<int>(first + 1)});
          }
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "unconnected_drive") {
        if (active) {
          std::string arg;
          if (!parse_directive_arg(&arg)) {
            return false;
          }
          if (directives) {
            directives->push_back(DirectiveEvent{
                DirectiveKind::kUnconnectedDrive, arg, line_number,
                static_cast<int>(first + 1)});
          }
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "nounconnected_drive") {
        if (active && directives) {
          directives->push_back(DirectiveEvent{DirectiveKind::kNoUnconnectedDrive,
                                               "", line_number,
                                               static_cast<int>(first + 1)});
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "resetall") {
        if (active && directives) {
          directives->push_back(
              DirectiveEvent{DirectiveKind::kResetAll, "", line_number,
                             static_cast<int>(first + 1)});
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (!directive.empty()) {
        diagnostics->Add(Severity::kError,
                         "unsupported compiler directive `" + directive + "'",
                         SourceLocation{path, line_number,
                                        static_cast<int>(first + 1)});
      } else {
        diagnostics->Add(Severity::kError,
                         "unsupported compiler directive",
                         SourceLocation{path, line_number,
                                        static_cast<int>(first + 1)});
      }
      return false;
    }
    bool active = if_stack.empty() ? true : if_stack.back().active;
    if (!active) {
      output << "\n";
      ++line_number;
      continue;
    }
    auto strip_line_comment = [](const std::string& input) -> std::string {
      std::string out;
      out.reserve(input.size());
      bool in_string = false;
      for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '"' && (i == 0 || input[i - 1] != '\\')) {
          in_string = !in_string;
          out.push_back(c);
          continue;
        }
        if (!in_string && c == '/' && i + 1 < input.size() &&
            input[i + 1] == '/') {
          break;
        }
        out.push_back(c);
      }
      return out;
    };
    std::string line_for_expand = strip_line_comment(line);
    std::string expanded;
    if (!ExpandDefines(line_for_expand, *defines, path, line_number, diagnostics,
                       &expanded)) {
      return false;
    }
    output << expanded;
    if (!stream.eof()) {
      output << "\n";
    }
    ++line_number;
  }
  if (!if_stack.empty()) {
    diagnostics->Add(Severity::kError,
                     "unterminated `ifdef block",
                     SourceLocation{path, line_number});
    return false;
  }
  *out_text = output.str();
  return true;
}

bool PreprocessVerilog(const std::string& input, const std::string& path,
                       Diagnostics* diagnostics, std::string* out_text,
                       std::vector<DirectiveEvent>* directives) {
  std::unordered_map<std::string, std::string> defines;
  return PreprocessVerilogInternal(input, path, diagnostics, &defines,
                                   out_text, 0, directives);
}

class Parser {
 public:
  Parser(std::string path, std::vector<Token> tokens, Diagnostics* diagnostics,
         const ParseOptions& options,
         std::vector<DirectiveEvent> directives)
      : path_(std::move(path)),
        tokens_(std::move(tokens)),
        diagnostics_(diagnostics),
        options_(options),
        directives_(std::move(directives)) {}

  bool ParseProgram(Program* out_program) {
    while (!IsAtEnd()) {
      if (!ApplyDirectivesUpTo(Peek().line)) {
        return false;
      }
      if (MatchKeyword("module")) {
        if (!ParseModule(out_program)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("primitive")) {
        if (!ParsePrimitive(out_program)) {
          return false;
        }
        continue;
      }
      const Token& token = Peek();
      diagnostics_->Add(Severity::kError,
                        "unexpected token '" + token.text + "'",
                        SourceLocation{path_, token.line, token.column});
      return false;
    }
    return true;
  }

 private:
  struct GeneratedNetDecl {
    NetType type = NetType::kWire;
    std::string name;
    int width = 1;
    bool is_signed = false;
    ChargeStrength charge = ChargeStrength::kNone;
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    std::vector<ArrayDim> array_dims;
  };

  struct GenerateAssign {
    std::string lhs;
    bool lhs_has_range = false;
    bool lhs_is_range = false;
    std::unique_ptr<Expr> lhs_msb_expr;
    std::unique_ptr<Expr> lhs_lsb_expr;
    std::unique_ptr<Expr> rhs;
    Strength strength0 = Strength::kStrong;
    Strength strength1 = Strength::kStrong;
    bool has_strength = false;
  };

  struct GateAssign {
    std::string lhs;
    std::vector<std::unique_ptr<Expr>> lhs_indices;
    bool lhs_has_range = false;
    bool lhs_is_range = false;
    int lhs_msb = 0;
    int lhs_lsb = 0;
    std::unique_ptr<Expr> lhs_msb_expr;
    std::unique_ptr<Expr> lhs_lsb_expr;
    std::unique_ptr<Expr> rhs;
    Strength strength0 = Strength::kStrong;
    Strength strength1 = Strength::kStrong;
    bool has_strength = false;
  };

  struct GenerateLocalparam {
    std::string name;
    std::unique_ptr<Expr> expr;
  };

  struct GenerateBlock;

  struct GenerateFor {
    std::string var;
    std::unique_ptr<Expr> init_expr;
    std::unique_ptr<Expr> cond_expr;
    std::unique_ptr<Expr> step_expr;
    std::unique_ptr<GenerateBlock> body;
    int id = 0;
  };

  struct GenerateIf {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<GenerateBlock> then_block;
    bool has_else = false;
    std::unique_ptr<GenerateBlock> else_block;
  };

  struct GenerateCaseItem {
    std::vector<std::unique_ptr<Expr>> labels;
    std::unique_ptr<GenerateBlock> body;
  };

  struct GenerateCase {
    CaseKind kind = CaseKind::kCase;
    std::unique_ptr<Expr> expr;
    std::vector<GenerateCaseItem> items;
    std::unique_ptr<GenerateBlock> default_block;
  };

  struct GenerateItem {
    enum class Kind {
      kNet,
      kAssign,
      kInstance,
      kAlways,
      kInitial,
      kLocalparam,
      kFor,
      kIf,
      kCase,
      kBlock,
    };
    Kind kind = Kind::kNet;
    GeneratedNetDecl net;
    GenerateAssign assign;
    Instance instance;
    AlwaysBlock always_block;
    GenerateLocalparam localparam;
    GenerateFor gen_for;
    GenerateIf gen_if;
    GenerateCase gen_case;
    std::unique_ptr<GenerateBlock> block;
  };

  struct GenerateBlock {
    std::string label;
    std::vector<GenerateItem> items;
  };

  struct UdpPattern {
    bool is_edge = false;
    char value = '?';
    char prev = '?';
    char curr = '?';
  };

  struct UdpRow {
    std::vector<UdpPattern> inputs;
    bool has_current = false;
    char current = '?';
    char output = '?';
  };

  struct UdpInfo {
    std::string name;
    std::string output;
    bool output_is_reg = false;
    int output_width = 1;
    std::vector<std::string> inputs;
    std::vector<int> input_widths;
    std::vector<bool> input_has_edge;
    bool sequential = false;
    std::vector<UdpRow> rows;
  };

  const Token& Peek() const { return tokens_[pos_]; }
  const Token& Peek(size_t lookahead) const {
    size_t index = pos_ + lookahead;
    if (index >= tokens_.size()) {
      return tokens_.back();
    }
    return tokens_[index];
  }
  const Token& Previous() const { return tokens_[pos_ - 1]; }

  bool IsAtEnd() const { return Peek().kind == TokenKind::kEnd; }

  bool MatchSymbol(const char* symbol) {
    if (Peek().kind == TokenKind::kSymbol && Peek().text == symbol) {
      Advance();
      return true;
    }
    return false;
  }

  bool MatchKeyword(const char* keyword) {
    if (Peek().kind == TokenKind::kIdentifier && Peek().text == keyword) {
      Advance();
      return true;
    }
    return false;
  }

  bool ConsumeIdentifier(std::string* out) {
    if (Peek().kind == TokenKind::kIdentifier) {
      *out = Peek().text;
      Advance();
      return true;
    }
    return false;
  }

  bool ConsumeHierIdentifier(std::string* out) {
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      return false;
    }
    while (true) {
      if (Peek().kind == TokenKind::kSymbol && Peek().text == "[") {
        if (Peek(1).kind == TokenKind::kNumber &&
            Peek(2).kind == TokenKind::kSymbol && Peek(2).text == "]" &&
            Peek(3).kind == TokenKind::kSymbol && Peek(3).text == ".") {
          Advance();
          std::string index = Peek().text;
          Advance();
          Advance();
          name += "__";
          name += index;
        }
      }
      if (!MatchSymbol(".")) {
        break;
      }
      std::string part;
      if (!ConsumeIdentifier(&part)) {
        ErrorHere("expected identifier after '.'");
        return false;
      }
      name += ".";
      name += part;
    }
    *out = name;
    return true;
  }

  bool ConsumeNumber(int* out) {
    if (Peek().kind == TokenKind::kNumber) {
      *out = std::stoi(Peek().text);
      Advance();
      return true;
    }
    return false;
  }

  void Advance() {
    if (!IsAtEnd()) {
      ++pos_;
    }
  }

  bool ParseModule(Program* program) {
    std::string module_name;
    if (!ConsumeIdentifier(&module_name)) {
      ErrorHere("expected module name after 'module'");
      return false;
    }
    Module module;
    module.name = module_name;
    module.timescale = current_timescale_;
    module.unconnected_drive = unconnected_drive_;
    current_params_.clear();
    current_real_params_.clear();
    current_real_values_.clear();
    current_genvars_.Reset();
    current_module_ = &module;

    if (MatchSymbol("#")) {
      if (!ParseParameterList(&module)) {
        return false;
      }
    }

    if (MatchSymbol("(")) {
      if (!ParsePortList(&module)) {
        return false;
      }
      if (!MatchSymbol(")")) {
        ErrorHere("expected ')' after port list");
        return false;
      }
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after module header");
      return false;
    }

    while (!IsAtEnd()) {
      if (!ApplyDirectivesUpTo(Peek().line)) {
        return false;
      }
      if (MatchKeyword("endmodule")) {
        current_module_ = nullptr;
        if (!ApplyDefparams(&module)) {
          return false;
        }
        program->modules.push_back(std::move(module));
        return true;
      }
      if (MatchKeyword("input")) {
        if (!ParseDecl(&module, PortDir::kInput)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("output")) {
        if (!ParseDecl(&module, PortDir::kOutput)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("inout")) {
        if (!ParseDecl(&module, PortDir::kInout)) {
          return false;
        }
        continue;
      }
      NetType net_type = NetType::kWire;
      if (MatchNetType(&net_type)) {
        if (!ParseNetDecl(&module, net_type)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("genvar")) {
        if (!ParseGenvarDecl(&current_genvars_)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("generate")) {
        if (!ParseGenerateBlock(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("event")) {
        if (!ParseEventDecl(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("integer")) {
        if (!ParseIntegerDecl(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("time")) {
        if (!ParseTimeDecl(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("real")) {
        if (!ParseRealDecl(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("reg")) {
        if (!ParseRegDecl(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("assign")) {
        if (!ParseAssign(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("parameter")) {
        if (!ParseParameterDecl(&module, false)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("localparam")) {
        if (!ParseParameterDecl(&module, true)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("always")) {
        if (!ParseAlways(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("function")) {
        if (!ParseFunction(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("task")) {
        if (!ParseTask(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("specify")) {
        if (!SkipSpecifyBlock()) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("defparam")) {
        if (!ParseDefparam(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("pullup")) {
        if (!ParsePullPrimitive(&module, true)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("pulldown")) {
        if (!ParsePullPrimitive(&module, false)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("initial")) {
        if (!ParseInitial(&module)) {
          return false;
        }
        continue;
      }
      if (Peek().kind == TokenKind::kIdentifier &&
          IsGatePrimitiveKeyword(Peek().text)) {
        std::string gate = Peek().text;
        Advance();
        std::vector<GateAssign> gate_assigns;
        if (!ParseGatePrimitiveAssignments(gate, &gate_assigns)) {
          return false;
        }
        for (auto& gate_assign : gate_assigns) {
          if (!gate_assign.lhs_indices.empty()) {
            ErrorHere("gate output array select not supported in v0");
            return false;
          }
          Assign assign;
          assign.lhs = gate_assign.lhs;
          assign.lhs_has_range = gate_assign.lhs_has_range;
          assign.lhs_msb = gate_assign.lhs_msb;
          assign.lhs_lsb = gate_assign.lhs_lsb;
          assign.rhs = std::move(gate_assign.rhs);
          assign.strength0 = gate_assign.strength0;
          assign.strength1 = gate_assign.strength1;
          assign.has_strength = gate_assign.has_strength;
          module.assigns.push_back(std::move(assign));
        }
        continue;
      }
      if (Peek().kind == TokenKind::kIdentifier &&
          IsSwitchPrimitiveKeyword(Peek().text)) {
        std::string prim = Peek().text;
        Advance();
        if (!ParseSwitchPrimitive(prim, &module)) {
          return false;
        }
        continue;
      }
      if (IsInstanceStart()) {
        if (!ParseInstance(&module)) {
          return false;
        }
        continue;
      }
      ErrorHere("unsupported module item '" + Peek().text + "'");
      return false;
    }

    ErrorHere("unexpected end of file (missing 'endmodule')");
    current_module_ = nullptr;
    return false;
  }

  bool ParsePrimitive(Program* program) {
    std::string prim_name;
    if (!ConsumeIdentifier(&prim_name)) {
      ErrorHere("expected primitive name after 'primitive'");
      return false;
    }
    Module module;
    module.name = prim_name;
    module.unconnected_drive = unconnected_drive_;
    current_params_.clear();
    current_real_params_.clear();
    current_real_values_.clear();
    current_genvars_.Reset();
    current_module_ = &module;

    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after primitive name");
      return false;
    }
    if (!ParsePortList(&module)) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after primitive port list");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after primitive header");
      return false;
    }
    if (!MatchKeyword("table")) {
      ErrorHere("expected 'table' in primitive body");
      return false;
    }

    if (module.ports.empty()) {
      ErrorHere("primitive requires at least one port");
      return false;
    }
    if (module.ports.front().dir != PortDir::kOutput) {
      ErrorHere("primitive output must be first port");
      return false;
    }

    UdpInfo info;
    info.name = prim_name;
    info.output = module.ports.front().name;
    info.output_width = module.ports.front().width;
    info.output_is_reg = false;
    for (const auto& net : module.nets) {
      if (net.name == info.output && net.type == NetType::kReg) {
        info.output_is_reg = true;
        break;
      }
    }
    for (size_t i = 1; i < module.ports.size(); ++i) {
      const Port& port = module.ports[i];
      if (port.dir != PortDir::kInput) {
        ErrorHere("primitive ports must be output followed by input ports");
        return false;
      }
      info.inputs.push_back(port.name);
      info.input_widths.push_back(port.width);
    }
    info.input_has_edge.assign(info.inputs.size(), false);
    if (info.output_width != 1) {
      ErrorHere("primitive output must be 1-bit in v0");
      return false;
    }
    for (size_t i = 0; i < info.input_widths.size(); ++i) {
      if (info.input_widths[i] != 1) {
        ErrorHere("primitive inputs must be 1-bit in v0");
        return false;
      }
    }

    while (true) {
      if (MatchKeyword("endtable")) {
        break;
      }
      if (Peek().kind == TokenKind::kEnd) {
        ErrorHere("unexpected end of file in primitive table");
        return false;
      }
      UdpRow row;
      if (!ParseUdpRow(&info, &row)) {
        return false;
      }
      info.rows.push_back(std::move(row));
    }
    if (!MatchKeyword("endprimitive")) {
      ErrorHere("expected 'endprimitive' after primitive");
      return false;
    }

    if (!LowerUdpToModule(info, &module)) {
      return false;
    }
    current_module_ = nullptr;
    program->modules.push_back(std::move(module));
    return true;
  }

  bool ParsePullPrimitive(Module* module, bool pull_up) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after pullup/pulldown");
      return false;
    }
    std::vector<std::string> targets;
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected net name in pullup/pulldown");
        return false;
      }
      targets.push_back(name);
      if (MatchSymbol(",")) {
        continue;
      }
      break;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after pullup/pulldown");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after pullup/pulldown");
      return false;
    }
    uint64_t value = pull_up ? 1u : 0u;
    for (const auto& name : targets) {
      Assign assign;
      assign.lhs = name;
      assign.rhs = MakeNumberExpr(value);
      assign.has_strength = true;
      if (pull_up) {
        assign.strength0 = Strength::kHighZ;
        assign.strength1 = Strength::kPull;
      } else {
        assign.strength0 = Strength::kPull;
        assign.strength1 = Strength::kHighZ;
      }
      module->assigns.push_back(std::move(assign));
    }
    return true;
  }

  bool ParseUdpRow(UdpInfo* info, UdpRow* out) {
    if (!info || !out) {
      return false;
    }
    out->inputs.clear();
    out->has_current = false;
    out->current = '?';
    out->output = '?';
    out->inputs.reserve(info->inputs.size());
    for (size_t i = 0; i < info->inputs.size(); ++i) {
      UdpPattern pattern;
      if (Peek().kind == TokenKind::kSymbol && Peek().text == "(") {
        if (!ParseUdpEdgePattern(&pattern)) {
          return false;
        }
        if (i < info->input_has_edge.size()) {
          info->input_has_edge[i] = true;
        }
      } else {
        char value = '?';
        if (!ParseUdpPatternChar(&value)) {
          return false;
        }
        pattern.value = value;
      }
      out->inputs.push_back(pattern);
    }
    if (!MatchSymbol(":")) {
      ErrorHere("expected ':' after UDP input patterns");
      return false;
    }
    char mid = '?';
    if (!ParseUdpPatternChar(&mid)) {
      return false;
    }
    if (MatchSymbol(":")) {
      out->has_current = true;
      out->current = mid;
      if (!ParseUdpPatternChar(&out->output)) {
        return false;
      }
    } else {
      out->output = mid;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after UDP table row");
      return false;
    }
    if (out->has_current) {
      info->sequential = true;
    }
    for (const auto& pattern : out->inputs) {
      if (pattern.is_edge) {
        info->sequential = true;
        break;
      }
    }
    return true;
  }

  bool ParseUdpPatternChar(char* out) {
    if (!out) {
      return false;
    }
    if (Peek().kind == TokenKind::kSymbol &&
        (Peek().text == "?" || Peek().text == "-")) {
      *out = Peek().text[0];
      Advance();
      return true;
    }
    if (Peek().kind == TokenKind::kNumber) {
      if (Peek().text.size() != 1) {
        ErrorHere("invalid UDP pattern");
        return false;
      }
      *out = static_cast<char>(std::tolower(
          static_cast<unsigned char>(Peek().text[0])));
      Advance();
      return true;
    }
    if (Peek().kind == TokenKind::kIdentifier) {
      if (Peek().text.empty()) {
        ErrorHere("invalid UDP pattern");
        return false;
      }
      *out = static_cast<char>(std::tolower(
          static_cast<unsigned char>(Peek().text[0])));
      Advance();
      return true;
    }
    ErrorHere("expected UDP pattern");
    return false;
  }

  bool ParseUdpEdgePattern(UdpPattern* out) {
    if (!out) {
      return false;
    }
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' in UDP edge pattern");
      return false;
    }
    std::string chars;
    while (chars.size() < 2) {
      if (Peek().kind == TokenKind::kSymbol && Peek().text == ")") {
        break;
      }
      if (Peek().kind == TokenKind::kSymbol &&
          (Peek().text == "?" || Peek().text == "-")) {
        chars.push_back(Peek().text[0]);
        Advance();
        continue;
      }
      if (Peek().kind == TokenKind::kNumber ||
          Peek().kind == TokenKind::kIdentifier) {
        const std::string text = Peek().text;
        Advance();
        if (text.size() > 2) {
          ErrorHere("invalid UDP edge pattern");
          return false;
        }
        for (char c : text) {
          if (chars.size() >= 2) {
            break;
          }
          chars.push_back(static_cast<char>(std::tolower(
              static_cast<unsigned char>(c))));
        }
        continue;
      }
      ErrorHere("invalid UDP edge pattern");
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after UDP edge pattern");
      return false;
    }
    if (chars.size() != 2) {
      ErrorHere("invalid UDP edge pattern");
      return false;
    }
    out->is_edge = true;
    out->prev = chars[0];
    out->curr = chars[1];
    return true;
  }

  std::unique_ptr<Expr> MakeUdpLiteral(char symbol, int width) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kNumber;
    expr->has_width = true;
    expr->number_width = width;
    expr->has_base = true;
    expr->base_char = 'b';
    uint64_t mask = (width >= 64) ? 0xFFFFFFFFFFFFFFFFull
                                  : ((width > 0) ? ((1ull << width) - 1ull)
                                                 : 0ull);
    switch (symbol) {
      case '0':
        expr->number = 0;
        expr->value_bits = 0;
        expr->x_bits = 0;
        expr->z_bits = 0;
        break;
      case '1':
        expr->number = 1;
        expr->value_bits = 1;
        expr->x_bits = 0;
        expr->z_bits = 0;
        break;
      case 'z':
        expr->number = 0;
        expr->value_bits = 0;
        expr->x_bits = 0;
        expr->z_bits = mask;
        break;
      case 'x':
      default:
        expr->number = 0;
        expr->value_bits = mask;
        expr->x_bits = mask;
        expr->z_bits = 0;
        break;
    }
    return expr;
  }

  std::unique_ptr<Expr> MakeIdentifierExpr(const std::string& name) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kIdentifier;
    expr->ident = name;
    return expr;
  }

  std::unique_ptr<Expr> BuildUdpMatchExpr(const std::string& signal,
                                          const UdpPattern& pattern,
                                          const std::string& prev_signal) {
    auto build_simple =
        [&](const std::string& name, char value) -> std::unique_ptr<Expr> {
      if (value == '?' || value == '-') {
        return nullptr;
      }
      auto lhs = MakeIdentifierExpr(name);
      auto rhs = MakeUdpLiteral(value, 1);
      return MakeBinary('C', std::move(lhs), std::move(rhs));
    };
    if (!pattern.is_edge) {
      return build_simple(signal, pattern.value);
    }
    std::unique_ptr<Expr> prev_cond = build_simple(prev_signal, pattern.prev);
    std::unique_ptr<Expr> curr_cond = build_simple(signal, pattern.curr);
    if (!prev_cond) {
      return curr_cond;
    }
    if (!curr_cond) {
      return prev_cond;
    }
    return MakeBinary('A', std::move(prev_cond), std::move(curr_cond));
  }

  bool LowerUdpToModule(const UdpInfo& info, Module* module) {
    if (!module) {
      return false;
    }
    bool sequential = info.output_is_reg || info.sequential;
    if (!info.output_is_reg) {
      AddOrUpdateNet(module, info.output, NetType::kReg, info.output_width,
                     false, std::shared_ptr<Expr>(), std::shared_ptr<Expr>(),
                     {});
    }

    std::vector<std::string> prev_names(info.inputs.size());
    bool needs_prev = false;
    for (size_t i = 0; i < info.inputs.size(); ++i) {
      if (i < info.input_has_edge.size() && info.input_has_edge[i]) {
        prev_names[i] = "__udp_prev_" + info.inputs[i];
        AddOrUpdateNet(module, prev_names[i], NetType::kReg, 1, false,
                       std::shared_ptr<Expr>(), std::shared_ptr<Expr>(), {});
        needs_prev = true;
      }
    }

    if (sequential || needs_prev) {
      AlwaysBlock init;
      init.edge = EdgeKind::kInitial;
      init.clock = "initial";
      if (sequential) {
        Statement init_out;
        init_out.kind = StatementKind::kAssign;
        init_out.assign.lhs = info.output;
        init_out.assign.rhs = MakeUdpLiteral('x', 1);
        init_out.assign.nonblocking = false;
        init.statements.push_back(std::move(init_out));
      }
      for (const auto& prev_name : prev_names) {
        if (prev_name.empty()) {
          continue;
        }
        Statement init_prev;
        init_prev.kind = StatementKind::kAssign;
        init_prev.assign.lhs = prev_name;
        init_prev.assign.rhs = MakeUdpLiteral('x', 1);
        init_prev.assign.nonblocking = false;
        init.statements.push_back(std::move(init_prev));
      }
      if (!init.statements.empty()) {
        module->always_blocks.push_back(std::move(init));
      }
    }

    AlwaysBlock block;
    block.edge = EdgeKind::kCombinational;
    block.sensitivity = "*";
    if (!sequential) {
      Statement init_assign;
      init_assign.kind = StatementKind::kAssign;
      init_assign.assign.lhs = info.output;
      init_assign.assign.rhs = MakeUdpLiteral('x', 1);
      init_assign.assign.nonblocking = false;
      block.statements.push_back(std::move(init_assign));
    }

    Statement* last_if = nullptr;
    for (const auto& row : info.rows) {
      std::unique_ptr<Expr> cond;
      for (size_t i = 0; i < info.inputs.size(); ++i) {
        const std::string& input = info.inputs[i];
        const UdpPattern& pattern = row.inputs[i];
        const std::string& prev = prev_names[i];
        auto part = BuildUdpMatchExpr(input, pattern, prev);
        if (!part) {
          continue;
        }
        if (cond) {
          cond = MakeBinary('A', std::move(cond), std::move(part));
        } else {
          cond = std::move(part);
        }
      }
      if (row.has_current) {
        UdpPattern state_pattern;
        state_pattern.value = row.current;
        auto state_cond =
            BuildUdpMatchExpr(info.output, state_pattern, std::string());
        if (state_cond) {
          if (cond) {
            cond = MakeBinary('A', std::move(cond), std::move(state_cond));
          } else {
            cond = std::move(state_cond);
          }
        }
      }

      Statement row_stmt;
      row_stmt.kind = StatementKind::kIf;
      if (!cond) {
        row_stmt.condition = MakeNumberExpr(1u);
      } else {
        row_stmt.condition = std::move(cond);
      }
      if (row.output != '-') {
        Statement assign;
        assign.kind = StatementKind::kAssign;
        assign.assign.lhs = info.output;
        assign.assign.rhs = MakeUdpLiteral(row.output, 1);
        assign.assign.nonblocking = false;
        row_stmt.then_branch.push_back(std::move(assign));
      }

      if (!last_if) {
        block.statements.push_back(std::move(row_stmt));
        last_if = &block.statements.back();
      } else {
        last_if->else_branch.push_back(std::move(row_stmt));
        last_if = &last_if->else_branch.back();
      }
      if (!cond) {
        break;
      }
    }

    for (size_t i = 0; i < info.inputs.size(); ++i) {
      if (prev_names[i].empty()) {
        continue;
      }
      Statement update_prev;
      update_prev.kind = StatementKind::kAssign;
      update_prev.assign.lhs = prev_names[i];
      update_prev.assign.rhs = MakeIdentifierExpr(info.inputs[i]);
      update_prev.assign.nonblocking = false;
      block.statements.push_back(std::move(update_prev));
    }

    module->always_blocks.push_back(std::move(block));
    return true;
  }

  bool IsGatePrimitiveKeyword(const std::string& ident) const {
    return ident == "buf" || ident == "not" || ident == "and" ||
           ident == "nand" || ident == "or" || ident == "nor" ||
           ident == "xor" || ident == "xnor" || ident == "bufif0" ||
           ident == "bufif1" || ident == "notif0" || ident == "notif1" ||
           ident == "nmos" || ident == "pmos" || ident == "rnmos" ||
           ident == "rpmos";
  }

  bool IsSwitchPrimitiveKeyword(const std::string& ident) const {
    return ident == "tran" || ident == "tranif1" || ident == "tranif0" ||
           ident == "cmos" || ident == "rcmos";
  }

  std::unique_ptr<Expr> MakeBitSelectExpr(const Expr& base, int index) {
    auto select = std::make_unique<Expr>();
    select->kind = ExprKind::kSelect;
    select->base = CloneExprSimple(base);
    select->msb = index;
    select->lsb = index;
    select->has_range = false;
    select->msb_expr = MakeNumberExpr(static_cast<uint64_t>(index));
    select->lsb_expr = MakeNumberExpr(static_cast<uint64_t>(index));
    return select;
  }

  bool ResolveSwitchTerminal(const Expr& expr, std::string* name) {
    if (!name) {
      return false;
    }
    if (expr.kind == ExprKind::kIdentifier) {
      *name = expr.ident;
      return true;
    }
    ErrorHere("switch terminal must be identifier in v0");
    return false;
  }

  struct GateOutputInfo {
    std::string name;
    std::vector<std::unique_ptr<Expr>> indices;
    bool has_range = false;
    bool is_range = false;
    bool has_const_range = false;
    int msb = 0;
    int lsb = 0;
    std::unique_ptr<Expr> msb_expr;
    std::unique_ptr<Expr> lsb_expr;
  };

  int ArrayDimCount(const std::string& name) const {
    if (!current_module_) {
      return 0;
    }
    for (const auto& net : current_module_->nets) {
      if (net.name == name) {
        return static_cast<int>(net.array_dims.size());
      }
    }
    return 0;
  }

  bool ResolveGateOutput(const Expr& expr, GateOutputInfo* out,
                         bool allow_nonconst_select) {
    if (!out) {
      return false;
    }
    if (expr.kind == ExprKind::kIdentifier) {
      out->name = expr.ident;
      out->has_range = false;
      out->is_range = false;
      return true;
    }
    if (expr.kind == ExprKind::kSelect && expr.base &&
        expr.base->kind == ExprKind::kIdentifier) {
      out->name = expr.base->ident;
      out->has_range = true;
      out->is_range = expr.has_range;
      if (expr.msb_expr) {
        out->msb_expr = CloneExprSimple(*expr.msb_expr);
      }
      if (expr.has_range && expr.lsb_expr) {
        out->lsb_expr = CloneExprSimple(*expr.lsb_expr);
      }
      int64_t msb_val = 0;
      int64_t lsb_val = 0;
      if (out->msb_expr && TryEvalConstExpr(*out->msb_expr, &msb_val) &&
          (!expr.has_range ||
           (out->lsb_expr && TryEvalConstExpr(*out->lsb_expr, &lsb_val)))) {
        if (!expr.has_range) {
          lsb_val = msb_val;
        }
        out->msb = static_cast<int>(msb_val);
        out->lsb = static_cast<int>(lsb_val);
        out->has_const_range = true;
      } else if (expr.has_range || !allow_nonconst_select) {
        ErrorHere("gate output select must be constant in v0");
        return false;
      }
      return true;
    }
    if (expr.kind == ExprKind::kSelect && expr.base &&
        expr.base->kind == ExprKind::kIndex) {
      std::vector<const Expr*> indices;
      const Expr* current = expr.base.get();
      while (current && current->kind == ExprKind::kIndex) {
        if (!current->index || !current->base) {
          break;
        }
        indices.push_back(current->index.get());
        current = current->base.get();
      }
      if (current && current->kind == ExprKind::kIdentifier &&
          IsArrayName(current->ident)) {
        out->name = current->ident;
        int dims = ArrayDimCount(out->name);
        if (dims <= 0) {
          ErrorHere("gate output array select must be valid in v0");
          return false;
        }
        if (static_cast<int>(indices.size()) != dims) {
          ErrorHere("gate output array select must match dimensions in v0");
          return false;
        }
        out->indices.reserve(indices.size());
        for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
          out->indices.push_back(CloneExprSimple(**it));
        }
        out->has_range = true;
        out->is_range = expr.has_range;
        if (expr.msb_expr) {
          out->msb_expr = CloneExprSimple(*expr.msb_expr);
        }
        if (expr.has_range && expr.lsb_expr) {
          out->lsb_expr = CloneExprSimple(*expr.lsb_expr);
        }
        int64_t msb_val = 0;
        int64_t lsb_val = 0;
        if (out->msb_expr && TryEvalConstExpr(*out->msb_expr, &msb_val) &&
            (!expr.has_range ||
             (out->lsb_expr && TryEvalConstExpr(*out->lsb_expr, &lsb_val)))) {
          if (!expr.has_range) {
            lsb_val = msb_val;
          }
          out->msb = static_cast<int>(msb_val);
          out->lsb = static_cast<int>(lsb_val);
          out->has_const_range = true;
        } else if (expr.has_range || !allow_nonconst_select) {
          ErrorHere("gate output select must be constant in v0");
          return false;
        }
        return true;
      }
    }
    if (expr.kind == ExprKind::kIndex) {
      std::vector<const Expr*> indices;
      const Expr* current = &expr;
      while (current && current->kind == ExprKind::kIndex) {
        if (!current->index || !current->base) {
          break;
        }
        indices.push_back(current->index.get());
        current = current->base.get();
      }
      if (current && current->kind == ExprKind::kIdentifier) {
        out->name = current->ident;
        if (IsArrayName(out->name)) {
          int dims = ArrayDimCount(out->name);
          if (dims <= 0) {
            ErrorHere("gate output array select must be valid in v0");
            return false;
          }
          if (static_cast<int>(indices.size()) < dims ||
              static_cast<int>(indices.size()) > dims + 1) {
            ErrorHere("gate output array select must match dimensions in v0");
            return false;
          }
          out->indices.reserve(dims);
          for (int i = 0; i < dims; ++i) {
            auto it = indices.rbegin() + i;
            out->indices.push_back(CloneExprSimple(**it));
          }
          if (static_cast<int>(indices.size()) == dims + 1) {
            const Expr* bit_expr = indices.front();
            out->has_range = true;
            out->is_range = false;
            out->msb_expr = CloneExprSimple(*bit_expr);
            int64_t bit_val = 0;
            if (out->msb_expr &&
                TryEvalConstExpr(*out->msb_expr, &bit_val)) {
              out->msb = static_cast<int>(bit_val);
              out->lsb = static_cast<int>(bit_val);
              out->has_const_range = true;
            } else if (!allow_nonconst_select) {
              ErrorHere("gate output select must be constant in v0");
              return false;
            }
          }
          return true;
        }
        if (indices.size() == 1) {
          out->has_range = true;
          out->is_range = false;
          out->msb_expr = CloneExprSimple(*indices.front());
          int64_t msb_val = 0;
          if (out->msb_expr && TryEvalConstExpr(*out->msb_expr, &msb_val)) {
            out->msb = static_cast<int>(msb_val);
            out->lsb = static_cast<int>(msb_val);
            out->has_const_range = true;
          } else if (!allow_nonconst_select) {
            ErrorHere("gate output select must be constant in v0");
            return false;
          }
          return true;
        }
      }
    }
    ErrorHere("gate output must be identifier or constant select in v0");
    return false;
  }

  std::unique_ptr<Expr> CloneOrIndexExpr(const Expr& expr, bool index_inputs,
                                         int index) {
    if (index_inputs && expr.kind == ExprKind::kIdentifier) {
      return MakeBitSelectExpr(expr, index);
    }
    return CloneExprSimple(expr);
  }

  bool ParseGatePrimitiveAssignments(const std::string& gate,
                                     std::vector<GateAssign>* out_assigns,
                                     bool allow_nonconst_output = false) {
    if (!out_assigns) {
      return false;
    }
    Strength strength0 = Strength::kStrong;
    Strength strength1 = Strength::kStrong;
    bool has_strength = false;
    if (!ParseDriveStrengthIfPresent(&strength0, &strength1, &has_strength)) {
      return false;
    }
    if (MatchSymbol("#")) {
      if (!SkipDelayControl()) {
        return false;
      }
    }

    bool has_array = false;
    int array_msb = 0;
    int array_lsb = 0;
    if (Peek().kind == TokenKind::kIdentifier) {
      Advance();
      if (MatchSymbol("[")) {
        std::unique_ptr<Expr> msb_expr = ParseExpr();
        if (!msb_expr) {
          return false;
        }
        int64_t msb_val = 0;
        if (!TryEvalConstExpr(*msb_expr, &msb_val)) {
          ErrorHere("gate array range must be constant");
          return false;
        }
        int64_t lsb_val = msb_val;
        if (MatchSymbol(":")) {
          std::unique_ptr<Expr> lsb_expr = ParseExpr();
          if (!lsb_expr) {
            return false;
          }
          if (!TryEvalConstExpr(*lsb_expr, &lsb_val)) {
            ErrorHere("gate array range must be constant");
            return false;
          }
        }
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after gate array range");
          return false;
        }
        has_array = true;
        array_msb = static_cast<int>(msb_val);
        array_lsb = static_cast<int>(lsb_val);
      }
    }

    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after gate primitive");
      return false;
    }
    std::vector<std::unique_ptr<Expr>> ports;
    std::unique_ptr<Expr> first = ParseExpr();
    if (!first) {
      return false;
    }
    ports.push_back(std::move(first));
    while (MatchSymbol(",")) {
      std::unique_ptr<Expr> expr = ParseExpr();
      if (!expr) {
        return false;
      }
      ports.push_back(std::move(expr));
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after gate primitive ports");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after gate primitive");
      return false;
    }

    if (gate == "buf" || gate == "not") {
      if (ports.size() != 2) {
        ErrorHere("gate requires exactly 2 ports in v0");
        return false;
      }
    } else if (gate == "bufif0" || gate == "bufif1" || gate == "notif0" ||
               gate == "notif1" || gate == "nmos" || gate == "pmos" ||
               gate == "rnmos" || gate == "rpmos") {
      if (ports.size() != 3) {
        ErrorHere("gate requires exactly 3 ports in v0");
        return false;
      }
    } else {
      if (ports.size() < 3) {
        ErrorHere("gate requires at least 3 ports in v0");
        return false;
      }
    }

    GateOutputInfo out_info;
    if (!ResolveGateOutput(*ports[0], &out_info, allow_nonconst_output)) {
      return false;
    }
    if (has_array && (out_info.has_range || !out_info.indices.empty())) {
      ErrorHere("gate array output must be identifier in v0");
      return false;
    }

    bool needs_tristate = gate == "bufif0" || gate == "bufif1" ||
                          gate == "notif0" || gate == "notif1" ||
                          gate == "nmos" || gate == "pmos" ||
                          gate == "rnmos" || gate == "rpmos";
    if (needs_tristate && !options_.enable_4state) {
      ErrorHere("tristate primitives require --4state");
      return false;
    }

    int step = (array_msb <= array_lsb) ? 1 : -1;
    int index = array_msb;
    bool index_inputs = has_array;
    bool has_any = false;
    while (true) {
      int output_width = 1;
      GateAssign assign;
      assign.lhs = out_info.name;
      assign.strength0 = strength0;
      assign.strength1 = strength1;
      assign.has_strength = has_strength;
      for (const auto& idx : out_info.indices) {
        assign.lhs_indices.push_back(CloneExprSimple(*idx));
      }
      if (has_array) {
        assign.lhs_has_range = true;
        assign.lhs_is_range = false;
        assign.lhs_msb = index;
        assign.lhs_lsb = index;
        output_width = 1;
      } else if (out_info.has_range) {
        assign.lhs_has_range = true;
        assign.lhs_is_range = out_info.is_range;
        assign.lhs_msb = out_info.msb;
        assign.lhs_lsb = out_info.lsb;
        if (out_info.msb_expr) {
          assign.lhs_msb_expr = CloneExprSimple(*out_info.msb_expr);
        }
        if (out_info.lsb_expr) {
          assign.lhs_lsb_expr = CloneExprSimple(*out_info.lsb_expr);
        }
        if (out_info.is_range) {
          if (!out_info.has_const_range) {
            ErrorHere("gate output select must be constant in v0");
            return false;
          }
          output_width =
              (out_info.msb >= out_info.lsb)
                  ? (out_info.msb - out_info.lsb + 1)
                  : (out_info.lsb - out_info.msb + 1);
        } else {
          output_width = 1;
        }
      } else {
        output_width = LookupSignalWidth(out_info.name);
        if (output_width <= 0) {
          AddOrUpdateNet(current_module_, out_info.name, NetType::kWire, 1,
                         false,
                         nullptr, nullptr, {});
          output_width = 1;
        }
      }

      std::vector<std::unique_ptr<Expr>> inputs;
      for (size_t i = 1; i < ports.size(); ++i) {
        inputs.push_back(CloneOrIndexExpr(*ports[i], index_inputs, index));
      }

      std::unique_ptr<Expr> rhs;
      if (gate == "buf") {
        rhs = std::move(inputs[0]);
      } else if (gate == "not") {
        rhs = MakeUnaryExpr('~', std::move(inputs[0]));
      } else if (gate == "and" || gate == "nand") {
        std::unique_ptr<Expr> chain = std::move(inputs[0]);
        for (size_t i = 1; i < inputs.size(); ++i) {
          chain = MakeBinary('&', std::move(chain), std::move(inputs[i]));
        }
        rhs = (gate == "nand") ? MakeUnaryExpr('~', std::move(chain))
                               : std::move(chain);
      } else if (gate == "or" || gate == "nor") {
        std::unique_ptr<Expr> chain = std::move(inputs[0]);
        for (size_t i = 1; i < inputs.size(); ++i) {
          chain = MakeBinary('|', std::move(chain), std::move(inputs[i]));
        }
        rhs = (gate == "nor") ? MakeUnaryExpr('~', std::move(chain))
                              : std::move(chain);
      } else if (gate == "xor" || gate == "xnor") {
        std::unique_ptr<Expr> chain = std::move(inputs[0]);
        for (size_t i = 1; i < inputs.size(); ++i) {
          chain = MakeBinary('^', std::move(chain), std::move(inputs[i]));
        }
        rhs = (gate == "xnor") ? MakeUnaryExpr('~', std::move(chain))
                               : std::move(chain);
      } else if (gate == "bufif0" || gate == "bufif1") {
        std::unique_ptr<Expr> enable = std::move(inputs[1]);
        if (gate == "bufif0") {
          enable = MakeUnaryExpr('!', std::move(enable));
        }
        rhs = MakeTernaryExpr(std::move(enable), std::move(inputs[0]),
                              MakeZExpr(output_width));
      } else if (gate == "notif0" || gate == "notif1") {
        std::unique_ptr<Expr> enable = std::move(inputs[1]);
        if (gate == "notif0") {
          enable = MakeUnaryExpr('!', std::move(enable));
        }
        std::unique_ptr<Expr> data = MakeUnaryExpr('~', std::move(inputs[0]));
        rhs = MakeTernaryExpr(std::move(enable), std::move(data),
                              MakeZExpr(output_width));
      } else if (gate == "nmos" || gate == "pmos" || gate == "rnmos" ||
                 gate == "rpmos") {
        std::unique_ptr<Expr> gate_expr = std::move(inputs[1]);
        if (gate == "pmos" || gate == "rpmos") {
          gate_expr = MakeUnaryExpr('!', std::move(gate_expr));
        }
        rhs = MakeTernaryExpr(std::move(gate_expr), std::move(inputs[0]),
                              MakeZExpr(output_width));
      } else {
        ErrorHere("unsupported gate primitive in v0");
        return false;
      }

      assign.rhs = std::move(rhs);
      out_assigns->push_back(std::move(assign));
      has_any = true;
      if (!has_array || index == array_lsb) {
        break;
      }
      index += step;
    }
    return has_any;
  }

  bool ParseSwitchPrimitive(const std::string& prim, Module* module) {
    if (!module) {
      return false;
    }
    if (!options_.enable_4state) {
      ErrorHere("switch primitives require --4state");
      return false;
    }
    Strength strength0 = Strength::kStrong;
    Strength strength1 = Strength::kStrong;
    bool has_strength = false;
    if (!ParseDriveStrengthIfPresent(&strength0, &strength1, &has_strength)) {
      return false;
    }
    if (MatchSymbol("#")) {
      if (!SkipDelayControl()) {
        return false;
      }
    }
    if (Peek().kind == TokenKind::kIdentifier) {
      Advance();
      if (MatchSymbol("[")) {
        ErrorHere("switch arrays not supported in v0");
        return false;
      }
    }
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after switch primitive");
      return false;
    }
    std::vector<std::unique_ptr<Expr>> ports;
    std::unique_ptr<Expr> first = ParseExpr();
    if (!first) {
      return false;
    }
    ports.push_back(std::move(first));
    while (MatchSymbol(",")) {
      std::unique_ptr<Expr> expr = ParseExpr();
      if (!expr) {
        return false;
      }
      ports.push_back(std::move(expr));
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after switch primitive ports");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after switch primitive");
      return false;
    }

    if (prim == "tran") {
      if (ports.size() != 2) {
        ErrorHere("tran requires exactly 2 ports in v0");
        return false;
      }
    } else if (prim == "tranif1" || prim == "tranif0") {
      if (ports.size() != 3) {
        ErrorHere("tranif requires exactly 3 ports in v0");
        return false;
      }
    } else if (prim == "cmos" || prim == "rcmos") {
      if (ports.size() != 4) {
        ErrorHere("cmos requires exactly 4 ports in v0");
        return false;
      }
    } else {
      ErrorHere("unsupported switch primitive in v0");
      return false;
    }

    std::string a_name;
    std::string b_name;
    if (!ResolveSwitchTerminal(*ports[0], &a_name) ||
        !ResolveSwitchTerminal(*ports[1], &b_name)) {
      return false;
    }

    Switch sw;
    sw.strength0 = strength0;
    sw.strength1 = strength1;
    sw.has_strength = has_strength;
    if (prim == "tran") {
      sw.kind = SwitchKind::kTran;
    } else if (prim == "tranif1") {
      sw.kind = SwitchKind::kTranif1;
    } else if (prim == "tranif0") {
      sw.kind = SwitchKind::kTranif0;
    } else {
      sw.kind = SwitchKind::kCmos;
    }
    sw.a = a_name;
    sw.b = b_name;
    if (prim == "tranif1" || prim == "tranif0") {
      sw.control = std::move(ports[2]);
    } else if (prim == "cmos" || prim == "rcmos") {
      sw.control = std::move(ports[2]);
      sw.control_n = std::move(ports[3]);
    }
    module->switches.push_back(std::move(sw));
    return true;
  }

  bool SkipSpecifyBlock() {
    const Token& start = Previous();
    diagnostics_->Add(Severity::kWarning,
                      "specify block ignored in v0",
                      SourceLocation{path_, start.line, start.column});
    int depth = 1;
    while (!IsAtEnd()) {
      if (MatchKeyword("specify")) {
        ++depth;
        continue;
      }
      if (MatchKeyword("endspecify")) {
        --depth;
        if (depth == 0) {
          return true;
        }
        continue;
      }
      Advance();
    }
    diagnostics_->Add(Severity::kError,
                      "missing 'endspecify' for specify block",
                      SourceLocation{path_, start.line, start.column});
    return false;
  }

  bool ParseDefparam(Module* module) {
    while (true) {
      const Token& start_token = Peek();
      std::string path;
      if (!ConsumeHierIdentifier(&path)) {
        ErrorHere("expected instance name in defparam");
        return false;
      }
      size_t dot = path.rfind('.');
      if (dot == std::string::npos) {
        ErrorHere("expected parameter name in defparam");
        return false;
      }
      std::string instance_name = path.substr(0, dot);
      std::string param_name = path.substr(dot + 1);
      if (!MatchSymbol("=")) {
        ErrorHere("expected '=' in defparam");
        return false;
      }
      std::unique_ptr<Expr> expr = ParseExpr();
      if (!expr) {
        return false;
      }
      DefParam defparam;
      defparam.instance = instance_name;
      defparam.param = param_name;
      defparam.expr = std::move(expr);
      defparam.line = start_token.line;
      defparam.column = start_token.column;
      module->defparams.push_back(std::move(defparam));
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after defparam");
        return false;
      }
      break;
    }
    return true;
  }

  bool ApplyDefparams(Module* module) {
    return module != nullptr;
  }

  bool ParseFunction(Module* module) {
    Function func;
    if (MatchKeyword("automatic")) {
      // automatic functions are treated like static in v0.
    }
    bool is_signed = false;
    bool is_real = false;
    if (MatchKeyword("real")) {
      is_real = true;
      is_signed = true;
    }
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    if (!is_real && MatchKeyword("real")) {
      is_real = true;
      is_signed = true;
    }
    int width = is_real ? 64 : 1;
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    bool had_range = false;
    if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
      return false;
    }
    if (is_real) {
      if (had_range) {
        ErrorHere("real function return cannot use packed ranges");
        return false;
      }
      width = 64;
      msb_expr.reset();
      lsb_expr.reset();
    } else if (!had_range) {
      msb_expr.reset();
      lsb_expr.reset();
    }
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      ErrorHere("expected function name after 'function'");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after function header");
      return false;
    }

    func.name = name;
    func.width = width;
    func.is_signed = is_signed;
    func.is_real = is_real;
    func.msb_expr = msb_expr;
    func.lsb_expr = lsb_expr;

    bool saw_statement = false;
    bool saw_endfunction = false;
    while (!IsAtEnd()) {
      if (MatchKeyword("endfunction")) {
        saw_endfunction = true;
        break;
      }
      if (!saw_statement) {
        if (MatchKeyword("input")) {
          if (!ParseFunctionInput(&func)) {
            return false;
          }
          continue;
        }
        if (MatchKeyword("real")) {
          if (!ParseFunctionRealDecl(&func)) {
            return false;
          }
          continue;
        }
        if (MatchKeyword("integer")) {
          if (!ParseFunctionIntegerDecl(&func)) {
            return false;
          }
          continue;
        }
        if (MatchKeyword("time")) {
          if (!ParseFunctionTimeDecl(&func)) {
            return false;
          }
          continue;
        }
        if (MatchKeyword("reg")) {
          if (!ParseFunctionRegDecl(&func)) {
            return false;
          }
          continue;
        }
      } else if (Peek().kind == TokenKind::kIdentifier &&
                 (Peek().text == "input" || Peek().text == "real" ||
                  Peek().text == "integer" || Peek().text == "time" ||
                  Peek().text == "reg")) {
        ErrorHere("function declarations must appear before statements");
        return false;
      }

      Statement stmt;
      if (!ParseFunctionStatement(&func, &stmt)) {
        return false;
      }
      saw_statement = true;
      func.body.push_back(std::move(stmt));
    }

    if (!saw_endfunction) {
      ErrorHere("missing 'endfunction'");
      return false;
    }
    if (func.body.empty()) {
      ErrorHere("function missing body");
      return false;
    }
    MaybeSetFunctionBodyExpr(&func);
    module->functions.push_back(std::move(func));
    return true;
  }

  bool ParseFunctionInput(Function* func) {
    bool is_signed = false;
    bool is_real = false;
    if (MatchKeyword("real")) {
      is_real = true;
      is_signed = true;
    }
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    if (!is_real && MatchKeyword("real")) {
      is_real = true;
      is_signed = true;
    }
    int width = is_real ? 64 : 1;
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    bool had_range = false;
    if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
      return false;
    }
    if (is_real) {
      if (had_range) {
        ErrorHere("real function input cannot use packed ranges");
        return false;
      }
      width = 64;
      msb_expr.reset();
      lsb_expr.reset();
    } else if (!had_range) {
      msb_expr.reset();
      lsb_expr.reset();
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected function input name");
        return false;
      }
      FunctionArg arg;
      arg.name = name;
      arg.width = width;
      arg.is_signed = is_signed;
      arg.is_real = is_real;
      arg.msb_expr = msb_expr;
      arg.lsb_expr = lsb_expr;
      func->args.push_back(std::move(arg));
      if (MatchSymbol(",")) {
        continue;
      }
      break;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after function input");
      return false;
    }
    return true;
  }

  bool ParseFunctionIntegerDecl(Function* func) {
    const int width = 32;
    bool is_signed = true;
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in integer declaration");
        return false;
      }
      if (!AddFunctionLocal(func, name, width, is_signed, false)) {
        return false;
      }
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after integer declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseFunctionTimeDecl(Function* func) {
    const int width = 64;
    bool is_signed = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in time declaration");
        return false;
      }
      if (!AddFunctionLocal(func, name, width, is_signed, false)) {
        return false;
      }
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after time declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseFunctionRegDecl(Function* func) {
    bool is_signed = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    int width = 1;
    std::shared_ptr<Expr> range_msb;
    std::shared_ptr<Expr> range_lsb;
    if (!ParseRange(&width, &range_msb, &range_lsb, nullptr)) {
      return false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in reg declaration");
        return false;
      }
      if (MatchSymbol("[")) {
        ErrorHere("arrayed reg locals not supported in functions");
        return false;
      }
      if (!AddFunctionLocal(func, name, width, is_signed, false)) {
        return false;
      }
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after reg declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseFunctionStatement(Function* func, Statement* out_statement) {
    if (!ParseStatement(out_statement)) {
      return false;
    }
    switch (out_statement->kind) {
      case StatementKind::kAssign:
      case StatementKind::kIf:
      case StatementKind::kBlock:
      case StatementKind::kCase:
      case StatementKind::kFor:
      case StatementKind::kWhile:
      case StatementKind::kRepeat:
        break;
      default:
        ErrorHere("unsupported statement in function");
        return false;
    }
    if (out_statement->kind == StatementKind::kAssign &&
        out_statement->assign.nonblocking) {
      ErrorHere("nonblocking assignment not allowed in function");
      return false;
    }
    return true;
  }

  void MaybeSetFunctionBodyExpr(Function* func) {
    if (!func || func->body.size() != 1) {
      return;
    }
    const Statement& stmt = func->body.front();
    if (stmt.kind != StatementKind::kAssign) {
      return;
    }
    const auto& assign = stmt.assign;
    if (assign.lhs != func->name || assign.lhs_index ||
        !assign.lhs_indices.empty() || assign.lhs_has_range || !assign.rhs) {
      return;
    }
    func->body_expr = CloneExpr(*assign.rhs);
  }

  bool AddFunctionLocal(Function* func, const std::string& name, int width,
                        bool is_signed, bool is_real) {
    if (!func) {
      return false;
    }
    if (name == func->name) {
      ErrorHere("function local '" + name + "' redeclares function name");
      return false;
    }
    for (const auto& arg : func->args) {
      if (arg.name == name) {
        ErrorHere("function local '" + name + "' redeclares argument");
        return false;
      }
    }
    for (const auto& local : func->locals) {
      if (local.name == name) {
        ErrorHere("duplicate function local '" + name + "'");
        return false;
      }
    }
    LocalVar local;
    local.name = name;
    local.width = width;
    local.is_signed = is_signed;
    local.is_real = is_real;
    func->locals.push_back(std::move(local));
    return true;
  }

  bool ParseFunctionRealDecl(Function* func) {
    const int width = 64;
    const bool is_signed = true;
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in real declaration");
        return false;
      }
      if (MatchSymbol("[")) {
        ErrorHere("arrayed real locals not supported in functions");
        return false;
      }
      if (!AddFunctionLocal(func, name, width, is_signed, true)) {
        return false;
      }
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after real declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseTaskArgDecl(TaskArgDir dir, Task* task) {
    bool is_signed = false;
    bool is_real = false;
    if (MatchKeyword("reg")) {
      // Tasks allow "output reg" syntax; treat as output.
    }
    if (MatchKeyword("real")) {
      is_real = true;
      is_signed = true;
    }
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    if (!is_real && MatchKeyword("real")) {
      is_real = true;
      is_signed = true;
    }
    int width = is_real ? 64 : 1;
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    bool had_range = false;
    if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
      return false;
    }
    if (is_real) {
      if (had_range) {
        ErrorHere("real task args cannot use packed ranges");
        return false;
      }
      width = 64;
      msb_expr.reset();
      lsb_expr.reset();
    } else if (!had_range) {
      msb_expr.reset();
      lsb_expr.reset();
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected task argument name");
        return false;
      }
      TaskArg arg;
      arg.dir = dir;
      arg.name = name;
      arg.width = width;
      arg.is_signed = is_signed;
      arg.is_real = is_real;
      arg.msb_expr = msb_expr;
      arg.lsb_expr = lsb_expr;
      task->args.push_back(std::move(arg));
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after task argument");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseTask(Module* module) {
    Task task;
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      ErrorHere("expected task name after 'task'");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after task header");
      return false;
    }
    task.name = name;

    bool saw_endtask = false;
    while (!IsAtEnd()) {
      if (MatchKeyword("endtask")) {
        saw_endtask = true;
        break;
      }
      if (MatchKeyword("input")) {
        if (!ParseTaskArgDecl(TaskArgDir::kInput, &task)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("output")) {
        if (!ParseTaskArgDecl(TaskArgDir::kOutput, &task)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("inout")) {
        if (!ParseTaskArgDecl(TaskArgDir::kInout, &task)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("integer")) {
        if (!ParseLocalIntegerDecl()) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("real")) {
        if (!ParseLocalRealDecl()) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("time")) {
        if (!ParseLocalTimeDecl()) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("reg")) {
        if (!ParseLocalRegDecl(current_module_)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("begin")) {
        Statement block;
        block.kind = StatementKind::kBlock;
        while (true) {
          if (MatchKeyword("end")) {
            break;
          }
          if (MatchKeyword("integer")) {
            if (!ParseLocalIntegerDecl()) {
              return false;
            }
            continue;
          }
          if (MatchKeyword("time")) {
            if (!ParseLocalTimeDecl()) {
              return false;
            }
            continue;
          }
          if (MatchKeyword("reg")) {
            if (!ParseLocalRegDecl(current_module_)) {
              return false;
            }
            continue;
          }
          Statement inner;
          if (!ParseStatement(&inner)) {
            return false;
          }
          block.block.push_back(std::move(inner));
        }
        task.body.push_back(std::move(block));
        continue;
      }
      Statement stmt;
      if (!ParseStatement(&stmt)) {
        return false;
      }
      task.body.push_back(std::move(stmt));
    }
    if (!saw_endtask) {
      ErrorHere("expected 'endtask'");
      return false;
    }
    module->tasks.push_back(std::move(task));
    return true;
  }

  bool ParsePortList(Module* module) {
    if (MatchSymbol(")")) {
      --pos_;
      return true;
    }
    PortDir current_dir = PortDir::kInout;
    int current_width = 1;
    bool current_is_reg = false;
    bool current_is_signed = false;
    bool current_is_real = false;
    NetType current_net_type = NetType::kWire;
    bool current_has_net_type = false;
    std::shared_ptr<Expr> current_msb;
    std::shared_ptr<Expr> current_lsb;
    while (true) {
      PortDir dir = current_dir;
      int width = current_width;
      bool is_reg = current_is_reg;
      bool is_signed = current_is_signed;
      bool is_real = current_is_real;
      NetType net_type = current_net_type;
      bool has_net_type = current_has_net_type;
      std::shared_ptr<Expr> range_msb = current_msb;
      std::shared_ptr<Expr> range_lsb = current_lsb;
      if (MatchKeyword("input")) {
        dir = PortDir::kInput;
        width = 1;
        is_reg = false;
        is_signed = false;
        is_real = false;
        net_type = NetType::kWire;
        has_net_type = false;
        if (MatchKeyword("real")) {
          is_real = true;
          is_signed = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchNetType(&net_type)) {
          has_net_type = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (!is_real && MatchKeyword("real")) {
          is_real = true;
          is_signed = true;
        }
        if (is_real && has_net_type) {
          ErrorHere("real declarations cannot use net types");
          return false;
        }
        if (has_net_type && NetTypeRequires4State(net_type) &&
            !options_.enable_4state) {
          ErrorHere("net type requires --4state");
          return false;
        }
        bool had_range = false;
        if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
          return false;
        }
        if (is_real) {
          if (had_range) {
            ErrorHere("real declarations cannot use packed ranges");
            return false;
          }
          width = 64;
          range_msb.reset();
          range_lsb.reset();
        } else if (!had_range) {
          range_msb.reset();
          range_lsb.reset();
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
        current_is_signed = is_signed;
        current_is_real = is_real;
        current_net_type = net_type;
        current_has_net_type = has_net_type;
        current_msb = had_range ? range_msb : std::shared_ptr<Expr>();
        current_lsb = had_range ? range_lsb : std::shared_ptr<Expr>();
      } else if (MatchKeyword("output")) {
        dir = PortDir::kOutput;
        width = 1;
        is_reg = false;
        is_signed = false;
        is_real = false;
        net_type = NetType::kWire;
        has_net_type = false;
        if (MatchKeyword("real")) {
          is_real = true;
          is_signed = true;
          is_reg = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchKeyword("reg")) {
          is_reg = true;
        } else if (MatchNetType(&net_type)) {
          has_net_type = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (!is_real && MatchKeyword("real")) {
          is_real = true;
          is_signed = true;
          is_reg = true;
        }
        if (is_real && has_net_type) {
          ErrorHere("real declarations cannot use net types");
          return false;
        }
        if (has_net_type && NetTypeRequires4State(net_type) &&
            !options_.enable_4state) {
          ErrorHere("net type requires --4state");
          return false;
        }
        bool had_range = false;
        if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
          return false;
        }
        if (is_real) {
          if (had_range) {
            ErrorHere("real declarations cannot use packed ranges");
            return false;
          }
          width = 64;
          range_msb.reset();
          range_lsb.reset();
        } else if (!had_range) {
          range_msb.reset();
          range_lsb.reset();
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
        current_is_signed = is_signed;
        current_is_real = is_real;
        current_net_type = net_type;
        current_has_net_type = has_net_type;
        current_msb = had_range ? range_msb : std::shared_ptr<Expr>();
        current_lsb = had_range ? range_lsb : std::shared_ptr<Expr>();
      } else if (MatchKeyword("inout")) {
        dir = PortDir::kInout;
        width = 1;
        is_reg = false;
        is_signed = false;
        is_real = false;
        net_type = NetType::kWire;
        has_net_type = false;
        if (MatchKeyword("real")) {
          is_real = true;
          is_signed = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchNetType(&net_type)) {
          has_net_type = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (!is_real && MatchKeyword("real")) {
          is_real = true;
          is_signed = true;
        }
        if (is_real && has_net_type) {
          ErrorHere("real declarations cannot use net types");
          return false;
        }
        if (has_net_type && NetTypeRequires4State(net_type) &&
            !options_.enable_4state) {
          ErrorHere("net type requires --4state");
          return false;
        }
        bool had_range = false;
        if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
          return false;
        }
        if (is_real) {
          if (had_range) {
            ErrorHere("real declarations cannot use packed ranges");
            return false;
          }
          width = 64;
          range_msb.reset();
          range_lsb.reset();
        } else if (!had_range) {
          range_msb.reset();
          range_lsb.reset();
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
        current_is_signed = is_signed;
        current_is_real = is_real;
        current_net_type = net_type;
        current_has_net_type = has_net_type;
        current_msb = had_range ? range_msb : std::shared_ptr<Expr>();
        current_lsb = had_range ? range_lsb : std::shared_ptr<Expr>();
      } else {
        bool had_range = false;
        if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
          return false;
        }
        if (!had_range) {
          range_msb = current_msb;
          range_lsb = current_lsb;
        }
      }
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected port name");
        return false;
      }
      AddOrUpdatePort(module, name, dir, width, is_signed, is_real, range_msb,
                      range_lsb);
      if (is_real) {
        NetType real_type =
            (dir == PortDir::kOutput) ? NetType::kReg : NetType::kWire;
        AddOrUpdateNet(module, name, real_type, width, is_signed, range_msb,
                       range_lsb, {}, true);
      } else {
        if ((dir == PortDir::kOutput || dir == PortDir::kInout) && !is_reg &&
            net_type != NetType::kWire) {
          AddOrUpdateNet(module, name, net_type, width, is_signed, range_msb,
                         range_lsb, {});
          AddImplicitNetDriver(module, name, net_type);
        }
        if (dir == PortDir::kOutput && is_reg) {
          AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                         range_msb, range_lsb, {});
        }
      }
      if (MatchSymbol(",")) {
        continue;
      }
      break;
    }
    return true;
  }

  bool ParseDecl(Module* module, PortDir dir) {
    bool is_reg = false;
    bool is_signed = false;
    bool is_real = false;
    NetType net_type = NetType::kWire;
    bool has_net_type = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    if (dir == PortDir::kOutput) {
      if (MatchKeyword("reg")) {
        is_reg = true;
      } else if (MatchKeyword("real")) {
        is_real = true;
        is_reg = true;
      } else if (MatchNetType(&net_type)) {
        has_net_type = true;
      }
    } else {
      if (MatchKeyword("real")) {
        is_real = true;
      } else if (MatchNetType(&net_type)) {
        has_net_type = true;
      }
    }
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    if (!is_real && MatchKeyword("real")) {
      is_real = true;
    }
    if (is_real) {
      is_signed = true;
      if (has_net_type) {
        ErrorHere("real declarations cannot use net types");
        return false;
      }
    }
    if (has_net_type && NetTypeRequires4State(net_type) &&
        !options_.enable_4state) {
      ErrorHere("net type requires --4state");
      return false;
    }
    int width = is_real ? 64 : 1;
    std::shared_ptr<Expr> range_msb;
    std::shared_ptr<Expr> range_lsb;
    bool had_range = false;
    if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
      return false;
    }
    if (is_real) {
      if (had_range) {
        ErrorHere("real declarations cannot use packed ranges");
        return false;
      }
      width = 64;
      range_msb.reset();
      range_lsb.reset();
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in declaration");
        return false;
      }
      AddOrUpdatePort(module, name, dir, width, is_signed, is_real, range_msb,
                      range_lsb);
      if (is_real) {
        NetType real_type =
            (dir == PortDir::kOutput) ? NetType::kReg : NetType::kWire;
        AddOrUpdateNet(module, name, real_type, width, is_signed, range_msb,
                       range_lsb, {}, true);
      } else {
        if ((dir == PortDir::kOutput || dir == PortDir::kInout) && !is_reg &&
            net_type != NetType::kWire) {
          AddOrUpdateNet(module, name, net_type, width, is_signed, range_msb,
                         range_lsb, {});
          AddImplicitNetDriver(module, name, net_type);
        }
        if (dir == PortDir::kOutput && is_reg) {
          AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                         range_msb, range_lsb, {});
        }
      }
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseNetDecl(Module* module, NetType net_type) {
    bool is_signed = false;
    Strength strength0 = Strength::kStrong;
    Strength strength1 = Strength::kStrong;
    bool has_strength = false;
    ChargeStrength charge = ChargeStrength::kNone;
    bool has_charge = false;
    bool progressed = true;
    while (progressed) {
      progressed = false;
      if (!has_strength && IsDriveStrengthLookahead()) {
        if (!ParseDriveStrength(&strength0, &strength1, &has_strength)) {
          return false;
        }
        progressed = true;
      }
      if (!is_signed && MatchKeyword("signed")) {
        is_signed = true;
        progressed = true;
      }
      if (net_type == NetType::kTrireg && !has_charge &&
          IsChargeStrengthLookahead()) {
        if (!ParseChargeStrengthIfPresent(&charge, &has_charge)) {
          return false;
        }
        progressed = true;
      }
    }
    if (NetTypeRequires4State(net_type) && !options_.enable_4state) {
      ErrorHere("net type requires --4state");
      return false;
    }
    int width = 1;
    std::shared_ptr<Expr> range_msb;
    std::shared_ptr<Expr> range_lsb;
    if (!ParseRange(&width, &range_msb, &range_lsb, nullptr)) {
      return false;
    }
    std::vector<ArrayDim> packed_array_dims;
    while (true) {
      int array_size = 0;
      std::shared_ptr<Expr> array_msb;
      std::shared_ptr<Expr> array_lsb;
      bool had_array = false;
      if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
        return false;
      }
      if (!had_array) {
        break;
      }
      packed_array_dims.push_back(ArrayDim{array_size, array_msb, array_lsb});
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in net declaration");
        return false;
      }
      std::unique_ptr<Expr> init;
      std::vector<ArrayDim> array_dims = packed_array_dims;
      while (true) {
        int array_size = 0;
        std::shared_ptr<Expr> array_msb;
        std::shared_ptr<Expr> array_lsb;
        bool had_array = false;
        if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
          return false;
        }
        if (!had_array) {
          break;
        }
        array_dims.push_back(ArrayDim{array_size, array_msb, array_lsb});
      }
      if (MatchSymbol("=")) {
        init = ParseExpr();
        if (!init) {
          return false;
        }
      }
      AddOrUpdateNet(module, name, net_type, width, is_signed, range_msb,
                     range_lsb, array_dims, false, charge);
      AddImplicitNetDriver(module, name, net_type);
      if (init) {
        Assign assign;
        assign.lhs = name;
        assign.rhs = std::move(init);
        module->assigns.push_back(std::move(assign));
      }
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after net declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseRegDecl(Module* module) {
    bool is_signed = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    int width = 1;
    std::shared_ptr<Expr> range_msb;
    std::shared_ptr<Expr> range_lsb;
    if (!ParseRange(&width, &range_msb, &range_lsb, nullptr)) {
      return false;
    }
    std::vector<ArrayDim> packed_array_dims;
    while (true) {
      int array_size = 0;
      std::shared_ptr<Expr> array_msb;
      std::shared_ptr<Expr> array_lsb;
      bool had_array = false;
      if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
        return false;
      }
      if (!had_array) {
        break;
      }
      packed_array_dims.push_back(ArrayDim{array_size, array_msb, array_lsb});
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in reg declaration");
        return false;
      }
      std::vector<ArrayDim> array_dims = packed_array_dims;
      while (true) {
        int array_size = 0;
        std::shared_ptr<Expr> array_msb;
        std::shared_ptr<Expr> array_lsb;
        bool had_array = false;
        if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
          return false;
        }
        if (!had_array) {
          break;
        }
        array_dims.push_back(ArrayDim{array_size, array_msb, array_lsb});
      }
      AddOrUpdateNet(module, name, NetType::kReg, width, is_signed, range_msb,
                     range_lsb, array_dims);
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after reg declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseParameterList(Module* module) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after '#'");
      return false;
    }
    if (MatchSymbol(")")) {
      return true;
    }
    bool require_keyword = true;
    while (true) {
      if (MatchKeyword("parameter")) {
        require_keyword = false;
      } else if (require_keyword) {
        ErrorHere("expected 'parameter' in parameter list");
        return false;
      }
      if (!ParseParameterItem(module, false)) {
        return false;
      }
      if (MatchSymbol(",")) {
        if (Peek().kind == TokenKind::kIdentifier &&
            Peek().text == "parameter") {
          require_keyword = true;
        }
        continue;
      }
      break;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after parameter list");
      return false;
    }
    return true;
  }

  bool ParseParameterDecl(Module* module, bool is_local) {
    if (!ParseParameterItem(module, is_local)) {
      return false;
    }
    while (MatchSymbol(",")) {
      if (MatchKeyword("parameter")) {
        if (!ParseParameterItem(module, is_local)) {
          return false;
        }
      } else {
        if (!ParseParameterItem(module, is_local)) {
          return false;
        }
      }
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after parameter declaration");
      return false;
    }
    return true;
  }

  bool ParseParameterItem(Module* module, bool is_local) {
    bool param_is_real = false;
    if (Peek().kind == TokenKind::kIdentifier &&
        Peek(1).kind == TokenKind::kIdentifier &&
        Peek(2).kind == TokenKind::kSymbol && Peek(2).text == "=") {
      if (Peek().text == "real") {
        param_is_real = true;
      }
      Advance();
    }
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      ErrorHere("expected parameter name");
      return false;
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in parameter assignment");
      return false;
    }
    std::unique_ptr<Expr> expr = ParseExpr();
    if (!expr) {
      return false;
    }
    if (!param_is_real && ExprIsRealParamExpr(*expr)) {
      param_is_real = true;
    }
    if (param_is_real) {
      double real_value = 0.0;
      if (TryEvalConstRealExpr(*expr, &real_value)) {
        current_real_values_[name] = real_value;
      }
    } else {
      int64_t value = 0;
      if (TryEvalConstExpr(*expr, &value)) {
        current_params_[name] = value;
      }
    }
    current_real_params_[name] = param_is_real;
    Parameter param;
    param.name = name;
    param.value = std::move(expr);
    param.is_local = is_local;
    param.is_real = param_is_real;
    module->parameters.push_back(std::move(param));
    return true;
  }

  bool ParseIntegerDecl(Module* module) {
    const int width = 32;
    bool is_signed = true;
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in integer declaration");
        return false;
      }
      AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(), {});
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after integer declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseTimeDecl(Module* module) {
    const int width = 64;
    bool is_signed = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in time declaration");
        return false;
      }
      AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(), {});
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after time declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseRealDecl(Module* module) {
    const int width = 64;
    bool is_signed = true;
    std::vector<Statement> init_statements;
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in real declaration");
        return false;
      }
      std::vector<ArrayDim> array_dims;
      while (true) {
        int array_size = 0;
        std::shared_ptr<Expr> array_msb;
        std::shared_ptr<Expr> array_lsb;
        bool had_array = false;
        if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
          return false;
        }
        if (!had_array) {
          break;
        }
        array_dims.push_back(ArrayDim{array_size, array_msb, array_lsb});
      }
      AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(),
                     array_dims, true);
      if (MatchSymbol("=")) {
        if (!array_dims.empty()) {
          ErrorHere("real array initializer not supported");
          return false;
        }
        std::unique_ptr<Expr> rhs = ParseExpr();
        if (!rhs) {
          return false;
        }
        Statement init_stmt;
        init_stmt.kind = StatementKind::kAssign;
        init_stmt.assign.lhs = name;
        init_stmt.assign.rhs = std::move(rhs);
        init_stmt.assign.nonblocking = false;
        init_statements.push_back(std::move(init_stmt));
      }
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after real declaration");
        return false;
      }
      break;
    }
    if (!init_statements.empty()) {
      AlwaysBlock init_block;
      init_block.edge = EdgeKind::kInitial;
      init_block.clock = "initial";
      init_block.statements = std::move(init_statements);
      module->always_blocks.push_back(std::move(init_block));
    }
    return true;
  }

  bool ParseEventDecl(Module* module) {
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in event declaration");
        return false;
      }
      module->events.push_back(EventDecl{name});
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after event declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseLocalIntegerDecl() {
    const int width = 32;
    bool is_signed = true;
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in integer declaration");
        return false;
      }
      if (current_module_) {
        for (const auto& port : current_module_->ports) {
          if (port.name == name) {
            ErrorHere("local integer redeclares port '" + name + "'");
            return false;
          }
        }
        for (const auto& net : current_module_->nets) {
          if (net.name == name) {
            ErrorHere("local integer redeclares net '" + name + "'");
            return false;
          }
        }
      }
      AddOrUpdateNet(current_module_, name, NetType::kWire, width, is_signed,
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(), {});
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after integer declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseLocalTimeDecl() {
    const int width = 64;
    bool is_signed = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in time declaration");
        return false;
      }
      if (current_module_) {
        for (const auto& port : current_module_->ports) {
          if (port.name == name) {
            ErrorHere("local time redeclares port '" + name + "'");
            return false;
          }
        }
        for (const auto& net : current_module_->nets) {
          if (net.name == name) {
            ErrorHere("local time redeclares net '" + name + "'");
            return false;
          }
        }
      }
      AddOrUpdateNet(current_module_, name, NetType::kWire, width, is_signed,
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(), {});
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after time declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseLocalRealDecl() {
    const int width = 64;
    bool is_signed = true;
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in real declaration");
        return false;
      }
      std::vector<ArrayDim> array_dims;
      while (true) {
        int array_size = 0;
        std::shared_ptr<Expr> array_msb;
        std::shared_ptr<Expr> array_lsb;
        bool had_array = false;
        if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
          return false;
        }
        if (!had_array) {
          break;
        }
        array_dims.push_back(ArrayDim{array_size, array_msb, array_lsb});
      }
      if (current_module_) {
        for (const auto& port : current_module_->ports) {
          if (port.name == name) {
            ErrorHere("local real redeclares port '" + name + "'");
            return false;
          }
        }
        for (const auto& net : current_module_->nets) {
          if (net.name == name) {
            ErrorHere("local real redeclares net '" + name + "'");
            return false;
          }
        }
      }
      AddOrUpdateNet(current_module_, name, NetType::kWire, width, is_signed,
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(),
                     array_dims, true);
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after real declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseLocalRegDecl(Module* module) {
    bool is_signed = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    int width = 1;
    std::shared_ptr<Expr> range_msb;
    std::shared_ptr<Expr> range_lsb;
    if (!ParseRange(&width, &range_msb, &range_lsb, nullptr)) {
      return false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in reg declaration");
        return false;
      }
      std::vector<ArrayDim> array_dims;
      while (true) {
        int array_size = 0;
        std::shared_ptr<Expr> array_msb;
        std::shared_ptr<Expr> array_lsb;
        bool had_array = false;
        if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
          return false;
        }
        if (!had_array) {
          break;
        }
        array_dims.push_back(ArrayDim{array_size, array_msb, array_lsb});
      }
      if (current_module_) {
        for (const auto& port : current_module_->ports) {
          if (port.name == name) {
            ErrorHere("local reg redeclares port '" + name + "'");
            return false;
          }
        }
        for (const auto& net : current_module_->nets) {
          if (net.name == name) {
            ErrorHere("local reg redeclares net '" + name + "'");
            return false;
          }
        }
      }
      AddOrUpdateNet(current_module_, name, NetType::kWire, width, is_signed,
                     range_msb, range_lsb, array_dims);
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after reg declaration");
        return false;
      }
      break;
    }
    return true;
  }

  struct GenvarScope {
    std::vector<std::unordered_set<std::string>> scopes;

    void Reset() {
      scopes.clear();
      scopes.emplace_back();
    }

    void Push() { scopes.emplace_back(); }

    void Pop() {
      if (!scopes.empty()) {
        scopes.pop_back();
      }
    }

    void Declare(const std::string& name) {
      if (scopes.empty()) {
        scopes.emplace_back();
      }
      scopes.back().insert(name);
    }

    bool IsDeclared(const std::string& name) const {
      for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->count(name) > 0) {
          return true;
        }
      }
      return false;
    }
  };

  struct GenvarScopeGuard {
    explicit GenvarScopeGuard(GenvarScope* scope) : scope_(scope) {
      if (scope_) {
        scope_->Push();
      }
    }
    ~GenvarScopeGuard() {
      if (scope_) {
        scope_->Pop();
      }
    }

   private:
    GenvarScope* scope_ = nullptr;
  };

  bool ParseGenvarDecl(GenvarScope* genvars) {
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in genvar declaration");
        return false;
      }
      genvars->Declare(name);
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after genvar declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseStrengthToken(const std::string& token, Strength* strength,
                          int* drive_value) const {
    if (!strength || !drive_value) {
      return false;
    }
    std::string lower = token;
    for (char& c : lower) {
      c = static_cast<char>(std::tolower(c));
    }
    if (lower.size() < 2) {
      return false;
    }
    char last = lower.back();
    if (last != '0' && last != '1') {
      return false;
    }
    int value = last - '0';
    std::string base = lower.substr(0, lower.size() - 1);
    Strength parsed;
    if (base == "supply") {
      parsed = Strength::kSupply;
    } else if (base == "strong") {
      parsed = Strength::kStrong;
    } else if (base == "pull") {
      parsed = Strength::kPull;
    } else if (base == "weak") {
      parsed = Strength::kWeak;
    } else if (base == "highz") {
      parsed = Strength::kHighZ;
    } else {
      return false;
    }
    *strength = parsed;
    *drive_value = value;
    return true;
  }

  bool ParseDriveStrength(Strength* strength0, Strength* strength1,
                          bool* has_strength) {
    if (!strength0 || !strength1 || !has_strength) {
      return false;
    }
    *has_strength = false;
    if (!MatchSymbol("(")) {
      return true;
    }
    Strength first_strength = Strength::kStrong;
    Strength second_strength = Strength::kStrong;
    int first_value = -1;
    int second_value = -1;
    if (!ParseStrengthToken(Peek().text, &first_strength, &first_value)) {
      ErrorHere("expected drive strength after '('");
      return false;
    }
    Advance();
    if (!MatchSymbol(",")) {
      ErrorHere("expected ',' between drive strengths");
      return false;
    }
    if (!ParseStrengthToken(Peek().text, &second_strength, &second_value)) {
      ErrorHere("expected drive strength after ','");
      return false;
    }
    Advance();
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after drive strengths");
      return false;
    }
    if (first_value == second_value) {
      ErrorHere("drive strengths must specify both 0 and 1");
      return false;
    }
    Strength out0 = Strength::kStrong;
    Strength out1 = Strength::kStrong;
    if (first_value == 0) {
      out0 = first_strength;
    } else {
      out1 = first_strength;
    }
    if (second_value == 0) {
      out0 = second_strength;
    } else {
      out1 = second_strength;
    }
    *strength0 = out0;
    *strength1 = out1;
    *has_strength = true;
    return true;
  }

  bool MatchNetType(NetType* out_type) {
    if (!out_type) {
      return false;
    }
    if (MatchKeyword("wire") || MatchKeyword("tri")) {
      *out_type = NetType::kWire;
      return true;
    }
    if (MatchKeyword("wand")) {
      *out_type = NetType::kWand;
      return true;
    }
    if (MatchKeyword("wor")) {
      *out_type = NetType::kWor;
      return true;
    }
    if (MatchKeyword("tri0")) {
      *out_type = NetType::kTri0;
      return true;
    }
    if (MatchKeyword("tri1")) {
      *out_type = NetType::kTri1;
      return true;
    }
    if (MatchKeyword("triand")) {
      *out_type = NetType::kTriand;
      return true;
    }
    if (MatchKeyword("trior")) {
      *out_type = NetType::kTrior;
      return true;
    }
    if (MatchKeyword("trireg")) {
      *out_type = NetType::kTrireg;
      return true;
    }
    if (MatchKeyword("supply0")) {
      *out_type = NetType::kSupply0;
      return true;
    }
    if (MatchKeyword("supply1")) {
      *out_type = NetType::kSupply1;
      return true;
    }
    return false;
  }

  bool NetTypeRequires4State(NetType type) const {
    return type == NetType::kTri0 || type == NetType::kTri1 ||
           type == NetType::kTriand || type == NetType::kTrior ||
           type == NetType::kTrireg;
  }

  bool IsDriveStrengthLookahead() const {
    if (Peek().kind != TokenKind::kSymbol || Peek().text != "(") {
      return false;
    }
    if (Peek(1).kind != TokenKind::kIdentifier) {
      return false;
    }
    Strength strength = Strength::kStrong;
    int value = 0;
    if (!ParseStrengthToken(Peek(1).text, &strength, &value)) {
      return false;
    }
    if (Peek(2).kind != TokenKind::kSymbol || Peek(2).text != ",") {
      return false;
    }
    if (Peek(3).kind != TokenKind::kIdentifier) {
      return false;
    }
    if (!ParseStrengthToken(Peek(3).text, &strength, &value)) {
      return false;
    }
    if (Peek(4).kind != TokenKind::kSymbol || Peek(4).text != ")") {
      return false;
    }
    return true;
  }

  bool ParseDriveStrengthIfPresent(Strength* strength0, Strength* strength1,
                                   bool* has_strength) {
    if (!strength0 || !strength1 || !has_strength) {
      return false;
    }
    if (!IsDriveStrengthLookahead()) {
      *has_strength = false;
      return true;
    }
    return ParseDriveStrength(strength0, strength1, has_strength);
  }

  bool ParseChargeStrengthToken(const std::string& token,
                                ChargeStrength* out) const {
    if (!out) {
      return false;
    }
    std::string lower = token;
    for (char& c : lower) {
      c = static_cast<char>(std::tolower(c));
    }
    if (lower == "small") {
      *out = ChargeStrength::kSmall;
      return true;
    }
    if (lower == "medium") {
      *out = ChargeStrength::kMedium;
      return true;
    }
    if (lower == "large") {
      *out = ChargeStrength::kLarge;
      return true;
    }
    return false;
  }

  bool IsChargeStrengthLookahead() const {
    if (Peek().kind != TokenKind::kSymbol || Peek().text != "(") {
      return false;
    }
    if (Peek(1).kind != TokenKind::kIdentifier) {
      return false;
    }
    ChargeStrength strength = ChargeStrength::kNone;
    if (!ParseChargeStrengthToken(Peek(1).text, &strength)) {
      return false;
    }
    if (Peek(2).kind != TokenKind::kSymbol || Peek(2).text != ")") {
      return false;
    }
    return true;
  }

  bool ParseChargeStrengthIfPresent(ChargeStrength* out_strength,
                                    bool* has_strength) {
    if (!out_strength || !has_strength) {
      return false;
    }
    if (!IsChargeStrengthLookahead()) {
      *has_strength = false;
      *out_strength = ChargeStrength::kNone;
      return true;
    }
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' for charge strength");
      return false;
    }
    if (Peek().kind != TokenKind::kIdentifier ||
        !ParseChargeStrengthToken(Peek().text, out_strength)) {
      ErrorHere("expected charge strength");
      return false;
    }
    Advance();
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after charge strength");
      return false;
    }
    *has_strength = true;
    return true;
  }

  bool SkipDelayControl() {
    if (MatchSymbol("(")) {
      int depth = 1;
      while (!IsAtEnd() && depth > 0) {
        if (MatchSymbol("(")) {
          ++depth;
          continue;
        }
        if (MatchSymbol(")")) {
          --depth;
          continue;
        }
        Advance();
      }
      if (depth != 0) {
        ErrorHere("expected ')' after delay control");
        return false;
      }
      return true;
    }
    if (Peek().kind == TokenKind::kNumber ||
        Peek().kind == TokenKind::kIdentifier) {
      Advance();
      return true;
    }
    ErrorHere("expected delay value after '#'");
    return false;
  }

  std::unique_ptr<Expr> MakeNumberExpr(uint64_t value) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kNumber;
    expr->number = value;
    expr->value_bits = value;
    return expr;
  }

  std::unique_ptr<Expr> MakeZExpr(int width) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kNumber;
    expr->number = 0;
    expr->value_bits = 0;
    expr->x_bits = 0;
    if (width >= 64) {
      expr->z_bits = 0xFFFFFFFFFFFFFFFFull;
    } else if (width > 0) {
      expr->z_bits = (1ull << width) - 1ull;
    }
    expr->has_width = true;
    expr->number_width = (width > 0) ? width : 1;
    expr->has_base = true;
    expr->base_char = 'b';
    return expr;
  }

  std::unique_ptr<Expr> MakeUnaryExpr(char op, std::unique_ptr<Expr> operand) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kUnary;
    expr->unary_op = op;
    expr->operand = std::move(operand);
    return expr;
  }

  std::unique_ptr<Expr> MakeTernaryExpr(std::unique_ptr<Expr> condition,
                                        std::unique_ptr<Expr> then_expr,
                                        std::unique_ptr<Expr> else_expr) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kTernary;
    expr->condition = std::move(condition);
    expr->then_expr = std::move(then_expr);
    expr->else_expr = std::move(else_expr);
    return expr;
  }

  int LookupSignalWidth(const std::string& name) const {
    if (!current_module_) {
      return -1;
    }
    for (const auto& port : current_module_->ports) {
      if (port.name == name) {
        return port.width;
      }
    }
    for (const auto& net : current_module_->nets) {
      if (net.name == name) {
        return net.width;
      }
    }
    return -1;
  }

  std::unique_ptr<Expr> CloneExprGenerate(
      const Expr& expr,
      const std::unordered_map<std::string, std::string>& renames,
      const std::unordered_map<std::string, int64_t>& consts) {
    if (expr.kind == ExprKind::kIdentifier) {
      auto it = renames.find(expr.ident);
      if (it != renames.end()) {
        auto out = std::make_unique<Expr>();
        out->kind = ExprKind::kIdentifier;
        out->ident = it->second;
        return out;
      }
      auto cit = consts.find(expr.ident);
      if (cit != consts.end()) {
        return MakeNumberExpr(static_cast<uint64_t>(cit->second));
      }
    }
    auto out = std::make_unique<Expr>();
    out->kind = expr.kind;
    out->ident = expr.ident;
    out->number = expr.number;
    out->value_bits = expr.value_bits;
    out->x_bits = expr.x_bits;
    out->z_bits = expr.z_bits;
    out->number_width = expr.number_width;
    out->has_width = expr.has_width;
    out->has_base = expr.has_base;
    out->base_char = expr.base_char;
    out->is_signed = expr.is_signed;
    out->is_real_literal = expr.is_real_literal;
    out->op = expr.op;
    out->unary_op = expr.unary_op;
    out->msb = expr.msb;
    out->lsb = expr.lsb;
    out->has_range = expr.has_range;
    out->indexed_range = expr.indexed_range;
    out->indexed_desc = expr.indexed_desc;
    out->indexed_width = expr.indexed_width;
    out->repeat = expr.repeat;
    if (expr.operand) {
      out->operand = CloneExprGenerate(*expr.operand, renames, consts);
    }
    if (expr.lhs) {
      out->lhs = CloneExprGenerate(*expr.lhs, renames, consts);
    }
    if (expr.rhs) {
      out->rhs = CloneExprGenerate(*expr.rhs, renames, consts);
    }
    if (expr.condition) {
      out->condition = CloneExprGenerate(*expr.condition, renames, consts);
    }
    if (expr.then_expr) {
      out->then_expr = CloneExprGenerate(*expr.then_expr, renames, consts);
    }
    if (expr.else_expr) {
      out->else_expr = CloneExprGenerate(*expr.else_expr, renames, consts);
    }
    if (expr.base) {
      out->base = CloneExprGenerate(*expr.base, renames, consts);
    }
    if (expr.index) {
      out->index = CloneExprGenerate(*expr.index, renames, consts);
    }
    if (expr.msb_expr) {
      out->msb_expr = CloneExprGenerate(*expr.msb_expr, renames, consts);
    }
    if (expr.lsb_expr) {
      out->lsb_expr = CloneExprGenerate(*expr.lsb_expr, renames, consts);
    }
    if (expr.repeat_expr) {
      out->repeat_expr =
          CloneExprGenerate(*expr.repeat_expr, renames, consts);
    }
    for (const auto& element : expr.elements) {
      out->elements.push_back(CloneExprGenerate(*element, renames, consts));
    }
    for (const auto& arg : expr.call_args) {
      out->call_args.push_back(CloneExprGenerate(*arg, renames, consts));
    }
    if (out->kind == ExprKind::kSelect && out->msb_expr && out->lsb_expr) {
      int64_t msb = 0;
      int64_t lsb = 0;
      if (TryEvalConstExpr(*out->msb_expr, &msb) &&
          TryEvalConstExpr(*out->lsb_expr, &lsb)) {
        out->msb = static_cast<int>(msb);
        out->lsb = static_cast<int>(lsb);
      }
    }
    return out;
  }

  std::unique_ptr<Expr> CloneExprSimple(const Expr& expr) {
    const std::unordered_map<std::string, std::string> empty_renames;
    const std::unordered_map<std::string, int64_t> empty_consts;
    return CloneExprGenerate(expr, empty_renames, empty_consts);
  }

  bool ParseGenerateNetDecl(NetType type,
                            std::vector<GeneratedNetDecl>* out_decls) {
    bool is_signed = false;
    Strength strength0 = Strength::kStrong;
    Strength strength1 = Strength::kStrong;
    bool has_strength = false;
    ChargeStrength charge = ChargeStrength::kNone;
    bool has_charge = false;
    bool progressed = true;
    while (progressed) {
      progressed = false;
      if (!has_strength && IsDriveStrengthLookahead()) {
        if (!ParseDriveStrength(&strength0, &strength1, &has_strength)) {
          return false;
        }
        progressed = true;
      }
      if (!is_signed && MatchKeyword("signed")) {
        is_signed = true;
        progressed = true;
      }
      if (type == NetType::kTrireg && !has_charge &&
          IsChargeStrengthLookahead()) {
        if (!ParseChargeStrengthIfPresent(&charge, &has_charge)) {
          return false;
        }
        progressed = true;
      }
    }
    if (NetTypeRequires4State(type) && !options_.enable_4state) {
      ErrorHere("net type requires --4state");
      return false;
    }
    int width = 1;
    std::shared_ptr<Expr> range_msb;
    std::shared_ptr<Expr> range_lsb;
    if (!ParseRange(&width, &range_msb, &range_lsb, nullptr)) {
      return false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in declaration");
        return false;
      }
      std::vector<ArrayDim> array_dims;
      while (true) {
        int array_size = 0;
        std::shared_ptr<Expr> array_msb;
        std::shared_ptr<Expr> array_lsb;
        bool had_array = false;
        if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
          return false;
        }
        if (!had_array) {
          break;
        }
        array_dims.push_back(ArrayDim{array_size, array_msb, array_lsb});
      }
      if (MatchSymbol("=")) {
        ErrorHere("initializer not supported in generate declaration");
        return false;
      }
      GeneratedNetDecl decl;
      decl.type = type;
      decl.name = name;
      decl.width = width;
      decl.is_signed = is_signed;
      decl.charge = charge;
      decl.msb_expr = range_msb;
      decl.lsb_expr = range_lsb;
      decl.array_dims = array_dims;
      out_decls->push_back(std::move(decl));
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after declaration");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseGenerateAssign(GenerateAssign* out) {
    Strength strength0 = Strength::kStrong;
    Strength strength1 = Strength::kStrong;
    bool has_strength = false;
    if (!ParseDriveStrength(&strength0, &strength1, &has_strength)) {
      return false;
    }
    if (MatchSymbol("#")) {
      if (!SkipDelayControl()) {
        return false;
      }
    }
    std::string lhs;
    if (!ConsumeHierIdentifier(&lhs)) {
      ErrorHere("expected identifier after 'assign'");
      return false;
    }
    GenerateAssign assign;
    assign.lhs = lhs;
    assign.strength0 = strength0;
    assign.strength1 = strength1;
    assign.has_strength = has_strength;
    if (MatchSymbol("[")) {
      std::unique_ptr<Expr> msb_expr = ParseExpr();
      if (!msb_expr) {
        return false;
      }
      if (MatchSymbol("+:") || MatchSymbol("-:")) {
        bool indexed_desc = (Previous().text == "-:");
        std::unique_ptr<Expr> width_expr = ParseExpr();
        if (!width_expr) {
          return false;
        }
        int64_t width_value = 0;
        if (!EvalConstExpr(*width_expr, &width_value) || width_value <= 0) {
          ErrorHere("indexed part select width must be constant");
          return false;
        }
        auto base_clone = CloneExprSimple(*msb_expr);
        auto width_minus = MakeNumberExpr(
            static_cast<uint64_t>(width_value - 1));
        if (indexed_desc) {
          assign.lhs_has_range = true;
          assign.lhs_is_range = true;
          assign.lhs_msb_expr = std::move(msb_expr);
          assign.lhs_lsb_expr = MakeBinary('-', std::move(base_clone),
                                           std::move(width_minus));
        } else {
          assign.lhs_has_range = true;
          assign.lhs_is_range = true;
          assign.lhs_lsb_expr = std::move(msb_expr);
          assign.lhs_msb_expr = MakeBinary('+', std::move(base_clone),
                                           std::move(width_minus));
        }
      } else if (MatchSymbol(":")) {
        std::unique_ptr<Expr> lsb_expr = ParseExpr();
        if (!lsb_expr) {
          return false;
        }
        assign.lhs_has_range = true;
        assign.lhs_is_range = true;
        assign.lhs_msb_expr = std::move(msb_expr);
        assign.lhs_lsb_expr = std::move(lsb_expr);
      } else {
        assign.lhs_has_range = true;
        assign.lhs_is_range = false;
        assign.lhs_msb_expr = std::move(msb_expr);
      }
      if (!MatchSymbol("]")) {
        ErrorHere("expected ']' after select");
        return false;
      }
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in assign");
      return false;
    }
    assign.rhs = ParseExpr();
    if (!assign.rhs) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after assign");
      return false;
    }
    *out = std::move(assign);
    return true;
  }

  bool ParseGenerateInstance(Instance* out_instance) {
    std::string module_name;
    std::string instance_name;
    if (!ConsumeIdentifier(&module_name)) {
      ErrorHere("expected module name in instance");
      return false;
    }
    Instance instance;
    instance.module_name = std::move(module_name);
    if (MatchSymbol("#")) {
      if (!ParseParamOverrides(&instance)) {
        return false;
      }
    }
    if (!ConsumeIdentifier(&instance_name)) {
      ErrorHere("expected instance name");
      return false;
    }
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after instance name");
      return false;
    }
    instance.name = std::move(instance_name);
    if (!MatchSymbol(")")) {
      bool named = false;
      if (Peek().kind == TokenKind::kSymbol && Peek().text == ".") {
        named = true;
      }
      if (named) {
        while (true) {
          if (!MatchSymbol(".")) {
            ErrorHere("expected named port connection ('.port(signal)')");
            return false;
          }
          std::string port_name;
          if (!ConsumeIdentifier(&port_name)) {
            ErrorHere("expected port name after '.'");
            return false;
          }
          if (!MatchSymbol("(")) {
            ErrorHere("expected '(' after port name");
            return false;
          }
          std::unique_ptr<Expr> expr;
          if (!MatchSymbol(")")) {
            expr = ParseExpr();
            if (!expr) {
              return false;
            }
            if (!MatchSymbol(")")) {
              ErrorHere("expected ')' after port expression");
              return false;
            }
          }
          instance.connections.push_back(
              Connection{port_name, std::move(expr)});
          if (MatchSymbol(",")) {
            continue;
          }
          break;
        }
      } else {
        int position = 0;
        while (true) {
          std::unique_ptr<Expr> expr;
          if (!(Peek().kind == TokenKind::kSymbol &&
                (Peek().text == "," || Peek().text == ")"))) {
            expr = ParseExpr();
            if (!expr) {
              return false;
            }
          }
          instance.connections.push_back(
              Connection{std::to_string(position), std::move(expr)});
          ++position;
          if (MatchSymbol(",")) {
            continue;
          }
          break;
        }
      }
      if (!MatchSymbol(")")) {
        ErrorHere("expected ')' after instance connections");
        return false;
      }
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after instance");
      return false;
    }
    *out_instance = std::move(instance);
    return true;
  }

  struct GenerateContext {
    std::unordered_map<std::string, std::string> renames;
    std::unordered_map<std::string, int64_t> consts;
    std::shared_ptr<Expr> guard;
  };

  std::string RenameIdent(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& renames) {
    auto it = renames.find(name);
    if (it != renames.end()) {
      return it->second;
    }
    return name;
  }

  bool EvalConstExprWithContext(const Expr& expr, const GenerateContext& ctx,
                                int64_t* out_value) {
    auto cloned = CloneExprGenerate(expr, ctx.renames, ctx.consts);
    if (!cloned) {
      return false;
    }
    return EvalConstExpr(*cloned, out_value);
  }

  int LookupSignalWidth(const Module& module,
                        const std::string& name) const {
    for (const auto& net : module.nets) {
      if (net.name == name) {
        return net.width;
      }
    }
    for (const auto& port : module.ports) {
      if (port.name == name) {
        return port.width;
      }
    }
    return 1;
  }

  bool IsModuleParamName(const Module& module,
                         const std::string& name) const {
    for (const auto& param : module.parameters) {
      if (param.name == name) {
        return true;
      }
    }
    return false;
  }

  bool ExprUsesOnlyConstsOrParams(const Expr& expr, const GenerateContext& ctx,
                                  const Module& module) const {
    if (expr.kind == ExprKind::kIdentifier) {
      if (ctx.consts.count(expr.ident) != 0) {
        return true;
      }
      return IsModuleParamName(module, expr.ident);
    }
    if (expr.kind == ExprKind::kNumber) {
      return true;
    }
    if (expr.kind == ExprKind::kCall || expr.kind == ExprKind::kString) {
      return false;
    }
    auto check = [&](const std::unique_ptr<Expr>& subexpr) -> bool {
      return subexpr ? ExprUsesOnlyConstsOrParams(*subexpr, ctx, module) : true;
    };
    if (!check(expr.operand) || !check(expr.lhs) || !check(expr.rhs) ||
        !check(expr.condition) || !check(expr.then_expr) ||
        !check(expr.else_expr) || !check(expr.base) || !check(expr.index) ||
        !check(expr.msb_expr) || !check(expr.lsb_expr) ||
        !check(expr.repeat_expr)) {
      return false;
    }
    for (const auto& element : expr.elements) {
      if (element && !ExprUsesOnlyConstsOrParams(*element, ctx, module)) {
        return false;
      }
    }
    for (const auto& arg : expr.call_args) {
      if (arg && !ExprUsesOnlyConstsOrParams(*arg, ctx, module)) {
        return false;
      }
    }
    return true;
  }

  bool ExprUsesOverridableParam(const Expr& expr,
                                const Module& module) const {
    if (expr.kind == ExprKind::kIdentifier) {
      for (const auto& param : module.parameters) {
        if (param.name == expr.ident) {
          return !param.is_local;
        }
      }
      return false;
    }
    auto check = [&](const std::unique_ptr<Expr>& subexpr) -> bool {
      return subexpr ? ExprUsesOverridableParam(*subexpr, module) : false;
    };
    if (check(expr.operand) || check(expr.lhs) || check(expr.rhs) ||
        check(expr.condition) || check(expr.then_expr) ||
        check(expr.else_expr) || check(expr.base) || check(expr.index) ||
        check(expr.msb_expr) || check(expr.lsb_expr) ||
        check(expr.repeat_expr)) {
      return true;
    }
    for (const auto& element : expr.elements) {
      if (element && ExprUsesOverridableParam(*element, module)) {
        return true;
      }
    }
    for (const auto& arg : expr.call_args) {
      if (arg && ExprUsesOverridableParam(*arg, module)) {
        return true;
      }
    }
    return false;
  }

  std::shared_ptr<Expr> CombineGuard(const std::shared_ptr<Expr>& base,
                                     std::unique_ptr<Expr> extra) {
    if (!extra) {
      return base;
    }
    if (!base) {
      return std::shared_ptr<Expr>(extra.release());
    }
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kBinary;
    expr->op = 'A';
    expr->lhs = CloneExpr(*base);
    expr->rhs = std::move(extra);
    return std::shared_ptr<Expr>(expr.release());
  }

  struct ConstBits {
    uint64_t value = 0;
    uint64_t x = 0;
    uint64_t z = 0;
    int width = 0;
  };

  uint64_t MaskForWidth64(int width) const {
    if (width <= 0) {
      return 0;
    }
    if (width >= 64) {
      return 0xFFFFFFFFFFFFFFFFull;
    }
    return (1ull << width) - 1ull;
  }

  int ConstExprWidth(const Expr& expr) const {
    if (expr.has_width && expr.number_width > 0) {
      return expr.number_width;
    }
    return 32;
  }

  bool EvalConstBits(const Expr& expr, ConstBits* out_bits) {
    if (!out_bits) {
      return false;
    }
    int width = ConstExprWidth(expr);
    if (width > 64) {
      width = 64;
    }
    const uint64_t mask = MaskForWidth64(width);
    out_bits->width = width;
    if (expr.kind == ExprKind::kNumber) {
      out_bits->value = expr.value_bits & mask;
      out_bits->x = expr.x_bits & mask;
      out_bits->z = expr.z_bits & mask;
      return true;
    }
    int64_t value = 0;
    if (!EvalConstExpr(expr, &value)) {
      return false;
    }
    out_bits->value = static_cast<uint64_t>(value) & mask;
    out_bits->x = 0;
    out_bits->z = 0;
    return true;
  }

  bool EvalConstBitsWithContext(const Expr& expr, const GenerateContext& ctx,
                                ConstBits* out_bits) {
    auto cloned = CloneExprGenerate(expr, ctx.renames, ctx.consts);
    if (!cloned) {
      return false;
    }
    return EvalConstBits(*cloned, out_bits);
  }

  bool MatchGenerateCase(const ConstBits& expr_bits,
                         const ConstBits& label_bits,
                         CaseKind case_kind) const {
    int width = expr_bits.width;
    if (label_bits.width > width) {
      width = label_bits.width;
    }
    if (width > 64) {
      width = 64;
    }
    const uint64_t mask = MaskForWidth64(width);
    const uint64_t expr_val = expr_bits.value & mask;
    const uint64_t expr_x = expr_bits.x & mask;
    const uint64_t expr_z = expr_bits.z & mask;
    const uint64_t label_val = label_bits.value & mask;
    const uint64_t label_x = label_bits.x & mask;
    const uint64_t label_z = label_bits.z & mask;

    if (case_kind == CaseKind::kCase) {
      if (expr_x != label_x || expr_z != label_z) {
        return false;
      }
      const uint64_t known_mask = ~(expr_x | expr_z) & mask;
      return ((expr_val ^ label_val) & known_mask) == 0;
    }
    if (case_kind == CaseKind::kCaseZ) {
      const uint64_t dontcare = (expr_z | label_z) & mask;
      if (((expr_x ^ label_x) & ~dontcare) != 0) {
        return false;
      }
      const uint64_t known_mask =
          ~(expr_x | label_x | expr_z | label_z) & mask;
      return ((expr_val ^ label_val) & known_mask) == 0;
    }
    const uint64_t dontcare = (expr_x | label_x | expr_z | label_z) & mask;
    const uint64_t known_mask = ~dontcare & mask;
    return ((expr_val ^ label_val) & known_mask) == 0;
  }

  bool CloneStatementGenerate(const Statement& statement,
                              const GenerateContext& ctx,
                              Statement* out_statement) {
    out_statement->kind = statement.kind;
    out_statement->block_label = statement.block_label;
    if (statement.kind == StatementKind::kAssign ||
        statement.kind == StatementKind::kForce ||
        statement.kind == StatementKind::kRelease) {
      out_statement->assign.lhs =
          RenameIdent(statement.assign.lhs, ctx.renames);
      out_statement->assign.lhs_has_range = statement.assign.lhs_has_range;
      out_statement->assign.lhs_indexed_range =
          statement.assign.lhs_indexed_range;
      out_statement->assign.lhs_indexed_desc =
          statement.assign.lhs_indexed_desc;
      out_statement->assign.lhs_indexed_width =
          statement.assign.lhs_indexed_width;
      out_statement->assign.lhs_msb = statement.assign.lhs_msb;
      out_statement->assign.lhs_lsb = statement.assign.lhs_lsb;
      if (statement.assign.lhs_index) {
        out_statement->assign.lhs_index =
            CloneExprGenerate(*statement.assign.lhs_index, ctx.renames,
                              ctx.consts);
      }
      if (!statement.assign.lhs_indices.empty()) {
        out_statement->assign.lhs_indices.reserve(
            statement.assign.lhs_indices.size());
        for (const auto& index : statement.assign.lhs_indices) {
          out_statement->assign.lhs_indices.push_back(
              CloneExprGenerate(*index, ctx.renames, ctx.consts));
        }
      }
      if (statement.assign.rhs) {
        out_statement->assign.rhs =
            CloneExprGenerate(*statement.assign.rhs, ctx.renames, ctx.consts);
      }
      if (statement.assign.delay) {
        out_statement->assign.delay =
            CloneExprGenerate(*statement.assign.delay, ctx.renames, ctx.consts);
      }
      if (statement.assign.lhs_msb_expr) {
        out_statement->assign.lhs_msb_expr = CloneExprGenerate(
            *statement.assign.lhs_msb_expr, ctx.renames, ctx.consts);
      }
      if (statement.assign.lhs_lsb_expr) {
        out_statement->assign.lhs_lsb_expr = CloneExprGenerate(
            *statement.assign.lhs_lsb_expr, ctx.renames, ctx.consts);
      }
      if (ctx.guard && out_statement->assign.rhs) {
        int width = 1;
        if (out_statement->assign.lhs_has_range) {
          int64_t msb = 0;
          int64_t lsb = 0;
          if (statement.assign.lhs_msb_expr &&
              EvalConstExprWithContext(*statement.assign.lhs_msb_expr, ctx,
                                       &msb)) {
            if (statement.assign.lhs_lsb_expr) {
              if (EvalConstExprWithContext(*statement.assign.lhs_lsb_expr, ctx,
                                           &lsb)) {
                width = (msb >= lsb)
                            ? static_cast<int>(msb - lsb + 1)
                            : static_cast<int>(lsb - msb + 1);
              } else {
                width = LookupSignalWidth(*current_module_,
                                          out_statement->assign.lhs);
              }
            } else {
              width = 1;
            }
          } else {
            width = LookupSignalWidth(*current_module_,
                                      out_statement->assign.lhs);
          }
        } else if (out_statement->assign.lhs_index) {
          if (IsArrayName(out_statement->assign.lhs)) {
            width = LookupSignalWidth(*current_module_,
                                      out_statement->assign.lhs);
          } else {
            width = 1;
          }
        } else {
          width =
              LookupSignalWidth(*current_module_, out_statement->assign.lhs);
        }
        out_statement->assign.rhs =
            MakeTernaryExpr(CloneExpr(*ctx.guard),
                            std::move(out_statement->assign.rhs),
                            MakeZExpr(width));
      }
      out_statement->assign.nonblocking = statement.assign.nonblocking;
      if (statement.kind == StatementKind::kForce) {
        out_statement->force_target =
            RenameIdent(statement.force_target, ctx.renames);
      }
      if (statement.kind == StatementKind::kRelease) {
        out_statement->release_target =
            RenameIdent(statement.release_target, ctx.renames);
      }
      return true;
    }
    if (statement.kind == StatementKind::kIf) {
      if (statement.condition) {
        out_statement->condition = CloneExprGenerate(
            *statement.condition, ctx.renames, ctx.consts);
      }
      for (const auto& inner : statement.then_branch) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->then_branch.push_back(std::move(cloned));
      }
      for (const auto& inner : statement.else_branch) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->else_branch.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kBlock) {
      for (const auto& inner : statement.block) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->block.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kCase) {
      out_statement->case_kind = statement.case_kind;
      if (statement.case_expr) {
        out_statement->case_expr = CloneExprGenerate(
            *statement.case_expr, ctx.renames, ctx.consts);
      }
      for (const auto& item : statement.case_items) {
        CaseItem cloned_item;
        for (const auto& label : item.labels) {
          cloned_item.labels.push_back(
              CloneExprGenerate(*label, ctx.renames, ctx.consts));
        }
        for (const auto& inner : item.body) {
          Statement cloned;
          if (!CloneStatementGenerate(inner, ctx, &cloned)) {
            return false;
          }
          cloned_item.body.push_back(std::move(cloned));
        }
        out_statement->case_items.push_back(std::move(cloned_item));
      }
      for (const auto& inner : statement.default_branch) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->default_branch.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kFor) {
      out_statement->for_init_lhs = statement.for_init_lhs;
      out_statement->for_step_lhs = statement.for_step_lhs;
      if (statement.for_init_rhs) {
        out_statement->for_init_rhs = CloneExprGenerate(
            *statement.for_init_rhs, ctx.renames, ctx.consts);
      }
      if (statement.for_condition) {
        out_statement->for_condition = CloneExprGenerate(
            *statement.for_condition, ctx.renames, ctx.consts);
      }
      if (statement.for_step_rhs) {
        out_statement->for_step_rhs = CloneExprGenerate(
            *statement.for_step_rhs, ctx.renames, ctx.consts);
      }
      for (const auto& inner : statement.for_body) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->for_body.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kWhile) {
      if (statement.while_condition) {
        out_statement->while_condition = CloneExprGenerate(
            *statement.while_condition, ctx.renames, ctx.consts);
      }
      for (const auto& inner : statement.while_body) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->while_body.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kRepeat) {
      if (statement.repeat_count) {
        out_statement->repeat_count = CloneExprGenerate(
            *statement.repeat_count, ctx.renames, ctx.consts);
      }
      for (const auto& inner : statement.repeat_body) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->repeat_body.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kDelay) {
      if (statement.delay) {
        out_statement->delay =
            CloneExprGenerate(*statement.delay, ctx.renames, ctx.consts);
      }
      for (const auto& inner : statement.delay_body) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->delay_body.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kEventControl) {
      out_statement->event_edge = statement.event_edge;
      for (const auto& item : statement.event_items) {
        EventItem cloned_item;
        cloned_item.edge = item.edge;
        if (item.expr) {
          cloned_item.expr =
              CloneExprGenerate(*item.expr, ctx.renames, ctx.consts);
        }
        out_statement->event_items.push_back(std::move(cloned_item));
      }
      if (statement.event_expr) {
        out_statement->event_expr =
            CloneExprGenerate(*statement.event_expr, ctx.renames, ctx.consts);
      }
      for (const auto& inner : statement.event_body) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->event_body.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kEventTrigger) {
      out_statement->trigger_target =
          RenameIdent(statement.trigger_target, ctx.renames);
      return true;
    }
    if (statement.kind == StatementKind::kWait) {
      if (statement.wait_condition) {
        out_statement->wait_condition =
            CloneExprGenerate(*statement.wait_condition, ctx.renames,
                              ctx.consts);
      }
      for (const auto& inner : statement.wait_body) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->wait_body.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kForever) {
      for (const auto& inner : statement.forever_body) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->forever_body.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kFork) {
      for (const auto& inner : statement.fork_branches) {
        Statement cloned;
        if (!CloneStatementGenerate(inner, ctx, &cloned)) {
          return false;
        }
        out_statement->fork_branches.push_back(std::move(cloned));
      }
      return true;
    }
    if (statement.kind == StatementKind::kDisable) {
      out_statement->disable_target =
          RenameIdent(statement.disable_target, ctx.renames);
      return true;
    }
    if (statement.kind == StatementKind::kTaskCall) {
      out_statement->task_name = statement.task_name;
      for (const auto& arg : statement.task_args) {
        out_statement->task_args.push_back(
            CloneExprGenerate(*arg, ctx.renames, ctx.consts));
      }
      return true;
    }
    return true;
  }

  bool CloneAlwaysGenerate(const AlwaysBlock& block,
                           const GenerateContext& ctx,
                           AlwaysBlock* out_block) {
    out_block->edge = block.edge;
    out_block->clock = RenameIdent(block.clock, ctx.renames);
    out_block->sensitivity = block.sensitivity;
    for (const auto& stmt : block.statements) {
      Statement cloned;
      if (!CloneStatementGenerate(stmt, ctx, &cloned)) {
        return false;
      }
      out_block->statements.push_back(std::move(cloned));
    }
    return true;
  }

  bool EmitGenerateBlock(const GenerateBlock& block,
                         const GenerateContext& parent_ctx,
                         const std::string& prefix,
                         Module* module) {
    GenerateContext ctx = parent_ctx;
    auto child_prefix_for_block =
        [&](const GenerateBlock* child) -> std::string {
      std::string child_prefix = prefix;
      if (!child) {
        return child_prefix;
      }
      if (!child->label.empty()) {
        child_prefix += child->label + "__";
      } else {
        child_prefix += "genblk" + std::to_string(generate_id_++) + "__";
      }
      return child_prefix;
    };
    for (const auto& item : block.items) {
      if (item.kind == GenerateItem::Kind::kNet) {
        ctx.renames[item.net.name] = prefix + item.net.name;
      }
    }

    for (const auto& item : block.items) {
      switch (item.kind) {
        case GenerateItem::Kind::kLocalparam: {
          int64_t value = 0;
          if (!item.localparam.expr ||
              !EvalConstExprWithContext(*item.localparam.expr, ctx, &value)) {
            ErrorHere("invalid localparam expression in generate");
            return false;
          }
          ctx.consts[item.localparam.name] = value;
          break;
        }
        case GenerateItem::Kind::kNet: {
          const auto& decl = item.net;
          std::string name = prefix + decl.name;
          AddOrUpdateNet(module, name, decl.type, decl.width, decl.is_signed,
                         decl.msb_expr, decl.lsb_expr, decl.array_dims, false,
                         decl.charge);
          AddImplicitNetDriver(module, name, decl.type);
          break;
        }
        case GenerateItem::Kind::kAssign: {
          const auto& gen_assign = item.assign;
          Assign assign;
          assign.lhs = RenameIdent(gen_assign.lhs, ctx.renames);
          assign.strength0 = gen_assign.strength0;
          assign.strength1 = gen_assign.strength1;
          assign.has_strength = gen_assign.has_strength;
          if (gen_assign.lhs_has_range) {
            int64_t msb = 0;
            int64_t lsb = 0;
            if (!gen_assign.lhs_msb_expr ||
                !EvalConstExprWithContext(*gen_assign.lhs_msb_expr, ctx,
                                          &msb)) {
              ErrorHere("generate assign select must be constant");
              return false;
            }
            if (gen_assign.lhs_is_range) {
              if (!gen_assign.lhs_lsb_expr ||
                  !EvalConstExprWithContext(*gen_assign.lhs_lsb_expr, ctx,
                                            &lsb)) {
                ErrorHere("generate assign select must be constant");
                return false;
              }
            } else {
              lsb = msb;
            }
            assign.lhs_has_range = true;
            assign.lhs_msb = static_cast<int>(msb);
            assign.lhs_lsb = static_cast<int>(lsb);
          }
          if (gen_assign.rhs) {
            assign.rhs =
                CloneExprGenerate(*gen_assign.rhs, ctx.renames, ctx.consts);
          }
          if (ctx.guard && assign.rhs) {
            int width = 1;
            if (assign.lhs_has_range) {
              width = std::abs(assign.lhs_msb - assign.lhs_lsb) + 1;
            } else {
              width = LookupSignalWidth(*module, assign.lhs);
            }
            assign.rhs = MakeTernaryExpr(CloneExpr(*ctx.guard),
                                         std::move(assign.rhs),
                                         MakeZExpr(width));
          }
          module->assigns.push_back(std::move(assign));
          break;
        }
        case GenerateItem::Kind::kInstance: {
          Instance inst;
          inst.module_name = item.instance.module_name;
          inst.name = prefix + item.instance.name;
          for (const auto& override_item : item.instance.param_overrides) {
            ParamOverride param;
            param.name = override_item.name;
            if (override_item.expr) {
              param.expr = CloneExprGenerate(*override_item.expr, ctx.renames,
                                             ctx.consts);
            }
            inst.param_overrides.push_back(std::move(param));
          }
          for (const auto& conn : item.instance.connections) {
            Connection connection;
            connection.port = conn.port;
            if (conn.expr) {
              connection.expr =
                  CloneExprGenerate(*conn.expr, ctx.renames, ctx.consts);
            }
            inst.connections.push_back(std::move(connection));
          }
          module->instances.push_back(std::move(inst));
          break;
        }
        case GenerateItem::Kind::kAlways:
        case GenerateItem::Kind::kInitial: {
          AlwaysBlock cloned;
          if (!CloneAlwaysGenerate(item.always_block, ctx, &cloned)) {
            return false;
          }
          module->always_blocks.push_back(std::move(cloned));
          break;
        }
        case GenerateItem::Kind::kBlock: {
          if (!item.block) {
            break;
          }
          std::string child_prefix = child_prefix_for_block(item.block.get());
          if (!EmitGenerateBlock(*item.block, ctx, child_prefix, module)) {
            return false;
          }
          break;
        }
        case GenerateItem::Kind::kFor: {
          const auto& gen_for = item.gen_for;
          if (!gen_for.body) {
            break;
          }
          int64_t init_value = 0;
          if (!gen_for.init_expr ||
              !EvalConstExprWithContext(*gen_for.init_expr, ctx,
                                        &init_value)) {
            ErrorHere("generate for init must be constant");
            return false;
          }
          int64_t current = init_value;
          const int kMaxIterations = 100000;
          int iterations = 0;
          std::string block_label = gen_for.body->label.empty()
                                        ? ("genblk" + std::to_string(gen_for.id))
                                        : gen_for.body->label;
          std::string base_prefix = prefix + block_label + "__";
          while (iterations++ < kMaxIterations) {
            GenerateContext iter_ctx = ctx;
            iter_ctx.consts[gen_for.var] = current;
            int64_t cond_value = 0;
            if (!gen_for.cond_expr ||
                !EvalConstExprWithContext(*gen_for.cond_expr, iter_ctx,
                                          &cond_value)) {
              ErrorHere("generate for condition must be constant");
              return false;
            }
            if (cond_value == 0) {
              break;
            }
            std::string iter_prefix =
                base_prefix + std::to_string(current) + "__";
            if (!EmitGenerateBlock(*gen_for.body, iter_ctx, iter_prefix,
                                   module)) {
              return false;
            }
            int64_t next_value = 0;
            if (!gen_for.step_expr ||
                !EvalConstExprWithContext(*gen_for.step_expr, iter_ctx,
                                          &next_value)) {
              ErrorHere("generate for step must be constant");
              return false;
            }
            current = next_value;
          }
          if (iterations >= kMaxIterations) {
            ErrorHere("generate for loop exceeds iteration limit");
            return false;
          }
          break;
        }
        case GenerateItem::Kind::kIf: {
          const auto& gen_if = item.gen_if;
          if (!gen_if.then_block || !gen_if.condition) {
            break;
          }
          const bool uses_overridable =
              ExprUsesOverridableParam(*gen_if.condition, *module);
          int64_t cond_value = 0;
          if (!uses_overridable &&
              EvalConstExprWithContext(*gen_if.condition, ctx, &cond_value)) {
            const GenerateBlock* chosen =
                (cond_value != 0) ? gen_if.then_block.get()
                                  : (gen_if.has_else ? gen_if.else_block.get()
                                                     : nullptr);
            if (chosen) {
              std::string child_prefix = child_prefix_for_block(chosen);
              if (!EmitGenerateBlock(*chosen, ctx, child_prefix, module)) {
                return false;
              }
            }
            break;
          }
          if (!ExprUsesOnlyConstsOrParams(*gen_if.condition, ctx, *module)) {
            ErrorHere("generate if condition must be constant");
            return false;
          }
          auto cond_then =
              CloneExprGenerate(*gen_if.condition, ctx.renames, ctx.consts);
          if (!cond_then) {
            ErrorHere("generate if condition must be constant");
            return false;
          }
          std::unique_ptr<Expr> cond_else = CloneExpr(*cond_then);
          GenerateContext then_ctx = ctx;
          then_ctx.guard = CombineGuard(ctx.guard, std::move(cond_then));
          std::string then_prefix =
              child_prefix_for_block(gen_if.then_block.get());
          if (!EmitGenerateBlock(*gen_if.then_block, then_ctx, then_prefix,
                                 module)) {
            return false;
          }
          if (gen_if.has_else && gen_if.else_block) {
            GenerateContext else_ctx = ctx;
            auto not_cond = MakeUnaryExpr('!', std::move(cond_else));
            else_ctx.guard = CombineGuard(ctx.guard, std::move(not_cond));
            std::string else_prefix =
                child_prefix_for_block(gen_if.else_block.get());
            if (!EmitGenerateBlock(*gen_if.else_block, else_ctx, else_prefix,
                                   module)) {
              return false;
            }
          }
          break;
        }
        case GenerateItem::Kind::kCase: {
          const auto& gen_case = item.gen_case;
          if (!gen_case.expr) {
            break;
          }
          ConstBits case_bits;
          if (!EvalConstBitsWithContext(*gen_case.expr, ctx, &case_bits)) {
            ErrorHere("generate case expression must be constant");
            return false;
          }
          const GenerateBlock* chosen = nullptr;
          for (const auto& case_item : gen_case.items) {
            if (!case_item.body) {
              continue;
            }
            for (const auto& label : case_item.labels) {
              ConstBits label_bits;
              if (!label ||
                  !EvalConstBitsWithContext(*label, ctx, &label_bits)) {
                ErrorHere("generate case label must be constant");
                return false;
              }
              if (MatchGenerateCase(case_bits, label_bits, gen_case.kind)) {
                chosen = case_item.body.get();
                break;
              }
            }
            if (chosen) {
              break;
            }
          }
          if (!chosen && gen_case.default_block) {
            chosen = gen_case.default_block.get();
          }
          if (chosen) {
            std::string child_prefix = child_prefix_for_block(chosen);
            if (!EmitGenerateBlock(*chosen, ctx, child_prefix, module)) {
              return false;
            }
          }
          break;
        }
      }
    }
    return true;
  }

  bool ParseGenerateLocalparam(std::vector<GenerateItem>* out_items) {
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected localparam name");
        return false;
      }
      if (!MatchSymbol("=")) {
        ErrorHere("expected '=' in localparam");
        return false;
      }
      std::unique_ptr<Expr> expr = ParseExpr();
      if (!expr) {
        return false;
      }
      GenerateItem item;
      item.kind = GenerateItem::Kind::kLocalparam;
      item.localparam.name = name;
      item.localparam.expr = std::move(expr);
      out_items->push_back(std::move(item));
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after localparam");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseGenerateBlockBody(GenerateBlock* out_block, GenvarScope* genvars) {
    GenvarScopeGuard scope_guard(genvars);
    out_block->label.clear();
    out_block->items.clear();
    if (MatchKeyword("begin")) {
      if (MatchSymbol(":")) {
        std::string label;
        if (!ConsumeIdentifier(&label)) {
          ErrorHere("expected label after ':'");
          return false;
        }
        out_block->label = label;
      }
      while (true) {
        if (MatchKeyword("end")) {
          break;
        }
        if (!ParseGenerateItem(out_block, genvars)) {
          return false;
        }
      }
      return true;
    }
    return ParseGenerateItem(out_block, genvars);
  }

  bool ParseGenerateFor(std::vector<GenerateItem>* out_items,
                        GenvarScope* genvars) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'for'");
      return false;
    }
    std::string var;
    if (!ConsumeIdentifier(&var)) {
      ErrorHere("expected loop variable in generate for");
      return false;
    }
    if (!genvars->IsDeclared(var)) {
      ErrorHere("generate for loop variable must be a genvar");
      return false;
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in generate for init");
      return false;
    }
    std::unique_ptr<Expr> init_expr = ParseExpr();
    if (!init_expr) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after generate for init");
      return false;
    }
    std::unique_ptr<Expr> cond_expr = ParseExpr();
    if (!cond_expr) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after generate for condition");
      return false;
    }
    std::string step_lhs;
    if (!ConsumeIdentifier(&step_lhs)) {
      ErrorHere("expected loop variable in generate for step");
      return false;
    }
    if (step_lhs != var) {
      ErrorHere("generate for step must update loop variable");
      return false;
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in generate for step");
      return false;
    }
    std::unique_ptr<Expr> step_expr = ParseExpr();
    if (!step_expr) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after generate for step");
      return false;
    }
    auto body = std::make_unique<GenerateBlock>();
    if (!ParseGenerateBlockBody(body.get(), genvars)) {
      return false;
    }

    GenerateItem item;
    item.kind = GenerateItem::Kind::kFor;
    item.gen_for.var = var;
    item.gen_for.init_expr = std::move(init_expr);
    item.gen_for.cond_expr = std::move(cond_expr);
    item.gen_for.step_expr = std::move(step_expr);
    item.gen_for.body = std::move(body);
    item.gen_for.id = generate_id_++;
    out_items->push_back(std::move(item));
    return true;
  }

  bool ParseGenerateIf(std::vector<GenerateItem>* out_items,
                       GenvarScope* genvars) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'if'");
      return false;
    }
    std::unique_ptr<Expr> condition = ParseExpr();
    if (!condition) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after generate if condition");
      return false;
    }
    auto then_block = std::make_unique<GenerateBlock>();
    if (!ParseGenerateBlockBody(then_block.get(), genvars)) {
      return false;
    }
    std::unique_ptr<GenerateBlock> else_block;
    bool has_else = false;
    if (MatchKeyword("else")) {
      has_else = true;
      if (MatchKeyword("if")) {
        auto nested_block = std::make_unique<GenerateBlock>();
        if (!ParseGenerateIf(&nested_block->items, genvars)) {
          return false;
        }
        else_block = std::move(nested_block);
      } else {
        else_block = std::make_unique<GenerateBlock>();
        if (!ParseGenerateBlockBody(else_block.get(), genvars)) {
          return false;
        }
      }
    }
    GenerateItem item;
    item.kind = GenerateItem::Kind::kIf;
    item.gen_if.condition = std::move(condition);
    item.gen_if.then_block = std::move(then_block);
    item.gen_if.has_else = has_else;
    item.gen_if.else_block = std::move(else_block);
    out_items->push_back(std::move(item));
    return true;
  }

  bool ParseGenerateCase(std::vector<GenerateItem>* out_items,
                         GenvarScope* genvars, CaseKind case_kind) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'case'");
      return false;
    }
    std::unique_ptr<Expr> case_expr = ParseExpr();
    if (!case_expr) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after case expression");
      return false;
    }
    GenerateCase gen_case;
    gen_case.kind = case_kind;
    gen_case.expr = std::move(case_expr);
    bool saw_default = false;
    while (true) {
      if (MatchKeyword("endcase")) {
        break;
      }
      if (MatchKeyword("default")) {
        if (saw_default) {
          ErrorHere("duplicate default in generate case");
          return false;
        }
        saw_default = true;
        if (!MatchSymbol(":")) {
          ErrorHere("expected ':' after default");
          return false;
        }
        auto block = std::make_unique<GenerateBlock>();
        if (!ParseGenerateBlockBody(block.get(), genvars)) {
          return false;
        }
        gen_case.default_block = std::move(block);
        continue;
      }
      GenerateCaseItem item;
      while (true) {
        std::unique_ptr<Expr> label = ParseExpr();
        if (!label) {
          return false;
        }
        item.labels.push_back(std::move(label));
        if (MatchSymbol(",")) {
          continue;
        }
        break;
      }
      if (!MatchSymbol(":")) {
        ErrorHere("expected ':' after case item");
        return false;
      }
      item.body = std::make_unique<GenerateBlock>();
      if (!ParseGenerateBlockBody(item.body.get(), genvars)) {
        return false;
      }
      gen_case.items.push_back(std::move(item));
    }
    GenerateItem item;
    item.kind = GenerateItem::Kind::kCase;
    item.gen_case = std::move(gen_case);
    out_items->push_back(std::move(item));
    return true;
  }

  bool ParseGenerateItem(GenerateBlock* out_block, GenvarScope* genvars) {
    if (MatchKeyword("genvar")) {
      return ParseGenvarDecl(genvars);
    }
    if (MatchKeyword("localparam")) {
      return ParseGenerateLocalparam(&out_block->items);
    }
    if (MatchKeyword("for")) {
      return ParseGenerateFor(&out_block->items, genvars);
    }
    if (MatchKeyword("if")) {
      return ParseGenerateIf(&out_block->items, genvars);
    }
    if (MatchKeyword("casez")) {
      return ParseGenerateCase(&out_block->items, genvars, CaseKind::kCaseZ);
    }
    if (MatchKeyword("casex")) {
      return ParseGenerateCase(&out_block->items, genvars, CaseKind::kCaseX);
    }
    if (MatchKeyword("case")) {
      return ParseGenerateCase(&out_block->items, genvars, CaseKind::kCase);
    }
    if (MatchKeyword("begin")) {
      auto block = std::make_unique<GenerateBlock>();
      GenvarScopeGuard scope_guard(genvars);
      if (MatchSymbol(":")) {
        std::string label;
        if (!ConsumeIdentifier(&label)) {
          ErrorHere("expected label after ':'");
          return false;
        }
        block->label = label;
      }
      while (true) {
        if (MatchKeyword("end")) {
          break;
        }
      if (!ParseGenerateItem(block.get(), genvars)) {
        return false;
      }
      }
      GenerateItem item;
      item.kind = GenerateItem::Kind::kBlock;
      item.block = std::move(block);
      out_block->items.push_back(std::move(item));
      return true;
    }
    NetType net_type = NetType::kWire;
    if (MatchNetType(&net_type)) {
      std::vector<GeneratedNetDecl> decls;
      if (!ParseGenerateNetDecl(net_type, &decls)) {
        return false;
      }
      for (auto& decl : decls) {
        GenerateItem item;
        item.kind = GenerateItem::Kind::kNet;
        item.net = std::move(decl);
        out_block->items.push_back(std::move(item));
      }
      return true;
    }
    if (MatchKeyword("reg")) {
      std::vector<GeneratedNetDecl> decls;
      if (!ParseGenerateNetDecl(NetType::kReg, &decls)) {
        return false;
      }
      for (auto& decl : decls) {
        GenerateItem item;
        item.kind = GenerateItem::Kind::kNet;
        item.net = std::move(decl);
        out_block->items.push_back(std::move(item));
      }
      return true;
    }
    if (MatchKeyword("assign")) {
      GenerateAssign assign;
      if (!ParseGenerateAssign(&assign)) {
        return false;
      }
      GenerateItem item;
      item.kind = GenerateItem::Kind::kAssign;
      item.assign = std::move(assign);
      out_block->items.push_back(std::move(item));
      return true;
    }
    if (MatchKeyword("always")) {
      AlwaysBlock block;
      if (!ParseAlwaysBlock(&block)) {
        return false;
      }
      GenerateItem item;
      item.kind = GenerateItem::Kind::kAlways;
      item.always_block = std::move(block);
      out_block->items.push_back(std::move(item));
      return true;
    }
    if (MatchKeyword("initial")) {
      AlwaysBlock block;
      if (!ParseInitialBlock(&block)) {
        return false;
      }
      GenerateItem item;
      item.kind = GenerateItem::Kind::kInitial;
      item.always_block = std::move(block);
      out_block->items.push_back(std::move(item));
      return true;
    }
    if (Peek().kind == TokenKind::kIdentifier &&
        IsGatePrimitiveKeyword(Peek().text)) {
      std::string gate = Peek().text;
      Advance();
      std::vector<GateAssign> gate_assigns;
      if (!ParseGatePrimitiveAssignments(gate, &gate_assigns, true)) {
        return false;
      }
      for (auto& gate_assign : gate_assigns) {
        if (!gate_assign.lhs_indices.empty()) {
          AlwaysBlock block;
          block.edge = EdgeKind::kCombinational;
          block.sensitivity = "*";
          Statement stmt;
          stmt.kind = StatementKind::kAssign;
          stmt.assign.lhs = gate_assign.lhs;
          stmt.assign.lhs_has_range = gate_assign.lhs_has_range;
          stmt.assign.lhs_msb_expr = std::move(gate_assign.lhs_msb_expr);
          stmt.assign.lhs_lsb_expr = std::move(gate_assign.lhs_lsb_expr);
          stmt.assign.lhs_msb = gate_assign.lhs_msb;
          stmt.assign.lhs_lsb = gate_assign.lhs_lsb;
          for (auto& idx : gate_assign.lhs_indices) {
            stmt.assign.lhs_indices.push_back(std::move(idx));
          }
          stmt.assign.rhs = std::move(gate_assign.rhs);
          stmt.assign.nonblocking = false;
          block.statements.push_back(std::move(stmt));
          GenerateItem item;
          item.kind = GenerateItem::Kind::kAlways;
          item.always_block = std::move(block);
          out_block->items.push_back(std::move(item));
          continue;
        }
        GenerateAssign assign;
        assign.lhs = gate_assign.lhs;
        assign.lhs_has_range = gate_assign.lhs_has_range;
        assign.lhs_is_range = gate_assign.lhs_is_range;
        if (gate_assign.lhs_has_range) {
          if (gate_assign.lhs_msb_expr) {
            assign.lhs_msb_expr = std::move(gate_assign.lhs_msb_expr);
          } else {
            assign.lhs_msb_expr =
                MakeNumberExpr(static_cast<uint64_t>(gate_assign.lhs_msb));
          }
          if (gate_assign.lhs_is_range) {
            if (gate_assign.lhs_lsb_expr) {
              assign.lhs_lsb_expr = std::move(gate_assign.lhs_lsb_expr);
            } else {
              assign.lhs_lsb_expr =
                  MakeNumberExpr(static_cast<uint64_t>(gate_assign.lhs_lsb));
            }
          }
        }
        assign.rhs = std::move(gate_assign.rhs);
        assign.strength0 = gate_assign.strength0;
        assign.strength1 = gate_assign.strength1;
        assign.has_strength = gate_assign.has_strength;
        GenerateItem item;
        item.kind = GenerateItem::Kind::kAssign;
        item.assign = std::move(assign);
        out_block->items.push_back(std::move(item));
      }
      return true;
    }
    if (Peek().kind == TokenKind::kIdentifier &&
        IsSwitchPrimitiveKeyword(Peek().text)) {
      ErrorHere("switch primitives not supported in generate blocks in v0");
      return false;
    }
    if (Peek().kind == TokenKind::kIdentifier) {
      Instance instance;
      if (!ParseGenerateInstance(&instance)) {
        return false;
      }
      GenerateItem item;
      item.kind = GenerateItem::Kind::kInstance;
      item.instance = std::move(instance);
      out_block->items.push_back(std::move(item));
      return true;
    }
    ErrorHere("unsupported generate item in v0");
    return false;
  }

  bool ParseGenerateBlock(Module* module) {
    GenerateBlock block;
    GenvarScopeGuard scope_guard(&current_genvars_);
    while (true) {
      if (MatchKeyword("endgenerate")) {
        break;
      }
      if (!ParseGenerateItem(&block, &current_genvars_)) {
        return false;
      }
    }
    GenerateContext ctx;
    if (!EmitGenerateBlock(block, ctx, "", module)) {
      return false;
    }
    return true;
  }

  bool ParseAssign(Module* module) {
    Strength strength0 = Strength::kStrong;
    Strength strength1 = Strength::kStrong;
    bool has_strength = false;
    if (!ParseDriveStrength(&strength0, &strength1, &has_strength)) {
      return false;
    }
    if (MatchSymbol("#")) {
      if (!SkipDelayControl()) {
        return false;
      }
    }
    std::string lhs;
    if (!ConsumeHierIdentifier(&lhs)) {
      ErrorHere("expected identifier after 'assign'");
      return false;
    }
    Assign assign;
    assign.lhs = lhs;
    assign.strength0 = strength0;
    assign.strength1 = strength1;
    assign.has_strength = has_strength;
    if (MatchSymbol("[")) {
      std::unique_ptr<Expr> msb_expr = ParseExpr();
      if (!msb_expr) {
        return false;
      }
      if (MatchSymbol("+:") || MatchSymbol("-:")) {
        bool indexed_desc = (Previous().text == "-:");
        std::unique_ptr<Expr> width_expr = ParseExpr();
        if (!width_expr) {
          return false;
        }
        int64_t width_value = 0;
        if (!EvalConstExpr(*width_expr, &width_value) || width_value <= 0) {
          ErrorHere("assign indexed part select width must be constant");
          return false;
        }
        int64_t base_value = 0;
        if (!EvalConstExpr(*msb_expr, &base_value)) {
          ErrorHere("assign indexed part select base must be constant");
          return false;
        }
        int64_t msb = indexed_desc ? base_value : (base_value + width_value - 1);
        int64_t lsb = indexed_desc ? (base_value - width_value + 1) : base_value;
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after part select");
          return false;
        }
        assign.lhs_has_range = true;
        assign.lhs_msb = static_cast<int>(msb);
        assign.lhs_lsb = static_cast<int>(lsb);
      } else if (MatchSymbol(":")) {
        std::unique_ptr<Expr> lsb_expr = ParseExpr();
        if (!lsb_expr) {
          return false;
        }
        int64_t msb = 0;
        int64_t lsb = 0;
        if (!EvalConstExpr(*msb_expr, &msb) ||
            !EvalConstExpr(*lsb_expr, &lsb)) {
          ErrorHere("assign part select requires constant expressions");
          return false;
        }
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after part select");
          return false;
        }
        assign.lhs_has_range = true;
        assign.lhs_msb = static_cast<int>(msb);
        assign.lhs_lsb = static_cast<int>(lsb);
      } else {
        int64_t index = 0;
        if (!EvalConstExpr(*msb_expr, &index)) {
          ErrorHere("assign bit select requires constant expression");
          return false;
        }
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after bit select");
          return false;
        }
        assign.lhs_has_range = true;
        assign.lhs_msb = static_cast<int>(index);
        assign.lhs_lsb = static_cast<int>(index);
      }
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in assign");
      return false;
    }
    std::unique_ptr<Expr> rhs = ParseExpr();
    if (!rhs) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after assign");
      return false;
    }
    assign.rhs = std::move(rhs);
    if (assign.lhs.find('.') == std::string::npos &&
        LookupSignalWidth(assign.lhs) <= 0) {
      if (default_nettype_none_) {
        ErrorHere("implicit net not allowed with `default_nettype none`");
        return false;
      }
      NetType net_type = default_nettype_;
      if (NetTypeRequires4State(net_type) && !options_.enable_4state) {
        ErrorHere("net type requires --4state");
        return false;
      }
      AddOrUpdateNet(module, assign.lhs, net_type, 1, false, nullptr, nullptr,
                     {});
      AddImplicitNetDriver(module, assign.lhs, net_type);
    }
    module->assigns.push_back(std::move(assign));
    return true;
  }

  bool ParseInitial(Module* module) {
    AlwaysBlock block;
    if (!ParseInitialBlock(&block)) {
      return false;
    }
    module->always_blocks.push_back(std::move(block));
    return true;
  }

  bool ParseAlways(Module* module) {
    AlwaysBlock block;
    if (!ParseAlwaysBlock(&block)) {
      return false;
    }
    module->always_blocks.push_back(std::move(block));
    return true;
  }

  bool ParseInitialBlock(AlwaysBlock* out_block) {
    if (!out_block) {
      return false;
    }
    AlwaysBlock block;
    block.edge = EdgeKind::kInitial;
    block.clock = "initial";
    if (!ParseStatementBody(&block.statements)) {
      return false;
    }
    *out_block = std::move(block);
    return true;
  }

  bool ParseAlwaysBlock(AlwaysBlock* out_block) {
    if (!out_block) {
      return false;
    }
    EdgeKind edge = EdgeKind::kCombinational;
    std::string clock;
    std::string sensitivity;
    bool has_event = false;
    bool saw_star = false;
    std::vector<EventItem> items;
    bool has_delay_control = false;
    if (MatchSymbol("@")) {
      has_event = true;
      bool has_paren = MatchSymbol("(");
      if (!ParseEventList(has_paren, &items, &saw_star, &sensitivity)) {
        return false;
      }
    } else if (!(Peek().kind == TokenKind::kSymbol && Peek().text == "#")) {
      ErrorHere("expected '@' or delay control after 'always'");
      return false;
    } else {
      has_delay_control = true;
    }

    std::vector<Statement> statements;
    if (!ParseStatementBody(&statements)) {
      return false;
    }

    bool complex_sensitivity = false;
    if (has_event && !saw_star) {
      if (items.size() > 1) {
        complex_sensitivity = true;
      } else if (items.size() == 1) {
        if ((items[0].edge == EventEdgeKind::kPosedge ||
             items[0].edge == EventEdgeKind::kNegedge) &&
            items[0].expr &&
            items[0].expr->kind != ExprKind::kIdentifier) {
          complex_sensitivity = true;
        }
      }
    }
    if (!saw_star && !complex_sensitivity && items.size() == 1) {
      if (items[0].edge == EventEdgeKind::kPosedge ||
          items[0].edge == EventEdgeKind::kNegedge) {
        if (items[0].expr && items[0].expr->kind == ExprKind::kIdentifier) {
          edge = (items[0].edge == EventEdgeKind::kPosedge)
                     ? EdgeKind::kPosedge
                     : EdgeKind::kNegedge;
          clock = items[0].expr->ident;
        } else {
          complex_sensitivity = true;
        }
      } else {
        edge = EdgeKind::kCombinational;
      }
    }
    if (has_delay_control) {
      AlwaysBlock block;
      block.edge = EdgeKind::kInitial;
      block.clock = "initial";
      Statement forever_stmt;
      forever_stmt.kind = StatementKind::kForever;
      forever_stmt.forever_body = std::move(statements);
      block.statements.push_back(std::move(forever_stmt));
      *out_block = std::move(block);
      return true;
    }

    if (complex_sensitivity) {
      AlwaysBlock block;
      block.edge = EdgeKind::kInitial;
      block.clock = "initial";
      block.sensitivity = std::move(sensitivity);
      Statement event_stmt;
      event_stmt.kind = StatementKind::kEventControl;
      if (items.size() == 1) {
        event_stmt.event_edge = items[0].edge;
        event_stmt.event_expr = std::move(items[0].expr);
      } else {
        event_stmt.event_items = std::move(items);
      }
      event_stmt.event_body = std::move(statements);
      Statement forever_stmt;
      forever_stmt.kind = StatementKind::kForever;
      forever_stmt.forever_body.push_back(std::move(event_stmt));
      block.statements.push_back(std::move(forever_stmt));
      *out_block = std::move(block);
      return true;
    }

    AlwaysBlock block;
    block.edge = edge;
    block.clock = std::move(clock);
    block.sensitivity = std::move(sensitivity);
    block.statements = std::move(statements);

    *out_block = std::move(block);
    return true;
  }

  bool ParseStatementBody(std::vector<Statement>* out_statements) {
    if (MatchKeyword("begin")) {
      Statement block;
      if (!ParseBlockStatement(&block)) {
        return false;
      }
      if (block.block_label.empty()) {
        for (auto& inner : block.block) {
          out_statements->push_back(std::move(inner));
        }
      } else {
        out_statements->push_back(std::move(block));
      }
      return true;
    }
    if (MatchKeyword("integer")) {
      return ParseLocalIntegerDecl();
    }
    if (MatchKeyword("time")) {
      return ParseLocalTimeDecl();
    }
    if (MatchKeyword("real")) {
      return ParseLocalRealDecl();
    }
    if (MatchKeyword("reg")) {
      return ParseLocalRegDecl(current_module_);
    }
    Statement stmt;
    if (!ParseStatement(&stmt)) {
      return false;
    }
    out_statements->push_back(std::move(stmt));
    return true;
  }

  bool ParseStatement(Statement* out_statement) {
    if (Peek().kind == TokenKind::kSymbol && Peek().text == "#") {
      return ParseDelayStatement(out_statement);
    }
    if (Peek().kind == TokenKind::kSymbol && Peek().text == "@") {
      return ParseEventControlStatement(out_statement);
    }
    if (Peek().kind == TokenKind::kSymbol && Peek().text == "$") {
      return ParseSystemTaskStatement(out_statement);
    }
    if (Peek().kind == TokenKind::kSymbol &&
        (Peek().text == "->" ||
         (Peek().text == "-" && Peek(1).kind == TokenKind::kSymbol &&
          Peek(1).text == ">"))) {
      return ParseEventTriggerStatement(out_statement);
    }
    if (MatchKeyword("force")) {
      return ParseForceStatement(out_statement);
    }
    if (MatchKeyword("release")) {
      return ParseReleaseStatement(out_statement);
    }
    if (MatchKeyword("assert")) {
      return ParseAssertStatement(out_statement);
    }
    if (MatchKeyword("unique")) {
      if (MatchKeyword("casez")) {
        return ParseCaseStatement(out_statement, CaseKind::kCaseZ);
      }
      if (MatchKeyword("casex")) {
        return ParseCaseStatement(out_statement, CaseKind::kCaseX);
      }
      if (MatchKeyword("case")) {
        return ParseCaseStatement(out_statement, CaseKind::kCase);
      }
      if (MatchKeyword("if")) {
        return ParseIfStatement(out_statement);
      }
      ErrorHere("unique statement not supported in v0");
      return false;
    }
    if (MatchKeyword("priority")) {
      if (MatchKeyword("casez")) {
        return ParseCaseStatement(out_statement, CaseKind::kCaseZ);
      }
      if (MatchKeyword("casex")) {
        return ParseCaseStatement(out_statement, CaseKind::kCaseX);
      }
      if (MatchKeyword("case")) {
        return ParseCaseStatement(out_statement, CaseKind::kCase);
      }
      if (MatchKeyword("if")) {
        return ParseIfStatement(out_statement);
      }
      ErrorHere("priority statement not supported in v0");
      return false;
    }
    if (MatchKeyword("if")) {
      return ParseIfStatement(out_statement);
    }
    if (MatchKeyword("for")) {
      return ParseForStatement(out_statement);
    }
    if (MatchKeyword("while")) {
      return ParseWhileStatement(out_statement);
    }
    if (MatchKeyword("wait")) {
      return ParseWaitStatement(out_statement);
    }
    if (MatchKeyword("repeat")) {
      return ParseRepeatStatement(out_statement);
    }
    if (MatchKeyword("forever")) {
      return ParseForeverStatement(out_statement);
    }
    if (MatchKeyword("fork")) {
      return ParseForkStatement(out_statement);
    }
    if (MatchKeyword("disable")) {
      return ParseDisableStatement(out_statement);
    }
    if (MatchKeyword("casez")) {
      return ParseCaseStatement(out_statement, CaseKind::kCaseZ);
    }
    if (MatchKeyword("casex")) {
      return ParseCaseStatement(out_statement, CaseKind::kCaseX);
    }
    if (MatchKeyword("case")) {
      return ParseCaseStatement(out_statement, CaseKind::kCase);
    }
    if (MatchKeyword("begin")) {
      return ParseBlockStatement(out_statement);
    }
    if (Peek().kind == TokenKind::kIdentifier) {
      if (Peek(1).kind == TokenKind::kSymbol && Peek(1).text == "(") {
        return ParseTaskCallStatement(out_statement);
      }
      if (Peek(1).kind == TokenKind::kSymbol &&
          (Peek(1).text == ";" || Peek(1).text == ",")) {
        return ParseTaskCallStatement(out_statement);
      }
      if (Peek(1).kind == TokenKind::kSymbol &&
          (Peek(1).text == "=" || Peek(1).text == "<")) {
        return ParseSequentialAssign(out_statement);
      }
    }
    return ParseSequentialAssign(out_statement);
  }

  bool ParseAssignTarget(SequentialAssign* out_assign,
                         const std::string& context) {
    if (!out_assign) {
      return false;
    }
    std::string lhs;
    if (!ConsumeHierIdentifier(&lhs)) {
      ErrorHere("expected identifier in " + context);
      return false;
    }
    std::unique_ptr<Expr> lhs_index;
    std::vector<std::unique_ptr<Expr>> lhs_indices;
    bool lhs_has_range = false;
    bool lhs_indexed_range = false;
    bool lhs_indexed_desc = false;
    int lhs_indexed_width = 0;
    int lhs_msb = 0;
    int lhs_lsb = 0;
    std::unique_ptr<Expr> lhs_msb_expr;
    std::unique_ptr<Expr> lhs_lsb_expr;
    while (MatchSymbol("[")) {
      auto msb_expr = ParseExpr();
      if (!msb_expr) {
        return false;
      }
      if (MatchSymbol("+:") || MatchSymbol("-:")) {
        bool indexed_desc = (Previous().text == "-:");
        if (lhs_has_range || !lhs_indices.empty() || IsArrayName(lhs)) {
          ErrorHere("indexed part select requires identifier");
          return false;
        }
        std::unique_ptr<Expr> width_expr = ParseExpr();
        if (!width_expr) {
          return false;
        }
        int64_t width_value = 0;
        if (!EvalConstExpr(*width_expr, &width_value) || width_value <= 0) {
          ErrorHere("indexed part select width must be constant");
          return false;
        }
        auto base_clone = CloneExprSimple(*msb_expr);
        auto width_minus = MakeNumberExpr(
            static_cast<uint64_t>(width_value - 1));
        std::unique_ptr<Expr> lsb_expr;
        std::unique_ptr<Expr> msb_out;
        if (indexed_desc) {
          msb_out = std::move(msb_expr);
          lsb_expr = MakeBinary('-', std::move(base_clone),
                                std::move(width_minus));
        } else {
          lsb_expr = std::move(msb_expr);
          msb_out = MakeBinary('+', std::move(base_clone),
                               std::move(width_minus));
        }
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after part select");
          return false;
        }
        lhs_has_range = true;
        lhs_indexed_range = true;
        lhs_indexed_desc = indexed_desc;
        lhs_indexed_width = static_cast<int>(width_value);
        lhs_msb_expr = std::move(msb_out);
        lhs_lsb_expr = std::move(lsb_expr);
        break;
      }
      if (MatchSymbol(":")) {
        if (lhs_has_range || !lhs_indices.empty() || IsArrayName(lhs)) {
          ErrorHere("part select requires identifier");
          return false;
        }
        std::unique_ptr<Expr> lsb_expr = ParseExpr();
        if (!lsb_expr) {
          return false;
        }
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after part select");
          return false;
        }
        lhs_has_range = true;
        lhs_msb_expr = std::move(msb_expr);
        lhs_lsb_expr = std::move(lsb_expr);
        int64_t msb = 0;
        int64_t lsb = 0;
        if (!TryEvalConstExpr(*lhs_msb_expr, &msb) ||
            !TryEvalConstExpr(*lhs_lsb_expr, &lsb)) {
          ErrorHere("part select indices must be constant in v0");
          return false;
        }
        lhs_msb = static_cast<int>(msb);
        lhs_lsb = static_cast<int>(lsb);
        break;
      }
      if (!MatchSymbol("]")) {
        ErrorHere("expected ']' after assignment target");
        return false;
      }
      lhs_indices.push_back(std::move(msb_expr));
    }
    if (!lhs_has_range && lhs_indices.size() == 1) {
      lhs_index = std::move(lhs_indices.front());
      lhs_indices.clear();
    }
    out_assign->lhs = std::move(lhs);
    out_assign->lhs_index = std::move(lhs_index);
    out_assign->lhs_indices = std::move(lhs_indices);
    out_assign->lhs_has_range = lhs_has_range;
    out_assign->lhs_indexed_range = lhs_indexed_range;
    out_assign->lhs_indexed_desc = lhs_indexed_desc;
    out_assign->lhs_indexed_width = lhs_indexed_width;
    out_assign->lhs_msb = lhs_msb;
    out_assign->lhs_lsb = lhs_lsb;
    out_assign->lhs_msb_expr = std::move(lhs_msb_expr);
    out_assign->lhs_lsb_expr = std::move(lhs_lsb_expr);
    return true;
  }

  bool ParseDelayStatement(Statement* out_statement) {
    if (!MatchSymbol("#")) {
      return false;
    }
    std::unique_ptr<Expr> delay_expr = ParseExpr();
    if (!delay_expr) {
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kDelay;
    stmt.delay = std::move(delay_expr);
    if (MatchSymbol(";")) {
      *out_statement = std::move(stmt);
      return true;
    }
    if (!ParseStatementBody(&stmt.delay_body)) {
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseEventList(bool has_paren, std::vector<EventItem>* items,
                      bool* saw_star, std::string* sensitivity_text) {
    if (saw_star) {
      *saw_star = false;
    }
    if (sensitivity_text) {
      sensitivity_text->clear();
    }
    if (MatchSymbol("*")) {
      if (saw_star) {
        *saw_star = true;
      }
      if (sensitivity_text) {
        *sensitivity_text = "*";
      }
      if (has_paren && !MatchSymbol(")")) {
        ErrorHere("expected ')' after sensitivity list");
        return false;
      }
      return true;
    }
    bool first_item = true;
    while (true) {
      bool item_has_edge = false;
      EventEdgeKind item_edge = EventEdgeKind::kAny;
      if (MatchKeyword("posedge")) {
        item_has_edge = true;
        item_edge = EventEdgeKind::kPosedge;
      } else if (MatchKeyword("negedge")) {
        item_has_edge = true;
        item_edge = EventEdgeKind::kNegedge;
      }
      auto expr = ParseExpr();
      if (!expr) {
        return false;
      }
      std::string label = "expr";
      if (expr->kind == ExprKind::kIdentifier) {
        label = expr->ident;
      }
      if (sensitivity_text) {
        if (!first_item) {
          *sensitivity_text += ", ";
        }
        if (item_has_edge) {
          *sensitivity_text +=
              (item_edge == EventEdgeKind::kPosedge) ? "posedge " : "negedge ";
        }
        *sensitivity_text += label;
      }
      if (items) {
        EventItem item;
        item.edge = item_edge;
        item.expr = std::move(expr);
        items->push_back(std::move(item));
      }
      if (!has_paren) {
        return true;
      }
      if (MatchSymbol(")")) {
        return true;
      }
      if (MatchSymbol(",") || MatchKeyword("or")) {
        first_item = false;
        continue;
      }
      ErrorHere("expected ')' after sensitivity list");
      return false;
    }
  }

  bool ParseEventControlStatement(Statement* out_statement) {
    if (!MatchSymbol("@")) {
      return false;
    }
    bool has_paren = MatchSymbol("(");
    bool saw_star = false;
    std::vector<EventItem> items;
    if (!ParseEventList(has_paren, &items, &saw_star, nullptr)) {
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kEventControl;
    if (!items.empty()) {
      if (items.size() == 1) {
        stmt.event_edge = items[0].edge;
        stmt.event_expr = std::move(items[0].expr);
      } else {
        stmt.event_items = std::move(items);
      }
    } else {
      stmt.event_edge = EventEdgeKind::kAny;
      stmt.event_expr = nullptr;
    }
    if (MatchSymbol(";")) {
      *out_statement = std::move(stmt);
      return true;
    }
    if (!ParseStatementBody(&stmt.event_body)) {
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseEventTriggerStatement(Statement* out_statement) {
    if (!MatchSymbol("->")) {
      if (!MatchSymbol("-")) {
        return false;
      }
      if (!MatchSymbol(">")) {
        ErrorHere("expected '>' after '-' in event trigger");
        return false;
      }
    }
    std::string name;
    if (!ConsumeHierIdentifier(&name)) {
      ErrorHere("expected event name after '->'");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after event trigger");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kEventTrigger;
    stmt.trigger_target = std::move(name);
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseSystemTaskStatement(Statement* out_statement) {
    if (!MatchSymbol("$")) {
      return false;
    }
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      ErrorHere("expected system task name after '$'");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kTaskCall;
    stmt.task_name = "$" + name;
    if (MatchSymbol(";")) {
      *out_statement = std::move(stmt);
      return true;
    }
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after system task");
      return false;
    }
    bool prev_allow = allow_string_literals_;
    allow_string_literals_ = true;
    if (!MatchSymbol(")")) {
      while (true) {
        auto arg = ParseExpr();
        if (!arg) {
          allow_string_literals_ = prev_allow;
          return false;
        }
        stmt.task_args.push_back(std::move(arg));
        if (MatchSymbol(",")) {
          continue;
        }
        break;
      }
      if (!MatchSymbol(")")) {
        allow_string_literals_ = prev_allow;
        ErrorHere("expected ')' after system task");
        return false;
      }
    }
    allow_string_literals_ = prev_allow;
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after system task");
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseAssertStatement(Statement* out_statement) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'assert'");
      return false;
    }
    std::unique_ptr<Expr> condition = ParseExpr();
    if (!condition) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after assert condition");
      return false;
    }
    std::vector<Statement> then_body;
    if (!ParseStatementBody(&then_body)) {
      return false;
    }
    if (MatchKeyword("else")) {
      std::vector<Statement> else_body;
      if (!ParseStatementBody(&else_body)) {
        return false;
      }
    }
    Statement stmt;
    stmt.kind = StatementKind::kBlock;
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseWaitStatement(Statement* out_statement) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'wait'");
      return false;
    }
    std::unique_ptr<Expr> condition = ParseExpr();
    if (!condition) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after wait condition");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kWait;
    stmt.wait_condition = std::move(condition);
    if (MatchSymbol(";")) {
      *out_statement = std::move(stmt);
      return true;
    }
    if (!ParseStatementBody(&stmt.wait_body)) {
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseForeverStatement(Statement* out_statement) {
    Statement stmt;
    stmt.kind = StatementKind::kForever;
    if (!ParseStatementBody(&stmt.forever_body)) {
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseForkStatement(Statement* out_statement) {
    Statement stmt;
    stmt.kind = StatementKind::kFork;
    if (MatchSymbol(":")) {
      if (!ConsumeIdentifier(&stmt.block_label)) {
        ErrorHere("expected fork label after ':'");
        return false;
      }
    }
    while (true) {
      if (MatchKeyword("join")) {
        break;
      }
      if (Peek().kind == TokenKind::kIdentifier &&
          (Peek().text == "join_any" || Peek().text == "join_none")) {
        ErrorHere("fork/join_any/join_none not supported in v0");
        return false;
      }
      std::vector<Statement> branch_body;
      if (!ParseStatementBody(&branch_body)) {
        return false;
      }
      if (branch_body.size() == 1) {
        stmt.fork_branches.push_back(std::move(branch_body.front()));
      } else if (!branch_body.empty()) {
        Statement block;
        block.kind = StatementKind::kBlock;
        block.block = std::move(branch_body);
        stmt.fork_branches.push_back(std::move(block));
      }
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseDisableStatement(Statement* out_statement) {
    std::string target;
    if (!ConsumeIdentifier(&target)) {
      ErrorHere("expected identifier after 'disable'");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after disable");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kDisable;
    stmt.disable_target = std::move(target);
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseTaskCallStatement(Statement* out_statement) {
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      ErrorHere("expected task name");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kTaskCall;
    stmt.task_name = name;
    if (MatchSymbol("(")) {
      if (!MatchSymbol(")")) {
        while (true) {
          auto arg = ParseExpr();
          if (!arg) {
            return false;
          }
          stmt.task_args.push_back(std::move(arg));
          if (MatchSymbol(",")) {
            continue;
          }
          break;
        }
        if (!MatchSymbol(")")) {
          ErrorHere("expected ')' after task call");
          return false;
        }
      }
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after task call");
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseForStatement(Statement* out_statement) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'for'");
      return false;
    }
    std::string init_lhs;
    bool init_decl = false;
    int init_width = 0;
    bool init_signed = false;
    bool init_real = false;
    if (MatchKeyword("integer") || MatchKeyword("int")) {
      init_decl = true;
      init_width = 32;
      init_signed = true;
      if (MatchKeyword("signed")) {
        init_signed = true;
      } else if (MatchKeyword("unsigned")) {
        init_signed = false;
      }
    } else if (MatchKeyword("time")) {
      init_decl = true;
      init_width = 64;
      init_signed = false;
      if (MatchKeyword("signed")) {
        init_signed = true;
      } else if (MatchKeyword("unsigned")) {
        init_signed = false;
      }
    } else if (MatchKeyword("real")) {
      init_decl = true;
      init_width = 64;
      init_signed = true;
      init_real = true;
    }
    if (!ConsumeIdentifier(&init_lhs)) {
      ErrorHere("expected loop variable in for init");
      return false;
    }
    if (init_decl) {
      if (current_module_) {
        for (const auto& port : current_module_->ports) {
          if (port.name == init_lhs) {
            ErrorHere("loop variable redeclares port '" + init_lhs + "'");
            return false;
          }
        }
        for (const auto& net : current_module_->nets) {
          if (net.name == init_lhs) {
            ErrorHere("loop variable redeclares net '" + init_lhs + "'");
            return false;
          }
        }
      }
      AddOrUpdateNet(current_module_, init_lhs, NetType::kWire, init_width,
                     init_signed, std::shared_ptr<Expr>(),
                     std::shared_ptr<Expr>(), {}, init_real);
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in for init");
      return false;
    }
    std::unique_ptr<Expr> init_rhs = ParseExpr();
    if (!init_rhs) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after for init");
      return false;
    }
    std::unique_ptr<Expr> condition = ParseExpr();
    if (!condition) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after for condition");
      return false;
    }
    std::string step_lhs;
    if (!ConsumeIdentifier(&step_lhs)) {
      ErrorHere("expected loop variable in for step");
      return false;
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in for step");
      return false;
    }
    std::unique_ptr<Expr> step_rhs = ParseExpr();
    if (!step_rhs) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after for step");
      return false;
    }

    Statement stmt;
    stmt.kind = StatementKind::kFor;
    stmt.for_init_lhs = std::move(init_lhs);
    stmt.for_init_rhs = std::move(init_rhs);
    stmt.for_condition = std::move(condition);
    stmt.for_step_lhs = std::move(step_lhs);
    stmt.for_step_rhs = std::move(step_rhs);
    if (!ParseStatementBody(&stmt.for_body)) {
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseWhileStatement(Statement* out_statement) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'while'");
      return false;
    }
    std::unique_ptr<Expr> condition = ParseExpr();
    if (!condition) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after while condition");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kWhile;
    stmt.while_condition = std::move(condition);
    if (!ParseStatementBody(&stmt.while_body)) {
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseRepeatStatement(Statement* out_statement) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'repeat'");
      return false;
    }
    std::unique_ptr<Expr> count = ParseExpr();
    if (!count) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after repeat count");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kRepeat;
    stmt.repeat_count = std::move(count);
    if (!ParseStatementBody(&stmt.repeat_body)) {
      return false;
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseBlockStatement(Statement* out_statement) {
    Statement stmt;
    stmt.kind = StatementKind::kBlock;
    if (MatchSymbol(":")) {
      if (!ConsumeIdentifier(&stmt.block_label)) {
        ErrorHere("expected block label after ':'");
        return false;
      }
    }
    while (true) {
      if (MatchKeyword("end")) {
        if (MatchSymbol(":")) {
          std::string end_label;
          if (!ConsumeIdentifier(&end_label)) {
            ErrorHere("expected label after 'end:'");
            return false;
          }
          if (!stmt.block_label.empty() && end_label != stmt.block_label) {
            ErrorHere("end label does not match block label");
            return false;
          }
        }
        break;
      }
      if (MatchKeyword("integer")) {
        if (!ParseLocalIntegerDecl()) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("time")) {
        if (!ParseLocalTimeDecl()) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("reg")) {
        if (!ParseLocalRegDecl(current_module_)) {
          return false;
        }
        continue;
      }
      Statement inner;
      if (!ParseStatement(&inner)) {
        return false;
      }
      stmt.block.push_back(std::move(inner));
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseIfStatement(Statement* out_statement) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'if'");
      return false;
    }
    std::unique_ptr<Expr> condition = ParseExpr();
    if (!condition) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after if condition");
      return false;
    }

    Statement stmt;
    stmt.kind = StatementKind::kIf;
    stmt.condition = std::move(condition);
    if (!ParseStatementBody(&stmt.then_branch)) {
      return false;
    }
    if (MatchKeyword("else")) {
      if (!ParseStatementBody(&stmt.else_branch)) {
        return false;
      }
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseCaseStatement(Statement* out_statement, CaseKind case_kind) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'case'");
      return false;
    }
    std::unique_ptr<Expr> case_expr = ParseExpr();
    if (!case_expr) {
      return false;
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after case expression");
      return false;
    }

    Statement stmt;
    stmt.kind = StatementKind::kCase;
    stmt.case_kind = case_kind;
    stmt.case_expr = std::move(case_expr);
    bool saw_default = false;

    while (true) {
      if (MatchKeyword("endcase")) {
        break;
      }
      if (MatchKeyword("default")) {
        if (saw_default) {
          ErrorHere("duplicate default in case statement");
          return false;
        }
        saw_default = true;
        MatchSymbol(":");
        if (!ParseStatementBody(&stmt.default_branch)) {
          return false;
        }
        continue;
      }

      CaseItem item;
      while (true) {
        std::unique_ptr<Expr> label = ParseExpr();
        if (!label) {
          return false;
        }
        item.labels.push_back(std::move(label));
        if (MatchSymbol(",")) {
          continue;
        }
        break;
      }
      if (!MatchSymbol(":")) {
        ErrorHere("expected ':' after case item");
        return false;
      }
      if (!ParseStatementBody(&item.body)) {
        return false;
      }
      stmt.case_items.push_back(std::move(item));
    }
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseSequentialAssign(Statement* out_statement) {
    SequentialAssign assign;
    if (!ParseAssignTarget(&assign, "sequential assignment")) {
      return false;
    }
    bool nonblocking = false;
    if (MatchSymbol("<")) {
      if (!MatchSymbol("=")) {
        ErrorHere("expected '<=' in nonblocking assignment");
        return false;
      }
      nonblocking = true;
    } else if (MatchSymbol("=")) {
      nonblocking = false;
    } else {
      ErrorHere("expected assignment operator");
      return false;
    }
    std::unique_ptr<Expr> delay;
    if (MatchSymbol("#")) {
      delay = ParseExpr();
      if (!delay) {
        return false;
      }
    }
    std::unique_ptr<Expr> rhs = ParseExpr();
    if (!rhs) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after assignment");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kAssign;
    assign.rhs = std::move(rhs);
    assign.delay = std::move(delay);
    assign.nonblocking = nonblocking;
    stmt.assign = std::move(assign);
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseForceStatement(Statement* out_statement) {
    SequentialAssign assign;
    std::string target;
    if (!ConsumeHierIdentifier(&target)) {
      ErrorHere("expected identifier after 'force'");
      return false;
    }
    if (target.empty()) {
      ErrorHere("force target must be an identifier");
      return false;
    }
    assign.lhs = target;
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in force statement");
      return false;
    }
    std::unique_ptr<Expr> delay;
    if (MatchSymbol("#")) {
      delay = ParseExpr();
      if (!delay) {
        return false;
      }
    }
    std::unique_ptr<Expr> rhs = ParseExpr();
    if (!rhs) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after force statement");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kForce;
    stmt.force_target = std::move(target);
    assign.rhs = std::move(rhs);
    assign.delay = std::move(delay);
    assign.nonblocking = false;
    stmt.assign = std::move(assign);
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseReleaseStatement(Statement* out_statement) {
    SequentialAssign assign;
    std::string target;
    if (!ConsumeHierIdentifier(&target)) {
      ErrorHere("expected identifier after 'release'");
      return false;
    }
    if (target.empty()) {
      ErrorHere("release target must be an identifier");
      return false;
    }
    assign.lhs = target;
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after release statement");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kRelease;
    stmt.release_target = std::move(target);
    assign.nonblocking = false;
    stmt.assign = std::move(assign);
    *out_statement = std::move(stmt);
    return true;
  }

  bool ParseInstance(Module* module) {
    std::string module_name;
    std::string instance_name;
    if (!ConsumeIdentifier(&module_name)) {
      ErrorHere("expected module name in instance");
      return false;
    }
    Instance instance;
    instance.module_name = std::move(module_name);
    if (MatchSymbol("#")) {
      if (!ParseParamOverrides(&instance)) {
        return false;
      }
    }
    if (!ConsumeIdentifier(&instance_name)) {
      ErrorHere("expected instance name");
      return false;
    }
    std::vector<int64_t> indices;
    if (MatchSymbol("[")) {
      int64_t msb = 0;
      int64_t lsb = 0;
      if (!ParseConstExpr(nullptr, &msb, "instance array msb")) {
        return false;
      }
      if (MatchSymbol(":")) {
        if (!ParseConstExpr(nullptr, &lsb, "instance array lsb")) {
          return false;
        }
      } else {
        lsb = msb;
      }
      if (!MatchSymbol("]")) {
        ErrorHere("expected ']' after instance array");
        return false;
      }
      int64_t step = (msb <= lsb) ? 1 : -1;
      for (int64_t value = msb;; value += step) {
        indices.push_back(value);
        if (value == lsb) {
          break;
        }
      }
    }
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after instance name");
      return false;
    }
    instance.name = std::move(instance_name);
    if (!MatchSymbol(")")) {
      bool named = false;
      if (Peek().kind == TokenKind::kSymbol && Peek().text == ".") {
        named = true;
      }
      if (named) {
        while (true) {
          if (!MatchSymbol(".")) {
            ErrorHere("expected named port connection ('.port(signal)')");
            return false;
          }
          std::string port_name;
          if (!ConsumeIdentifier(&port_name)) {
            ErrorHere("expected port name after '.'");
            return false;
          }
          if (!MatchSymbol("(")) {
            ErrorHere("expected '(' after port name");
            return false;
          }
          std::unique_ptr<Expr> expr;
          if (!MatchSymbol(")")) {
            expr = ParseExpr();
            if (!expr) {
              return false;
            }
            if (!MatchSymbol(")")) {
              ErrorHere("expected ')' after port expression");
              return false;
            }
          }
          instance.connections.push_back(Connection{port_name, std::move(expr)});
          if (MatchSymbol(",")) {
            continue;
          }
          break;
        }
      } else {
        int position = 0;
        while (true) {
          std::unique_ptr<Expr> expr;
          if (!(Peek().kind == TokenKind::kSymbol &&
                (Peek().text == "," || Peek().text == ")"))) {
            expr = ParseExpr();
            if (!expr) {
              return false;
            }
          }
          instance.connections.push_back(
              Connection{std::to_string(position), std::move(expr)});
          ++position;
          if (MatchSymbol(",")) {
            continue;
          }
          break;
        }
      }
      if (!MatchSymbol(")")) {
        ErrorHere("expected ')' after instance connections");
        return false;
      }
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after instance");
      return false;
    }
    if (indices.empty()) {
      module->instances.push_back(std::move(instance));
      return true;
    }
    auto clone_array_expr = [&](const Expr& expr, size_t count,
                                size_t slot) -> std::unique_ptr<Expr> {
      if (expr.kind == ExprKind::kConcat && expr.elements.size() == count) {
        return CloneExprSimple(*expr.elements[slot]);
      }
      return CloneExprSimple(expr);
    };
    const size_t count = indices.size();
    for (size_t slot = 0; slot < count; ++slot) {
      Instance expanded;
      expanded.module_name = instance.module_name;
      expanded.name =
          instance.name + "__" + std::to_string(indices[slot]);
      for (const auto& override_item : instance.param_overrides) {
        ParamOverride param;
        param.name = override_item.name;
        if (override_item.expr) {
          param.expr = CloneExprSimple(*override_item.expr);
        }
        expanded.param_overrides.push_back(std::move(param));
      }
      for (const auto& conn : instance.connections) {
        Connection connection;
        connection.port = conn.port;
        if (conn.expr) {
          connection.expr = clone_array_expr(*conn.expr, count, slot);
        }
        expanded.connections.push_back(std::move(connection));
      }
      module->instances.push_back(std::move(expanded));
    }
    return true;
  }

  bool ParseRange(int* width_out) {
    return ParseRange(width_out, nullptr, nullptr, nullptr);
  }

  bool ParseRange(int* width_out, std::shared_ptr<Expr>* msb_out,
                  std::shared_ptr<Expr>* lsb_out, bool* had_range) {
    if (!MatchSymbol("[")) {
      if (had_range) {
        *had_range = false;
      }
      return true;
    }
    if (had_range) {
      *had_range = true;
    }
    std::unique_ptr<Expr> msb_expr;
    std::unique_ptr<Expr> lsb_expr;
    int64_t msb = 0;
    int64_t lsb = 0;
    if (!ParseConstExpr(&msb_expr, &msb, "range msb")) {
      return false;
    }
    if (!MatchSymbol(":")) {
      ErrorHere("expected ':' in range");
      return false;
    }
    if (!ParseConstExpr(&lsb_expr, &lsb, "range lsb")) {
      return false;
    }
    if (!MatchSymbol("]")) {
      ErrorHere("expected ']' after range");
      return false;
    }
    int64_t width64 = msb >= lsb ? (msb - lsb + 1) : (lsb - msb + 1);
    if (width64 <= 0 || width64 > 0x7FFFFFFF) {
      ErrorHere("invalid range width");
      return false;
    }
    *width_out = static_cast<int>(width64);
    if (msb_out) {
      *msb_out = std::shared_ptr<Expr>(std::move(msb_expr));
    }
    if (lsb_out) {
      *lsb_out = std::shared_ptr<Expr>(std::move(lsb_expr));
    }
    return true;
  }

  std::unique_ptr<Expr> ParseExpr() { return ParseConditional(); }

  std::unique_ptr<Expr> ParseConditional() {
    auto condition = ParseLogicalOr();
    if (MatchSymbol("?")) {
      auto then_expr = ParseExpr();
      if (!MatchSymbol(":")) {
        ErrorHere("expected ':' in conditional expression");
        return nullptr;
      }
      auto else_expr = ParseConditional();
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kTernary;
      expr->condition = std::move(condition);
      expr->then_expr = std::move(then_expr);
      expr->else_expr = std::move(else_expr);
      return expr;
    }
    return condition;
  }

  std::unique_ptr<Expr> ParseLogicalOr() {
    auto left = ParseLogicalAnd();
    while (MatchSymbol2("||")) {
      auto right = ParseLogicalAnd();
      left = MakeBinary('O', std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<Expr> ParseLogicalAnd() {
    auto left = ParseEquality();
    while (MatchSymbol2("&&")) {
      auto right = ParseEquality();
      left = MakeBinary('A', std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<Expr> ParseEquality() {
    auto left = ParseRelational();
    while (true) {
      if (MatchSymbol3("===")) {
        auto right = ParseRelational();
        left = MakeBinary('C', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol3("!==")) {
        auto right = ParseRelational();
        left = MakeBinary('c', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol3("==?")) {
        auto right = ParseRelational();
        left = MakeBinary('W', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol3("!=?")) {
        auto right = ParseRelational();
        left = MakeBinary('w', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol2("==")) {
        auto right = ParseRelational();
        left = MakeBinary('E', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol2("!=")) {
        auto right = ParseRelational();
        left = MakeBinary('N', std::move(left), std::move(right));
        continue;
      }
      break;
    }
    return left;
  }

  std::unique_ptr<Expr> ParseRelational() {
    auto left = ParseBitwiseOr();
    while (true) {
      if (MatchSymbol2("<=")) {
        auto right = ParseBitwiseOr();
        left = MakeBinary('L', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol2(">=")) {
        auto right = ParseBitwiseOr();
        left = MakeBinary('G', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol("<")) {
        auto right = ParseBitwiseOr();
        left = MakeBinary('<', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol(">")) {
        auto right = ParseBitwiseOr();
        left = MakeBinary('>', std::move(left), std::move(right));
        continue;
      }
      break;
    }
    return left;
  }

  std::unique_ptr<Expr> ParseBitwiseOr() {
    auto left = ParseBitwiseXor();
    while (true) {
      if (Peek().kind == TokenKind::kSymbol && Peek().text == "|" &&
          Peek(1).kind == TokenKind::kSymbol && Peek(1).text == "|") {
        break;
      }
      if (!MatchSymbol("|")) {
        break;
      }
      auto right = ParseBitwiseXor();
      left = MakeBinary('|', std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<Expr> ParseBitwiseXor() {
    auto left = ParseBitwiseAnd();
    while (MatchSymbol("^")) {
      auto right = ParseBitwiseAnd();
      left = MakeBinary('^', std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<Expr> ParseBitwiseAnd() {
    auto left = ParseShift();
    while (true) {
      if (Peek().kind == TokenKind::kSymbol && Peek().text == "&" &&
          Peek(1).kind == TokenKind::kSymbol && Peek(1).text == "&") {
        break;
      }
      if (!MatchSymbol("&")) {
        break;
      }
      auto right = ParseShift();
      left = MakeBinary('&', std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<Expr> ParseShift() {
    auto left = ParseAddSub();
    while (true) {
      if (MatchSymbol3(">>>")) {
        auto right = ParseAddSub();
        left = MakeBinary('R', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol3("<<<")) {
        auto right = ParseAddSub();
        left = MakeBinary('l', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol2("<<")) {
        auto right = ParseAddSub();
        left = MakeBinary('l', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol2(">>")) {
        auto right = ParseAddSub();
        left = MakeBinary('r', std::move(left), std::move(right));
        continue;
      }
      break;
    }
    return left;
  }

  std::unique_ptr<Expr> ParseAddSub() {
    auto left = ParseMulDiv();
    while (true) {
      if (MatchSymbol("+")) {
        auto right = ParseMulDiv();
        left = MakeBinary('+', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol("-")) {
        auto right = ParseMulDiv();
        left = MakeBinary('-', std::move(left), std::move(right));
        continue;
      }
      break;
    }
    return left;
  }

  std::unique_ptr<Expr> ParseMulDiv() {
    auto left = ParseUnary();
    while (true) {
      if (MatchSymbol("*")) {
        auto right = ParseUnary();
        left = MakeBinary('*', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol("/")) {
        auto right = ParseUnary();
        left = MakeBinary('/', std::move(left), std::move(right));
        continue;
      }
      if (MatchSymbol("%")) {
        auto right = ParseUnary();
        left = MakeBinary('%', std::move(left), std::move(right));
        continue;
      }
      break;
    }
    return left;
  }

  std::unique_ptr<Expr> ParsePower() {
    auto left = ParsePrimary();
    if (!left) {
      return nullptr;
    }
    if (MatchSymbol2("**")) {
      auto right = ParseUnary();
      if (!right) {
        return nullptr;
      }
      left = MakeBinary('p', std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<Expr> ParseUnary() {
    if (MatchSymbol("!")) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kUnary;
      expr->unary_op = '!';
      expr->operand = ParseUnary();
      return expr;
    }
    if (MatchSymbol("~")) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kUnary;
      expr->unary_op = '~';
      expr->operand = ParseUnary();
      return expr;
    }
    if (MatchSymbol("&")) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kUnary;
      expr->unary_op = '&';
      expr->operand = ParseUnary();
      return expr;
    }
    if (MatchSymbol("|")) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kUnary;
      expr->unary_op = '|';
      expr->operand = ParseUnary();
      return expr;
    }
    if (MatchSymbol("^")) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kUnary;
      expr->unary_op = '^';
      expr->operand = ParseUnary();
      return expr;
    }
    if (MatchSymbol("-")) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kUnary;
      expr->unary_op = '-';
      expr->operand = ParseUnary();
      return expr;
    }
    if (MatchSymbol("+")) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kUnary;
      expr->unary_op = '+';
      expr->operand = ParseUnary();
      return expr;
    }
    return ParsePower();
  }

  std::unique_ptr<Expr> ParsePrimary() {
    std::unique_ptr<Expr> expr;
    if (MatchSymbol("$")) {
      auto parse_system_call = [&](const std::string& name,
                                   bool allow_no_parens) -> std::unique_ptr<Expr> {
        auto call = std::make_unique<Expr>();
        call->kind = ExprKind::kCall;
        call->ident = name;
        if (MatchSymbol("(")) {
          bool prev_allow = allow_string_literals_;
          allow_string_literals_ = true;
          if (!MatchSymbol(")")) {
            while (true) {
              if (name == "$fread" &&
                  Peek().kind == TokenKind::kSymbol &&
                  Peek().text == ",") {
                call->call_args.push_back(MakeNumberExpr(0u));
                MatchSymbol(",");
                continue;
              }
              auto arg = ParseExpr();
              if (!arg) {
                allow_string_literals_ = prev_allow;
                return nullptr;
              }
              call->call_args.push_back(std::move(arg));
              if (MatchSymbol(",")) {
                continue;
              }
              break;
            }
            if (!MatchSymbol(")")) {
              allow_string_literals_ = prev_allow;
              ErrorHere("expected ')' after system function");
              return nullptr;
            }
          }
          allow_string_literals_ = prev_allow;
          return call;
        }
        if (!allow_no_parens) {
          ErrorHere("expected '(' after system function");
          return nullptr;
        }
        return call;
      };

      char op = 0;
      if (MatchKeyword("time")) {
        expr = parse_system_call("$time", true);
      } else if (MatchKeyword("stime")) {
        expr = parse_system_call("$stime", true);
      } else if (MatchKeyword("random")) {
        expr = parse_system_call("$random", true);
      } else if (MatchKeyword("urandom_range")) {
        expr = parse_system_call("$urandom_range", false);
      } else if (MatchKeyword("urandom")) {
        expr = parse_system_call("$urandom", true);
      } else if (MatchKeyword("realtime")) {
        expr = parse_system_call("$realtime", true);
      } else if (MatchKeyword("realtobits")) {
        expr = parse_system_call("$realtobits", false);
      } else if (MatchKeyword("bitstoreal")) {
        expr = parse_system_call("$bitstoreal", false);
      } else if (MatchKeyword("rtoi")) {
        expr = parse_system_call("$rtoi", false);
      } else if (MatchKeyword("itor")) {
        expr = parse_system_call("$itor", false);
      } else if (MatchKeyword("log10")) {
        expr = parse_system_call("$log10", false);
      } else if (MatchKeyword("ln")) {
        expr = parse_system_call("$ln", false);
      } else if (MatchKeyword("exp")) {
        expr = parse_system_call("$exp", false);
      } else if (MatchKeyword("sqrt")) {
        expr = parse_system_call("$sqrt", false);
      } else if (MatchKeyword("pow")) {
        expr = parse_system_call("$pow", false);
      } else if (MatchKeyword("floor")) {
        expr = parse_system_call("$floor", false);
      } else if (MatchKeyword("ceil")) {
        expr = parse_system_call("$ceil", false);
      } else if (MatchKeyword("sin")) {
        expr = parse_system_call("$sin", false);
      } else if (MatchKeyword("cos")) {
        expr = parse_system_call("$cos", false);
      } else if (MatchKeyword("tan")) {
        expr = parse_system_call("$tan", false);
      } else if (MatchKeyword("asin")) {
        expr = parse_system_call("$asin", false);
      } else if (MatchKeyword("acos")) {
        expr = parse_system_call("$acos", false);
      } else if (MatchKeyword("atan")) {
        expr = parse_system_call("$atan", false);
      } else if (MatchKeyword("bits")) {
        expr = parse_system_call("$bits", false);
      } else if (MatchKeyword("size")) {
        expr = parse_system_call("$size", false);
      } else if (MatchKeyword("dimensions")) {
        expr = parse_system_call("$dimensions", false);
      } else if (MatchKeyword("left")) {
        expr = parse_system_call("$left", false);
      } else if (MatchKeyword("right")) {
        expr = parse_system_call("$right", false);
      } else if (MatchKeyword("low")) {
        expr = parse_system_call("$low", false);
      } else if (MatchKeyword("high")) {
        expr = parse_system_call("$high", false);
      } else if (MatchKeyword("fopen")) {
        expr = parse_system_call("$fopen", false);
      } else if (MatchKeyword("fgetc")) {
        expr = parse_system_call("$fgetc", false);
      } else if (MatchKeyword("feof")) {
        expr = parse_system_call("$feof", false);
      } else if (MatchKeyword("ftell")) {
        expr = parse_system_call("$ftell", false);
      } else if (MatchKeyword("fseek")) {
        expr = parse_system_call("$fseek", false);
      } else if (MatchKeyword("ferror")) {
        expr = parse_system_call("$ferror", false);
      } else if (MatchKeyword("ungetc")) {
        expr = parse_system_call("$ungetc", false);
      } else if (MatchKeyword("fread")) {
        expr = parse_system_call("$fread", false);
      } else if (MatchKeyword("fgets")) {
        expr = parse_system_call("$fgets", false);
      } else if (MatchKeyword("fscanf")) {
        expr = parse_system_call("$fscanf", false);
      } else if (MatchKeyword("sscanf")) {
        expr = parse_system_call("$sscanf", false);
      } else if (MatchKeyword("test")) {
        if (!MatchSymbol("$") || !MatchKeyword("plusargs")) {
          ErrorHere("unsupported system function");
          return nullptr;
        }
        expr = parse_system_call("$test$plusargs", false);
      } else if (MatchKeyword("value")) {
        if (!MatchSymbol("$") || !MatchKeyword("plusargs")) {
          ErrorHere("unsupported system function");
          return nullptr;
        }
        expr = parse_system_call("$value$plusargs", false);
      } else if (MatchKeyword("signed")) {
        op = 'S';
      } else if (MatchKeyword("unsigned")) {
        op = 'U';
      } else if (MatchKeyword("clog2")) {
        op = 'C';
      } else {
        ErrorHere("unsupported system function");
        return nullptr;
      }
      if (!expr && (op == 'S' || op == 'U' || op == 'C')) {
        if (!MatchSymbol("(")) {
          ErrorHere("expected '(' after system function");
          return nullptr;
        }
        auto operand = ParseExpr();
        if (!operand) {
          return nullptr;
        }
        if (!MatchSymbol(")")) {
          ErrorHere("expected ')' after system function");
          return nullptr;
        }
        expr = std::make_unique<Expr>();
        expr->kind = ExprKind::kUnary;
        expr->unary_op = op;
        expr->operand = std::move(operand);
        if (op == 'C') {
          int64_t value = 0;
          if (!EvalConstExpr(*expr, &value)) {
            ErrorHere("$clog2 requires a constant expression in v0");
            return nullptr;
          }
          auto folded = MakeNumberExpr(static_cast<uint64_t>(value));
          folded->is_signed = true;
          expr = std::move(folded);
        }
      }
    } else if (Peek().kind == TokenKind::kString) {
      expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kString;
      expr->string_value = Peek().text;
      Advance();
    } else if (MatchSymbol("{")) {
      expr = ParseConcat();
    } else if (MatchSymbol("'")) {
      expr = ParseBasedLiteral(0);
    } else if (MatchSymbol("(")) {
      expr = ParseExpr();
      if (!MatchSymbol(")")) {
        ErrorHere("expected ')' after expression");
        return nullptr;
      }
    } else if (Peek().kind == TokenKind::kNumber) {
      const std::string token = Peek().text;
      if (token.find_first_of(".eE") != std::string::npos) {
        char* endptr = nullptr;
        double real_value = std::strtod(token.c_str(), &endptr);
        if (endptr == token.c_str()) {
          ErrorHere("invalid real literal");
          return nullptr;
        }
        uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(real_value),
                      "double size mismatch");
        std::memcpy(&bits, &real_value, sizeof(bits));
        auto lit = std::make_unique<Expr>();
        lit->kind = ExprKind::kNumber;
        lit->number = bits;
        lit->value_bits = bits;
        lit->has_width = true;
        lit->number_width = 64;
        lit->is_real_literal = true;
        expr = std::move(lit);
        Advance();
      } else {
        uint64_t size = std::stoull(token);
        Advance();
        if (MatchSymbol("'")) {
          expr = ParseBasedLiteral(size);
        } else {
          expr = std::make_unique<Expr>();
          expr->kind = ExprKind::kNumber;
          expr->number = size;
          expr->value_bits = size;
          expr->is_signed = true;
        }
      }
    } else if (Peek().kind == TokenKind::kIdentifier) {
      std::string name;
      if (!ConsumeHierIdentifier(&name)) {
        return nullptr;
      }
      if (MatchSymbol("'")) {
        auto size_expr = std::make_unique<Expr>();
        size_expr->kind = ExprKind::kIdentifier;
        size_expr->ident = name;
        int64_t size_value = 0;
        if (!EvalConstExpr(*size_expr, &size_value) || size_value <= 0) {
          ErrorHere("literal width must be constant and positive");
          return nullptr;
        }
        expr = ParseBasedLiteral(static_cast<uint64_t>(size_value));
      } else if (MatchSymbol("(")) {
        auto call = std::make_unique<Expr>();
        call->kind = ExprKind::kCall;
        call->ident = name;
        if (!MatchSymbol(")")) {
          while (true) {
            auto arg = ParseExpr();
            if (!arg) {
              return nullptr;
            }
            call->call_args.push_back(std::move(arg));
            if (MatchSymbol(",")) {
              continue;
            }
            break;
          }
          if (!MatchSymbol(")")) {
            ErrorHere("expected ')' after function call");
            return nullptr;
          }
        }
        expr = std::move(call);
      } else {
        expr = std::make_unique<Expr>();
        expr->kind = ExprKind::kIdentifier;
        expr->ident = std::move(name);
      }
    }
    if (!expr) {
      ErrorHere("expected expression");
      return nullptr;
    }
    while (MatchSymbol("[")) {
      if (expr->kind != ExprKind::kIdentifier &&
          expr->kind != ExprKind::kIndex) {
        ErrorHere("bit/part select requires identifier or array index");
        return nullptr;
      }
      std::unique_ptr<Expr> msb_expr = ParseExpr();
      if (!msb_expr) {
        return nullptr;
      }
      bool base_is_array = false;
      bool base_is_array_index = false;
      if (expr->kind == ExprKind::kIdentifier) {
        base_is_array = IsArrayName(expr->ident);
      } else if (expr->kind == ExprKind::kIndex) {
        base_is_array_index = IsArrayIndexExpr(*expr);
      }
      if (MatchSymbol("+:") || MatchSymbol("-:")) {
        bool indexed_desc = (Previous().text == "-:");
        if (base_is_array ||
            (expr->kind == ExprKind::kIndex && !base_is_array_index)) {
          ErrorHere("indexed part select requires identifier or array element");
          return nullptr;
        }
        std::unique_ptr<Expr> width_expr = ParseExpr();
        if (!width_expr) {
          return nullptr;
        }
        int64_t width_value = 0;
        if (!EvalConstExpr(*width_expr, &width_value) || width_value <= 0) {
          ErrorHere("indexed part select width must be constant");
          return nullptr;
        }
        auto base_clone = CloneExprSimple(*msb_expr);
        auto width_minus = MakeNumberExpr(
            static_cast<uint64_t>(width_value - 1));
        std::unique_ptr<Expr> lsb_expr;
        std::unique_ptr<Expr> msb_out;
        if (indexed_desc) {
          msb_out = std::move(msb_expr);
          lsb_expr = MakeBinary('-', std::move(base_clone),
                                std::move(width_minus));
        } else {
          lsb_expr = std::move(msb_expr);
          msb_out = MakeBinary('+', std::move(base_clone),
                               std::move(width_minus));
        }
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after part select");
          return nullptr;
        }
        auto select = std::make_unique<Expr>();
        select->kind = ExprKind::kSelect;
        select->base = std::move(expr);
        select->has_range = true;
        select->indexed_range = true;
        select->indexed_desc = indexed_desc;
        select->indexed_width = static_cast<int>(width_value);
        select->msb_expr = std::move(msb_out);
        select->lsb_expr = std::move(lsb_expr);
        int64_t msb = 0;
        int64_t lsb = 0;
        if (select->msb_expr && select->lsb_expr &&
            TryEvalConstExpr(*select->msb_expr, &msb) &&
            TryEvalConstExpr(*select->lsb_expr, &lsb)) {
          select->msb = static_cast<int>(msb);
          select->lsb = static_cast<int>(lsb);
        }
        expr = std::move(select);
        continue;
      }
      if (MatchSymbol(":")) {
        if (base_is_array ||
            (expr->kind == ExprKind::kIndex && !base_is_array_index)) {
          ErrorHere("part select requires identifier or array element");
          return nullptr;
        }
        std::unique_ptr<Expr> lsb_expr = ParseExpr();
        if (!lsb_expr) {
          return nullptr;
        }
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after part select");
          return nullptr;
        }
        auto select = std::make_unique<Expr>();
        select->kind = ExprKind::kSelect;
        select->base = std::move(expr);
        select->has_range = true;
        select->msb_expr = std::move(msb_expr);
        select->lsb_expr = std::move(lsb_expr);
        int64_t msb = 0;
        int64_t lsb = 0;
        if (select->msb_expr && select->lsb_expr &&
            TryEvalConstExpr(*select->msb_expr, &msb) &&
            TryEvalConstExpr(*select->lsb_expr, &lsb)) {
          select->msb = static_cast<int>(msb);
          select->lsb = static_cast<int>(lsb);
        }
        expr = std::move(select);
        continue;
      }
      if (!MatchSymbol("]")) {
        ErrorHere("expected ']' after bit select");
        return nullptr;
      }
      if (base_is_array || expr->kind == ExprKind::kIndex) {
        auto index = std::make_unique<Expr>();
        index->kind = ExprKind::kIndex;
        index->base = std::move(expr);
        index->index = std::move(msb_expr);
        expr = std::move(index);
        continue;
      }
      int64_t index_value = 0;
      if (TryEvalConstExpr(*msb_expr, &index_value)) {
        auto select = std::make_unique<Expr>();
        select->kind = ExprKind::kSelect;
        select->base = std::move(expr);
        select->msb = static_cast<int>(index_value);
        select->lsb = static_cast<int>(index_value);
        select->has_range = false;
        select->msb_expr = std::move(msb_expr);
        expr = std::move(select);
      } else {
        auto index = std::make_unique<Expr>();
        index->kind = ExprKind::kIndex;
        index->base = std::move(expr);
        index->index = std::move(msb_expr);
        expr = std::move(index);
      }
    }
    return expr;
  }

  std::unique_ptr<Expr> ParseConcat() {
    if (Peek().kind == TokenKind::kSymbol &&
        Peek(1).kind == TokenKind::kSymbol &&
        ((Peek().text == "<" && Peek(1).text == "<") ||
         (Peek().text == ">" && Peek(1).text == ">"))) {
      ErrorHere("streaming operator not supported in v0");
      return nullptr;
    }
    std::unique_ptr<Expr> first = ParseExpr();
    if (!first) {
      return nullptr;
    }
    if (MatchSymbol("{")) {
      int64_t repeat = 0;
      if (!EvalConstExpr(*first, &repeat)) {
        ErrorHere("expected constant replication count");
        return nullptr;
      }
      if (repeat <= 0 || repeat > 0x7FFFFFFF) {
        ErrorHere("invalid replication count");
        return nullptr;
      }
      std::unique_ptr<Expr> repeat_expr = std::move(first);
      std::vector<std::unique_ptr<Expr>> elements;
      if (MatchSymbol("}")) {
        ErrorHere("empty replication body");
        return nullptr;
      }
      while (true) {
        std::unique_ptr<Expr> element = ParseExpr();
        if (!element) {
          return nullptr;
        }
        elements.push_back(std::move(element));
        if (MatchSymbol(",")) {
          continue;
        }
        break;
      }
      if (!MatchSymbol("}")) {
        ErrorHere("expected '}' after replication body");
        return nullptr;
      }
      if (!MatchSymbol("}")) {
        ErrorHere("expected '}' after replication");
        return nullptr;
      }
      auto concat = std::make_unique<Expr>();
      concat->kind = ExprKind::kConcat;
      concat->repeat = static_cast<int>(repeat);
      concat->repeat_expr = std::move(repeat_expr);
      concat->elements = std::move(elements);
      return concat;
    }

    std::vector<std::unique_ptr<Expr>> elements;
    elements.push_back(std::move(first));
    while (MatchSymbol(",")) {
      std::unique_ptr<Expr> element = ParseExpr();
      if (!element) {
        return nullptr;
      }
      elements.push_back(std::move(element));
    }
    if (!MatchSymbol("}")) {
      ErrorHere("expected '}' after concatenation");
      return nullptr;
    }
    auto concat = std::make_unique<Expr>();
    concat->kind = ExprKind::kConcat;
    concat->repeat = 1;
    concat->elements = std::move(elements);
    return concat;
  }

  std::unique_ptr<Expr> ParseBasedLiteral(uint64_t size) {
    if (Peek().kind != TokenKind::kIdentifier &&
        Peek().kind != TokenKind::kNumber) {
      ErrorHere("expected base digits after '''");
      return nullptr;
    }
    const Token base_token = Peek();
    std::string token = base_token.text;
    Advance();
    if (token.empty()) {
      ErrorHere("invalid base literal");
      return nullptr;
    }
    int last_line = base_token.line;
    int last_end_column =
        base_token.column + static_cast<int>(base_token.text.size());
    bool is_signed = false;
    size_t base_index = 0;
    if (token[0] == 's' || token[0] == 'S') {
      is_signed = true;
      base_index = 1;
      if (token.size() <= base_index) {
        ErrorHere("invalid base literal");
        return nullptr;
      }
    }
    char base_char = static_cast<char>(std::tolower(
        static_cast<unsigned char>(token[base_index])));
    std::string digits = token.substr(base_index + 1);
    auto append_token = [&](const Token& next, const std::string& text) {
      digits += text;
      last_line = next.line;
      last_end_column = next.column + static_cast<int>(next.text.size());
    };
    if (digits.empty() &&
        (Peek().kind == TokenKind::kNumber ||
         Peek().kind == TokenKind::kIdentifier)) {
      const Token next = Peek();
      append_token(next, next.text);
      Advance();
    }
    if (digits.empty() && Peek().kind == TokenKind::kSymbol &&
        Peek().text == "?") {
      const Token next = Peek();
      append_token(next, "?");
      Advance();
    }
    auto is_adjacent = [&](const Token& next) -> bool {
      return next.line == last_line && next.column == last_end_column;
    };
    while (true) {
      const Token next = Peek();
      if (!is_adjacent(next)) {
        break;
      }
      if (next.kind == TokenKind::kSymbol && next.text == "?") {
        append_token(next, "?");
        Advance();
        continue;
      }
      if (next.kind == TokenKind::kNumber ||
          next.kind == TokenKind::kIdentifier) {
        append_token(next, next.text);
        Advance();
        continue;
      }
      break;
    }
    std::string cleaned;
    for (char c : digits) {
      if (c != '_') {
        cleaned.push_back(c);
      }
    }
    if (cleaned.empty()) {
      ErrorHere("invalid base literal");
      return nullptr;
    }
    int base = 10;
    int bits_per_digit = 0;
    switch (base_char) {
      case 'b':
        base = 2;
        bits_per_digit = 1;
        break;
      case 'o':
        base = 8;
        bits_per_digit = 3;
        break;
      case 'd':
        base = 10;
        break;
      case 'h':
        base = 16;
        bits_per_digit = 4;
        break;
      default:
        ErrorHere("unsupported base in literal");
        return nullptr;
    }
    bool has_xz = false;
    for (char c : cleaned) {
      if (c == 'x' || c == 'X' || c == 'z' || c == 'Z' || c == '?') {
        has_xz = true;
        break;
      }
    }
    if (has_xz && !options_.enable_4state) {
      ErrorHere("x/z literals require --4state");
      return nullptr;
    }
    if (has_xz && base_char == 'd') {
      ErrorHere("x/z digits not allowed in decimal literal");
      return nullptr;
    }

    if (base_char != 'd' && bits_per_digit > 0) {
      const size_t digit_count = cleaned.size();
      const uint64_t total_bits =
          static_cast<uint64_t>(digit_count) * bits_per_digit;
      if (size == 0 && has_xz) {
        size = total_bits;
      }
      const uint64_t target_bits = (size > 0) ? size : total_bits;
      if (target_bits > 64 || total_bits > 64) {
        const size_t digits_per_chunk =
            static_cast<size_t>(64 / bits_per_digit);
        const size_t needed_digits =
            static_cast<size_t>((target_bits + bits_per_digit - 1) /
                                bits_per_digit);
        std::string padded;
        if (digit_count >= needed_digits) {
          padded = cleaned.substr(digit_count - needed_digits);
        } else {
          padded.assign(needed_digits - digit_count, '0');
          padded += cleaned;
        }
        uint64_t msb_bits = target_bits;
        if (needed_digits > 0) {
          msb_bits -=
              static_cast<uint64_t>(needed_digits - 1) * bits_per_digit;
        }
        int leading_drop = bits_per_digit - static_cast<int>(msb_bits);

        auto make_chunk = [&](const std::string& chunk_digits,
                              uint64_t chunk_bits)
            -> std::unique_ptr<Expr> {
          uint64_t value_bits = 0;
          uint64_t x_bits = 0;
          uint64_t z_bits = 0;
          for (size_t i = 0; i < chunk_digits.size(); ++i) {
            char c = chunk_digits[i];
            const int shift = static_cast<int>(
                (chunk_digits.size() - 1 - i) * bits_per_digit);
            if (shift >= 64) {
              continue;
            }
            uint64_t mask = ((1ull << bits_per_digit) - 1ull) << shift;
            if (c == 'x' || c == 'X') {
              value_bits |= mask;
              x_bits |= mask;
              continue;
            }
            if (c == 'z' || c == 'Z' || c == '?') {
              z_bits |= mask;
              continue;
            }
            int digit = 0;
            if (c >= '0' && c <= '9') {
              digit = c - '0';
            } else if (c >= 'a' && c <= 'f') {
              digit = 10 + (c - 'a');
            } else if (c >= 'A' && c <= 'F') {
              digit = 10 + (c - 'A');
            } else {
              digit = 0;
            }
            value_bits |= (static_cast<uint64_t>(digit) << shift);
          }
          if (chunk_bits < 64) {
            uint64_t mask = (chunk_bits == 0)
                                ? 0ull
                                : ((1ull << chunk_bits) - 1ull);
            value_bits &= mask;
            x_bits &= mask;
            z_bits &= mask;
          }
          auto expr = std::make_unique<Expr>();
          expr->kind = ExprKind::kNumber;
          expr->number = value_bits;
          expr->value_bits = value_bits;
          expr->x_bits = x_bits;
          expr->z_bits = z_bits;
          expr->has_base = true;
          expr->base_char = base_char;
          expr->is_signed = false;
          expr->has_width = true;
          expr->number_width = static_cast<int>(chunk_bits);
          return expr;
        };

        auto concat = std::make_unique<Expr>();
        concat->kind = ExprKind::kConcat;
        concat->repeat = 1;
        size_t pos = 0;
        while (pos < padded.size()) {
          size_t len = std::min(digits_per_chunk, padded.size() - pos);
          std::string chunk = padded.substr(pos, len);
          uint64_t chunk_bits =
              static_cast<uint64_t>(len) * bits_per_digit;
          if (pos == 0 && leading_drop > 0) {
            chunk_bits -= static_cast<uint64_t>(leading_drop);
          }
          concat->elements.push_back(make_chunk(chunk, chunk_bits));
          pos += len;
        }
        return concat;
      }
    }

    uint64_t value_bits = 0;
    uint64_t x_bits = 0;
    uint64_t z_bits = 0;
    if (base_char == 'd') {
      uint64_t value = 0;
      for (char c : cleaned) {
        int digit = 0;
        if (c >= '0' && c <= '9') {
          digit = c - '0';
        } else {
          ErrorHere("invalid digit in literal");
          return nullptr;
        }
        if (digit >= base) {
          ErrorHere("digit out of range for base literal");
          return nullptr;
        }
        value = value * static_cast<uint64_t>(base) +
                static_cast<uint64_t>(digit);
      }
      value_bits = value;
    } else {
      const size_t digit_count = cleaned.size();
      const int total_bits = static_cast<int>(digit_count) * bits_per_digit;
      for (size_t i = 0; i < digit_count; ++i) {
        char c = cleaned[i];
        const int shift =
            static_cast<int>((digit_count - 1 - i) * bits_per_digit);
        if (shift >= 64) {
          continue;
        }
        uint64_t mask = 0;
        if (bits_per_digit >= 64) {
          mask = 0xFFFFFFFFFFFFFFFFull;
        } else {
          mask = ((1ull << bits_per_digit) - 1ull) << shift;
        }
        if (c == 'x' || c == 'X') {
          value_bits |= mask;
          x_bits |= mask;
          continue;
        }
        if (c == 'z' || c == 'Z' || c == '?') {
          z_bits |= mask;
          continue;
        }
        int digit = 0;
        if (c >= '0' && c <= '9') {
          digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
          digit = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
          digit = 10 + (c - 'A');
        } else {
          ErrorHere("invalid digit in literal");
          return nullptr;
        }
        if (digit >= base) {
          ErrorHere("digit out of range for base literal");
          return nullptr;
        }
        value_bits |= (static_cast<uint64_t>(digit) << shift);
      }
      if (!has_xz && total_bits == 0) {
        ErrorHere("invalid base literal");
        return nullptr;
      }
      if (size == 0 && has_xz) {
        size = static_cast<uint64_t>(total_bits);
      }
    }
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kNumber;
    expr->number = value_bits;
    expr->value_bits = value_bits;
    expr->x_bits = x_bits;
    expr->z_bits = z_bits;
    expr->has_base = true;
    expr->base_char = base_char;
    expr->is_signed = is_signed;
    if (size > 0) {
      expr->has_width = true;
      expr->number_width = static_cast<int>(size);
      if (size < 64) {
        uint64_t mask = (1ull << size) - 1ull;
        expr->number &= mask;
        expr->value_bits &= mask;
        expr->x_bits &= mask;
        expr->z_bits &= mask;
      }
    }
    return expr;
  }

  std::unique_ptr<Expr> MakeBinary(char op, std::unique_ptr<Expr> lhs,
                                   std::unique_ptr<Expr> rhs) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kBinary;
    expr->op = op;
    expr->lhs = std::move(lhs);
    expr->rhs = std::move(rhs);
    return expr;
  }

  void AddOrUpdatePort(Module* module, const std::string& name, PortDir dir,
                       int width, bool is_signed, bool is_real,
                       const std::shared_ptr<Expr>& msb_expr,
                       const std::shared_ptr<Expr>& lsb_expr) {
    for (auto& port : module->ports) {
      if (port.name == name) {
        port.dir = dir;
        port.width = width;
        port.is_signed = is_signed;
        port.is_real = is_real;
        port.msb_expr = msb_expr;
        port.lsb_expr = lsb_expr;
        return;
      }
    }
    module->ports.push_back(
        Port{dir, name, width, is_signed, is_real, msb_expr, lsb_expr});
  }

  void AddOrUpdateNet(Module* module, const std::string& name, NetType type,
                      int width, bool is_signed,
                      const std::shared_ptr<Expr>& msb_expr,
                      const std::shared_ptr<Expr>& lsb_expr,
                      const std::vector<ArrayDim>& array_dims,
                      bool is_real = false,
                      ChargeStrength charge = ChargeStrength::kNone) {
    int array_size = 0;
    if (array_dims.size() == 1) {
      array_size = array_dims.front().size;
    }
    for (auto& net : module->nets) {
      if (net.name == name) {
        net.type = type;
        net.width = width;
        net.is_signed = is_signed;
        net.is_real = is_real;
        net.charge = charge;
        net.msb_expr = msb_expr;
        net.lsb_expr = lsb_expr;
        net.array_size = array_size;
        net.array_dims = array_dims;
        return;
      }
    }
    Net net;
    net.type = type;
    net.name = name;
    net.width = width;
    net.is_signed = is_signed;
    net.is_real = is_real;
    net.charge = charge;
    net.msb_expr = msb_expr;
    net.lsb_expr = lsb_expr;
    net.array_size = array_size;
    net.array_dims = array_dims;
    module->nets.push_back(std::move(net));
  }

  void AddImplicitNetDriver(Module* module, const std::string& name,
                            NetType type) {
    if (!module) {
      return;
    }
    Assign assign;
    assign.lhs = name;
    assign.has_strength = true;
    switch (type) {
      case NetType::kTri0:
        assign.rhs = MakeNumberExpr(0u);
        assign.strength0 = Strength::kPull;
        assign.strength1 = Strength::kHighZ;
        break;
      case NetType::kTri1:
        assign.rhs = MakeNumberExpr(1u);
        assign.strength0 = Strength::kHighZ;
        assign.strength1 = Strength::kPull;
        break;
      case NetType::kSupply0:
        assign.rhs = MakeNumberExpr(0u);
        assign.strength0 = Strength::kSupply;
        assign.strength1 = Strength::kHighZ;
        break;
      case NetType::kSupply1:
        assign.rhs = MakeNumberExpr(1u);
        assign.strength0 = Strength::kHighZ;
        assign.strength1 = Strength::kSupply;
        break;
      default:
        return;
    }
    module->assigns.push_back(std::move(assign));
  }

  bool IsArrayName(const std::string& name) const {
    if (!current_module_) {
      return false;
    }
    for (const auto& net : current_module_->nets) {
      if (net.name == name && !net.array_dims.empty()) {
        return true;
      }
    }
    return false;
  }

  bool IsArrayIndexExpr(const Expr& expr) const {
    const Expr* current = &expr;
    while (current->kind == ExprKind::kIndex) {
      if (!current->base) {
        return false;
      }
      current = current->base.get();
    }
    if (current->kind != ExprKind::kIdentifier) {
      return false;
    }
    return IsArrayName(current->ident);
  }

  void ErrorHere(const std::string& message) {
    const Token& token = Peek();
    diagnostics_->Add(Severity::kError, message,
                      SourceLocation{path_, token.line, token.column});
  }

  bool ParseNetTypeName(const std::string& name, NetType* out_type) const {
    if (!out_type) {
      return false;
    }
    if (name == "wire" || name == "tri") {
      *out_type = NetType::kWire;
      return true;
    }
    if (name == "wand") {
      *out_type = NetType::kWand;
      return true;
    }
    if (name == "wor") {
      *out_type = NetType::kWor;
      return true;
    }
    if (name == "tri0") {
      *out_type = NetType::kTri0;
      return true;
    }
    if (name == "tri1") {
      *out_type = NetType::kTri1;
      return true;
    }
    if (name == "triand") {
      *out_type = NetType::kTriand;
      return true;
    }
    if (name == "trior") {
      *out_type = NetType::kTrior;
      return true;
    }
    if (name == "trireg") {
      *out_type = NetType::kTrireg;
      return true;
    }
    if (name == "supply0") {
      *out_type = NetType::kSupply0;
      return true;
    }
    if (name == "supply1") {
      *out_type = NetType::kSupply1;
      return true;
    }
    return false;
  }

  bool ApplyDirective(const DirectiveEvent& directive) {
    switch (directive.kind) {
      case DirectiveKind::kDefaultNettype: {
        if (directive.arg == "none") {
          default_nettype_none_ = true;
          return true;
        }
        NetType type = NetType::kWire;
        if (!ParseNetTypeName(directive.arg, &type)) {
          diagnostics_->Add(
              Severity::kError,
              "unknown net type '" + directive.arg + "' in `default_nettype",
              SourceLocation{path_, directive.line, directive.column});
          return false;
        }
        if (NetTypeRequires4State(type) && !options_.enable_4state) {
          diagnostics_->Add(
              Severity::kError, "net type requires --4state",
              SourceLocation{path_, directive.line, directive.column});
          return false;
        }
        default_nettype_ = type;
        default_nettype_none_ = false;
        return true;
      }
      case DirectiveKind::kUnconnectedDrive: {
        if (directive.arg == "pull0") {
          unconnected_drive_ = UnconnectedDrive::kPull0;
          return true;
        }
        if (directive.arg == "pull1") {
          unconnected_drive_ = UnconnectedDrive::kPull1;
          return true;
        }
        diagnostics_->Add(
            Severity::kError,
            "unknown unconnected drive '" + directive.arg + "'",
            SourceLocation{path_, directive.line, directive.column});
        return false;
      }
      case DirectiveKind::kNoUnconnectedDrive:
        unconnected_drive_ = UnconnectedDrive::kNone;
        return true;
      case DirectiveKind::kResetAll:
        default_nettype_ = NetType::kWire;
        default_nettype_none_ = false;
        unconnected_drive_ = UnconnectedDrive::kNone;
        current_timescale_ = "1ns";
        return true;
      case DirectiveKind::kTimescale:
        if (!directive.arg.empty()) {
          current_timescale_ = directive.arg;
          if (current_module_) {
            current_module_->timescale = current_timescale_;
          }
        }
        return true;
    }
    return true;
  }

  bool ApplyDirectivesUpTo(int line) {
    while (directive_pos_ < directives_.size() &&
           directives_[directive_pos_].line <= line) {
      if (!ApplyDirective(directives_[directive_pos_])) {
        return false;
      }
      ++directive_pos_;
    }
    return true;
  }

  std::string path_;
  std::vector<Token> tokens_;
  Diagnostics* diagnostics_ = nullptr;
  size_t pos_ = 0;
  std::unordered_map<std::string, int64_t> current_params_;
  std::unordered_map<std::string, bool> current_real_params_;
  std::unordered_map<std::string, double> current_real_values_;
  GenvarScope current_genvars_;
  Module* current_module_ = nullptr;
  ParseOptions options_;
  std::vector<DirectiveEvent> directives_;
  size_t directive_pos_ = 0;
  NetType default_nettype_ = NetType::kWire;
  bool default_nettype_none_ = false;
  UnconnectedDrive unconnected_drive_ = UnconnectedDrive::kNone;
  std::string current_timescale_ = "1ns";
  bool allow_string_literals_ = false;
  int generate_id_ = 0;

  bool ExprIsRealParamExpr(const Expr& expr) const {
    switch (expr.kind) {
      case ExprKind::kIdentifier: {
        auto it = current_real_params_.find(expr.ident);
        return it != current_real_params_.end() && it->second;
      }
      case ExprKind::kNumber:
        return expr.is_real_literal;
      case ExprKind::kUnary:
        if (expr.unary_op == '+' || expr.unary_op == '-') {
          return expr.operand ? ExprIsRealParamExpr(*expr.operand) : false;
        }
        return false;
      case ExprKind::kBinary:
        if (expr.op == '+' || expr.op == '-' || expr.op == '*' ||
            expr.op == '/' || expr.op == 'p') {
          return (expr.lhs && ExprIsRealParamExpr(*expr.lhs)) ||
                 (expr.rhs && ExprIsRealParamExpr(*expr.rhs));
        }
        return false;
      case ExprKind::kTernary:
        return (expr.then_expr && ExprIsRealParamExpr(*expr.then_expr)) ||
               (expr.else_expr && ExprIsRealParamExpr(*expr.else_expr));
      case ExprKind::kCall:
        return expr.ident == "$realtime" || expr.ident == "$itor" ||
               expr.ident == "$bitstoreal";
      case ExprKind::kString:
      case ExprKind::kSelect:
      case ExprKind::kIndex:
      case ExprKind::kConcat:
        return false;
    }
    return false;
  }

  bool EvalConstRealExpr(const Expr& expr, double* out_value) {
    if (!out_value) {
      return false;
    }
    switch (expr.kind) {
      case ExprKind::kNumber: {
        if (expr.x_bits != 0 || expr.z_bits != 0) {
          ErrorHere("x/z not allowed in real constant expression");
          return false;
        }
        if (expr.is_real_literal) {
          double value = 0.0;
          uint64_t bits = expr.value_bits;
          std::memcpy(&value, &bits, sizeof(value));
          *out_value = value;
          return true;
        }
        *out_value = static_cast<double>(static_cast<int64_t>(expr.number));
        return true;
      }
      case ExprKind::kIdentifier: {
        auto real_it = current_real_values_.find(expr.ident);
        if (real_it != current_real_values_.end()) {
          *out_value = real_it->second;
          return true;
        }
        auto it = current_params_.find(expr.ident);
        if (it == current_params_.end()) {
          ErrorHere("unknown parameter '" + expr.ident + "'");
          return false;
        }
        *out_value = static_cast<double>(it->second);
        return true;
      }
      case ExprKind::kUnary: {
        double value = 0.0;
        if (!EvalConstRealExpr(*expr.operand, &value)) {
          return false;
        }
        switch (expr.unary_op) {
          case '+':
            *out_value = value;
            return true;
          case '-':
            *out_value = -value;
            return true;
          case '!':
            *out_value = (value == 0.0) ? 1.0 : 0.0;
            return true;
          default:
            ErrorHere("unsupported unary operator in real constant expression");
            return false;
        }
      }
      case ExprKind::kBinary: {
        double lhs = 0.0;
        double rhs = 0.0;
        if (!EvalConstRealExpr(*expr.lhs, &lhs) ||
            !EvalConstRealExpr(*expr.rhs, &rhs)) {
          return false;
        }
        switch (expr.op) {
          case '+':
            *out_value = lhs + rhs;
            return true;
          case '-':
            *out_value = lhs - rhs;
            return true;
          case '*':
            *out_value = lhs * rhs;
            return true;
          case '/':
            if (rhs == 0.0) {
              ErrorHere("division by zero in real constant expression");
              return false;
            }
            *out_value = lhs / rhs;
            return true;
          case 'A':
            *out_value = ((lhs != 0.0) && (rhs != 0.0)) ? 1.0 : 0.0;
            return true;
          case 'O':
            *out_value = ((lhs != 0.0) || (rhs != 0.0)) ? 1.0 : 0.0;
            return true;
          case 'E':
          case 'C':
          case 'W':
            *out_value = (lhs == rhs) ? 1.0 : 0.0;
            return true;
          case 'N':
          case 'c':
          case 'w':
            *out_value = (lhs != rhs) ? 1.0 : 0.0;
            return true;
          case '<':
            *out_value = (lhs < rhs) ? 1.0 : 0.0;
            return true;
          case '>':
            *out_value = (lhs > rhs) ? 1.0 : 0.0;
            return true;
          case 'L':
            *out_value = (lhs <= rhs) ? 1.0 : 0.0;
            return true;
          case 'G':
            *out_value = (lhs >= rhs) ? 1.0 : 0.0;
            return true;
          default:
            ErrorHere("unsupported operator in real constant expression");
            return false;
        }
      }
      case ExprKind::kTernary: {
        int64_t cond = 0;
        if (!EvalConstExpr(*expr.condition, &cond)) {
          return false;
        }
        if (cond != 0) {
          return EvalConstRealExpr(*expr.then_expr, out_value);
        }
        return EvalConstRealExpr(*expr.else_expr, out_value);
      }
      case ExprKind::kCall:
        if (expr.ident == "$itor") {
          if (expr.call_args.size() != 1) {
            ErrorHere("$itor expects 1 argument");
            return false;
          }
          int64_t value = 0;
          if (!EvalConstExpr(*expr.call_args[0], &value)) {
            return false;
          }
          *out_value = static_cast<double>(value);
          return true;
        }
        if (expr.ident == "$bitstoreal") {
          if (expr.call_args.size() != 1) {
            ErrorHere("$bitstoreal expects 1 argument");
            return false;
          }
          int64_t bits_value = 0;
          if (!EvalConstExpr(*expr.call_args[0], &bits_value)) {
            return false;
          }
          uint64_t bits = static_cast<uint64_t>(bits_value);
          double value = 0.0;
          std::memcpy(&value, &bits, sizeof(value));
          *out_value = value;
          return true;
        }
        if (expr.ident == "$rtoi") {
          if (expr.call_args.size() != 1) {
            ErrorHere("$rtoi expects 1 argument");
            return false;
          }
          double value = 0.0;
          if (!EvalConstRealExpr(*expr.call_args[0], &value)) {
            return false;
          }
          *out_value = static_cast<double>(static_cast<int64_t>(value));
          return true;
        }
        ErrorHere("function call not allowed in real constant expression");
        return false;
      case ExprKind::kString:
        ErrorHere("string literal not allowed in real constant expression");
        return false;
      case ExprKind::kSelect:
        ErrorHere("bit/part select not allowed in real constant expression");
        return false;
      case ExprKind::kIndex:
        ErrorHere("indexing not allowed in real constant expression");
        return false;
      case ExprKind::kConcat:
        ErrorHere("concatenation not allowed in real constant expression");
        return false;
    }
    return false;
  }

  bool TryEvalConstRealExpr(const Expr& expr, double* out_value) const {
    if (!out_value) {
      return false;
    }
    switch (expr.kind) {
      case ExprKind::kNumber: {
        if (expr.x_bits != 0 || expr.z_bits != 0) {
          return false;
        }
        if (expr.is_real_literal) {
          double value = 0.0;
          uint64_t bits = expr.value_bits;
          std::memcpy(&value, &bits, sizeof(value));
          *out_value = value;
          return true;
        }
        *out_value = static_cast<double>(static_cast<int64_t>(expr.number));
        return true;
      }
      case ExprKind::kIdentifier: {
        auto real_it = current_real_values_.find(expr.ident);
        if (real_it != current_real_values_.end()) {
          *out_value = real_it->second;
          return true;
        }
        auto it = current_params_.find(expr.ident);
        if (it == current_params_.end()) {
          return false;
        }
        *out_value = static_cast<double>(it->second);
        return true;
      }
      case ExprKind::kUnary: {
        double value = 0.0;
        if (!TryEvalConstRealExpr(*expr.operand, &value)) {
          return false;
        }
        switch (expr.unary_op) {
          case '+':
            *out_value = value;
            return true;
          case '-':
            *out_value = -value;
            return true;
          case '!':
            *out_value = (value == 0.0) ? 1.0 : 0.0;
            return true;
          default:
            return false;
        }
      }
      case ExprKind::kBinary: {
        double lhs = 0.0;
        double rhs = 0.0;
        if (!TryEvalConstRealExpr(*expr.lhs, &lhs) ||
            !TryEvalConstRealExpr(*expr.rhs, &rhs)) {
          return false;
        }
        switch (expr.op) {
          case '+':
            *out_value = lhs + rhs;
            return true;
          case '-':
            *out_value = lhs - rhs;
            return true;
          case '*':
            *out_value = lhs * rhs;
            return true;
          case '/':
            if (rhs == 0.0) {
              return false;
            }
            *out_value = lhs / rhs;
            return true;
          case 'A':
            *out_value = ((lhs != 0.0) && (rhs != 0.0)) ? 1.0 : 0.0;
            return true;
          case 'O':
            *out_value = ((lhs != 0.0) || (rhs != 0.0)) ? 1.0 : 0.0;
            return true;
          case 'E':
          case 'C':
          case 'W':
            *out_value = (lhs == rhs) ? 1.0 : 0.0;
            return true;
          case 'N':
          case 'c':
          case 'w':
            *out_value = (lhs != rhs) ? 1.0 : 0.0;
            return true;
          case '<':
            *out_value = (lhs < rhs) ? 1.0 : 0.0;
            return true;
          case '>':
            *out_value = (lhs > rhs) ? 1.0 : 0.0;
            return true;
          case 'L':
            *out_value = (lhs <= rhs) ? 1.0 : 0.0;
            return true;
          case 'G':
            *out_value = (lhs >= rhs) ? 1.0 : 0.0;
            return true;
          default:
            return false;
        }
      }
      case ExprKind::kTernary: {
        int64_t cond = 0;
        if (!TryEvalConstExpr(*expr.condition, &cond)) {
          return false;
        }
        if (cond != 0) {
          return TryEvalConstRealExpr(*expr.then_expr, out_value);
        }
        return TryEvalConstRealExpr(*expr.else_expr, out_value);
      }
      case ExprKind::kCall:
        if (expr.ident == "$itor") {
          if (expr.call_args.size() != 1) {
            return false;
          }
          int64_t value = 0;
          if (!TryEvalConstExpr(*expr.call_args[0], &value)) {
            return false;
          }
          *out_value = static_cast<double>(value);
          return true;
        }
        if (expr.ident == "$bitstoreal") {
          if (expr.call_args.size() != 1) {
            return false;
          }
          int64_t bits_value = 0;
          if (!TryEvalConstExpr(*expr.call_args[0], &bits_value)) {
            return false;
          }
          uint64_t bits = static_cast<uint64_t>(bits_value);
          double value = 0.0;
          std::memcpy(&value, &bits, sizeof(value));
          *out_value = value;
          return true;
        }
        if (expr.ident == "$rtoi") {
          if (expr.call_args.size() != 1) {
            return false;
          }
          double value = 0.0;
          if (!TryEvalConstRealExpr(*expr.call_args[0], &value)) {
            return false;
          }
          *out_value = static_cast<double>(static_cast<int64_t>(value));
          return true;
        }
        return false;
      case ExprKind::kString:
      case ExprKind::kSelect:
      case ExprKind::kIndex:
      case ExprKind::kConcat:
        return false;
    }
    return false;
  }

  bool EvalConstExpr(const Expr& expr, int64_t* out_value) {
    switch (expr.kind) {
      case ExprKind::kNumber:
        if (expr.is_real_literal) {
          ErrorHere("real literal not allowed in constant expression");
          return false;
        }
        if (expr.x_bits != 0 || expr.z_bits != 0) {
          ErrorHere("x/z not allowed in constant expression");
          return false;
        }
        *out_value = static_cast<int64_t>(expr.number);
        return true;
      case ExprKind::kString:
        ErrorHere("string literal not allowed in constant expression");
        return false;
      case ExprKind::kIdentifier: {
        auto it = current_params_.find(expr.ident);
        if (it == current_params_.end()) {
          ErrorHere("unknown parameter '" + expr.ident + "'");
          return false;
        }
        *out_value = it->second;
        return true;
      }
      case ExprKind::kUnary: {
        int64_t value = 0;
        if (!EvalConstExpr(*expr.operand, &value)) {
          return false;
        }
        switch (expr.unary_op) {
          case '+':
            *out_value = value;
            return true;
          case '-':
            *out_value = -value;
            return true;
          case '~':
            *out_value = ~value;
            return true;
          case '!':
            *out_value = (value == 0) ? 1 : 0;
            return true;
          case 'S':
            *out_value = value;
            return true;
          case 'U':
            *out_value = value;
            return true;
          case 'C': {
            if (value < 0) {
              ErrorHere("negative $clog2 argument");
              return false;
            }
            uint64_t input = static_cast<uint64_t>(value);
            uint64_t power = 1ull;
            int64_t result = 0;
            while (power < input) {
              power <<= 1;
              ++result;
            }
            *out_value = result;
            return true;
          }
          case '&': {
            uint64_t bits = static_cast<uint64_t>(value);
            *out_value = (bits == 0xFFFFFFFFFFFFFFFFull) ? 1 : 0;
            return true;
          }
          case '|': {
            uint64_t bits = static_cast<uint64_t>(value);
            *out_value = (bits != 0) ? 1 : 0;
            return true;
          }
          case '^': {
            uint64_t bits = static_cast<uint64_t>(value);
            int parity = 0;
            while (bits != 0) {
              parity ^= static_cast<int>(bits & 1ull);
              bits >>= 1;
            }
            *out_value = parity;
            return true;
          }
          default:
            ErrorHere("unsupported unary operator in constant expression");
            return false;
        }
      }
      case ExprKind::kBinary: {
        const bool lhs_real = ExprIsRealParamExpr(*expr.lhs);
        const bool rhs_real = ExprIsRealParamExpr(*expr.rhs);
        if ((lhs_real || rhs_real) &&
            (expr.op == 'A' || expr.op == 'O' || expr.op == 'E' ||
             expr.op == 'N' || expr.op == 'C' || expr.op == 'W' ||
             expr.op == 'c' || expr.op == 'w' || expr.op == '<' ||
             expr.op == '>' || expr.op == 'L' || expr.op == 'G')) {
          double lhs = 0.0;
          double rhs = 0.0;
          if (!EvalConstRealExpr(*expr.lhs, &lhs) ||
              !EvalConstRealExpr(*expr.rhs, &rhs)) {
            return false;
          }
          switch (expr.op) {
            case 'A':
              *out_value = ((lhs != 0.0) && (rhs != 0.0)) ? 1 : 0;
              return true;
            case 'O':
              *out_value = ((lhs != 0.0) || (rhs != 0.0)) ? 1 : 0;
              return true;
            case 'E':
            case 'C':
            case 'W':
              *out_value = (lhs == rhs) ? 1 : 0;
              return true;
            case 'N':
            case 'c':
            case 'w':
              *out_value = (lhs != rhs) ? 1 : 0;
              return true;
            case '<':
              *out_value = (lhs < rhs) ? 1 : 0;
              return true;
            case '>':
              *out_value = (lhs > rhs) ? 1 : 0;
              return true;
            case 'L':
              *out_value = (lhs <= rhs) ? 1 : 0;
              return true;
            case 'G':
              *out_value = (lhs >= rhs) ? 1 : 0;
              return true;
            default:
              break;
          }
        }
        if (lhs_real || rhs_real) {
          ErrorHere("real operands not allowed in constant expression");
          return false;
        }
        int64_t lhs = 0;
        int64_t rhs = 0;
        if (!EvalConstExpr(*expr.lhs, &lhs) ||
            !EvalConstExpr(*expr.rhs, &rhs)) {
          return false;
        }
        switch (expr.op) {
          case '+':
            *out_value = lhs + rhs;
            return true;
          case '-':
            *out_value = lhs - rhs;
            return true;
          case '*':
            *out_value = lhs * rhs;
            return true;
          case 'p': {
            if (rhs < 0) {
              *out_value = 0;
              return true;
            }
            int64_t result = 1;
            int64_t base = lhs;
            uint64_t exp = static_cast<uint64_t>(rhs);
            while (exp != 0) {
              if (exp & 1ull) {
                result *= base;
              }
              base *= base;
              exp >>= 1ull;
            }
            *out_value = result;
            return true;
          }
          case '/':
            if (rhs == 0) {
              ErrorHere("division by zero in constant expression");
              return false;
            }
            *out_value = lhs / rhs;
            return true;
          case '%':
            if (rhs == 0) {
              ErrorHere("division by zero in constant expression");
              return false;
            }
            *out_value = lhs % rhs;
            return true;
          case '&':
            *out_value = lhs & rhs;
            return true;
          case '|':
            *out_value = lhs | rhs;
            return true;
          case '^':
            *out_value = lhs ^ rhs;
            return true;
          case 'A':
            *out_value = ((lhs != 0) && (rhs != 0)) ? 1 : 0;
            return true;
          case 'O':
            *out_value = ((lhs != 0) || (rhs != 0)) ? 1 : 0;
            return true;
          case 'E':
            *out_value = (lhs == rhs) ? 1 : 0;
            return true;
          case 'N':
            *out_value = (lhs != rhs) ? 1 : 0;
            return true;
          case 'C':
          case 'W':
            *out_value = (lhs == rhs) ? 1 : 0;
            return true;
          case 'c':
          case 'w':
            *out_value = (lhs != rhs) ? 1 : 0;
            return true;
          case '<':
            *out_value = (lhs < rhs) ? 1 : 0;
            return true;
          case '>':
            *out_value = (lhs > rhs) ? 1 : 0;
            return true;
          case 'L':
            *out_value = (lhs <= rhs) ? 1 : 0;
            return true;
          case 'G':
            *out_value = (lhs >= rhs) ? 1 : 0;
            return true;
          case 'l':
            if (rhs < 0) {
              ErrorHere("negative shift in constant expression");
              return false;
            }
            *out_value = lhs << rhs;
            return true;
          case 'r':
            if (rhs < 0) {
              ErrorHere("negative shift in constant expression");
              return false;
            }
            *out_value = lhs >> rhs;
            return true;
          case 'R':
            if (rhs < 0) {
              ErrorHere("negative shift in constant expression");
              return false;
            }
            *out_value = lhs >> rhs;
            return true;
          default:
            ErrorHere("unsupported operator in constant expression");
            return false;
        }
      }
      case ExprKind::kTernary: {
        int64_t cond = 0;
        if (!EvalConstExpr(*expr.condition, &cond)) {
          return false;
        }
        if (cond != 0) {
          return EvalConstExpr(*expr.then_expr, out_value);
        }
        return EvalConstExpr(*expr.else_expr, out_value);
      }
      case ExprKind::kSelect:
        ErrorHere("bit/part select not allowed in constant expression");
        return false;
      case ExprKind::kIndex:
        ErrorHere("indexing not allowed in constant expression");
        return false;
      case ExprKind::kCall:
        if (expr.ident == "$rtoi") {
          if (expr.call_args.size() != 1) {
            ErrorHere("$rtoi expects 1 argument");
            return false;
          }
          double value = 0.0;
          if (!EvalConstRealExpr(*expr.call_args[0], &value)) {
            return false;
          }
          *out_value = static_cast<int64_t>(value);
          return true;
        }
        ErrorHere("function call not allowed in constant expression");
        return false;
      case ExprKind::kConcat:
        ErrorHere("concatenation not allowed in constant expression");
        return false;
    }
    return false;
  }

  bool TryEvalConstExpr(const Expr& expr, int64_t* out_value) const {
    switch (expr.kind) {
      case ExprKind::kNumber:
        if (expr.is_real_literal) {
          return false;
        }
        if (expr.x_bits != 0 || expr.z_bits != 0) {
          return false;
        }
        *out_value = static_cast<int64_t>(expr.number);
        return true;
      case ExprKind::kString:
        return false;
      case ExprKind::kIdentifier: {
        auto it = current_params_.find(expr.ident);
        if (it == current_params_.end()) {
          return false;
        }
        *out_value = it->second;
        return true;
      }
      case ExprKind::kUnary: {
        int64_t value = 0;
        if (!TryEvalConstExpr(*expr.operand, &value)) {
          return false;
        }
        switch (expr.unary_op) {
          case '+':
            *out_value = value;
            return true;
          case '-':
            *out_value = -value;
            return true;
          case '~':
            *out_value = ~value;
            return true;
          case '!':
            *out_value = (value == 0) ? 1 : 0;
            return true;
          case 'S':
            *out_value = value;
            return true;
          case 'U':
            *out_value = value;
            return true;
          case 'C': {
            if (value < 0) {
              return false;
            }
            uint64_t input = static_cast<uint64_t>(value);
            uint64_t power = 1ull;
            int64_t result = 0;
            while (power < input) {
              power <<= 1;
              ++result;
            }
            *out_value = result;
            return true;
          }
          case '&': {
            uint64_t bits = static_cast<uint64_t>(value);
            *out_value = (bits == 0xFFFFFFFFFFFFFFFFull) ? 1 : 0;
            return true;
          }
          case '|': {
            uint64_t bits = static_cast<uint64_t>(value);
            *out_value = (bits != 0) ? 1 : 0;
            return true;
          }
          case '^': {
            uint64_t bits = static_cast<uint64_t>(value);
            int parity = 0;
            while (bits != 0) {
              parity ^= static_cast<int>(bits & 1ull);
              bits >>= 1;
            }
            *out_value = parity;
            return true;
          }
          default:
            return false;
        }
      }
      case ExprKind::kBinary: {
        const bool lhs_real = ExprIsRealParamExpr(*expr.lhs);
        const bool rhs_real = ExprIsRealParamExpr(*expr.rhs);
        if ((lhs_real || rhs_real) &&
            (expr.op == 'A' || expr.op == 'O' || expr.op == 'E' ||
             expr.op == 'N' || expr.op == 'C' || expr.op == 'W' ||
             expr.op == 'c' || expr.op == 'w' || expr.op == '<' ||
             expr.op == '>' || expr.op == 'L' || expr.op == 'G')) {
          double lhs = 0.0;
          double rhs = 0.0;
          if (!TryEvalConstRealExpr(*expr.lhs, &lhs) ||
              !TryEvalConstRealExpr(*expr.rhs, &rhs)) {
            return false;
          }
          switch (expr.op) {
            case 'A':
              *out_value = ((lhs != 0.0) && (rhs != 0.0)) ? 1 : 0;
              return true;
            case 'O':
              *out_value = ((lhs != 0.0) || (rhs != 0.0)) ? 1 : 0;
              return true;
            case 'E':
            case 'C':
            case 'W':
              *out_value = (lhs == rhs) ? 1 : 0;
              return true;
            case 'N':
            case 'c':
            case 'w':
              *out_value = (lhs != rhs) ? 1 : 0;
              return true;
            case '<':
              *out_value = (lhs < rhs) ? 1 : 0;
              return true;
            case '>':
              *out_value = (lhs > rhs) ? 1 : 0;
              return true;
            case 'L':
              *out_value = (lhs <= rhs) ? 1 : 0;
              return true;
            case 'G':
              *out_value = (lhs >= rhs) ? 1 : 0;
              return true;
            default:
              break;
          }
        }
        if (lhs_real || rhs_real) {
          return false;
        }
        int64_t lhs = 0;
        int64_t rhs = 0;
        if (!TryEvalConstExpr(*expr.lhs, &lhs) ||
            !TryEvalConstExpr(*expr.rhs, &rhs)) {
          return false;
        }
        switch (expr.op) {
          case '+':
            *out_value = lhs + rhs;
            return true;
          case '-':
            *out_value = lhs - rhs;
            return true;
          case '*':
            *out_value = lhs * rhs;
            return true;
          case 'p': {
            if (rhs < 0) {
              *out_value = 0;
              return true;
            }
            int64_t result = 1;
            int64_t base = lhs;
            uint64_t exp = static_cast<uint64_t>(rhs);
            while (exp != 0) {
              if (exp & 1ull) {
                result *= base;
              }
              base *= base;
              exp >>= 1ull;
            }
            *out_value = result;
            return true;
          }
          case '/':
            if (rhs == 0) {
              return false;
            }
            *out_value = lhs / rhs;
            return true;
          case '%':
            if (rhs == 0) {
              return false;
            }
            *out_value = lhs % rhs;
            return true;
          case '&':
            *out_value = lhs & rhs;
            return true;
          case '|':
            *out_value = lhs | rhs;
            return true;
          case '^':
            *out_value = lhs ^ rhs;
            return true;
          case 'A':
            *out_value = ((lhs != 0) && (rhs != 0)) ? 1 : 0;
            return true;
          case 'O':
            *out_value = ((lhs != 0) || (rhs != 0)) ? 1 : 0;
            return true;
          case 'E':
            *out_value = (lhs == rhs) ? 1 : 0;
            return true;
          case 'N':
            *out_value = (lhs != rhs) ? 1 : 0;
            return true;
          case 'C':
          case 'W':
            *out_value = (lhs == rhs) ? 1 : 0;
            return true;
          case 'c':
          case 'w':
            *out_value = (lhs != rhs) ? 1 : 0;
            return true;
          case '<':
            *out_value = (lhs < rhs) ? 1 : 0;
            return true;
          case '>':
            *out_value = (lhs > rhs) ? 1 : 0;
            return true;
          case 'L':
            *out_value = (lhs <= rhs) ? 1 : 0;
            return true;
          case 'G':
            *out_value = (lhs >= rhs) ? 1 : 0;
            return true;
          case 'l':
            if (rhs < 0) {
              return false;
            }
            *out_value = lhs << rhs;
            return true;
          case 'r':
            if (rhs < 0) {
              return false;
            }
            *out_value = lhs >> rhs;
            return true;
          case 'R':
            if (rhs < 0) {
              return false;
            }
            *out_value = lhs >> rhs;
            return true;
          default:
            return false;
        }
      }
      case ExprKind::kTernary: {
        int64_t cond = 0;
        if (!TryEvalConstExpr(*expr.condition, &cond)) {
          return false;
        }
        if (cond != 0) {
          return TryEvalConstExpr(*expr.then_expr, out_value);
        }
        return TryEvalConstExpr(*expr.else_expr, out_value);
      }
      case ExprKind::kSelect:
      case ExprKind::kIndex:
      case ExprKind::kCall:
        if (expr.ident == "$rtoi") {
          if (expr.call_args.size() != 1) {
            return false;
          }
          double value = 0.0;
          if (!TryEvalConstRealExpr(*expr.call_args[0], &value)) {
            return false;
          }
          *out_value = static_cast<int64_t>(value);
          return true;
        }
        return false;
      case ExprKind::kConcat:
        return false;
    }
    return false;
  }

  bool ParseConstExpr(std::unique_ptr<Expr>* out_expr, int64_t* out_value,
                      const std::string& context) {
    std::unique_ptr<Expr> expr = ParseExpr();
    if (!expr) {
      return false;
    }
    int64_t value = 0;
    if (!EvalConstExpr(*expr, &value)) {
      ErrorHere("expected constant expression for " + context);
      return false;
    }
    if (out_expr) {
      *out_expr = std::move(expr);
    }
    if (out_value) {
      *out_value = value;
    }
    return true;
  }

  bool IsInstanceStart() const {
    if (Peek().kind != TokenKind::kIdentifier) {
      return false;
    }
    if (Peek(1).kind == TokenKind::kSymbol && Peek(1).text == "#") {
      return true;
    }
    if (Peek(1).kind != TokenKind::kIdentifier) {
      return false;
    }
    if (Peek(2).kind == TokenKind::kSymbol &&
        (Peek(2).text == "(" || Peek(2).text == "[")) {
      return true;
    }
    return false;
  }

  bool ParseParamOverrides(Instance* instance) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after '#'");
      return false;
    }
    if (MatchSymbol(")")) {
      return true;
    }
    bool named = false;
    if (Peek().kind == TokenKind::kSymbol && Peek().text == ".") {
      named = true;
    }
    if (named) {
      while (true) {
        if (!MatchSymbol(".")) {
          ErrorHere("expected named parameter override ('.PARAM(expr)')");
          return false;
        }
        std::string name;
        if (!ConsumeIdentifier(&name)) {
          ErrorHere("expected parameter name after '.'");
          return false;
        }
        if (!MatchSymbol("(")) {
          ErrorHere("expected '(' after parameter name");
          return false;
        }
        std::unique_ptr<Expr> expr = ParseExpr();
        if (!expr) {
          return false;
        }
        if (!MatchSymbol(")")) {
          ErrorHere("expected ')' after parameter expression");
          return false;
        }
        instance->param_overrides.push_back(
            ParamOverride{name, std::move(expr)});
        if (MatchSymbol(",")) {
          continue;
        }
        break;
      }
    } else {
      while (true) {
        std::unique_ptr<Expr> expr = ParseExpr();
        if (!expr) {
          return false;
        }
        instance->param_overrides.push_back(ParamOverride{"", std::move(expr)});
        if (MatchSymbol(",")) {
          continue;
        }
        break;
      }
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after parameter overrides");
      return false;
    }
    return true;
  }

  bool MatchSymbol2(const char* symbol) {
    if (Peek().kind == TokenKind::kSymbol &&
        Peek(1).kind == TokenKind::kSymbol &&
        Peek().text == std::string(1, symbol[0]) &&
        Peek(1).text == std::string(1, symbol[1])) {
      Advance();
      Advance();
      return true;
    }
    return false;
  }

  bool MatchSymbol3(const char* symbol) {
    if (Peek().kind == TokenKind::kSymbol &&
        Peek(1).kind == TokenKind::kSymbol &&
        Peek(2).kind == TokenKind::kSymbol &&
        Peek().text == std::string(1, symbol[0]) &&
        Peek(1).text == std::string(1, symbol[1]) &&
        Peek(2).text == std::string(1, symbol[2])) {
      Advance();
      Advance();
      Advance();
      return true;
    }
    return false;
  }
};

}  // namespace

bool ParseVerilogFile(const std::string& path, Program* out_program,
                      Diagnostics* diagnostics,
                      const ParseOptions& options) {
  if (!out_program || !diagnostics) {
    return false;
  }

  std::ifstream file(path);
  if (!file) {
    diagnostics->Add(Severity::kError,
                     "failed to open input file",
                     SourceLocation{path});
    return false;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  const std::string raw_text = buffer.str();
  if (raw_text.empty() && !options.allow_empty) {
    diagnostics->Add(Severity::kError,
                     "input file is empty",
                     SourceLocation{path});
    return false;
  }
  std::string text;
  std::vector<DirectiveEvent> directives;
  if (!PreprocessVerilog(raw_text, path, diagnostics, &text, &directives)) {
    return false;
  }

  Parser parser(path, Tokenize(text), diagnostics, options,
                std::move(directives));
  if (!parser.ParseProgram(out_program)) {
    return false;
  }

  if (out_program->modules.empty() && !options.allow_empty) {
    diagnostics->Add(Severity::kError,
                     "no modules found in input",
                     SourceLocation{path});
    return false;
  }
  return true;
}

}  // namespace gpga
