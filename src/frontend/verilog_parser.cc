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
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

std::string StripUnderscores(const std::string& text) {
  if (text.find('_') == std::string::npos) {
    return text;
  }
  std::string cleaned;
  cleaned.reserve(text.size());
  for (char c : text) {
    if (c != '_') {
      cleaned.push_back(c);
    }
  }
  return cleaned;
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
      bool prev_is_at = false;
      size_t back = i;
      while (back > 0) {
        --back;
        char prev = text[back];
        if (std::isspace(static_cast<unsigned char>(prev))) {
          continue;
        }
        prev_is_at = (prev == '@');
        break;
      }
      if (!prev_is_at) {
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
    if (c == '\\') {
      int token_line = line;
      int token_column = column;
      ++i;
      ++column;
      size_t start = i;
      while (i < text.size() &&
             !std::isspace(static_cast<unsigned char>(text[i]))) {
        ++i;
        ++column;
      }
      push(TokenKind::kIdentifier, text.substr(start, i - start), token_line,
           token_column);
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
               (std::isdigit(static_cast<unsigned char>(text[i])) ||
                text[i] == '_')) {
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
               (std::isdigit(static_cast<unsigned char>(text[i])) ||
                text[i] == '_')) {
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
               (std::isdigit(static_cast<unsigned char>(text[i])) ||
                text[i] == '_')) {
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

struct MacroDef {
  std::vector<std::string> args;
  std::string body;
};

bool HasUnterminatedStringLiteral(const std::string& text) {
  bool in_string = false;
  bool escaped = false;
  for (char ch : text) {
    if (!in_string) {
      if (ch == '"') {
        in_string = true;
      }
      continue;
    }
    if (escaped) {
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      in_string = false;
    }
  }
  return in_string;
}

bool ExpandDefines(const std::string& line,
                   const std::unordered_map<std::string, MacroDef>& defines,
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
    const MacroDef& macro = it->second;
    std::string expansion = macro.body;
    size_t invoke_end = end;
    if (!macro.args.empty()) {
      size_t pos = end;
      while (pos < line.size() &&
             std::isspace(static_cast<unsigned char>(line[pos]))) {
        ++pos;
      }
      if (pos >= line.size() || line[pos] != '(') {
        if (diagnostics) {
          diagnostics->Add(Severity::kError,
                           "expected '(' after macro '" + name + "'",
                           SourceLocation{path, line_number,
                                          static_cast<int>(pos + 1)});
        }
        return false;
      }
      ++pos;
      std::vector<std::string> arg_values;
      std::string current;
      int depth = 1;
      bool in_string = false;
      auto flush_arg = [&]() {
        size_t left = current.find_first_not_of(" \t");
        size_t right = current.find_last_not_of(" \t");
        if (left == std::string::npos) {
          arg_values.emplace_back("");
        } else {
          arg_values.push_back(current.substr(left, right - left + 1));
        }
        current.clear();
      };
      for (; pos < line.size(); ++pos) {
        char ch = line[pos];
        if (in_string) {
          current.push_back(ch);
          if (ch == '"' && (pos == 0 || line[pos - 1] != '\\')) {
            in_string = false;
          }
          continue;
        }
        if (ch == '"') {
          in_string = true;
          current.push_back(ch);
          continue;
        }
        if (ch == '(') {
          ++depth;
          current.push_back(ch);
          continue;
        }
        if (ch == ')') {
          --depth;
          if (depth == 0) {
            flush_arg();
            ++pos;
            break;
          }
          current.push_back(ch);
          continue;
        }
        if (ch == ',' && depth == 1) {
          flush_arg();
          continue;
        }
        current.push_back(ch);
      }
      if (depth != 0) {
        if (diagnostics) {
          diagnostics->Add(Severity::kError,
                           "unterminated macro invocation",
                           SourceLocation{path, line_number,
                                          static_cast<int>(i + 1)});
        }
        return false;
      }
      invoke_end = pos;
      if (arg_values.size() != macro.args.size()) {
        if (diagnostics) {
          diagnostics->Add(Severity::kError,
                           "macro '" + name + "' expects " +
                               std::to_string(macro.args.size()) +
                               " arguments",
                           SourceLocation{path, line_number,
                                          static_cast<int>(i + 1)});
        }
        return false;
      }
      std::unordered_map<std::string, std::string> arg_map;
      for (size_t arg_index = 0; arg_index < macro.args.size(); ++arg_index) {
        arg_map[macro.args[arg_index]] = arg_values[arg_index];
      }
      std::string substituted;
      substituted.reserve(expansion.size());
      for (size_t j = 0; j < expansion.size();) {
        char ch = expansion[j];
        if (!IsIdentStart(ch)) {
          substituted.push_back(ch);
          ++j;
          continue;
        }
        size_t ident_start = j;
        ++j;
        while (j < expansion.size() && IsIdentChar(expansion[j])) {
          ++j;
        }
        std::string ident = expansion.substr(ident_start, j - ident_start);
        auto arg_it = arg_map.find(ident);
        if (arg_it != arg_map.end()) {
          substituted += arg_it->second;
        } else {
          substituted += ident;
        }
      }
      expansion = std::move(substituted);
    }
    result += expansion;
    i = invoke_end - 1;
  }
  *out_line = std::move(result);
  return true;
}

struct IfdefState {
  bool parent_active = true;
  bool branch_taken = false;
  bool else_seen = false;
  bool active = true;
};

enum class DirectiveKind {
  kDefaultNettype,
  kUnconnectedDrive,
  kNoUnconnectedDrive,
  kResetAll,
  kTimescale,
  kBeginKeywords,
  kEndKeywords,
};

struct DirectiveEvent {
  DirectiveKind kind = DirectiveKind::kDefaultNettype;
  std::string arg;
  int line = 1;
  int column = 1;
};

enum class KeywordVersion {
  k1364_1995,
  k1364_2001,
  k1364_2005,
};

bool PreprocessVerilogInternal(
    const std::string& input, const std::string& path, Diagnostics* diagnostics,
    std::unordered_map<std::string, MacroDef>* defines,
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
          std::vector<std::string> args;
          size_t after_name = name_end;
          if (after_name < line.size() && line[after_name] == '(') {
            size_t arg_pos = after_name + 1;
            while (arg_pos < line.size() && line[arg_pos] != ')') {
              while (arg_pos < line.size() &&
                     std::isspace(static_cast<unsigned char>(line[arg_pos]))) {
                ++arg_pos;
              }
              if (arg_pos < line.size() && line[arg_pos] == ')') {
                break;
              }
              if (arg_pos >= line.size() ||
                  !IsIdentStart(line[arg_pos])) {
                diagnostics->Add(Severity::kError,
                                 "expected macro argument name",
                                 SourceLocation{path, line_number,
                                                static_cast<int>(arg_pos + 1)});
                return false;
              }
              size_t arg_end = arg_pos + 1;
              while (arg_end < line.size() && IsIdentChar(line[arg_end])) {
                ++arg_end;
              }
              args.push_back(line.substr(arg_pos, arg_end - arg_pos));
              arg_pos = arg_end;
              while (arg_pos < line.size() &&
                     std::isspace(static_cast<unsigned char>(line[arg_pos]))) {
                ++arg_pos;
              }
              if (arg_pos < line.size() && line[arg_pos] == ',') {
                ++arg_pos;
              }
            }
            if (arg_pos >= line.size() || line[arg_pos] != ')') {
              diagnostics->Add(Severity::kError,
                               "unterminated macro parameter list",
                               SourceLocation{path, line_number,
                                              static_cast<int>(after_name + 1)});
              return false;
            }
            after_name = arg_pos + 1;
          }
          size_t body_start = line.find_first_not_of(" \t", after_name);
          std::string body = (body_start == std::string::npos)
                                 ? ""
                                 : line.substr(body_start);
          if (HasUnterminatedStringLiteral(body)) {
            int column = static_cast<int>(
                (body_start == std::string::npos ? after_name : body_start) + 1);
            diagnostics->Add(Severity::kError,
                             "macro definition cannot split a string literal",
                             SourceLocation{path, line_number, column});
            return false;
          }
          (*defines)[name] = MacroDef{std::move(args), std::move(body)};
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
        state.branch_taken = condition_true;
        state.active = active && condition_true;
        if_stack.push_back(state);
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "elsif") {
        if (if_stack.empty()) {
          diagnostics->Add(Severity::kError,
                           "unexpected `elsif without `ifdef",
                           SourceLocation{path, line_number,
                                          static_cast<int>(first + 1)});
          return false;
        }
        IfdefState& state = if_stack.back();
        if (state.else_seen) {
          diagnostics->Add(Severity::kError,
                           "`elsif after `else",
                           SourceLocation{path, line_number,
                                          static_cast<int>(first + 1)});
          return false;
        }
        while (pos < line.size() &&
               std::isspace(static_cast<unsigned char>(line[pos]))) {
          ++pos;
        }
        size_t name_start = pos;
        if (name_start >= line.size() || !IsIdentStart(line[name_start])) {
          diagnostics->Add(Severity::kError,
                           "expected macro name after `elsif",
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
        bool condition_true = defined;
        bool take_branch =
            state.parent_active && !state.branch_taken && condition_true;
        if (condition_true && !state.branch_taken) {
          state.branch_taken = true;
        }
        state.active = take_branch;
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
        state.active = state.parent_active && !state.branch_taken;
        state.branch_taken = true;
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
      if (directive == "begin_keywords") {
        if (active && directives) {
          size_t arg_pos = line.find_first_not_of(" \t", pos);
          if (arg_pos == std::string::npos || line[arg_pos] != '"') {
            diagnostics->Add(Severity::kError,
                             "expected string literal after `begin_keywords",
                             SourceLocation{path, line_number,
                                            static_cast<int>(pos + 1)});
            return false;
          }
          size_t arg_end = arg_pos + 1;
          while (arg_end < line.size() && line[arg_end] != '"') {
            ++arg_end;
          }
          if (arg_end >= line.size()) {
            diagnostics->Add(Severity::kError,
                             "unterminated `begin_keywords string",
                             SourceLocation{path, line_number,
                                            static_cast<int>(arg_pos + 1)});
            return false;
          }
          directives->push_back(DirectiveEvent{
              DirectiveKind::kBeginKeywords,
              line.substr(arg_pos + 1, arg_end - arg_pos - 1), line_number,
              static_cast<int>(first + 1)});
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "end_keywords") {
        if (active && directives) {
          directives->push_back(DirectiveEvent{
              DirectiveKind::kEndKeywords, "", line_number,
              static_cast<int>(first + 1)});
        }
        output << "\n";
        ++line_number;
        continue;
      }
      if (directive == "line") {
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
      if (directive.empty()) {
        diagnostics->Add(Severity::kError,
                         "unsupported compiler directive",
                         SourceLocation{path, line_number,
                                        static_cast<int>(first + 1)});
        return false;
      }
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
  std::unordered_map<std::string, MacroDef> defines;
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
    bool is_implicit = false;
    int id = 0;
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

  struct GateArrayRange {
    bool has_range = false;
    int msb = 0;
    int lsb = 0;
  };

  struct GenerateLocalparam {
    std::string name;
    std::unique_ptr<Expr> expr;
  };

  struct GenerateHierPart {
    std::string name;
    std::unique_ptr<Expr> index;
  };

  struct GenerateDefparamTarget {
    std::vector<GenerateHierPart> instance_parts;
    std::string param;
    std::unique_ptr<Expr> expr;
    int line = 0;
    int column = 0;
  };

  struct GenerateDefparam {
    std::vector<GenerateDefparamTarget> targets;
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
      kTask,
      kLocalparam,
      kDefparam,
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
    Task task;
    GenerateLocalparam localparam;
    GenerateDefparam defparam;
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
    char edge_kind = 0;
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
    bool has_initial = false;
    char initial_value = 'x';
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

  bool PeekSymbol(const char* symbol) const {
    return Peek().kind == TokenKind::kSymbol && Peek().text == symbol;
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
      const Token& token = Peek();
      bool escaped = !token.text.empty() && token.text[0] == '\\';
      *out = token.text;
      if (escaped) {
        out->erase(0, 1);
      }
      if (!escaped && IsReservedIdentifier(*out)) {
        diagnostics_->Add(
            Severity::kError,
            "reserved keyword '" + *out + "' cannot be used as identifier",
            SourceLocation{path_, token.line, token.column});
      }
      Advance();
      return true;
    }
    if (Peek().kind == TokenKind::kSymbol && Peek().text == "\\") {
      std::string escaped;
      int line = Peek().line;
      int next_col = Peek().column + 1;
      Advance();
      while (Peek().kind != TokenKind::kEnd) {
        if (Peek().line != line || Peek().column != next_col) {
          break;
        }
        escaped += Peek().text;
        next_col += static_cast<int>(Peek().text.size());
        Advance();
      }
      if (!escaped.empty()) {
        *out = escaped;
        return true;
      }
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
      const std::string token = StripUnderscores(Peek().text);
      *out = std::stoi(token);
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
    current_specparams_.clear();
    current_genvars_.Reset();
    gate_array_ranges_.clear();
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
      SkipAttributeInstances();
      if (MatchKeyword("endmodule")) {
        current_module_ = nullptr;
        if (!ApplyDefparams(&module)) {
          return false;
        }
        program->modules.push_back(std::move(module));
        return true;
      }
      if (MatchKeyword("automatic")) {
        if (MatchKeyword("task")) {
          if (!ParseTask(&module, true)) {
            return false;
          }
          continue;
        }
        if (MatchKeyword("function")) {
          if (!ParseFunction(&module, true)) {
            return false;
          }
          continue;
        }
        ErrorHere("unsupported module item 'automatic'");
        return false;
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
      if (Peek().kind == TokenKind::kIdentifier &&
          (Peek().text == "for" || Peek().text == "if" ||
           Peek().text == "case" || Peek().text == "casez" ||
           Peek().text == "casex" || Peek().text == "begin")) {
        if (!ParseImplicitGenerate(&module)) {
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
      if (MatchKeyword("realtime")) {
        if (!ParseRealDecl(&module)) {
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
      if (MatchKeyword("specparam")) {
        if (!ParseSpecparamDecl(&module)) {
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
        if (!ParseFunction(&module, false)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("task")) {
        if (!ParseTask(&module, false)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("specify")) {
        if (!ParseSpecifyBlock(&module)) {
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
            AlwaysBlock block;
            block.edge = EdgeKind::kCombinational;
            block.sensitivity = "*";
            block.is_synthesized = true;
            Statement stmt;
            stmt.kind = StatementKind::kAssign;
            stmt.assign.lhs = gate_assign.lhs;
            stmt.assign.lhs_has_range = gate_assign.lhs_has_range;
            stmt.assign.lhs_msb = gate_assign.lhs_msb;
            stmt.assign.lhs_lsb = gate_assign.lhs_lsb;
            for (auto& idx : gate_assign.lhs_indices) {
              stmt.assign.lhs_indices.push_back(std::move(idx));
            }
            stmt.assign.rhs = std::move(gate_assign.rhs);
            stmt.assign.nonblocking = false;
            block.statements.push_back(std::move(stmt));
            module.always_blocks.push_back(std::move(block));
            continue;
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
          assign.is_implicit = true;
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
    current_specparams_.clear();
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
    bool udp_has_initial = false;
    char udp_initial_value = 'x';
    while (true) {
      if (MatchKeyword("table")) {
        break;
      }
      if (MatchKeyword("initial")) {
        std::string target;
        if (!ConsumeIdentifier(&target)) {
          ErrorHere("expected identifier after 'initial'");
          return false;
        }
        if (!MatchSymbol("=")) {
          ErrorHere("expected '=' after UDP initial target");
          return false;
        }
        std::unique_ptr<Expr> init_expr = ParseExpr();
        if (!init_expr) {
          return false;
        }
        if (!MatchSymbol(";")) {
          ErrorHere("expected ';' after UDP initial");
          return false;
        }
        char init_value = 'x';
        if (init_expr->kind == ExprKind::kNumber) {
          if (init_expr->x_bits || init_expr->z_bits) {
            init_value = 'x';
          } else {
            init_value = (init_expr->value_bits & 1u) ? '1' : '0';
          }
        } else {
          int64_t value = 0;
          if (TryEvalConstExpr(*init_expr, &value)) {
            init_value = (value != 0) ? '1' : '0';
          }
        }
        udp_has_initial = true;
        udp_initial_value = init_value;
        continue;
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
      if (MatchKeyword("reg")) {
        if (!ParseRegDecl(&module)) {
          return false;
        }
        continue;
      }
      if (Peek().kind == TokenKind::kEnd) {
        ErrorHere("unexpected end of file in primitive body");
        return false;
      }
      ErrorHere("expected declaration or 'table' in primitive body");
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
    info.has_initial = udp_has_initial;
    info.initial_value = udp_initial_value;
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
    Strength pull_strength = Strength::kPull;
    Strength pull_strength0 = Strength::kPull;
    Strength pull_strength1 = Strength::kPull;
    int pull_drive_value = pull_up ? 1 : 0;
    bool has_strength = false;
    bool has_pair_strength = false;
    if (IsDriveStrengthLookahead()) {
      Strength strength0 = Strength::kStrong;
      Strength strength1 = Strength::kStrong;
      bool parsed = false;
      if (!ParseDriveStrength(&strength0, &strength1, &parsed)) {
        return false;
      }
      if (parsed) {
        pull_strength0 = strength0;
        pull_strength1 = strength1;
        has_pair_strength = true;
      }
    } else if (Peek().kind == TokenKind::kSymbol && Peek().text == "(" &&
               Peek(1).kind == TokenKind::kIdentifier &&
               Peek(2).kind == TokenKind::kSymbol && Peek(2).text == ")") {
      Strength parsed = Strength::kStrong;
      int drive_value = 0;
      if (ParseStrengthToken(Peek(1).text, &parsed, &drive_value)) {
        MatchSymbol("(");
        Advance();
        MatchSymbol(")");
        pull_strength = parsed;
        pull_drive_value = drive_value;
        has_strength = true;
      }
    }
    std::vector<std::string> targets;
    while (true) {
      if (MatchSymbol("(")) {
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
      } else {
        std::string instance_name;
        if (!ConsumeIdentifier(&instance_name)) {
          ErrorHere("expected pullup/pulldown instance or net name");
          return false;
        }
        if (!MatchSymbol("(")) {
          ErrorHere("expected '(' after pullup/pulldown instance");
          return false;
        }
        std::string name;
        if (!ConsumeIdentifier(&name)) {
          ErrorHere("expected net name in pullup/pulldown");
          return false;
        }
        if (!MatchSymbol(")")) {
          ErrorHere("expected ')' after pullup/pulldown");
          return false;
        }
        targets.push_back(name);
      }
      if (MatchSymbol(",")) {
        continue;
      }
      break;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after pullup/pulldown");
      return false;
    }
    uint64_t value = pull_up ? 1u : 0u;
    for (const auto& name : targets) {
      if (LookupSignalWidth(name) <= 0) {
        AddOrUpdateNet(module, name, NetType::kWire, 1, false, nullptr, nullptr,
                       {});
      }
      Assign assign;
      assign.lhs = name;
      assign.rhs = MakeNumberExpr(value);
      assign.has_strength = true;
      assign.is_implicit = true;
      if (has_pair_strength) {
        assign.strength0 = pull_strength0;
        assign.strength1 = pull_strength1;
      } else if (has_strength) {
        if (pull_drive_value == 0) {
          assign.strength0 = pull_strength;
          assign.strength1 = Strength::kHighZ;
        } else {
          assign.strength0 = Strength::kHighZ;
          assign.strength1 = pull_strength;
        }
      } else if (pull_up) {
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
    std::string pending;
    for (size_t i = 0; i < info->inputs.size(); ++i) {
      UdpPattern pattern;
      if (pending.empty() && Peek().kind == TokenKind::kSymbol &&
          Peek().text == "(") {
        if (!ParseUdpEdgePattern(&pattern)) {
          return false;
        }
        if (i < info->input_has_edge.size()) {
          info->input_has_edge[i] = true;
        }
      } else {
        char value = '?';
        if (!ParseUdpPatternChar(&value, &pending)) {
          return false;
        }
        pattern.value = value;
        if (value == 'r' || value == 'f' || value == 'b' ||
            value == 'p' || value == 'n') {
          pattern.is_edge = true;
          pattern.edge_kind = value;
          if (i < info->input_has_edge.size()) {
            info->input_has_edge[i] = true;
          }
        }
      }
      out->inputs.push_back(pattern);
    }
    if (!pending.empty()) {
      ErrorHere("invalid UDP pattern");
      return false;
    }
    if (!MatchSymbol(":")) {
      ErrorHere("expected ':' after UDP input patterns");
      return false;
    }
    char mid = '?';
    std::string mid_pending;
    if (!ParseUdpPatternChar(&mid, &mid_pending) || !mid_pending.empty()) {
      return false;
    }
    if (MatchSymbol(":")) {
      out->has_current = true;
      out->current = mid;
      std::string out_pending;
      if (!ParseUdpPatternChar(&out->output, &out_pending) ||
          !out_pending.empty()) {
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

  bool IsUdpPatternChar(char value) const {
    switch (value) {
      case '0':
      case '1':
      case 'x':
      case 'z':
      case '?':
      case '-':
      case '*':
      case 'r':
      case 'f':
      case 'b':
      case 'p':
      case 'n':
        return true;
      default:
        return false;
    }
  }

  bool ParseUdpPatternChar(char* out) { return ParseUdpPatternChar(out, nullptr); }

  bool ParseUdpPatternChar(char* out, std::string* pending) {
    if (!out) {
      return false;
    }
    if (pending && !pending->empty()) {
      *out = pending->front();
      pending->erase(pending->begin());
      return true;
    }
    std::string text;
    if (Peek().kind == TokenKind::kSymbol) {
      if (Peek().text == ":" || Peek().text == ";" || Peek().text == "(" ||
          Peek().text == ")" || Peek().text == ",") {
        ErrorHere("expected UDP pattern");
        return false;
      }
      text = Peek().text;
      Advance();
    } else if (Peek().kind == TokenKind::kNumber ||
               Peek().kind == TokenKind::kIdentifier) {
      text = Peek().text;
      Advance();
    } else {
      ErrorHere("expected UDP pattern");
      return false;
    }
    if (text.empty()) {
      ErrorHere("invalid UDP pattern");
      return false;
    }
    std::string chars;
    chars.reserve(text.size());
    for (char c : text) {
      char lowered = static_cast<char>(
          std::tolower(static_cast<unsigned char>(c)));
      if (!IsUdpPatternChar(lowered)) {
        ErrorHere("invalid UDP pattern");
        return false;
      }
      chars.push_back(lowered);
    }
    *out = chars[0];
    if (chars.size() > 1) {
      if (!pending) {
        ErrorHere("invalid UDP pattern");
        return false;
      }
      pending->append(chars.substr(1));
    }
    return true;
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
        (Peek().text == "?" || Peek().text == "-" || Peek().text == "*")) {
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
      if (value == '?' || value == '-' || value == '*') {
        return nullptr;
      }
      auto lhs = MakeIdentifierExpr(name);
      auto rhs = MakeUdpLiteral(value, 1);
      return MakeBinary('C', std::move(lhs), std::move(rhs));
    };
    if (!pattern.is_edge) {
      return build_simple(signal, pattern.value);
    }
    auto combine_and = [&](std::unique_ptr<Expr> lhs,
                           std::unique_ptr<Expr> rhs) -> std::unique_ptr<Expr> {
      if (!lhs) {
        return rhs;
      }
      if (!rhs) {
        return lhs;
      }
      return MakeBinary('A', std::move(lhs), std::move(rhs));
    };
    auto combine_or = [&](std::unique_ptr<Expr> lhs,
                          std::unique_ptr<Expr> rhs) -> std::unique_ptr<Expr> {
      if (!lhs) {
        return rhs;
      }
      if (!rhs) {
        return lhs;
      }
      return MakeBinary('O', std::move(lhs), std::move(rhs));
    };
    auto build_edge_pair =
        [&](char prev_val, char curr_val) -> std::unique_ptr<Expr> {
      auto prev_cond = build_simple(prev_signal, prev_val);
      auto curr_cond = build_simple(signal, curr_val);
      return combine_and(std::move(prev_cond), std::move(curr_cond));
    };
    if (pattern.edge_kind != 0) {
      std::unique_ptr<Expr> edge_expr;
      auto add_edge = [&](char prev_val, char curr_val) {
        edge_expr =
            combine_or(std::move(edge_expr), build_edge_pair(prev_val, curr_val));
      };
      if (pattern.edge_kind == 'r') {
        add_edge('0', '1');
      } else if (pattern.edge_kind == 'f') {
        add_edge('1', '0');
      } else if (pattern.edge_kind == 'p') {
        add_edge('0', '1');
        add_edge('0', 'x');
        add_edge('x', '1');
        add_edge('0', 'z');
        add_edge('z', '1');
      } else if (pattern.edge_kind == 'n') {
        add_edge('1', '0');
        add_edge('1', 'x');
        add_edge('x', '0');
        add_edge('1', 'z');
        add_edge('z', '0');
      } else if (pattern.edge_kind == 'b') {
        add_edge('0', '1');
        add_edge('1', '0');
        add_edge('0', 'x');
        add_edge('x', '0');
        add_edge('1', 'x');
        add_edge('x', '1');
        add_edge('0', 'z');
        add_edge('z', '0');
        add_edge('1', 'z');
        add_edge('z', '1');
      }
      return edge_expr;
    }
    std::unique_ptr<Expr> prev_cond = build_simple(prev_signal, pattern.prev);
    std::unique_ptr<Expr> curr_cond = build_simple(signal, pattern.curr);
    return combine_and(std::move(prev_cond), std::move(curr_cond));
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
      init.is_synthesized = true;
      if (sequential) {
        Statement init_out;
        init_out.kind = StatementKind::kAssign;
        init_out.assign.lhs = info.output;
        init_out.assign.rhs =
            MakeUdpLiteral(info.has_initial ? info.initial_value : 'x', 1);
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
    block.is_synthesized = true;
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

  std::unique_ptr<Expr> MakeRangeSelectExpr(std::unique_ptr<Expr> base,
                                            int msb, int lsb) {
    auto select = std::make_unique<Expr>();
    select->kind = ExprKind::kSelect;
    select->base = std::move(base);
    select->has_range = true;
    select->msb = msb;
    select->lsb = lsb;
    select->msb_expr = MakeNumberExpr(static_cast<uint64_t>(msb));
    select->lsb_expr = MakeNumberExpr(static_cast<uint64_t>(lsb));
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
    auto emit_instance = [&](const std::vector<std::unique_ptr<Expr>>& ports,
                             bool has_array, int array_msb,
                             int array_lsb) -> bool {
      if (ports.empty()) {
        ErrorHere("gate requires ports in v0");
        return false;
      }
      const bool buf_or_not = (gate == "buf" || gate == "not");
      if (buf_or_not) {
        if (ports.size() < 2) {
          ErrorHere("gate requires at least 2 ports in v0");
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

      std::vector<const Expr*> output_exprs;
      std::vector<const Expr*> input_exprs;
      if (buf_or_not) {
        output_exprs.reserve(ports.size() - 1);
        for (size_t i = 0; i + 1 < ports.size(); ++i) {
          output_exprs.push_back(ports[i].get());
        }
        input_exprs.push_back(ports.back().get());
      } else {
        output_exprs.push_back(ports[0].get());
        for (size_t i = 1; i < ports.size(); ++i) {
          input_exprs.push_back(ports[i].get());
        }
      }

      std::vector<GateOutputInfo> outputs_info;
      outputs_info.reserve(output_exprs.size());
      for (const auto* output_expr : output_exprs) {
        GateOutputInfo out_info;
        if (!ResolveGateOutput(*output_expr, &out_info, allow_nonconst_output)) {
          return false;
        }
        if (has_array && (out_info.has_range || !out_info.indices.empty())) {
          ErrorHere("gate array output must be identifier in v0");
          return false;
        }
        outputs_info.push_back(std::move(out_info));
      }

      auto ensure_input_net = [&](const Expr& expr) {
        const Expr* base = &expr;
        while (base->kind == ExprKind::kIndex || base->kind == ExprKind::kSelect) {
          if (!base->base) {
            return;
          }
          base = base->base.get();
        }
        if (base->kind != ExprKind::kIdentifier) {
          return;
        }
        if (LookupSignalWidth(base->ident) <= 0) {
          AddOrUpdateNet(current_module_, base->ident, NetType::kWire, 1, false,
                         nullptr, nullptr, {});
        }
      };
      for (const auto* input_expr : input_exprs) {
        if (input_expr) {
          ensure_input_net(*input_expr);
        }
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
        for (const auto& out_info : outputs_info) {
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
                             false, nullptr, nullptr, {});
              output_width = 1;
            }
          }

          std::vector<std::unique_ptr<Expr>> inputs;
          inputs.reserve(input_exprs.size());
          for (const auto* input_expr : input_exprs) {
            inputs.push_back(CloneOrIndexExpr(*input_expr, index_inputs, index));
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
            std::unique_ptr<Expr> data =
                MakeUnaryExpr('~', std::move(inputs[0]));
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
        }

        if (!has_array || index == array_lsb) {
          break;
        }
        index += step;
      }
      return has_any;
    };

    bool has_any = false;
    while (true) {
      std::string instance_name;
      bool has_array = false;
      int array_msb = 0;
      int array_lsb = 0;
      if (Peek().kind == TokenKind::kIdentifier) {
        instance_name = Peek().text;
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
      if (!instance_name.empty()) {
        GateArrayRange range;
        range.has_range = has_array;
        range.msb = array_msb;
        range.lsb = array_lsb;
        auto [it, inserted] = gate_array_ranges_.emplace(instance_name, range);
        if (!inserted) {
          if (it->second.has_range != range.has_range ||
              (range.has_range &&
               (it->second.msb != range.msb || it->second.lsb != range.lsb))) {
            ErrorHere("gate instance '" + instance_name +
                      "' declared with different range");
            return false;
          }
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
      if (!emit_instance(ports, has_array, array_msb, array_lsb)) {
        return false;
      }
      has_any = true;
      if (MatchSymbol(",")) {
        continue;
      }
      break;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after gate primitive");
      return false;
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

  enum class SpecifyDestKind {
    kNone,
    kBitSelect,
    kPartSelect,
  };

  enum class SpecifyDelaySelectMode {
    kFast,
    kSlow,
  };

  enum class NegativeSetupMode {
    kAllow,
    kClamp,
    kError,
  };

  struct SpecifyDelayConfig {
    SpecifyDelaySelectMode mode = SpecifyDelaySelectMode::kFast;
    bool invalid_env = false;
  };

  struct NegativeSetupConfig {
    NegativeSetupMode mode = NegativeSetupMode::kAllow;
    bool invalid_env = false;
  };

  struct SpecifyDestInfo {
    std::string name;
    SpecifyDestKind kind = SpecifyDestKind::kNone;
  };

  struct SpecifyState {
    std::unordered_map<std::string, SpecifyDestKind> conditional_dest_kind;
    std::unordered_set<std::string> path_outputs;
    std::unordered_set<std::string> showcancelled_outputs;
  };

  SpecifyDelayConfig GetSpecifyDelayConfig() {
    static bool cached = false;
    static SpecifyDelayConfig config;
    if (cached) {
      return config;
    }
    cached = true;
    if (const char* env = std::getenv("METALFPGA_SPECIFY_DELAY_SELECT")) {
      std::string lowered;
      lowered.reserve(std::strlen(env));
      for (const char* p = env; *p != '\0'; ++p) {
        lowered.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
      }
      if (lowered == "slow") {
        config.mode = SpecifyDelaySelectMode::kSlow;
      } else if (lowered != "fast" && !lowered.empty()) {
        config.invalid_env = true;
      }
    }
    return config;
  }

  NegativeSetupConfig GetNegativeSetupConfig() {
    static bool cached = false;
    static NegativeSetupConfig config;
    if (cached) {
      return config;
    }
    cached = true;
    if (const char* env = std::getenv("METALFPGA_NEGATIVE_SETUP_MODE")) {
      std::string lowered;
      lowered.reserve(std::strlen(env));
      for (const char* p = env; *p != '\0'; ++p) {
        lowered.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
      }
      if (lowered == "clamp") {
        config.mode = NegativeSetupMode::kClamp;
      } else if (lowered == "error") {
        config.mode = NegativeSetupMode::kError;
      } else if (lowered != "allow" && !lowered.empty()) {
        config.invalid_env = true;
      }
    }
    return config;
  }

  void UpdateDepthForToken(const Token& token, int* paren_depth,
                           int* bracket_depth, int* brace_depth) {
    if (token.kind != TokenKind::kSymbol) {
      return;
    }
    if (token.text == "(") {
      ++(*paren_depth);
    } else if (token.text == ")" && *paren_depth > 0) {
      --(*paren_depth);
    } else if (token.text == "[") {
      ++(*bracket_depth);
    } else if (token.text == "]" && *bracket_depth > 0) {
      --(*bracket_depth);
    } else if (token.text == "{") {
      ++(*brace_depth);
    } else if (token.text == "}" && *brace_depth > 0) {
      --(*brace_depth);
    }
  }

  bool CollectSpecifyStatementTokens(std::vector<Token>* out_tokens) {
    out_tokens->clear();
    const Token& start_token = Peek();
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    while (!IsAtEnd()) {
      if (Peek().kind == TokenKind::kSymbol && Peek().text == ";" &&
          paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
        Advance();
        return true;
      }
      Token token = Peek();
      Advance();
      UpdateDepthForToken(token, &paren_depth, &bracket_depth, &brace_depth);
      out_tokens->push_back(std::move(token));
    }
    diagnostics_->Add(Severity::kError,
                      "missing ';' in specify block",
                      SourceLocation{path_, start_token.line,
                                     start_token.column});
    return false;
  }

  std::string ConsumeHierNameFromTokens(const std::vector<Token>& tokens,
                                        size_t* index) {
    if (!index || *index >= tokens.size()) {
      return {};
    }
    std::string name = tokens[*index].text;
    size_t i = *index + 1;
    while (i + 1 < tokens.size() &&
           tokens[i].kind == TokenKind::kSymbol && tokens[i].text == "." &&
           tokens[i + 1].kind == TokenKind::kIdentifier) {
      name.push_back('.');
      name.append(tokens[i + 1].text);
      i += 2;
    }
    *index = i;
    return name;
  }

  size_t SkipBracketTokens(const std::vector<Token>& tokens, size_t index) {
    if (index >= tokens.size() ||
        tokens[index].kind != TokenKind::kSymbol ||
        tokens[index].text != "[") {
      return index;
    }
    int depth = 0;
    size_t i = index;
    while (i < tokens.size()) {
      const Token& token = tokens[i];
      if (token.kind == TokenKind::kSymbol && token.text == "[") {
        ++depth;
      } else if (token.kind == TokenKind::kSymbol && token.text == "]") {
        --depth;
        if (depth == 0) {
          return i + 1;
        }
      }
      ++i;
    }
    return tokens.size();
  }

  SpecifyDestKind ParseSelectKind(const std::vector<Token>& tokens,
                                  size_t* index) {
    if (!index || *index >= tokens.size() ||
        tokens[*index].kind != TokenKind::kSymbol ||
        tokens[*index].text != "[") {
      return SpecifyDestKind::kNone;
    }
    bool is_part = false;
    int depth = 0;
    size_t i = *index;
    while (i < tokens.size()) {
      const Token& token = tokens[i];
      if (token.kind == TokenKind::kSymbol && token.text == "[") {
        ++depth;
      } else if (token.kind == TokenKind::kSymbol && token.text == "]") {
        --depth;
        if (depth == 0) {
          *index = i + 1;
          return is_part ? SpecifyDestKind::kPartSelect
                         : SpecifyDestKind::kBitSelect;
        }
      } else if (depth == 1 && token.kind == TokenKind::kSymbol) {
        if (token.text == ":" || token.text == "+:" || token.text == "-:") {
          is_part = true;
        }
      }
      ++i;
    }
    *index = tokens.size();
    return SpecifyDestKind::kBitSelect;
  }

  std::vector<SpecifyDestInfo> ParseSpecifyDestinations(
      const std::vector<Token>& tokens) {
    std::vector<SpecifyDestInfo> outputs;
    size_t i = 0;
    while (i < tokens.size()) {
      const Token& token = tokens[i];
      if (token.kind == TokenKind::kIdentifier) {
        SpecifyDestInfo info;
        info.name = ConsumeHierNameFromTokens(tokens, &i);
        if (i < tokens.size() && tokens[i].kind == TokenKind::kSymbol &&
            tokens[i].text == "[") {
          info.kind = ParseSelectKind(tokens, &i);
        }
        if (!info.name.empty()) {
          outputs.push_back(std::move(info));
        }
        continue;
      }
      if (token.kind == TokenKind::kSymbol && token.text == "[") {
        i = SkipBracketTokens(tokens, i);
        continue;
      }
      ++i;
    }
    return outputs;
  }

  std::vector<std::string> ParseSpecifyOutputList(
      const std::vector<Token>& tokens, size_t start_index) {
    std::vector<std::string> outputs;
    size_t i = start_index;
    while (i < tokens.size()) {
      const Token& token = tokens[i];
      if (token.kind == TokenKind::kIdentifier) {
        std::string name = ConsumeHierNameFromTokens(tokens, &i);
        if (i < tokens.size() && tokens[i].kind == TokenKind::kSymbol &&
            tokens[i].text == "[") {
          i = SkipBracketTokens(tokens, i);
        }
        if (!name.empty()) {
          outputs.push_back(std::move(name));
        }
        continue;
      }
      if (token.kind == TokenKind::kSymbol && token.text == "[") {
        i = SkipBracketTokens(tokens, i);
        continue;
      }
      ++i;
    }
    return outputs;
  }

  size_t FindPathArrow(const std::vector<Token>& tokens) {
    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
      if (tokens[i].kind != TokenKind::kSymbol ||
          tokens[i + 1].kind != TokenKind::kSymbol) {
        continue;
      }
      if (tokens[i].text == "=" && tokens[i + 1].text == ">") {
        return i;
      }
      if (tokens[i].text == "*" && tokens[i + 1].text == ">") {
        return i;
      }
    }
    return tokens.size();
  }

  size_t FindDelayAssignIndex(const std::vector<Token>& tokens,
                              size_t start_index) {
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    for (size_t i = 0; i < start_index && i < tokens.size(); ++i) {
      UpdateDepthForToken(tokens[i], &paren_depth, &bracket_depth, &brace_depth);
    }
    for (size_t i = start_index; i < tokens.size(); ++i) {
      const Token& token = tokens[i];
      if (token.kind == TokenKind::kSymbol && token.text == "=" &&
          paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
        return i;
      }
      UpdateDepthForToken(token, &paren_depth, &bracket_depth, &brace_depth);
    }
    return tokens.size();
  }

  bool SplitSpecifyCallArgs(const std::vector<Token>& tokens,
                            size_t lparen_index,
                            std::vector<std::vector<Token>>* out_args,
                            bool* out_has_empty) {
    if (!out_args || !out_has_empty) {
      return false;
    }
    out_args->clear();
    *out_has_empty = false;
    if (lparen_index >= tokens.size() ||
        tokens[lparen_index].kind != TokenKind::kSymbol ||
        tokens[lparen_index].text != "(") {
      return false;
    }
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    std::vector<Token> current;
    for (size_t i = lparen_index + 1; i < tokens.size(); ++i) {
      const Token& token = tokens[i];
      bool at_top = (paren_depth == 0 && bracket_depth == 0 &&
                     brace_depth == 0);
      if (token.kind == TokenKind::kSymbol && token.text == ")" && at_top) {
        if (current.empty()) {
          *out_has_empty = true;
        }
        out_args->push_back(std::move(current));
        return true;
      }
      if (token.kind == TokenKind::kSymbol && token.text == "," && at_top) {
        if (current.empty()) {
          *out_has_empty = true;
        }
        out_args->push_back(std::move(current));
        current.clear();
        continue;
      }
      UpdateDepthForToken(token, &paren_depth, &bracket_depth, &brace_depth);
      current.push_back(token);
    }
    return false;
  }

  bool IsNegativeLiteral(const std::vector<Token>& tokens) {
    if (tokens.size() == 2 &&
        tokens[0].kind == TokenKind::kSymbol && tokens[0].text == "-" &&
        tokens[1].kind == TokenKind::kNumber) {
      return true;
    }
    return false;
  }

  std::string ToLowerAscii(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
      out.push_back(static_cast<char>(std::tolower(
          static_cast<unsigned char>(c))));
    }
    return out;
  }

  std::vector<Token> StripOuterParensTokens(
      const std::vector<Token>& tokens) {
    size_t start = 0;
    size_t end = tokens.size();
    while (start + 1 < end &&
           tokens[start].kind == TokenKind::kSymbol &&
           tokens[start].text == "(" &&
           tokens[end - 1].kind == TokenKind::kSymbol &&
           tokens[end - 1].text == ")") {
      int depth = 0;
      bool ok = true;
      for (size_t i = start; i < end; ++i) {
        const Token& token = tokens[i];
        if (token.kind == TokenKind::kSymbol && token.text == "(") {
          ++depth;
        } else if (token.kind == TokenKind::kSymbol && token.text == ")") {
          --depth;
        }
        if (depth == 0 && i + 1 < end) {
          ok = false;
          break;
        }
      }
      if (!ok || depth != 0) {
        break;
      }
      ++start;
      --end;
    }
    if (start == 0 && end == tokens.size()) {
      return tokens;
    }
    return std::vector<Token>(tokens.begin() + start, tokens.begin() + end);
  }

  void SplitTimingCondition(const std::vector<Token>& tokens,
                            std::vector<Token>* out_event,
                            std::vector<Token>* out_cond) {
    out_event->clear();
    out_cond->clear();
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
      if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 &&
          i + 2 < tokens.size() &&
          tokens[i].kind == TokenKind::kSymbol && tokens[i].text == "&" &&
          tokens[i + 1].kind == TokenKind::kSymbol &&
          tokens[i + 1].text == "&" &&
          tokens[i + 2].kind == TokenKind::kSymbol &&
          tokens[i + 2].text == "&") {
        out_event->assign(tokens.begin(), tokens.begin() + i);
        out_cond->assign(tokens.begin() + i + 3, tokens.end());
        return;
      }
      UpdateDepthForToken(tokens[i], &paren_depth, &bracket_depth,
                          &brace_depth);
    }
    *out_event = tokens;
  }

  std::string NormalizeTokenExpr(const std::vector<Token>& tokens) {
    std::string out;
    for (const auto& token : tokens) {
      out += token.text;
    }
    return ToLowerAscii(out);
  }

  bool HandleSpecifyTimingCheck(Module* module,
                                const std::vector<Token>& tokens,
                                const Token& start_token,
                                NegativeSetupMode negative_setup_mode) {
    if (tokens.size() < 2 || tokens[1].kind != TokenKind::kIdentifier) {
      diagnostics_->Add(Severity::kError,
                        "expected system task name after '$'",
                        SourceLocation{path_, start_token.line,
                                       start_token.column});
      return false;
    }
    const std::string& name = tokens[1].text;
    size_t lparen_index = 0;
    for (size_t i = 2; i < tokens.size(); ++i) {
      if (tokens[i].kind == TokenKind::kSymbol && tokens[i].text == "(") {
        lparen_index = i;
        break;
      }
    }
    if (lparen_index == 0 || lparen_index >= tokens.size()) {
      return true;
    }
    std::vector<std::vector<Token>> args;
    bool has_empty = false;
    if (!SplitSpecifyCallArgs(tokens, lparen_index, &args, &has_empty)) {
      diagnostics_->Add(Severity::kError,
                        "expected ')' after system task",
                        SourceLocation{path_, start_token.line,
                                       start_token.column});
      return false;
    }
    if (name == "width") {
      if (has_empty || (args.size() != 2 && args.size() != 4)) {
        diagnostics_->Add(Severity::kError,
                          "$width arguments must be 2 or 4 with no gaps",
                          SourceLocation{path_, start_token.line,
                                         start_token.column});
        return false;
      }
    }
    if (name == "setuphold" && negative_setup_mode != NegativeSetupMode::kAllow) {
      if (args.size() >= 4 &&
          (IsNegativeLiteral(args[2]) || IsNegativeLiteral(args[3]))) {
        if (negative_setup_mode == NegativeSetupMode::kError) {
          diagnostics_->Add(Severity::kError,
                            "negative setup/hold not allowed",
                            SourceLocation{path_, start_token.line,
                                           start_token.column});
          return false;
        }
        diagnostics_->Add(Severity::kWarning,
                          "negative setup/hold will be clamped",
                          SourceLocation{path_, start_token.line,
                                         start_token.column});
      }
    }
    if (module && !args.empty()) {
      std::vector<Token> event_tokens;
      std::vector<Token> cond_tokens;
      SplitTimingCondition(args[0], &event_tokens, &cond_tokens);
      event_tokens = StripOuterParensTokens(event_tokens);
      cond_tokens = StripOuterParensTokens(cond_tokens);
      std::string edge;
      size_t signal_start = 0;
      if (!event_tokens.empty()) {
        std::string first = ToLowerAscii(event_tokens.front().text);
        if (first == "posedge" || first == "negedge") {
          edge = first;
          signal_start = 1;
        }
      }
      std::vector<Token> signal_tokens;
      if (signal_start < event_tokens.size()) {
        signal_tokens.assign(event_tokens.begin() + signal_start,
                             event_tokens.end());
      }
      signal_tokens = StripOuterParensTokens(signal_tokens);
      TimingCheck timing;
      timing.name = ToLowerAscii(name);
      timing.edge = edge;
      timing.signal = NormalizeTokenExpr(signal_tokens);
      timing.condition = NormalizeTokenExpr(cond_tokens);
      timing.line = start_token.line;
      timing.column = start_token.column;
      module->timing_checks.push_back(std::move(timing));
    }
    return true;
  }

  bool HandleSpecifyPath(const std::vector<Token>& tokens,
                         SpecifyState* state, const Token& start_token) {
    if (!state) {
      return false;
    }
    size_t arrow_index = FindPathArrow(tokens);
    if (arrow_index >= tokens.size()) {
      return true;
    }
    bool is_conditional = false;
    if (!tokens.empty() && tokens[0].kind == TokenKind::kIdentifier) {
      if (tokens[0].text == "if" || tokens[0].text == "ifnone") {
        is_conditional = true;
      }
    }
    size_t dest_start = arrow_index + 2;
    size_t dest_end = FindDelayAssignIndex(tokens, dest_start);
    if (dest_end <= dest_start || dest_end > tokens.size()) {
      return true;
    }
    std::vector<Token> dest_tokens(tokens.begin() + dest_start,
                                   tokens.begin() + dest_end);
    std::vector<SpecifyDestInfo> dests =
        ParseSpecifyDestinations(dest_tokens);
    for (const auto& dest : dests) {
      if (dest.name.empty()) {
        continue;
      }
      state->path_outputs.insert(dest.name);
      if (!is_conditional) {
        continue;
      }
      auto it = state->conditional_dest_kind.find(dest.name);
      if (it == state->conditional_dest_kind.end()) {
        state->conditional_dest_kind[dest.name] = dest.kind;
        continue;
      }
      if (it->second != dest.kind) {
        diagnostics_->Add(Severity::kError,
                          "conditional specify paths must use consistent "
                          "destination selection",
                          SourceLocation{path_, start_token.line,
                                         start_token.column});
        return false;
      }
    }
    return true;
  }

  bool HandleSpecifyShowCancelled(const std::vector<Token>& tokens,
                                  SpecifyState* state, bool show,
                                  const Token& start_token) {
    if (!state) {
      return false;
    }
    std::vector<std::string> outputs = ParseSpecifyOutputList(tokens, 1);
    for (const auto& output : outputs) {
      if (show && state->path_outputs.count(output) != 0u) {
        diagnostics_->Add(Severity::kError,
                          "showcancelled must appear before module path "
                          "declarations",
                          SourceLocation{path_, start_token.line,
                                         start_token.column});
        return false;
      }
      state->showcancelled_outputs.insert(output);
    }
    return true;
  }

  bool ParseSpecifyBlock(Module* module) {
    const Token& start = Previous();
    const SpecifyDelayConfig delay_config = GetSpecifyDelayConfig();
    if (delay_config.invalid_env) {
      diagnostics_->Add(Severity::kWarning,
                        "invalid METALFPGA_SPECIFY_DELAY_SELECT; "
                        "expected fast or slow",
                        SourceLocation{path_, start.line, start.column});
    }
    const NegativeSetupConfig negative_config = GetNegativeSetupConfig();
    if (negative_config.invalid_env) {
      diagnostics_->Add(Severity::kWarning,
                        "invalid METALFPGA_NEGATIVE_SETUP_MODE; "
                        "expected allow, clamp, or error",
                        SourceLocation{path_, start.line, start.column});
    }
    SpecifyState state;
    while (!IsAtEnd()) {
      if (MatchKeyword("endspecify")) {
        return true;
      }
      if (MatchKeyword("specparam")) {
        if (!ParseSpecparamDecl(module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("specify")) {
        if (!ParseSpecifyBlock(module)) {
          return false;
        }
        continue;
      }
      std::vector<Token> stmt_tokens;
      if (!CollectSpecifyStatementTokens(&stmt_tokens)) {
        return false;
      }
      if (stmt_tokens.empty()) {
        continue;
      }
      const Token& stmt_start = stmt_tokens.front();
      if (stmt_start.kind == TokenKind::kIdentifier &&
          (stmt_start.text == "showcancelled" ||
           stmt_start.text == "noshowcancelled")) {
        bool show = stmt_start.text == "showcancelled";
        if (!HandleSpecifyShowCancelled(stmt_tokens, &state, show,
                                        stmt_start)) {
          return false;
        }
        continue;
      }
      if (stmt_start.kind == TokenKind::kSymbol && stmt_start.text == "$") {
        if (!HandleSpecifyTimingCheck(module, stmt_tokens, stmt_start,
                                      negative_config.mode)) {
          return false;
        }
        continue;
      }
      if (!HandleSpecifyPath(stmt_tokens, &state, stmt_start)) {
        return false;
      }
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
      std::string instance_name;
      std::string param_name;
      if (dot == std::string::npos) {
        instance_name = "";
        param_name = path;
      } else {
        instance_name = path.substr(0, dot);
        param_name = path.substr(dot + 1);
      }
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

  bool ParseFunction(Module* module, bool automatic_prefix) {
    Function func;
    if (!automatic_prefix && MatchKeyword("automatic")) {
      // automatic functions are treated like static in v0.
    }
    bool is_signed = false;
    bool is_real = false;
    int width = 1;
    bool allow_range = true;
    if (MatchKeyword("integer")) {
      width = 32;
      is_signed = true;
      allow_range = false;
    } else if (MatchKeyword("time")) {
      width = 64;
      is_signed = false;
      allow_range = false;
    } else if (MatchKeyword("realtime")) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    } else if (MatchKeyword("real")) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    }
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    if (!is_real && (MatchKeyword("real") || MatchKeyword("realtime"))) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    }
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    bool had_range = false;
    if (allow_range) {
      if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
        return false;
      }
    } else if (Peek().kind == TokenKind::kSymbol && Peek().text == "[") {
      ErrorHere("function return type cannot use packed ranges");
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
    } else if (allow_range && !had_range) {
      msb_expr.reset();
      lsb_expr.reset();
    }
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      ErrorHere("expected function name after 'function'");
      return false;
    }
    if (MatchSymbol("(")) {
      if (!MatchSymbol(")")) {
        if (!ParseFunctionAnsiArgList(&func)) {
          return false;
        }
        if (!MatchSymbol(")")) {
          ErrorHere("expected ')' after function arguments");
          return false;
        }
      }
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
        if (MatchKeyword("real") || MatchKeyword("realtime")) {
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
    int width = 1;
    bool allow_range = true;
    if (MatchKeyword("integer")) {
      width = 32;
      is_signed = true;
      allow_range = false;
    } else if (MatchKeyword("time")) {
      width = 64;
      is_signed = false;
      allow_range = false;
    } else if (MatchKeyword("realtime")) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    } else if (MatchKeyword("real")) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    }
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    if (!is_real && (MatchKeyword("real") || MatchKeyword("realtime"))) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    }
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    bool had_range = false;
    if (allow_range) {
      if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
        return false;
      }
    } else if (Peek().kind == TokenKind::kSymbol && Peek().text == "[") {
      ErrorHere("function inputs of this type cannot use packed ranges");
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
    int width = 1;
    bool allow_range = true;
    if (MatchKeyword("reg")) {
      // Tasks allow "output reg" syntax; treat as output.
    }
    if (MatchKeyword("integer")) {
      width = 32;
      is_signed = true;
      allow_range = false;
    } else if (MatchKeyword("time")) {
      width = 64;
      is_signed = false;
      allow_range = false;
    } else if (MatchKeyword("realtime")) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    } else if (MatchKeyword("real")) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    }
    if (MatchKeyword("signed")) {
      is_signed = true;
    } else if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    if (!is_real && (MatchKeyword("real") || MatchKeyword("realtime"))) {
      width = 64;
      is_signed = true;
      is_real = true;
      allow_range = false;
    }
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    bool had_range = false;
    if (allow_range) {
      if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
        return false;
      }
    } else if (Peek().kind == TokenKind::kSymbol && Peek().text == "[") {
      ErrorHere("task arguments of this type cannot use packed ranges");
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

  bool ParseTaskAnsiArgList(Task* task) {
    TaskArgDir current_dir = TaskArgDir::kInput;
    int current_width = 1;
    bool current_is_signed = false;
    bool current_is_real = false;
    bool current_allow_range = true;
    std::shared_ptr<Expr> current_msb;
    std::shared_ptr<Expr> current_lsb;
    while (true) {
      TaskArgDir dir = current_dir;
      int width = current_width;
      bool is_signed = current_is_signed;
      bool is_real = current_is_real;
      bool allow_range = current_allow_range;
      std::shared_ptr<Expr> msb_expr = current_msb;
      std::shared_ptr<Expr> lsb_expr = current_lsb;
      bool saw_spec = false;
      if (MatchKeyword("input")) {
        dir = TaskArgDir::kInput;
        width = 1;
        is_signed = false;
        is_real = false;
        allow_range = true;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      } else if (MatchKeyword("output")) {
        dir = TaskArgDir::kOutput;
        width = 1;
        is_signed = false;
        is_real = false;
        allow_range = true;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      } else if (MatchKeyword("inout")) {
        dir = TaskArgDir::kInout;
        width = 1;
        is_signed = false;
        is_real = false;
        allow_range = true;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      }
      if (MatchKeyword("reg")) {
        saw_spec = true;
      }
      if (MatchKeyword("integer")) {
        width = 32;
        is_signed = true;
        is_real = false;
        allow_range = false;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      } else if (MatchKeyword("time")) {
        width = 64;
        is_signed = false;
        is_real = false;
        allow_range = false;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      } else if (MatchKeyword("realtime")) {
        width = 64;
        is_signed = true;
        is_real = true;
        allow_range = false;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      } else if (MatchKeyword("real")) {
        width = 64;
        is_signed = true;
        is_real = true;
        allow_range = false;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      }
      if (MatchKeyword("signed")) {
        is_signed = true;
        saw_spec = true;
      } else if (MatchKeyword("unsigned")) {
        is_signed = false;
        saw_spec = true;
      }
      bool had_range = false;
      if (Peek().kind == TokenKind::kSymbol && Peek().text == "[") {
        if (!allow_range) {
          ErrorHere("task arguments of this type cannot use packed ranges");
          return false;
        }
        if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
          return false;
        }
        saw_spec = true;
      } else if (saw_spec) {
        msb_expr.reset();
        lsb_expr.reset();
      }
      if (is_real && had_range) {
        ErrorHere("real task args cannot use packed ranges");
        return false;
      }
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
      if (saw_spec) {
        current_dir = dir;
        current_width = width;
        current_is_signed = is_signed;
        current_is_real = is_real;
        current_allow_range = allow_range;
        current_msb = msb_expr;
        current_lsb = lsb_expr;
      }
      if (MatchSymbol(",")) {
        continue;
      }
      break;
    }
    return true;
  }

  bool ParseFunctionAnsiArgList(Function* func) {
    int current_width = 1;
    bool current_is_signed = false;
    bool current_is_real = false;
    bool current_allow_range = true;
    std::shared_ptr<Expr> current_msb;
    std::shared_ptr<Expr> current_lsb;
    while (true) {
      int width = current_width;
      bool is_signed = current_is_signed;
      bool is_real = current_is_real;
      bool allow_range = current_allow_range;
      std::shared_ptr<Expr> msb_expr = current_msb;
      std::shared_ptr<Expr> lsb_expr = current_lsb;
      bool saw_spec = false;
      if (MatchKeyword("input") || MatchKeyword("output") ||
          MatchKeyword("inout")) {
        width = 1;
        is_signed = false;
        is_real = false;
        allow_range = true;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      }
      if (MatchKeyword("reg")) {
        saw_spec = true;
      }
      if (MatchKeyword("integer")) {
        width = 32;
        is_signed = true;
        is_real = false;
        allow_range = false;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      } else if (MatchKeyword("time")) {
        width = 64;
        is_signed = false;
        is_real = false;
        allow_range = false;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      } else if (MatchKeyword("realtime")) {
        width = 64;
        is_signed = true;
        is_real = true;
        allow_range = false;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      } else if (MatchKeyword("real")) {
        width = 64;
        is_signed = true;
        is_real = true;
        allow_range = false;
        msb_expr.reset();
        lsb_expr.reset();
        saw_spec = true;
      }
      if (MatchKeyword("signed")) {
        is_signed = true;
        saw_spec = true;
      } else if (MatchKeyword("unsigned")) {
        is_signed = false;
        saw_spec = true;
      }
      bool had_range = false;
      if (Peek().kind == TokenKind::kSymbol && Peek().text == "[") {
        if (!allow_range) {
          ErrorHere("function arguments of this type cannot use packed ranges");
          return false;
        }
        if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
          return false;
        }
        saw_spec = true;
      } else if (saw_spec) {
        msb_expr.reset();
        lsb_expr.reset();
      }
      if (is_real && had_range) {
        ErrorHere("real function args cannot use packed ranges");
        return false;
      }
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected function argument name");
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
      if (saw_spec) {
        current_width = width;
        current_is_signed = is_signed;
        current_is_real = is_real;
        current_allow_range = allow_range;
        current_msb = msb_expr;
        current_lsb = lsb_expr;
      }
      if (MatchSymbol(",")) {
        continue;
      }
      break;
    }
    return true;
  }

  bool ParseTaskInto(Task* task, bool automatic_prefix) {
    if (!task) {
      return false;
    }
    task->name.clear();
    task->args.clear();
    task->body.clear();
    if (!automatic_prefix && MatchKeyword("automatic")) {
      // automatic tasks are treated like static in v0.
    }
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      ErrorHere("expected task name after 'task'");
      return false;
    }
    if (MatchSymbol("(")) {
      if (!MatchSymbol(")")) {
        if (!ParseTaskAnsiArgList(task)) {
          return false;
        }
        if (!MatchSymbol(")")) {
          ErrorHere("expected ')' after task arguments");
          return false;
        }
      }
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after task header");
      return false;
    }
    task->name = name;

    bool saw_endtask = false;
    while (!IsAtEnd()) {
      if (MatchKeyword("endtask")) {
        saw_endtask = true;
        break;
      }
      if (MatchKeyword("input")) {
        if (!ParseTaskArgDecl(TaskArgDir::kInput, task)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("output")) {
        if (!ParseTaskArgDecl(TaskArgDir::kOutput, task)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("inout")) {
        if (!ParseTaskArgDecl(TaskArgDir::kInout, task)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("integer")) {
        if (!ParseLocalIntegerDecl(&task->body)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("real") || MatchKeyword("realtime")) {
        if (!ParseLocalRealDecl(&task->body)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("time")) {
        if (!ParseLocalTimeDecl(&task->body)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("reg")) {
        if (!ParseLocalRegDecl(current_module_, &task->body)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("begin")) {
        Statement block;
        if (!ParseBlockStatement(&block)) {
          return false;
        }
        task->body.push_back(std::move(block));
        continue;
      }
      Statement stmt;
      if (!ParseStatement(&stmt)) {
        return false;
      }
      task->body.push_back(std::move(stmt));
    }
    if (!saw_endtask) {
      ErrorHere("expected 'endtask'");
      return false;
    }
    return true;
  }

  bool ParseTask(Module* module, bool automatic_prefix) {
    Task task;
    if (!ParseTaskInto(&task, automatic_prefix)) {
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
    bool current_is_integer = false;
    bool current_is_time = false;
    NetType current_net_type = NetType::kWire;
    bool current_has_net_type = false;
    bool current_explicit = false;
    std::shared_ptr<Expr> current_msb;
    std::shared_ptr<Expr> current_lsb;
    while (true) {
      PortDir dir = current_dir;
      int width = current_width;
      bool is_reg = current_is_reg;
      bool is_signed = current_is_signed;
      bool is_real = current_is_real;
      bool is_integer = current_is_integer;
      bool is_time = current_is_time;
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
          is_integer = false;
          is_time = false;
          net_type = NetType::kWire;
          has_net_type = false;
          current_explicit = true;
        if (MatchKeyword("integer")) {
          is_integer = true;
          is_signed = true;
        } else if (MatchKeyword("time")) {
          is_time = true;
          is_signed = false;
        } else if (MatchKeyword("real") || MatchKeyword("realtime")) {
          is_real = true;
          is_signed = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchKeyword("unsigned")) {
          is_signed = false;
        }
        if (MatchNetType(&net_type)) {
          has_net_type = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (!is_real && !is_integer && !is_time &&
            (MatchKeyword("real") || MatchKeyword("realtime"))) {
          is_real = true;
          is_signed = true;
        }
        if ((is_real || is_integer || is_time) && has_net_type) {
          ErrorHere("real/integer/time declarations cannot use net types");
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
        } else if (is_integer || is_time) {
          if (had_range) {
            ErrorHere("integer/time declarations cannot use packed ranges");
            return false;
          }
          width = is_integer ? 32 : 64;
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
        current_is_integer = is_integer;
        current_is_time = is_time;
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
        is_integer = false;
        is_time = false;
        net_type = NetType::kWire;
        has_net_type = false;
        current_explicit = true;
        if (MatchKeyword("integer")) {
          is_integer = true;
          is_signed = true;
          is_reg = true;
        } else if (MatchKeyword("time")) {
          is_time = true;
          is_signed = false;
          is_reg = true;
        } else if (MatchKeyword("real") || MatchKeyword("realtime")) {
          is_real = true;
          is_signed = true;
          is_reg = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchKeyword("unsigned")) {
          is_signed = false;
        }
        if (MatchKeyword("reg")) {
          is_reg = true;
        } else if (MatchNetType(&net_type)) {
          has_net_type = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (!is_real && !is_integer && !is_time &&
            (MatchKeyword("real") || MatchKeyword("realtime"))) {
          is_real = true;
          is_signed = true;
          is_reg = true;
        }
        if ((is_real || is_integer || is_time) && has_net_type) {
          ErrorHere("real/integer/time declarations cannot use net types");
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
        } else if (is_integer || is_time) {
          if (had_range) {
            ErrorHere("integer/time declarations cannot use packed ranges");
            return false;
          }
          width = is_integer ? 32 : 64;
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
        current_is_integer = is_integer;
        current_is_time = is_time;
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
        is_integer = false;
        is_time = false;
        net_type = NetType::kWire;
        has_net_type = false;
        current_explicit = true;
        if (MatchKeyword("integer")) {
          is_integer = true;
          is_signed = true;
        } else if (MatchKeyword("time")) {
          is_time = true;
          is_signed = false;
        } else if (MatchKeyword("real") || MatchKeyword("realtime")) {
          is_real = true;
          is_signed = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchKeyword("unsigned")) {
          is_signed = false;
        }
        if (MatchNetType(&net_type)) {
          has_net_type = true;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (!is_real && !is_integer && !is_time &&
            (MatchKeyword("real") || MatchKeyword("realtime"))) {
          is_real = true;
          is_signed = true;
        }
        if ((is_real || is_integer || is_time) && has_net_type) {
          ErrorHere("real/integer/time declarations cannot use net types");
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
        } else if (is_integer || is_time) {
          if (had_range) {
            ErrorHere("integer/time declarations cannot use packed ranges");
            return false;
          }
          width = is_integer ? 32 : 64;
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
        current_is_integer = is_integer;
        current_is_time = is_time;
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
      if (!AddOrUpdatePort(module, name, dir, width, is_signed, is_real,
                           current_explicit, range_msb, range_lsb)) {
        return false;
      }
      if (dir == PortDir::kOutput && (is_real || is_integer || is_time)) {
        is_reg = true;
      }
      if (is_real || is_integer || is_time) {
        NetType var_type =
            (dir == PortDir::kOutput) ? NetType::kReg : NetType::kWire;
        AddOrUpdateNet(module, name, var_type, width, is_signed, range_msb,
                       range_lsb, {}, is_real);
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
    bool is_integer = false;
    bool is_time = false;
    NetType net_type = NetType::kWire;
    bool has_net_type = false;
    std::vector<Statement> init_statements;
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    if (dir == PortDir::kOutput) {
      if (MatchKeyword("integer")) {
        is_integer = true;
        is_signed = true;
        is_reg = true;
      } else if (MatchKeyword("time")) {
        is_time = true;
        is_signed = false;
        is_reg = true;
      } else if (MatchKeyword("reg")) {
        is_reg = true;
      } else if (MatchKeyword("real") || MatchKeyword("realtime")) {
        is_real = true;
        is_reg = true;
      } else if (MatchNetType(&net_type)) {
        has_net_type = true;
      }
    } else {
      if (MatchKeyword("integer")) {
        is_integer = true;
        is_signed = true;
      } else if (MatchKeyword("time")) {
        is_time = true;
        is_signed = false;
      } else if (MatchKeyword("real") || MatchKeyword("realtime")) {
        is_real = true;
      } else if (MatchNetType(&net_type)) {
        has_net_type = true;
      }
    }
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    if (MatchKeyword("unsigned")) {
      is_signed = false;
    }
    if (!is_real && !is_integer && !is_time &&
        (MatchKeyword("real") || MatchKeyword("realtime"))) {
      is_real = true;
    }
    if (is_real) {
      is_signed = true;
      if (has_net_type) {
        ErrorHere("real declarations cannot use net types");
        return false;
      }
    }
    if ((is_integer || is_time) && has_net_type) {
      ErrorHere("integer/time declarations cannot use net types");
      return false;
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
    } else if (is_integer || is_time) {
      if (had_range) {
        ErrorHere("integer/time declarations cannot use packed ranges");
        return false;
      }
      width = is_integer ? 32 : 64;
      range_msb.reset();
      range_lsb.reset();
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
      std::unique_ptr<Expr> init;
      if (MatchSymbol("=")) {
        if (!array_dims.empty()) {
          ErrorHere("net array initializer not supported");
          return false;
        }
        init = ParseExpr();
        if (!init) {
          return false;
        }
      }
      if (!AddOrUpdatePort(module, name, dir, width, is_signed, is_real, true,
                           range_msb, range_lsb)) {
        return false;
      }
      if (dir == PortDir::kOutput && (is_real || is_integer || is_time)) {
        is_reg = true;
      }
      if (is_real || is_integer || is_time) {
        NetType var_type =
            (dir == PortDir::kOutput) ? NetType::kReg : NetType::kWire;
        AddOrUpdateNet(module, name, var_type, width, is_signed, range_msb,
                       range_lsb, array_dims, is_real);
      } else {
        if ((dir == PortDir::kOutput || dir == PortDir::kInout) && !is_reg &&
            net_type != NetType::kWire) {
          AddOrUpdateNet(module, name, net_type, width, is_signed, range_msb,
                         range_lsb, array_dims);
          AddImplicitNetDriver(module, name, net_type);
        }
        if (dir == PortDir::kOutput && is_reg) {
          AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                         range_msb, range_lsb, array_dims);
        }
      }
      if (init) {
        if (dir != PortDir::kOutput || !is_reg) {
          ErrorHere("port initializer requires output variable");
          return false;
        }
        Statement init_stmt;
        init_stmt.kind = StatementKind::kAssign;
        init_stmt.assign.lhs = name;
        init_stmt.assign.rhs = std::move(init);
        init_stmt.assign.nonblocking = false;
        init_statements.push_back(std::move(init_stmt));
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
    if (!init_statements.empty()) {
      AlwaysBlock init_block;
      init_block.edge = EdgeKind::kInitial;
      init_block.clock = "initial";
      init_block.is_synthesized = true;
      init_block.is_decl_init = true;
      init_block.statements = std::move(init_statements);
      module->always_blocks.push_back(std::move(init_block));
    }
    return true;
  }

  bool ParseNetDecl(Module* module, NetType net_type) {
    bool is_signed = false;
    bool saw_scalared = false;
    bool saw_vectored = false;
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
      if (!is_signed && MatchKeyword("unsigned")) {
        is_signed = false;
        progressed = true;
      }
      if (!saw_scalared && MatchKeyword("scalared")) {
        saw_scalared = true;
        progressed = true;
      }
      if (!saw_vectored && MatchKeyword("vectored")) {
        saw_vectored = true;
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
    bool saw_delay = false;
    if (MatchSymbol("#")) {
      if (!SkipDelayControl()) {
        return false;
      }
      saw_delay = true;
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
    if (MatchSymbol("#")) {
      if (saw_delay) {
        ErrorHere("duplicate net delay");
        return false;
      }
      if (!SkipDelayControl()) {
        return false;
      }
      saw_delay = true;
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
        assign.is_implicit = true;
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
    std::vector<Statement> init_statements;
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
      if (MatchSymbol("=")) {
        if (!array_dims.empty()) {
          ErrorHere("reg array initializer not supported");
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
        ErrorHere("expected ';' after reg declaration");
        return false;
      }
      break;
    }
    if (!init_statements.empty()) {
      AlwaysBlock init_block;
      init_block.edge = EdgeKind::kInitial;
      init_block.clock = "initial";
      init_block.is_decl_init = true;
      init_block.statements = std::move(init_statements);
      module->always_blocks.push_back(std::move(init_block));
    }
    return true;
  }

  struct ParamTypeHint {
    bool has_type = false;
    bool is_real = false;
  };

  bool ParseParameterList(Module* module) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after '#'");
      return false;
    }
    if (MatchSymbol(")")) {
      return true;
    }
    bool require_keyword = true;
    ParamTypeHint type_hint;
    while (true) {
      if (MatchKeyword("parameter")) {
        require_keyword = false;
        type_hint = ParamTypeHint{};
      } else if (require_keyword) {
        ErrorHere("expected 'parameter' in parameter list");
        return false;
      }
      if (!ParseParameterItem(module, false, false, &type_hint)) {
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
    ParamTypeHint type_hint;
    if (!ParseParameterItem(module, is_local, false, &type_hint)) {
      return false;
    }
    while (MatchSymbol(",")) {
      if (MatchKeyword("parameter")) {
        type_hint = ParamTypeHint{};
        if (!ParseParameterItem(module, is_local, false, &type_hint)) {
          return false;
        }
      } else {
        if (!ParseParameterItem(module, is_local, false, &type_hint)) {
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

  bool ParseSpecparamDecl(Module* module) {
    ParamTypeHint type_hint;
    if (!ParseParameterItem(module, true, true, &type_hint)) {
      return false;
    }
    while (MatchSymbol(",")) {
      if (!ParseParameterItem(module, true, true, &type_hint)) {
        return false;
      }
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after specparam declaration");
      return false;
    }
    return true;
  }

  std::unique_ptr<Expr> ParseSpecparamValue(const std::string& name) {
    if (name.rfind("PATHPULSE$", 0) != 0) {
      return ParseExpr();
    }
    if (MatchSymbol("(")) {
      std::unique_ptr<Expr> first = ParseExpr();
      if (!first) {
        return nullptr;
      }
      if (MatchSymbol(",")) {
        std::unique_ptr<Expr> second = ParseExpr();
        if (!second) {
          return nullptr;
        }
      }
      if (!MatchSymbol(")")) {
        ErrorHere("expected ')' after PATHPULSE specparam");
        return nullptr;
      }
      return first;
    }
    return ParseExpr();
  }

  bool ParseParameterItem(Module* module, bool is_local, bool is_specparam,
                          ParamTypeHint* type_hint) {
    bool param_is_real = false;
    bool saw_type = false;
    while (true) {
      if (MatchKeyword("real") || MatchKeyword("realtime")) {
        param_is_real = true;
        saw_type = true;
        continue;
      }
      if (MatchKeyword("integer") || MatchKeyword("time")) {
        saw_type = true;
        continue;
      }
      if (MatchKeyword("signed") || MatchKeyword("unsigned")) {
        saw_type = true;
        continue;
      }
      break;
    }
    if (!saw_type && Peek().kind == TokenKind::kIdentifier &&
        Peek(1).kind == TokenKind::kIdentifier &&
        Peek(2).kind == TokenKind::kSymbol && Peek(2).text == "=") {
      if (Peek().text == "real" || Peek().text == "realtime") {
        param_is_real = true;
      }
      Advance();
      saw_type = true;
    }
    if (!saw_type && type_hint && type_hint->has_type) {
      param_is_real = type_hint->is_real;
    }
    int width = param_is_real ? 64 : 1;
    std::shared_ptr<Expr> range_msb;
    std::shared_ptr<Expr> range_lsb;
    bool had_range = false;
    if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
      return false;
    }
    if (param_is_real && had_range) {
      ErrorHere("real parameter cannot use packed ranges");
      return false;
    }
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      ErrorHere("expected parameter name");
      return false;
    }
    const Token& name_token = Previous();
    if (is_specparam) {
      current_specparams_.insert(name);
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in parameter assignment");
      return false;
    }
    std::unique_ptr<Expr> expr =
        is_specparam ? ParseSpecparamValue(name) : ParseExpr();
    if (!expr) {
      return false;
    }
    if (!is_specparam && ExprUsesSpecparam(*expr)) {
      if (options_.strict_1364) {
        diagnostics_->Add(Severity::kError,
                          "specparam value not allowed in parameter assignment",
                          SourceLocation{path_, name_token.line,
                                         name_token.column});
        return false;
      }
      diagnostics_->Add(Severity::kWarning,
                        "specparam value used in parameter assignment",
                        SourceLocation{path_, name_token.line,
                                       name_token.column});
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
    if (saw_type && type_hint) {
      type_hint->has_type = true;
      type_hint->is_real = param_is_real;
    }
    return true;
  }

  bool ParseIntegerDecl(Module* module) {
    const int width = 32;
    bool is_signed = true;
    std::vector<Statement> init_statements;
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
                     array_dims);
      if (MatchSymbol("=")) {
        if (!array_dims.empty()) {
          ErrorHere("integer array initializer not supported");
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
        ErrorHere("expected ';' after integer declaration");
        return false;
      }
      break;
    }
    if (!init_statements.empty()) {
      AlwaysBlock init_block;
      init_block.edge = EdgeKind::kInitial;
      init_block.clock = "initial";
      init_block.is_decl_init = true;
      init_block.statements = std::move(init_statements);
      module->always_blocks.push_back(std::move(init_block));
    }
    return true;
  }

  bool ParseTimeDecl(Module* module) {
    const int width = 64;
    bool is_signed = false;
    std::vector<Statement> init_statements;
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
                     array_dims);
      if (MatchSymbol("=")) {
        if (!array_dims.empty()) {
          ErrorHere("time array initializer not supported");
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
        ErrorHere("expected ';' after time declaration");
        return false;
      }
      break;
    }
    if (!init_statements.empty()) {
      AlwaysBlock init_block;
      init_block.edge = EdgeKind::kInitial;
      init_block.clock = "initial";
      init_block.is_decl_init = true;
      init_block.statements = std::move(init_statements);
      module->always_blocks.push_back(std::move(init_block));
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
      init_block.is_decl_init = true;
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

  bool ParseLocalIntegerDecl(std::vector<Statement>* init_statements) {
    const int width = 32;
    bool is_signed = true;
    std::vector<Statement> local_inits;
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
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(),
                     array_dims);
      if (MatchSymbol("=")) {
        if (!array_dims.empty()) {
          ErrorHere("integer array initializer not supported");
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
        local_inits.push_back(std::move(init_stmt));
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
    if (init_statements && !local_inits.empty()) {
      for (auto& stmt : local_inits) {
        init_statements->push_back(std::move(stmt));
      }
    }
    return true;
  }

  bool ParseLocalTimeDecl(std::vector<Statement>* init_statements) {
    const int width = 64;
    bool is_signed = false;
    std::vector<Statement> local_inits;
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
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(),
                     array_dims);
      if (MatchSymbol("=")) {
        if (!array_dims.empty()) {
          ErrorHere("time array initializer not supported");
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
        local_inits.push_back(std::move(init_stmt));
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
    if (init_statements && !local_inits.empty()) {
      for (auto& stmt : local_inits) {
        init_statements->push_back(std::move(stmt));
      }
    }
    return true;
  }

  bool ParseLocalRealDecl(std::vector<Statement>* init_statements) {
    const int width = 64;
    bool is_signed = true;
    std::vector<Statement> local_inits;
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
        local_inits.push_back(std::move(init_stmt));
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
    if (init_statements && !local_inits.empty()) {
      for (auto& stmt : local_inits) {
        init_statements->push_back(std::move(stmt));
      }
    }
    return true;
  }

  bool ParseLocalRegDecl(Module* module, std::vector<Statement>* init_statements) {
    bool is_signed = false;
    std::vector<Statement> local_inits;
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
      bool existing = false;
      if (current_module_) {
        for (const auto& port : current_module_->ports) {
          if (port.name == name) {
            existing = true;
            break;
          }
        }
        if (!existing) {
          for (const auto& net : current_module_->nets) {
            if (net.name == name) {
              existing = true;
              break;
            }
          }
        }
      }
      if (!existing) {
        AddOrUpdateNet(current_module_, name, NetType::kWire, width, is_signed,
                       range_msb, range_lsb, array_dims);
      }
      if (MatchSymbol("=")) {
        if (!array_dims.empty()) {
          ErrorHere("reg array initializer not supported");
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
        local_inits.push_back(std::move(init_stmt));
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
    if (init_statements && !local_inits.empty()) {
      for (auto& stmt : local_inits) {
        init_statements->push_back(std::move(stmt));
      }
    }
    return true;
  }

  struct GenvarScope {
    std::vector<std::unordered_set<std::string>> scopes;
    std::vector<std::string> active_loop_vars;

    void Reset() {
      scopes.clear();
      scopes.emplace_back();
      active_loop_vars.clear();
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

    bool IsLoopActive(const std::string& name) const {
      for (const auto& var : active_loop_vars) {
        if (var == name) {
          return true;
        }
      }
      return false;
    }

    void PushLoopVar(const std::string& name) {
      active_loop_vars.push_back(name);
    }

    void PopLoopVar() {
      if (!active_loop_vars.empty()) {
        active_loop_vars.pop_back();
      }
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

  struct GenvarLoopGuard {
    explicit GenvarLoopGuard(GenvarScope* scope, const std::string& name)
        : scope_(scope) {
      if (scope_) {
        scope_->PushLoopVar(name);
        active_ = true;
      }
    }
    ~GenvarLoopGuard() {
      if (scope_ && active_) {
        scope_->PopLoopVar();
      }
    }

   private:
    GenvarScope* scope_ = nullptr;
    bool active_ = false;
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
    auto expr = ParseExpr();
    if (!expr) {
      ErrorHere("expected delay value after '#'");
      return false;
    }
    while (Peek().kind == TokenKind::kSymbol &&
           (Peek().text == ":" || Peek().text == ",")) {
      Advance();
      auto extra = ParseExpr();
      if (!extra) {
        ErrorHere("expected delay value after separator");
        return false;
      }
    }
    return true;
  }

  std::unique_ptr<Expr> ParseDelayControlExpr() {
    if (Peek().kind == TokenKind::kSymbol && Peek().text == "(") {
      if (!SkipDelayControl()) {
        return nullptr;
      }
      return MakeNumberExpr(0u);
    }
    auto expr = ParseExpr();
    if (!expr) {
      return nullptr;
    }
    if (Peek().kind == TokenKind::kSymbol &&
        (Peek().text == ":" || Peek().text == ",")) {
      while (Peek().kind == TokenKind::kSymbol &&
             (Peek().text == ":" || Peek().text == ",")) {
        Advance();
        auto extra = ParseExpr();
        if (!extra) {
          return nullptr;
        }
      }
      return MakeNumberExpr(0u);
    }
    return expr;
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
      auto cit = consts.find(expr.ident);
      if (cit != consts.end()) {
        return MakeNumberExpr(static_cast<uint64_t>(cit->second));
      }
      std::string renamed = RenameIdent(expr.ident, renames);
      if (renamed != expr.ident) {
        auto out = std::make_unique<Expr>();
        out->kind = ExprKind::kIdentifier;
        out->ident = renamed;
        return out;
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
    bool saw_delay = false;
    if (MatchSymbol("#")) {
      if (!SkipDelayControl()) {
        return false;
      }
      saw_delay = true;
    }
    int width = 1;
    std::shared_ptr<Expr> range_msb;
    std::shared_ptr<Expr> range_lsb;
    if (!ParseRange(&width, &range_msb, &range_lsb, nullptr)) {
      return false;
    }
    if (MatchSymbol("#")) {
      if (saw_delay) {
        ErrorHere("duplicate net delay");
        return false;
      }
      if (!SkipDelayControl()) {
        return false;
      }
      saw_delay = true;
    }
    if (MatchSymbol("#")) {
      if (!SkipDelayControl()) {
        return false;
      }
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
    assign.id = generate_assign_id_++;
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

  struct GenerateContext {
    std::unordered_map<std::string, std::string> renames;
    std::unordered_map<std::string, int64_t> consts;
    std::shared_ptr<Expr> guard;
  };

  struct GeneratePrefixEntry {
    std::string name;
    bool has_index = false;
    int64_t index = 0;
  };

  bool IsSignedIntegerToken(const std::string& text) {
    if (text.empty()) {
      return false;
    }
    size_t i = 0;
    if (text[0] == '-' || text[0] == '+') {
      i = 1;
    }
    if (i >= text.size()) {
      return false;
    }
    for (; i < text.size(); ++i) {
      if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
        return false;
      }
    }
    return true;
  }

  std::vector<GeneratePrefixEntry> ParseGeneratePrefix(
      const std::string& prefix) {
    std::vector<GeneratePrefixEntry> entries;
    size_t start = 0;
    while (start < prefix.size()) {
      size_t next = prefix.find("__", start);
      std::string token =
          (next == std::string::npos) ? prefix.substr(start)
                                      : prefix.substr(start, next - start);
      if (!token.empty()) {
        if (!entries.empty() && !entries.back().has_index &&
            IsSignedIntegerToken(token)) {
          entries.back().has_index = true;
          entries.back().index = std::stoll(token);
        } else {
          GeneratePrefixEntry entry;
          entry.name = token;
          entries.push_back(std::move(entry));
        }
      }
      if (next == std::string::npos) {
        break;
      }
      start = next + 2;
    }
    return entries;
  }

  std::string JoinDefparamSegments(const std::vector<std::string>& segments) {
    if (segments.empty()) {
      return {};
    }
    std::string joined = segments.front();
    for (size_t i = 1; i < segments.size(); ++i) {
      joined += ".";
      joined += segments[i];
    }
    return joined;
  }

  std::string RenameIdent(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& renames) {
    auto it = renames.find(name);
    if (it != renames.end()) {
      return it->second;
    }
    size_t dot = name.find('.');
    if (dot != std::string::npos) {
      std::string head = name.substr(0, dot);
      auto hit = renames.find(head);
      if (hit != renames.end()) {
        return hit->second + name.substr(dot);
      }
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

  bool IsHierarchicalIdentifier(const std::string& name) const {
    return name.find('.') != std::string::npos ||
           name.find("__") != std::string::npos;
  }

  bool EnsureImplicitNet(Module* module, const std::string& name) {
    if (!module) {
      return true;
    }
    if (IsHierarchicalIdentifier(name)) {
      return true;
    }
    if (LookupSignalWidth(name) > 0) {
      return true;
    }
    if (IsModuleParamName(*module, name)) {
      return true;
    }
    if (current_genvars_.IsDeclared(name)) {
      return true;
    }
    if (default_nettype_none_) {
      ErrorHere("implicit net not allowed with `default_nettype none`");
      return false;
    }
    NetType net_type = default_nettype_;
    if (NetTypeRequires4State(net_type) && !options_.enable_4state) {
      ErrorHere("net type requires --4state");
      return false;
    }
    AddOrUpdateNet(module, name, net_type, 1, false, nullptr, nullptr, {});
    AddImplicitNetDriver(module, name, net_type);
    return true;
  }

  bool EnsureImplicitNetsFromExpr(Module* module, const Expr& expr) {
    if (!module) {
      return true;
    }
    auto check = [&](const std::unique_ptr<Expr>& subexpr) -> bool {
      return subexpr ? EnsureImplicitNetsFromExpr(module, *subexpr) : true;
    };
    switch (expr.kind) {
      case ExprKind::kIdentifier:
        return EnsureImplicitNet(module, expr.ident);
      case ExprKind::kNumber:
      case ExprKind::kString:
        return true;
      case ExprKind::kUnary:
        return check(expr.operand);
      case ExprKind::kBinary:
        return check(expr.lhs) && check(expr.rhs);
      case ExprKind::kTernary:
        return check(expr.condition) && check(expr.then_expr) &&
               check(expr.else_expr);
      case ExprKind::kSelect:
        return check(expr.base) && check(expr.index) &&
               check(expr.msb_expr) && check(expr.lsb_expr);
      case ExprKind::kIndex:
        return check(expr.base) && check(expr.index);
      case ExprKind::kCall:
        for (const auto& arg : expr.call_args) {
          if (!check(arg)) {
            return false;
          }
        }
        return true;
      case ExprKind::kConcat:
        if (!check(expr.repeat_expr)) {
          return false;
        }
        for (const auto& element : expr.elements) {
          if (!check(element)) {
            return false;
          }
        }
        return true;
    }
    return true;
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

  bool ExprUsesSpecparam(const Expr& expr) const {
    if (expr.kind == ExprKind::kIdentifier) {
      return current_specparams_.count(expr.ident) != 0;
    }
    if (expr.kind == ExprKind::kNumber || expr.kind == ExprKind::kString) {
      return false;
    }
    auto check = [&](const std::unique_ptr<Expr>& subexpr) -> bool {
      return subexpr ? ExprUsesSpecparam(*subexpr) : false;
    };
    if (check(expr.operand) || check(expr.lhs) || check(expr.rhs) ||
        check(expr.condition) || check(expr.then_expr) ||
        check(expr.else_expr) || check(expr.base) || check(expr.index) ||
        check(expr.msb_expr) || check(expr.lsb_expr) ||
        check(expr.repeat_expr)) {
      return true;
    }
    for (const auto& element : expr.elements) {
      if (element && ExprUsesSpecparam(*element)) {
        return true;
      }
    }
    for (const auto& arg : expr.call_args) {
      if (arg && ExprUsesSpecparam(*arg)) {
        return true;
      }
    }
    return false;
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
    out_statement->is_procedural = statement.is_procedural;
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
      out_statement->task_name =
          RenameIdent(statement.task_name, ctx.renames);
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
    out_block->is_synthesized = block.is_synthesized;
    out_block->is_decl_init = block.is_decl_init;
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
      if (item.kind == GenerateItem::Kind::kTask) {
        ctx.renames[item.task.name] = prefix + item.task.name;
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
        case GenerateItem::Kind::kDefparam: {
          const auto prefix_entries = ParseGeneratePrefix(prefix);
          std::vector<std::string> base_segments;
          base_segments.reserve(prefix_entries.size() * 2u);
          for (const auto& entry : prefix_entries) {
            base_segments.push_back(entry.name);
            if (entry.has_index) {
              base_segments.push_back(std::to_string(entry.index));
            }
          }
          std::string current_label;
          bool has_current_index = false;
          int64_t current_index = 0;
          if (!prefix_entries.empty()) {
            const auto& entry = prefix_entries.back();
            current_label = entry.name;
            has_current_index = entry.has_index;
            current_index = entry.index;
          }
          for (const auto& target : item.defparam.targets) {
            auto emit_error = [&](const std::string& message) {
              diagnostics_->Add(Severity::kError, message,
                                SourceLocation{path_, target.line,
                                               target.column});
            };
            bool explicit_self = false;
            bool apply_implied_index = false;
            if (!current_label.empty() && !target.instance_parts.empty() &&
                target.instance_parts.front().name == current_label) {
              if (target.instance_parts.front().index) {
                int64_t target_index = 0;
                if (!EvalConstExprWithContext(
                        *target.instance_parts.front().index, ctx,
                        &target_index)) {
                  emit_error("defparam index must be constant");
                  return false;
                }
                if (!has_current_index || target_index != current_index) {
                  emit_error(
                      "defparam cannot target other generate block instances");
                  return false;
                }
                explicit_self = true;
              } else if (has_current_index) {
                explicit_self = true;
                apply_implied_index = true;
              } else {
                explicit_self = true;
              }
            }
            std::vector<std::string> instance_segments;
            if (!explicit_self && !target.instance_parts.empty()) {
              instance_segments = base_segments;
            }
            for (size_t i = 0; i < target.instance_parts.size(); ++i) {
              const auto& part = target.instance_parts[i];
              instance_segments.push_back(part.name);
              if (part.index) {
                int64_t index_value = 0;
                if (!EvalConstExprWithContext(*part.index, ctx, &index_value)) {
                  emit_error("defparam index must be constant");
                  return false;
                }
                instance_segments.push_back(std::to_string(index_value));
              } else if (apply_implied_index && i == 0 && has_current_index) {
                instance_segments.push_back(std::to_string(current_index));
              }
            }
            DefParam defparam;
            defparam.instance = JoinDefparamSegments(instance_segments);
            defparam.param = target.param;
            if (target.expr) {
              defparam.expr =
                  CloneExprGenerate(*target.expr, ctx.renames, ctx.consts);
            }
            defparam.line = target.line;
            defparam.column = target.column;
            module->defparams.push_back(std::move(defparam));
          }
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
          assign.is_implicit = gen_assign.is_implicit;
          if (generate_assign_emitted_.insert(gen_assign.id).second) {
            assign.is_derived = false;
          } else if (!gen_assign.is_implicit) {
            assign.is_derived = true;
          }
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
        case GenerateItem::Kind::kTask: {
          Task cloned;
          cloned.name = RenameIdent(item.task.name, ctx.renames);
          for (const auto& arg : item.task.args) {
            TaskArg out_arg = arg;
            if (arg.msb_expr) {
              auto msb = CloneExprGenerate(*arg.msb_expr, ctx.renames,
                                           ctx.consts);
              out_arg.msb_expr = msb ? std::shared_ptr<Expr>(msb.release())
                                     : nullptr;
            }
            if (arg.lsb_expr) {
              auto lsb = CloneExprGenerate(*arg.lsb_expr, ctx.renames,
                                           ctx.consts);
              out_arg.lsb_expr = lsb ? std::shared_ptr<Expr>(lsb.release())
                                     : nullptr;
            }
            cloned.args.push_back(std::move(out_arg));
          }
          for (const auto& stmt : item.task.body) {
            Statement cloned_stmt;
            if (!CloneStatementGenerate(stmt, ctx, &cloned_stmt)) {
              return false;
            }
            cloned.body.push_back(std::move(cloned_stmt));
          }
          module->tasks.push_back(std::move(cloned));
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
            ctx.renames[block_label + "__" + std::to_string(current)] =
                prefix + block_label + "__" + std::to_string(current);
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

  bool NameConflictsWithModuleItem(const std::string& name) const {
    if (!current_module_) {
      return false;
    }
    for (const auto& port : current_module_->ports) {
      if (port.name == name) {
        return true;
      }
    }
    for (const auto& net : current_module_->nets) {
      if (net.name == name) {
        return true;
      }
    }
    for (const auto& param : current_module_->parameters) {
      if (param.name == name) {
        return true;
      }
    }
    for (const auto& inst : current_module_->instances) {
      if (inst.name == name) {
        return true;
      }
    }
    for (const auto& func : current_module_->functions) {
      if (func.name == name) {
        return true;
      }
    }
    for (const auto& task : current_module_->tasks) {
      if (task.name == name) {
        return true;
      }
    }
    for (const auto& event_decl : current_module_->events) {
      if (event_decl.name == name) {
        return true;
      }
    }
    return false;
  }

  bool RegisterGenerateLabel(const std::string& label) {
    if (!current_module_) {
      return true;
    }
    if (NameConflictsWithModuleItem(label)) {
      ErrorHere("generate block label '" + label +
                "' conflicts with existing declaration");
      return false;
    }
    current_module_->generate_labels.insert(label);
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
        if (!RegisterGenerateLabel(label)) {
          return false;
        }
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
    if (genvars->IsLoopActive(var)) {
      ErrorHere("generate for loop variable '" + var +
                "' already used in an outer generate loop");
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
    GenvarLoopGuard loop_guard(genvars, var);
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

  bool ParseGenerateHierPart(GenerateHierPart* out_part) {
    std::string name;
    if (!ConsumeIdentifier(&name)) {
      return false;
    }
    out_part->name = name;
    if (MatchSymbol("[")) {
      out_part->index = ParseExpr();
      if (!out_part->index) {
        return false;
      }
      if (!MatchSymbol("]")) {
        ErrorHere("expected ']' after defparam index");
        return false;
      }
    }
    return true;
  }

  bool ParseGenerateDefparam(std::vector<GenerateItem>* out_items) {
    GenerateDefparam defparam;
    while (true) {
      const Token& start_token = Peek();
      std::vector<GenerateHierPart> parts;
      GenerateHierPart part;
      if (!ParseGenerateHierPart(&part)) {
        ErrorHere("expected instance name in defparam");
        return false;
      }
      parts.push_back(std::move(part));
      while (MatchSymbol(".")) {
        GenerateHierPart next_part;
        if (!ParseGenerateHierPart(&next_part)) {
          ErrorHere("expected identifier after '.'");
          return false;
        }
        parts.push_back(std::move(next_part));
      }
      if (parts.empty()) {
        ErrorHere("expected instance name in defparam");
        return false;
      }
      GenerateHierPart param_part = std::move(parts.back());
      parts.pop_back();
      if (param_part.index) {
        diagnostics_->Add(Severity::kError,
                          "defparam parameter cannot be indexed",
                          SourceLocation{path_, start_token.line,
                                         start_token.column});
        return false;
      }
      if (!MatchSymbol("=")) {
        ErrorHere("expected '=' in defparam");
        return false;
      }
      std::unique_ptr<Expr> expr = ParseExpr();
      if (!expr) {
        return false;
      }
      GenerateDefparamTarget target;
      target.instance_parts = std::move(parts);
      target.param = std::move(param_part.name);
      target.expr = std::move(expr);
      target.line = start_token.line;
      target.column = start_token.column;
      defparam.targets.push_back(std::move(target));
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after defparam");
        return false;
      }
      break;
    }
    GenerateItem item;
    item.kind = GenerateItem::Kind::kDefparam;
    item.defparam = std::move(defparam);
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
    if (MatchSymbol(";")) {
      return true;
    }
    if (MatchKeyword("genvar")) {
      return ParseGenvarDecl(genvars);
    }
    if (MatchKeyword("localparam")) {
      return ParseGenerateLocalparam(&out_block->items);
    }
    if (MatchKeyword("defparam")) {
      return ParseGenerateDefparam(&out_block->items);
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
        if (!RegisterGenerateLabel(label)) {
          return false;
        }
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
    if (MatchKeyword("automatic")) {
      if (MatchKeyword("task")) {
        Task task;
        if (!ParseTaskInto(&task, true)) {
          return false;
        }
        GenerateItem item;
        item.kind = GenerateItem::Kind::kTask;
        item.task = std::move(task);
        out_block->items.push_back(std::move(item));
        return true;
      }
      ErrorHere("unsupported generate item 'automatic'");
      return false;
    }
    if (MatchKeyword("task")) {
      Task task;
      if (!ParseTaskInto(&task, false)) {
        return false;
      }
      GenerateItem item;
      item.kind = GenerateItem::Kind::kTask;
      item.task = std::move(task);
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
          block.is_synthesized = true;
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
        assign.id = generate_assign_id_++;
        assign.lhs = gate_assign.lhs;
        assign.lhs_has_range = gate_assign.lhs_has_range;
        assign.lhs_is_range = gate_assign.lhs_is_range;
        assign.is_implicit = true;
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
      std::vector<Instance> instances;
      if (!ParseInstanceList(&instances)) {
        return false;
      }
      for (auto& instance : instances) {
        GenerateItem item;
        item.kind = GenerateItem::Kind::kInstance;
        item.instance = std::move(instance);
        out_block->items.push_back(std::move(item));
      }
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

  bool ParseImplicitGenerate(Module* module) {
    GenerateBlock block;
    if (!ParseGenerateBlockBody(&block, &current_genvars_)) {
      return false;
    }
    GenerateContext ctx;
    return EmitGenerateBlock(block, ctx, "", module);
  }

  bool ParseAssign(Module* module) {
    struct ConcatLvalue {
      std::string name;
      bool has_range = false;
      int msb = 0;
      int lsb = 0;
      int width = 0;
    };
    auto ensure_net_width = [&](const std::string& name) -> int {
      int width = LookupSignalWidth(name);
      if (width > 0) {
        return width;
      }
      if (default_nettype_none_) {
        ErrorHere("implicit net not allowed with `default_nettype none`");
        return -1;
      }
      NetType net_type = default_nettype_;
      if (NetTypeRequires4State(net_type) && !options_.enable_4state) {
        ErrorHere("net type requires --4state");
        return -1;
      }
      AddOrUpdateNet(module, name, net_type, 1, false, nullptr, nullptr, {});
      AddImplicitNetDriver(module, name, net_type);
      return 1;
    };
    auto extract_concat_lvalues =
        [&](const Expr& concat, std::vector<ConcatLvalue>* out,
            int* total_width) -> bool {
      if (!out || !total_width) {
        return false;
      }
      if (concat.kind != ExprKind::kConcat) {
        ErrorHere("expected concatenation in assignment");
        return false;
      }
      if (concat.repeat != 1 || concat.repeat_expr) {
        ErrorHere("replication not allowed in assignment target");
        return false;
      }
      int width_sum = 0;
      for (const auto& element : concat.elements) {
        if (!element) {
          ErrorHere("invalid concatenation element");
          return false;
        }
        ConcatLvalue lv;
        if (element->kind == ExprKind::kIdentifier) {
          lv.name = element->ident;
          lv.width = ensure_net_width(lv.name);
          if (lv.width <= 0) {
            return false;
          }
        } else if (element->kind == ExprKind::kSelect) {
          if (!element->base ||
              element->base->kind != ExprKind::kIdentifier) {
            ErrorHere("concatenation lvalue must be identifier or select");
            return false;
          }
          if (element->indexed_range) {
            ErrorHere("indexed part select not supported in concatenation target");
            return false;
          }
          lv.name = element->base->ident;
          int base_width = ensure_net_width(lv.name);
          if (base_width <= 0) {
            return false;
          }
          if (element->has_range) {
            int64_t msb = 0;
            int64_t lsb = 0;
            if (!element->msb_expr || !element->lsb_expr ||
                !TryEvalConstExpr(*element->msb_expr, &msb) ||
                !TryEvalConstExpr(*element->lsb_expr, &lsb)) {
              ErrorHere("concatenation part select must be constant");
              return false;
            }
            lv.has_range = true;
            lv.msb = static_cast<int>(msb);
            lv.lsb = static_cast<int>(lsb);
            lv.width = (lv.msb >= lv.lsb) ? (lv.msb - lv.lsb + 1)
                                          : (lv.lsb - lv.msb + 1);
          } else {
            lv.has_range = true;
            lv.msb = element->msb;
            lv.lsb = element->lsb;
            lv.width = 1;
          }
        } else {
          ErrorHere("concatenation lvalue must be identifier or select");
          return false;
        }
        if (lv.width <= 0) {
          ErrorHere("invalid concatenation element width");
          return false;
        }
        width_sum += lv.width;
        out->push_back(std::move(lv));
      }
      *total_width = width_sum;
      return true;
    };

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
    bool is_first_assign = true;
    while (true) {
      std::unique_ptr<Expr> lhs_concat;
      std::string lhs;
      if (MatchSymbol("{")) {
        lhs_concat = ParseConcat();
        if (!lhs_concat) {
          return false;
        }
      } else {
        if (!ConsumeHierIdentifier(&lhs)) {
          ErrorHere("expected identifier after 'assign'");
          return false;
        }
      }
      Assign assign;
      bool is_primary = is_first_assign;
      if (!lhs_concat) {
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
            int64_t msb = indexed_desc ? base_value
                                       : (base_value + width_value - 1);
            int64_t lsb =
                indexed_desc ? (base_value - width_value + 1) : base_value;
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
      }
      if (!MatchSymbol("=")) {
        ErrorHere("expected '=' in assign");
        return false;
      }
      std::unique_ptr<Expr> rhs = ParseExpr();
      if (!rhs) {
        return false;
      }
      if (lhs_concat) {
        std::vector<ConcatLvalue> lvalues;
        int total_width = 0;
        if (!extract_concat_lvalues(*lhs_concat, &lvalues, &total_width)) {
          return false;
        }
        int cursor = total_width;
        for (size_t idx = 0; idx < lvalues.size(); ++idx) {
          const auto& lv = lvalues[idx];
          int msb = cursor - 1;
          int lsb = cursor - lv.width;
          cursor = lsb;
          std::unique_ptr<Expr> rhs_slice;
          if (lv.width == 1) {
            rhs_slice = MakeRangeSelectExpr(CloneExprSimple(*rhs), msb, lsb);
            rhs_slice->has_range = false;
          } else {
            rhs_slice = MakeRangeSelectExpr(CloneExprSimple(*rhs), msb, lsb);
          }
          Assign part_assign;
          part_assign.lhs = lv.name;
          part_assign.strength0 = strength0;
          part_assign.strength1 = strength1;
          part_assign.has_strength = has_strength;
          part_assign.lhs_has_range = lv.has_range;
          part_assign.lhs_msb = lv.msb;
          part_assign.lhs_lsb = lv.lsb;
          part_assign.is_derived = !is_primary || idx > 0;
          part_assign.rhs = std::move(rhs_slice);
          module->assigns.push_back(std::move(part_assign));
        }
      } else {
        assign.rhs = std::move(rhs);
        assign.is_derived = !is_primary;
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
          AddOrUpdateNet(module, assign.lhs, net_type, 1, false, nullptr,
                         nullptr, {});
          AddImplicitNetDriver(module, assign.lhs, net_type);
        }
        module->assigns.push_back(std::move(assign));
      }
      if (MatchSymbol(",")) {
        is_first_assign = false;
        continue;
      }
      break;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after assign");
      return false;
    }
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
    bool implicit_forever = false;
    if (MatchSymbol("@")) {
      has_event = true;
      bool has_paren = MatchSymbol("(");
      if (!ParseEventList(has_paren, &items, &saw_star, &sensitivity)) {
        return false;
      }
    } else if (!(Peek().kind == TokenKind::kSymbol && Peek().text == "#")) {
      implicit_forever = true;
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
    if (has_delay_control || implicit_forever) {
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
    SkipAttributeInstances();
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
      return ParseLocalIntegerDecl(out_statements);
    }
    if (MatchKeyword("time")) {
      return ParseLocalTimeDecl(out_statements);
    }
    if (MatchKeyword("realtime")) {
      return ParseLocalRealDecl(out_statements);
    }
    if (MatchKeyword("real")) {
      return ParseLocalRealDecl(out_statements);
    }
    if (MatchKeyword("reg")) {
      return ParseLocalRegDecl(current_module_, out_statements);
    }
    Statement stmt;
    if (!ParseStatement(&stmt)) {
      return false;
    }
    out_statements->push_back(std::move(stmt));
    return true;
  }

  bool ParseStatement(Statement* out_statement) {
    if (MatchSymbol(";")) {
      Statement stmt;
      stmt.kind = StatementKind::kBlock;
      *out_statement = std::move(stmt);
      return true;
    }
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
    if (MatchKeyword("assign")) {
      return ParseProceduralAssignStatement(out_statement);
    }
    if (MatchKeyword("deassign")) {
      return ParseProceduralDeassignStatement(out_statement);
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
    auto looks_like_task_call = [&]() -> bool {
      if (Peek().kind != TokenKind::kIdentifier) {
        return false;
      }
      size_t idx = pos_ + 1;
      auto skip_brackets = [&]() -> bool {
        if (idx >= tokens_.size()) {
          return false;
        }
        if (tokens_[idx].kind != TokenKind::kSymbol ||
            tokens_[idx].text != "[") {
          return true;
        }
        int depth = 0;
        while (idx < tokens_.size()) {
          if (tokens_[idx].kind == TokenKind::kSymbol) {
            if (tokens_[idx].text == "[") {
              ++depth;
            } else if (tokens_[idx].text == "]") {
              --depth;
              if (depth == 0) {
                ++idx;
                return true;
              }
            }
          }
          ++idx;
        }
        return false;
      };
      if (!skip_brackets()) {
        return false;
      }
      while (idx < tokens_.size()) {
        if (tokens_[idx].kind == TokenKind::kSymbol &&
            tokens_[idx].text == ".") {
          ++idx;
          if (idx >= tokens_.size() ||
              tokens_[idx].kind != TokenKind::kIdentifier) {
            return false;
          }
          ++idx;
          if (!skip_brackets()) {
            return false;
          }
          continue;
        }
        break;
      }
      if (idx >= tokens_.size()) {
        return false;
      }
      if (tokens_[idx].kind == TokenKind::kSymbol &&
          (tokens_[idx].text == "(" || tokens_[idx].text == ";" ||
           tokens_[idx].text == ",")) {
        return true;
      }
      return false;
    };
    if (Peek().kind == TokenKind::kIdentifier) {
      if (looks_like_task_call()) {
        return ParseTaskCallStatement(out_statement);
      }
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
    std::unique_ptr<Expr> delay_expr = ParseDelayControlExpr();
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
    bool prev_allow = allow_string_literals_;
    allow_string_literals_ = true;
    if (MatchSymbol("(")) {
      if (!MatchSymbol(")")) {
        auto make_empty_arg = []() -> std::unique_ptr<Expr> {
          auto expr = std::make_unique<Expr>();
          expr->kind = ExprKind::kString;
          expr->string_value.clear();
          return expr;
        };
        while (true) {
          if (Peek().kind == TokenKind::kSymbol && Peek().text == ",") {
            stmt.task_args.push_back(make_empty_arg());
            MatchSymbol(",");
            continue;
          }
          if (Peek().kind == TokenKind::kSymbol && Peek().text == ")") {
            break;
          }
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
    } else {
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
      if (!MatchSymbol(";")) {
        allow_string_literals_ = prev_allow;
        ErrorHere("expected ';' after system task");
        return false;
      }
      allow_string_literals_ = prev_allow;
      *out_statement = std::move(stmt);
      return true;
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
    if (MatchKeyword("fork")) {
      target = "fork";
    } else if (!ConsumeHierIdentifier(&target)) {
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
    if (!ConsumeHierIdentifier(&name)) {
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
    } else if (MatchKeyword("real") || MatchKeyword("realtime")) {
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
        if (!ParseLocalIntegerDecl(&stmt.block)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("time")) {
        if (!ParseLocalTimeDecl(&stmt.block)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("real") || MatchKeyword("realtime")) {
        if (!ParseLocalRealDecl(&stmt.block)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("reg")) {
        if (!ParseLocalRegDecl(current_module_, &stmt.block)) {
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
    struct ConcatLvalue {
      std::string name;
      bool has_range = false;
      int msb = 0;
      int lsb = 0;
      int width = 0;
    };
    SequentialAssign assign;
    std::unique_ptr<Expr> lhs_concat;
    if (MatchSymbol("{")) {
      lhs_concat = ParseConcat();
      if (!lhs_concat) {
        return false;
      }
    } else {
      if (!ParseAssignTarget(&assign, "sequential assignment")) {
        return false;
      }
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
    std::unique_ptr<Expr> repeat_expr;
    bool has_event_control = false;
    std::vector<EventItem> event_items;
    if (MatchKeyword("repeat")) {
      if (!MatchSymbol("(")) {
        ErrorHere("expected '(' after repeat");
        return false;
      }
      repeat_expr = ParseExpr();
      if (!repeat_expr) {
        return false;
      }
      if (!MatchSymbol(")")) {
        ErrorHere("expected ')' after repeat count");
        return false;
      }
      if (!MatchSymbol("@")) {
        ErrorHere("expected '@' after repeat");
        return false;
      }
      bool has_paren = MatchSymbol("(");
      if (!ParseEventList(has_paren, &event_items, nullptr, nullptr)) {
        return false;
      }
      has_event_control = true;
    } else if (MatchSymbol("#")) {
      delay = ParseDelayControlExpr();
      if (!delay) {
        return false;
      }
    } else if (MatchSymbol("@")) {
      bool has_paren = MatchSymbol("(");
      if (!ParseEventList(has_paren, &event_items, nullptr, nullptr)) {
        return false;
      }
      has_event_control = true;
    }
    std::unique_ptr<Expr> rhs = ParseExpr();
    if (!rhs) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after assignment");
      return false;
    }
    if (lhs_concat) {
      if (lhs_concat->kind != ExprKind::kConcat || lhs_concat->repeat != 1 ||
          lhs_concat->repeat_expr) {
        ErrorHere("replication not allowed in assignment target");
        return false;
      }
      std::vector<ConcatLvalue> lvalues;
      int total_width = 0;
      for (const auto& element : lhs_concat->elements) {
        if (!element) {
          ErrorHere("invalid concatenation element");
          return false;
        }
        ConcatLvalue lv;
        if (element->kind == ExprKind::kIdentifier) {
          lv.name = element->ident;
          lv.width = LookupSignalWidth(lv.name);
          if (lv.width <= 0) {
            ErrorHere("concatenation lvalue must be declared");
            return false;
          }
        } else if (element->kind == ExprKind::kSelect) {
          if (!element->base ||
              element->base->kind != ExprKind::kIdentifier) {
            ErrorHere("concatenation lvalue must be identifier or select");
            return false;
          }
          if (element->indexed_range) {
            ErrorHere("indexed part select not supported in concatenation target");
            return false;
          }
          lv.name = element->base->ident;
          int base_width = LookupSignalWidth(lv.name);
          if (base_width <= 0) {
            ErrorHere("concatenation lvalue must be declared");
            return false;
          }
          if (element->has_range) {
            int64_t msb = 0;
            int64_t lsb = 0;
            if (!element->msb_expr || !element->lsb_expr ||
                !TryEvalConstExpr(*element->msb_expr, &msb) ||
                !TryEvalConstExpr(*element->lsb_expr, &lsb)) {
              ErrorHere("concatenation part select must be constant");
              return false;
            }
            lv.has_range = true;
            lv.msb = static_cast<int>(msb);
            lv.lsb = static_cast<int>(lsb);
            lv.width = (lv.msb >= lv.lsb) ? (lv.msb - lv.lsb + 1)
                                          : (lv.lsb - lv.msb + 1);
          } else {
            lv.has_range = true;
            lv.msb = element->msb;
            lv.lsb = element->lsb;
            lv.width = 1;
          }
        } else {
          ErrorHere("concatenation lvalue must be identifier or select");
          return false;
        }
        if (lv.width <= 0) {
          ErrorHere("invalid concatenation element width");
          return false;
        }
        total_width += lv.width;
        lvalues.push_back(std::move(lv));
      }
      int cursor = total_width;
      std::vector<Statement> assigns;
      assigns.reserve(lvalues.size());
      for (const auto& lv : lvalues) {
        int msb = cursor - 1;
        int lsb = cursor - lv.width;
        cursor = lsb;
        std::unique_ptr<Expr> rhs_slice = MakeRangeSelectExpr(
            CloneExprSimple(*rhs), msb, lsb);
        if (lv.width == 1) {
          rhs_slice->has_range = false;
        }
        SequentialAssign part;
        part.lhs = lv.name;
        part.lhs_has_range = lv.has_range;
        part.lhs_msb = lv.msb;
        part.lhs_lsb = lv.lsb;
        part.rhs = std::move(rhs_slice);
        part.nonblocking = nonblocking;
        if (delay) {
          part.delay = CloneExprSimple(*delay);
        }
        Statement assign_stmt;
        assign_stmt.kind = StatementKind::kAssign;
        assign_stmt.assign = std::move(part);
        assigns.push_back(std::move(assign_stmt));
      }
      Statement block;
      block.kind = StatementKind::kBlock;
      block.block = std::move(assigns);
      if (has_event_control) {
        Statement event_stmt;
        event_stmt.kind = StatementKind::kEventControl;
        if (!event_items.empty()) {
          if (event_items.size() == 1) {
            event_stmt.event_edge = event_items[0].edge;
            event_stmt.event_expr = std::move(event_items[0].expr);
          } else {
            event_stmt.event_items = std::move(event_items);
          }
        } else {
          event_stmt.event_edge = EventEdgeKind::kAny;
          event_stmt.event_expr = nullptr;
        }
        event_stmt.event_body.push_back(std::move(block));
        if (repeat_expr) {
          Statement repeat_stmt;
          repeat_stmt.kind = StatementKind::kRepeat;
          repeat_stmt.repeat_count = std::move(repeat_expr);
          repeat_stmt.repeat_body.push_back(std::move(event_stmt));
          *out_statement = std::move(repeat_stmt);
        } else {
          *out_statement = std::move(event_stmt);
        }
        return true;
      }
      if (repeat_expr) {
        Statement repeat_stmt;
        repeat_stmt.kind = StatementKind::kRepeat;
        repeat_stmt.repeat_count = std::move(repeat_expr);
        repeat_stmt.repeat_body.push_back(std::move(block));
        *out_statement = std::move(repeat_stmt);
      } else {
        *out_statement = std::move(block);
      }
      return true;
    }
    if (has_event_control) {
      Statement assign_stmt;
      assign_stmt.kind = StatementKind::kAssign;
      assign.rhs = std::move(rhs);
      assign.delay = std::move(delay);
      assign.nonblocking = nonblocking;
      assign_stmt.assign = std::move(assign);
      Statement event_stmt;
      event_stmt.kind = StatementKind::kEventControl;
      if (!event_items.empty()) {
        if (event_items.size() == 1) {
          event_stmt.event_edge = event_items[0].edge;
          event_stmt.event_expr = std::move(event_items[0].expr);
        } else {
          event_stmt.event_items = std::move(event_items);
        }
      } else {
        event_stmt.event_edge = EventEdgeKind::kAny;
        event_stmt.event_expr = nullptr;
      }
      event_stmt.event_body.push_back(std::move(assign_stmt));
      if (repeat_expr) {
        Statement repeat_stmt;
        repeat_stmt.kind = StatementKind::kRepeat;
        repeat_stmt.repeat_count = std::move(repeat_expr);
        repeat_stmt.repeat_body.push_back(std::move(event_stmt));
        *out_statement = std::move(repeat_stmt);
      } else {
        *out_statement = std::move(event_stmt);
      }
      return true;
    }
    Statement stmt;
    stmt.kind = StatementKind::kAssign;
    assign.rhs = std::move(rhs);
    assign.delay = std::move(delay);
    assign.nonblocking = nonblocking;
    stmt.assign = std::move(assign);
    if (repeat_expr) {
      Statement repeat_stmt;
      repeat_stmt.kind = StatementKind::kRepeat;
      repeat_stmt.repeat_count = std::move(repeat_expr);
      repeat_stmt.repeat_body.push_back(std::move(stmt));
      *out_statement = std::move(repeat_stmt);
    } else {
      *out_statement = std::move(stmt);
    }
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
      delay = ParseDelayControlExpr();
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

  bool ParseProceduralAssignStatement(Statement* out_statement) {
    std::unique_ptr<Expr> delay;
    std::unique_ptr<Expr> repeat_expr;
    bool has_event_control = false;
    std::vector<EventItem> event_items;
    if (MatchKeyword("repeat")) {
      if (!MatchSymbol("(")) {
        ErrorHere("expected '(' after repeat");
        return false;
      }
      repeat_expr = ParseExpr();
      if (!repeat_expr) {
        return false;
      }
      if (!MatchSymbol(")")) {
        ErrorHere("expected ')' after repeat count");
        return false;
      }
      if (!MatchSymbol("@")) {
        ErrorHere("expected '@' after repeat");
        return false;
      }
      bool has_paren = MatchSymbol("(");
      if (!ParseEventList(has_paren, &event_items, nullptr, nullptr)) {
        return false;
      }
      has_event_control = true;
    } else if (MatchSymbol("#")) {
      delay = ParseDelayControlExpr();
      if (!delay) {
        return false;
      }
    } else if (MatchSymbol("@")) {
      bool has_paren = MatchSymbol("(");
      if (!ParseEventList(has_paren, &event_items, nullptr, nullptr)) {
        return false;
      }
      has_event_control = true;
    }
    SequentialAssign assign;
    if (!ParseAssignTarget(&assign, "assign statement")) {
      return false;
    }
    if (!MatchSymbol("=")) {
      ErrorHere("expected '=' in assign statement");
      return false;
    }
    std::unique_ptr<Expr> rhs = ParseExpr();
    if (!rhs) {
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after assign statement");
      return false;
    }
    std::string target = assign.lhs;
    Statement stmt;
    stmt.kind = StatementKind::kForce;
    stmt.is_procedural = true;
    stmt.force_target = std::move(target);
    assign.rhs = std::move(rhs);
    assign.delay = std::move(delay);
    assign.nonblocking = false;
    stmt.assign = std::move(assign);
    if (has_event_control) {
      Statement event_stmt;
      event_stmt.kind = StatementKind::kEventControl;
      if (event_items.size() == 1) {
        event_stmt.event_edge = event_items[0].edge;
        event_stmt.event_expr = std::move(event_items[0].expr);
      } else {
        event_stmt.event_items = std::move(event_items);
      }
      event_stmt.event_body.push_back(std::move(stmt));
      if (repeat_expr) {
        Statement repeat_stmt;
        repeat_stmt.kind = StatementKind::kRepeat;
        repeat_stmt.repeat_count = std::move(repeat_expr);
        repeat_stmt.repeat_body.push_back(std::move(event_stmt));
        *out_statement = std::move(repeat_stmt);
      } else {
        *out_statement = std::move(event_stmt);
      }
      return true;
    }
    if (repeat_expr) {
      Statement repeat_stmt;
      repeat_stmt.kind = StatementKind::kRepeat;
      repeat_stmt.repeat_count = std::move(repeat_expr);
      repeat_stmt.repeat_body.push_back(std::move(stmt));
      *out_statement = std::move(repeat_stmt);
    } else {
      *out_statement = std::move(stmt);
    }
    return true;
  }

  bool ParseProceduralDeassignStatement(Statement* out_statement) {
    std::string target;
    if (!ConsumeHierIdentifier(&target)) {
      ErrorHere("expected identifier after 'deassign'");
      return false;
    }
    if (!MatchSymbol(";")) {
      ErrorHere("expected ';' after deassign");
      return false;
    }
    Statement stmt;
    stmt.kind = StatementKind::kRelease;
    stmt.is_procedural = true;
    stmt.release_target = std::move(target);
    stmt.assign.lhs = stmt.release_target;
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

  bool ParseInstanceList(std::vector<Instance>* out_instances) {
    if (!out_instances) {
      return false;
    }
    std::string module_name;
    if (!ConsumeIdentifier(&module_name)) {
      ErrorHere("expected module name in instance");
      return false;
    }
    Instance base;
    base.module_name = std::move(module_name);
    if (MatchSymbol("#")) {
      if (Peek().kind == TokenKind::kSymbol && Peek().text == "(") {
        if (!ParseParamOverrides(&base)) {
          return false;
        }
      } else {
        if (!SkipDelayControl()) {
          return false;
        }
      }
    }
    auto clone_array_expr = [&](const Expr& expr, size_t count,
                                size_t slot) -> std::unique_ptr<Expr> {
      if (expr.kind == ExprKind::kConcat && expr.elements.size() == count) {
        return CloneExprSimple(*expr.elements[slot]);
      }
      return CloneExprSimple(expr);
    };
    auto append_instance = [&](Instance& instance,
                               const std::vector<int64_t>& indices) -> bool {
      if (indices.empty()) {
        out_instances->push_back(std::move(instance));
        return true;
      }
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
        out_instances->push_back(std::move(expanded));
      }
      return true;
    };
    while (true) {
      std::string instance_name;
      bool has_name = false;
      if (ConsumeIdentifier(&instance_name)) {
        has_name = true;
      } else if (Peek().kind == TokenKind::kSymbol && Peek().text == "(") {
        instance_name = "__gpga_inst" + std::to_string(instance_id_++);
      } else {
        ErrorHere("expected instance name");
        return false;
      }
      Instance instance;
      instance.module_name = base.module_name;
      for (const auto& override_item : base.param_overrides) {
        ParamOverride param;
        param.name = override_item.name;
        if (override_item.expr) {
          param.expr = CloneExprSimple(*override_item.expr);
        }
        instance.param_overrides.push_back(std::move(param));
      }
      instance.name = std::move(instance_name);
      std::vector<int64_t> indices;
      if (MatchSymbol("[")) {
        if (!has_name) {
          ErrorHere("instance array requires a name");
          return false;
        }
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
      if (!MatchSymbol(")")) {
        bool named = false;
        if (Peek().kind == TokenKind::kSymbol && Peek().text == ".") {
          named = true;
        }
        if (named) {
          std::unordered_set<std::string> seen_ports;
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
            if (seen_ports.count(port_name) != 0) {
              ErrorHere("duplicate connection for port '" + port_name + "'");
              return false;
            }
            seen_ports.insert(port_name);
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
              if (!EnsureImplicitNetsFromExpr(current_module_, *expr)) {
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
              if (!EnsureImplicitNetsFromExpr(current_module_, *expr)) {
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
      if (!append_instance(instance, indices)) {
        return false;
      }
      if (MatchSymbol(",")) {
        continue;
      }
      if (!MatchSymbol(";")) {
        ErrorHere("expected ';' after instance");
        return false;
      }
      break;
    }
    return true;
  }

  bool ParseInstance(Module* module) {
    std::vector<Instance> instances;
    if (!ParseInstanceList(&instances)) {
      return false;
    }
    for (auto& instance : instances) {
      module->instances.push_back(std::move(instance));
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
    std::unique_ptr<Expr> msb_expr = ParseExpr();
    if (!msb_expr) {
      return false;
    }
    if (!MatchSymbol(":")) {
      ErrorHere("expected ':' in range");
      return false;
    }
    std::unique_ptr<Expr> lsb_expr = ParseExpr();
    if (!lsb_expr) {
      return false;
    }
    if (!MatchSymbol("]")) {
      ErrorHere("expected ']' after range");
      return false;
    }
    int64_t msb = 0;
    int64_t lsb = 0;
    if (TryEvalConstExpr(*msb_expr, &msb) &&
        TryEvalConstExpr(*lsb_expr, &lsb)) {
      int64_t width64 = msb >= lsb ? (msb - lsb + 1) : (lsb - msb + 1);
      if (width64 <= 0 || width64 > 0x7FFFFFFF) {
        ErrorHere("invalid range width");
        return false;
      }
      *width_out = static_cast<int>(width64);
    } else {
      *width_out = 1;
    }
    if (msb_out) {
      *msb_out = std::shared_ptr<Expr>(std::move(msb_expr));
    }
    if (lsb_out) {
      *lsb_out = std::shared_ptr<Expr>(std::move(lsb_expr));
    }
    return true;
  }

  std::unique_ptr<Expr> ParseExpr() { return ParseConditional(); }

  void SkipAttributeInstances() {
    while (PeekSymbol("(") && Peek(1).kind == TokenKind::kSymbol &&
           Peek(1).text == "*") {
      if (pos_ > 0 && Previous().kind == TokenKind::kSymbol &&
          Previous().text == "@") {
        break;
      }
      Advance();
      Advance();
      while (!IsAtEnd()) {
        if (PeekSymbol("*") && Peek(1).kind == TokenKind::kSymbol &&
            Peek(1).text == ")") {
          Advance();
          Advance();
          break;
        }
        Advance();
      }
    }
  }

  bool IsMinTypMaxStopKeyword(const std::string& name) const {
    return name == "begin" || name == "end" || name == "if" ||
           name == "else" || name == "case" || name == "casez" ||
           name == "casex" || name == "endcase" || name == "default" ||
           name == "for" || name == "while" || name == "repeat" ||
           name == "forever" || name == "fork" || name == "join" ||
           name == "initial" || name == "always" || name == "assign" ||
           name == "generate" || name == "endgenerate";
  }

  bool LooksLikeMinTypMax() const {
    if (!PeekSymbol(":")) {
      return false;
    }
    if (Peek(1).kind == TokenKind::kIdentifier &&
        IsMinTypMaxStopKeyword(Peek(1).text)) {
      return false;
    }
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    int ternary_depth = 0;
    for (size_t lookahead = 1; pos_ + lookahead < tokens_.size();
         ++lookahead) {
      const Token& token = Peek(lookahead);
      if (token.kind == TokenKind::kEnd) {
        return false;
      }
      if (token.kind == TokenKind::kIdentifier &&
          IsMinTypMaxStopKeyword(token.text)) {
        return false;
      }
      if (token.kind != TokenKind::kSymbol) {
        continue;
      }
      const std::string& sym = token.text;
      if (sym == "(") {
        ++paren_depth;
        continue;
      }
      if (sym == ")") {
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
          return false;
        }
        if (paren_depth > 0) {
          --paren_depth;
        }
        continue;
      }
      if (sym == "[") {
        ++bracket_depth;
        continue;
      }
      if (sym == "]") {
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
          return false;
        }
        if (bracket_depth > 0) {
          --bracket_depth;
        }
        continue;
      }
      if (sym == "{") {
        ++brace_depth;
        continue;
      }
      if (sym == "}") {
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
          return false;
        }
        if (brace_depth > 0) {
          --brace_depth;
        }
        continue;
      }
      if (paren_depth != 0 || bracket_depth != 0 || brace_depth != 0) {
        continue;
      }
      if (sym == "?") {
        ++ternary_depth;
        continue;
      }
      if (sym == ":") {
        if (ternary_depth > 0) {
          --ternary_depth;
          continue;
        }
        return true;
      }
      if (sym == "," || sym == ";") {
        return false;
      }
    }
    return false;
  }

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
    if (PeekSymbol(":") && LooksLikeMinTypMax()) {
      Advance();
      auto typ_expr = ParseLogicalOr();
      if (!typ_expr) {
        return nullptr;
      }
      if (!MatchSymbol(":")) {
        ErrorHere("expected ':' in min:typ:max expression");
        return nullptr;
      }
      auto max_expr = ParseLogicalOr();
      if (!max_expr) {
        return nullptr;
      }
      return typ_expr;
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

      if (Peek().kind == TokenKind::kIdentifier &&
          Peek().text.find('$') != std::string::npos) {
        std::string name = Peek().text;
        Advance();
        if (name == "test$plusargs" || name == "value$plusargs") {
          expr = parse_system_call("$" + name, false);
        } else {
          ErrorHere("unsupported system function");
          return nullptr;
        }
      }

      char op = 0;
      if (!expr && MatchKeyword("time")) {
        expr = parse_system_call("$time", true);
      } else if (!expr && MatchKeyword("stime")) {
        expr = parse_system_call("$stime", true);
      } else if (!expr && MatchKeyword("random")) {
        expr = parse_system_call("$random", true);
      } else if (!expr && MatchKeyword("urandom_range")) {
        expr = parse_system_call("$urandom_range", false);
      } else if (!expr && MatchKeyword("urandom")) {
        expr = parse_system_call("$urandom", true);
      } else if (!expr && MatchKeyword("realtime")) {
        expr = parse_system_call("$realtime", true);
      } else if (!expr && MatchKeyword("realtobits")) {
        expr = parse_system_call("$realtobits", false);
      } else if (!expr && MatchKeyword("bitstoreal")) {
        expr = parse_system_call("$bitstoreal", false);
      } else if (!expr && MatchKeyword("rtoi")) {
        expr = parse_system_call("$rtoi", false);
      } else if (!expr && MatchKeyword("itor")) {
        expr = parse_system_call("$itor", false);
      } else if (!expr && MatchKeyword("log10")) {
        expr = parse_system_call("$log10", false);
      } else if (!expr && MatchKeyword("ln")) {
        expr = parse_system_call("$ln", false);
      } else if (!expr && MatchKeyword("exp")) {
        expr = parse_system_call("$exp", false);
      } else if (!expr && MatchKeyword("sqrt")) {
        expr = parse_system_call("$sqrt", false);
      } else if (!expr && MatchKeyword("pow")) {
        expr = parse_system_call("$pow", false);
      } else if (!expr && MatchKeyword("floor")) {
        expr = parse_system_call("$floor", false);
      } else if (!expr && MatchKeyword("ceil")) {
        expr = parse_system_call("$ceil", false);
      } else if (!expr && MatchKeyword("sin")) {
        expr = parse_system_call("$sin", false);
      } else if (!expr && MatchKeyword("cos")) {
        expr = parse_system_call("$cos", false);
      } else if (!expr && MatchKeyword("tan")) {
        expr = parse_system_call("$tan", false);
      } else if (!expr && MatchKeyword("asin")) {
        expr = parse_system_call("$asin", false);
      } else if (!expr && MatchKeyword("acos")) {
        expr = parse_system_call("$acos", false);
      } else if (!expr && MatchKeyword("atan")) {
        expr = parse_system_call("$atan", false);
      } else if (!expr && MatchKeyword("atan2")) {
        expr = parse_system_call("$atan2", false);
      } else if (!expr && MatchKeyword("hypot")) {
        expr = parse_system_call("$hypot", false);
      } else if (!expr && MatchKeyword("sinh")) {
        expr = parse_system_call("$sinh", false);
      } else if (!expr && MatchKeyword("cosh")) {
        expr = parse_system_call("$cosh", false);
      } else if (!expr && MatchKeyword("tanh")) {
        expr = parse_system_call("$tanh", false);
      } else if (!expr && MatchKeyword("asinh")) {
        expr = parse_system_call("$asinh", false);
      } else if (!expr && MatchKeyword("acosh")) {
        expr = parse_system_call("$acosh", false);
      } else if (!expr && MatchKeyword("atanh")) {
        expr = parse_system_call("$atanh", false);
      } else if (!expr && MatchKeyword("bits")) {
        expr = parse_system_call("$bits", false);
      } else if (!expr && MatchKeyword("size")) {
        expr = parse_system_call("$size", false);
      } else if (!expr && MatchKeyword("dimensions")) {
        expr = parse_system_call("$dimensions", false);
      } else if (!expr && MatchKeyword("left")) {
        expr = parse_system_call("$left", false);
      } else if (!expr && MatchKeyword("right")) {
        expr = parse_system_call("$right", false);
      } else if (!expr && MatchKeyword("low")) {
        expr = parse_system_call("$low", false);
      } else if (!expr && MatchKeyword("high")) {
        expr = parse_system_call("$high", false);
      } else if (!expr && MatchKeyword("fopen")) {
        expr = parse_system_call("$fopen", false);
      } else if (!expr && MatchKeyword("fgetc")) {
        expr = parse_system_call("$fgetc", false);
      } else if (!expr && MatchKeyword("feof")) {
        expr = parse_system_call("$feof", false);
      } else if (!expr && MatchKeyword("ftell")) {
        expr = parse_system_call("$ftell", false);
      } else if (!expr && MatchKeyword("fseek")) {
        expr = parse_system_call("$fseek", false);
      } else if (!expr && MatchKeyword("ferror")) {
        expr = parse_system_call("$ferror", false);
      } else if (!expr && MatchKeyword("ungetc")) {
        expr = parse_system_call("$ungetc", false);
      } else if (!expr && MatchKeyword("fread")) {
        expr = parse_system_call("$fread", false);
      } else if (!expr && MatchKeyword("fgets")) {
        expr = parse_system_call("$fgets", false);
      } else if (!expr && MatchKeyword("fscanf")) {
        expr = parse_system_call("$fscanf", false);
      } else if (!expr && MatchKeyword("sscanf")) {
        expr = parse_system_call("$sscanf", false);
      } else if (!expr && MatchKeyword("rewind")) {
        expr = parse_system_call("$rewind", false);
      } else if (!expr && MatchKeyword("test")) {
        if (!MatchSymbol("$") || !MatchKeyword("plusargs")) {
          ErrorHere("unsupported system function");
          return nullptr;
        }
        expr = parse_system_call("$test$plusargs", false);
      } else if (!expr && MatchKeyword("value")) {
        if (!MatchSymbol("$") || !MatchKeyword("plusargs")) {
          ErrorHere("unsupported system function");
          return nullptr;
        }
        expr = parse_system_call("$value$plusargs", false);
      } else if (!expr && MatchKeyword("signed")) {
        op = 'S';
      } else if (!expr && MatchKeyword("unsigned")) {
        op = 'U';
      } else if (!expr && MatchKeyword("clog2")) {
        op = 'C';
      } else if (!expr) {
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
          if (TryEvalConstExpr(*expr, &value)) {
            auto folded = MakeNumberExpr(static_cast<uint64_t>(value));
            folded->is_signed = true;
            expr = std::move(folded);
          }
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
      const std::string token = StripUnderscores(Peek().text);
      if (token.find_first_of(".eE") != std::string::npos) {
        bool warn_abbrev = false;
        size_t dot_pos = token.find('.');
        if (dot_pos != std::string::npos) {
          bool has_digit_before = dot_pos > 0 &&
              std::isdigit(static_cast<unsigned char>(token[dot_pos - 1]));
          size_t exp_pos = token.find_first_of("eE", dot_pos + 1);
          size_t frac_end =
              (exp_pos == std::string::npos) ? token.size() : exp_pos;
          bool has_digit_after = false;
          for (size_t i = dot_pos + 1; i < frac_end; ++i) {
            if (std::isdigit(static_cast<unsigned char>(token[i]))) {
              has_digit_after = true;
              break;
            }
          }
          warn_abbrev = (!has_digit_before || !has_digit_after);
        }
        if (warn_abbrev) {
          const Token& tok = Peek();
          diagnostics_->Add(
              Severity::kWarning,
              "abbreviated real literal is implementation-defined",
              SourceLocation{path_, tok.line, tok.column});
        }
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
      FourStateValue repeat_value;
      std::string repeat_error;
      if (!gpga::EvalConstExpr4State(*first, current_params_, &repeat_value,
                                     &repeat_error)) {
        ErrorHere("expected constant replication count");
        return nullptr;
      }
      if (repeat_value.HasXorZ()) {
        ErrorHere("replication count cannot contain x/z");
        return nullptr;
      }
      int width = repeat_value.width > 0 ? repeat_value.width : 64;
      uint64_t bits = repeat_value.value_bits;
      if (width > 0 && width < 64) {
        uint64_t sign_bit = 1ull << (width - 1);
        if (bits & sign_bit) {
          bits |= ~((1ull << width) - 1ull);
        }
      }
      int64_t repeat = static_cast<int64_t>(bits);
      if (repeat < 0 || repeat > 0x7FFFFFFF) {
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
    const bool is_unsized = (size == 0);
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
    std::string normalized = cleaned;
    if (base_char != 'd' && bits_per_digit > 0) {
      const size_t digit_count = normalized.size();
      const uint64_t total_bits =
          static_cast<uint64_t>(digit_count) * bits_per_digit;
      uint64_t padded_size = size;
      if (padded_size == 0) {
        padded_size = std::max<uint64_t>(32u, total_bits);
      }
      if (padded_size > total_bits) {
        char pad_char = '0';
        if (!normalized.empty()) {
          char msb = normalized.front();
          char sign_kind = '0';
          if (msb == 'x' || msb == 'X') {
            sign_kind = 'x';
          } else if (msb == 'z' || msb == 'Z' || msb == '?') {
            sign_kind = 'z';
          } else {
            int digit = -1;
            if (msb >= '0' && msb <= '9') {
              digit = msb - '0';
            } else if (msb >= 'a' && msb <= 'f') {
              digit = 10 + (msb - 'a');
            } else if (msb >= 'A' && msb <= 'F') {
              digit = 10 + (msb - 'A');
            }
            if (digit >= 0) {
              int sign_bit = (digit >> (bits_per_digit - 1)) & 1;
              sign_kind = sign_bit ? '1' : '0';
            }
          }
          if (sign_kind == 'x') {
            pad_char = 'x';
          } else if (sign_kind == 'z') {
            pad_char = 'z';
          } else if (is_signed && sign_kind == '1') {
            if (base_char == 'b') {
              pad_char = '1';
            } else if (base_char == 'o') {
              pad_char = '7';
            } else {
              pad_char = 'f';
            }
          }
        }
        const size_t needed_digits = static_cast<size_t>(
            (padded_size + bits_per_digit - 1) / bits_per_digit);
        if (normalized.size() < needed_digits) {
          normalized.insert(0, needed_digits - normalized.size(), pad_char);
        }
        if (size == 0) {
          size = padded_size;
        }
      }
    }
    bool has_xz = false;
    for (char c : normalized) {
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
      const size_t digit_count = normalized.size();
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
          padded = normalized.substr(digit_count - needed_digits);
        } else {
          padded.assign(needed_digits - digit_count, '0');
          padded += normalized;
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
      const size_t digit_count = normalized.size();
      const int total_bits = static_cast<int>(digit_count) * bits_per_digit;
      for (size_t i = 0; i < digit_count; ++i) {
        char c = normalized[i];
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
    expr->is_signed = is_signed || is_unsized;
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

  bool AddOrUpdatePort(Module* module, const std::string& name, PortDir dir,
                       int width, bool is_signed, bool is_real, bool is_explicit,
                       const std::shared_ptr<Expr>& msb_expr,
                       const std::shared_ptr<Expr>& lsb_expr) {
    for (auto& port : module->ports) {
      if (port.name == name) {
        if (port.is_declared || !is_explicit) {
          ErrorHere("duplicate port declaration for '" + name + "'");
          return false;
        }
        port.dir = dir;
        port.width = width;
        port.is_signed = is_signed;
        port.is_real = is_real;
        port.is_declared = true;
        port.msb_expr = msb_expr;
        port.lsb_expr = lsb_expr;
        return true;
      }
    }
    Port port;
    port.dir = dir;
    port.name = name;
    port.width = width;
    port.is_signed = is_signed;
    port.is_real = is_real;
    port.is_declared = is_explicit;
    port.msb_expr = msb_expr;
    port.lsb_expr = lsb_expr;
    module->ports.push_back(std::move(port));
    return true;
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
    assign.is_implicit = true;
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

  bool ParseKeywordVersion(const std::string& value,
                           KeywordVersion* out) const {
    if (!out) {
      return false;
    }
    if (value == "1364-1995") {
      *out = KeywordVersion::k1364_1995;
      return true;
    }
    if (value == "1364-2001") {
      *out = KeywordVersion::k1364_2001;
      return true;
    }
    if (value == "1364-2005") {
      *out = KeywordVersion::k1364_2005;
      return true;
    }
    return false;
  }

  bool IsReservedIdentifier(const std::string& name) const {
    if (current_keyword_version_ == KeywordVersion::k1364_2005) {
      return name == "uwire";
    }
    return false;
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
        current_keyword_version_ = KeywordVersion::k1364_2005;
        keyword_version_stack_.clear();
        return true;
      case DirectiveKind::kTimescale:
        if (!directive.arg.empty()) {
          current_timescale_ = directive.arg;
          if (current_module_) {
            current_module_->timescale = current_timescale_;
          }
        }
        return true;
      case DirectiveKind::kBeginKeywords: {
        KeywordVersion version = KeywordVersion::k1364_2005;
        if (!ParseKeywordVersion(directive.arg, &version)) {
          diagnostics_->Add(
              Severity::kError,
              "unknown `begin_keywords version '" + directive.arg + "'",
              SourceLocation{path_, directive.line, directive.column});
          return false;
        }
        keyword_version_stack_.push_back(current_keyword_version_);
        current_keyword_version_ = version;
        return true;
      }
      case DirectiveKind::kEndKeywords:
        if (keyword_version_stack_.empty()) {
          diagnostics_->Add(
              Severity::kError, "unmatched `end_keywords directive",
              SourceLocation{path_, directive.line, directive.column});
          return false;
        }
        current_keyword_version_ = keyword_version_stack_.back();
        keyword_version_stack_.pop_back();
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
  std::unordered_set<std::string> current_specparams_;
  GenvarScope current_genvars_;
  Module* current_module_ = nullptr;
  ParseOptions options_;
  std::vector<DirectiveEvent> directives_;
  size_t directive_pos_ = 0;
  NetType default_nettype_ = NetType::kWire;
  bool default_nettype_none_ = false;
  UnconnectedDrive unconnected_drive_ = UnconnectedDrive::kNone;
  std::string current_timescale_ = "1ns";
  KeywordVersion current_keyword_version_ = KeywordVersion::k1364_2005;
  std::vector<KeywordVersion> keyword_version_stack_;
  bool allow_string_literals_ = false;
  std::unordered_map<std::string, GateArrayRange> gate_array_ranges_;
  int generate_id_ = 0;
  int generate_assign_id_ = 0;
  std::unordered_set<int> generate_assign_emitted_;
  int instance_id_ = 0;

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
          uint64_t unknown = expr.x_bits | expr.z_bits;
          uint64_t value = expr.value_bits & ~unknown;
          *out_value = static_cast<int64_t>(value);
          return true;
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
        {
          int64_t repeat = expr.repeat;
          if (expr.repeat_expr) {
            if (!EvalConstExpr(*expr.repeat_expr, &repeat)) {
              return false;
            }
          }
          if (repeat < 0) {
            ErrorHere("repeat count must be >= 0 in constant expression");
            return false;
          }
          uint64_t result = 0;
          int width = 0;
          auto append_bits = [&](int64_t value, int elem_width) {
            if (elem_width <= 0) {
              return;
            }
            if (elem_width > 64) {
              elem_width = 64;
            }
            const uint64_t mask = MaskForWidth64(elem_width);
            const uint64_t bits = static_cast<uint64_t>(value) & mask;
            if (width >= 64) {
              width = 64;
              return;
            }
            if (width + elem_width <= 64) {
              result = (result << elem_width) | bits;
              width += elem_width;
              return;
            }
            int keep = 64 - width;
            if (keep <= 0) {
              width = 64;
              return;
            }
            const uint64_t keep_mask = MaskForWidth64(keep);
            result = (result << keep) | (bits & keep_mask);
            width = 64;
          };
          for (int64_t rep = 0; rep < repeat; ++rep) {
            for (const auto& element : expr.elements) {
              if (!element) {
                continue;
              }
              int64_t element_value = 0;
              if (!EvalConstExpr(*element, &element_value)) {
                return false;
              }
              append_bits(element_value, ConstExprWidth(*element));
            }
          }
          *out_value = static_cast<int64_t>(result);
          return true;
        }
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
          uint64_t unknown = expr.x_bits | expr.z_bits;
          uint64_t value = expr.value_bits & ~unknown;
          *out_value = static_cast<int64_t>(value);
          return true;
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
        {
          int64_t repeat = expr.repeat;
          if (expr.repeat_expr) {
            if (!TryEvalConstExpr(*expr.repeat_expr, &repeat)) {
              return false;
            }
          }
          if (repeat < 0) {
            return false;
          }
          uint64_t result = 0;
          int width = 0;
          auto append_bits = [&](int64_t value, int elem_width) {
            if (elem_width <= 0) {
              return;
            }
            if (elem_width > 64) {
              elem_width = 64;
            }
            const uint64_t mask = MaskForWidth64(elem_width);
            const uint64_t bits = static_cast<uint64_t>(value) & mask;
            if (width >= 64) {
              width = 64;
              return;
            }
            if (width + elem_width <= 64) {
              result = (result << elem_width) | bits;
              width += elem_width;
              return;
            }
            int keep = 64 - width;
            if (keep <= 0) {
              width = 64;
              return;
            }
            const uint64_t keep_mask = MaskForWidth64(keep);
            result = (result << keep) | (bits & keep_mask);
            width = 64;
          };
          for (int64_t rep = 0; rep < repeat; ++rep) {
            for (const auto& element : expr.elements) {
              if (!element) {
                continue;
              }
              int64_t element_value = 0;
              if (!TryEvalConstExpr(*element, &element_value)) {
                return false;
              }
              append_bits(element_value, ConstExprWidth(*element));
            }
          }
          *out_value = static_cast<int64_t>(result);
          return true;
        }
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
    if (Peek(1).kind == TokenKind::kSymbol && Peek(1).text == "(") {
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
        if (Peek().kind == TokenKind::kSymbol && Peek().text == ")") {
          ErrorHere("expected named parameter override ('.PARAM(expr)')");
          return false;
        }
        if (!MatchSymbol(".")) {
          ErrorHere("mixed positional and named parameter overrides are not allowed");
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
        std::unique_ptr<Expr> expr;
        if (!MatchSymbol(")")) {
          expr = ParseExpr();
          if (!expr) {
            return false;
          }
          if (!MatchSymbol(")")) {
            ErrorHere("expected ')' after parameter expression");
            return false;
          }
        }
        instance->param_overrides.push_back(
            ParamOverride{name, std::move(expr)});
        if (MatchSymbol(",")) {
          if (Peek().kind == TokenKind::kSymbol && Peek().text == ".") {
            continue;
          }
          if (Peek().kind == TokenKind::kSymbol && Peek().text == ")") {
            ErrorHere("expected named parameter override ('.PARAM(expr)')");
            return false;
          }
          ErrorHere("mixed positional and named parameter overrides are not allowed");
          return false;
        }
        break;
      }
    } else {
      while (true) {
        if (Peek().kind == TokenKind::kSymbol && Peek().text == ".") {
          ErrorHere("mixed positional and named parameter overrides are not allowed");
          return false;
        }
        if (Peek().kind == TokenKind::kSymbol && Peek().text == ")") {
          ErrorHere("expected parameter override expression");
          return false;
        }
        std::unique_ptr<Expr> expr = ParseExpr();
        if (!expr) {
          return false;
        }
        instance->param_overrides.push_back(ParamOverride{"", std::move(expr)});
        if (MatchSymbol(",")) {
          if (Peek().kind == TokenKind::kSymbol && Peek().text == ".") {
            ErrorHere("mixed positional and named parameter overrides are not allowed");
            return false;
          }
          if (Peek().kind == TokenKind::kSymbol && Peek().text == ")") {
            ErrorHere("expected parameter override expression");
            return false;
          }
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
