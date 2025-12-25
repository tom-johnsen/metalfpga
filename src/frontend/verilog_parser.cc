#include "frontend/verilog_parser.hh"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gpga {

namespace {

enum class TokenKind {
  kIdentifier,
  kNumber,
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
    if (std::isdigit(static_cast<unsigned char>(c))) {
      int token_line = line;
      int token_column = column;
      size_t start = i;
      ++i;
      ++column;
      while (i < text.size() &&
             std::isdigit(static_cast<unsigned char>(text[i]))) {
        ++i;
        ++column;
      }
      push(TokenKind::kNumber, text.substr(start, i - start), token_line,
           token_column);
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

class Parser {
 public:
  Parser(std::string path, std::vector<Token> tokens, Diagnostics* diagnostics,
         const ParseOptions& options)
      : path_(std::move(path)),
        tokens_(std::move(tokens)),
        diagnostics_(diagnostics),
        options_(options) {}

  bool ParseProgram(Program* out_program) {
    while (!IsAtEnd()) {
      if (MatchKeyword("module")) {
        if (!ParseModule(out_program)) {
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
    current_params_.clear();

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
      if (MatchKeyword("endmodule")) {
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
      if (MatchKeyword("wire")) {
        if (!ParseWireDecl(&module)) {
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
      if (MatchKeyword("initial")) {
        if (!ParseInitial(&module)) {
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
    return false;
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
    std::shared_ptr<Expr> current_msb;
    std::shared_ptr<Expr> current_lsb;
    while (true) {
      PortDir dir = current_dir;
      int width = current_width;
      bool is_reg = current_is_reg;
      bool is_signed = current_is_signed;
      std::shared_ptr<Expr> range_msb = current_msb;
      std::shared_ptr<Expr> range_lsb = current_lsb;
      if (MatchKeyword("input")) {
        dir = PortDir::kInput;
        width = 1;
        is_reg = false;
        is_signed = false;
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchKeyword("wire")) {
          is_reg = false;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        bool had_range = false;
        if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
          return false;
        }
        if (!had_range) {
          range_msb.reset();
          range_lsb.reset();
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
        current_is_signed = is_signed;
        current_msb = had_range ? range_msb : std::shared_ptr<Expr>();
        current_lsb = had_range ? range_lsb : std::shared_ptr<Expr>();
      } else if (MatchKeyword("output")) {
        dir = PortDir::kOutput;
        width = 1;
        is_reg = false;
        is_signed = false;
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchKeyword("reg")) {
          is_reg = true;
        } else if (MatchKeyword("wire")) {
          is_reg = false;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        bool had_range = false;
        if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
          return false;
        }
        if (!had_range) {
          range_msb.reset();
          range_lsb.reset();
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
        current_is_signed = is_signed;
        current_msb = had_range ? range_msb : std::shared_ptr<Expr>();
        current_lsb = had_range ? range_lsb : std::shared_ptr<Expr>();
      } else if (MatchKeyword("inout")) {
        dir = PortDir::kInout;
        width = 1;
        is_reg = false;
        is_signed = false;
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        if (MatchKeyword("wire")) {
          is_reg = false;
        }
        if (MatchKeyword("signed")) {
          is_signed = true;
        }
        bool had_range = false;
        if (!ParseRange(&width, &range_msb, &range_lsb, &had_range)) {
          return false;
        }
        if (!had_range) {
          range_msb.reset();
          range_lsb.reset();
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
        current_is_signed = is_signed;
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
      AddOrUpdatePort(module, name, dir, width, is_signed, range_msb,
                      range_lsb);
      if (dir == PortDir::kOutput && is_reg) {
        AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                       range_msb, range_lsb, 0, std::shared_ptr<Expr>(),
                       std::shared_ptr<Expr>());
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
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    if (dir == PortDir::kOutput) {
      if (MatchKeyword("reg")) {
        is_reg = true;
      } else if (MatchKeyword("wire")) {
        is_reg = false;
      }
    } else {
      if (MatchKeyword("wire")) {
        is_reg = false;
      }
    }
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
        ErrorHere("expected identifier in declaration");
        return false;
      }
      AddOrUpdatePort(module, name, dir, width, is_signed, range_msb,
                      range_lsb);
      if (dir == PortDir::kOutput && is_reg) {
        AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                       range_msb, range_lsb, 0, std::shared_ptr<Expr>(),
                       std::shared_ptr<Expr>());
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

  bool ParseWireDecl(Module* module) {
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
        ErrorHere("expected identifier in wire declaration");
        return false;
      }
      std::unique_ptr<Expr> init;
      int array_size = 0;
      std::shared_ptr<Expr> array_msb;
      std::shared_ptr<Expr> array_lsb;
      bool had_array = false;
      if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
        return false;
      }
      if (!had_array) {
        array_size = 0;
        array_msb.reset();
        array_lsb.reset();
      }
      if (MatchSymbol("=")) {
        init = ParseExpr();
        if (!init) {
          return false;
        }
      }
      AddOrUpdateNet(module, name, NetType::kWire, width, is_signed, range_msb,
                     range_lsb, array_size, array_msb, array_lsb);
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
        ErrorHere("expected ';' after wire declaration");
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
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in reg declaration");
        return false;
      }
      int array_size = 0;
      std::shared_ptr<Expr> array_msb;
      std::shared_ptr<Expr> array_lsb;
      bool had_array = false;
      if (!ParseRange(&array_size, &array_msb, &array_lsb, &had_array)) {
        return false;
      }
      if (!had_array) {
        array_size = 0;
        array_msb.reset();
        array_lsb.reset();
      }
      AddOrUpdateNet(module, name, NetType::kReg, width, is_signed, range_msb,
                     range_lsb, array_size, array_msb, array_lsb);
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
    if (Peek().kind == TokenKind::kIdentifier &&
        Peek(1).kind == TokenKind::kIdentifier &&
        Peek(2).kind == TokenKind::kSymbol && Peek(2).text == "=") {
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
    std::unique_ptr<Expr> expr;
    int64_t value = 0;
    if (!ParseConstExpr(&expr, &value, "parameter value")) {
      return false;
    }
    Parameter param;
    param.name = name;
    param.value = std::move(expr);
    param.is_local = is_local;
    module->parameters.push_back(std::move(param));
    current_params_[name] = value;
    return true;
  }

  bool ParseIntegerDecl(Module* module) {
    const int width = 32;
    const bool is_signed = true;
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in integer declaration");
        return false;
      }
      AddOrUpdateNet(module, name, NetType::kReg, width, is_signed,
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>(), 0,
                     std::shared_ptr<Expr>(), std::shared_ptr<Expr>());
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

  bool ParseAssign(Module* module) {
    std::string lhs;
    if (!ConsumeIdentifier(&lhs)) {
      ErrorHere("expected identifier after 'assign'");
      return false;
    }
    Assign assign;
    assign.lhs = lhs;
    if (MatchSymbol("[")) {
      std::unique_ptr<Expr> msb_expr = ParseExpr();
      if (!msb_expr) {
        return false;
      }
      if (MatchSymbol(":")) {
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
    module->assigns.push_back(std::move(assign));
    return true;
  }

  bool ParseInitial(Module* module) {
    AlwaysBlock block;
    block.edge = EdgeKind::kInitial;
    block.clock = "initial";
    if (!ParseStatementBody(&block.statements)) {
      return false;
    }
    module->always_blocks.push_back(std::move(block));
    return true;
  }

  bool ParseAlways(Module* module) {
    if (!MatchSymbol("@")) {
      ErrorHere("expected '@' after 'always'");
      return false;
    }
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after '@'");
      return false;
    }
    EdgeKind edge = EdgeKind::kPosedge;
    std::string clock;
    if (MatchSymbol("*")) {
      edge = EdgeKind::kCombinational;
    } else if (MatchKeyword("posedge")) {
      edge = EdgeKind::kPosedge;
    } else if (MatchKeyword("negedge")) {
      edge = EdgeKind::kNegedge;
    } else {
      ErrorHere("expected 'posedge', 'negedge', or '*' in sensitivity list");
      return false;
    }
    if (edge != EdgeKind::kCombinational) {
      if (!ConsumeIdentifier(&clock)) {
        ErrorHere("expected clock identifier after edge");
        return false;
      }
    } else {
      clock = "*";
    }
    if (!MatchSymbol(")")) {
      ErrorHere("expected ')' after sensitivity list");
      return false;
    }

    AlwaysBlock block;
    block.edge = edge;
    block.clock = std::move(clock);

    if (!ParseStatementBody(&block.statements)) {
      return false;
    }

    module->always_blocks.push_back(std::move(block));
    return true;
  }

  bool ParseStatementBody(std::vector<Statement>* out_statements) {
    if (MatchKeyword("begin")) {
      while (true) {
        if (MatchKeyword("end")) {
          break;
        }
        Statement stmt;
        if (!ParseStatement(&stmt)) {
          return false;
        }
        out_statements->push_back(std::move(stmt));
      }
      return true;
    }
    Statement stmt;
    if (!ParseStatement(&stmt)) {
      return false;
    }
    out_statements->push_back(std::move(stmt));
    return true;
  }

  bool ParseStatement(Statement* out_statement) {
    if (MatchKeyword("if")) {
      return ParseIfStatement(out_statement);
    }
    if (MatchKeyword("for")) {
      return ParseForStatement(out_statement);
    }
    if (MatchKeyword("while")) {
      return ParseWhileStatement(out_statement);
    }
    if (MatchKeyword("repeat")) {
      return ParseRepeatStatement(out_statement);
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
    return ParseSequentialAssign(out_statement);
  }

  bool ParseForStatement(Statement* out_statement) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'for'");
      return false;
    }
    std::string init_lhs;
    if (!ConsumeIdentifier(&init_lhs)) {
      ErrorHere("expected loop variable in for init");
      return false;
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
    while (true) {
      if (MatchKeyword("end")) {
        break;
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
    std::string lhs;
    if (!ConsumeIdentifier(&lhs)) {
      ErrorHere("expected identifier in sequential assignment");
      return false;
    }
    std::unique_ptr<Expr> lhs_index;
    if (MatchSymbol("[")) {
      lhs_index = ParseExpr();
      if (!lhs_index) {
        return false;
      }
      if (MatchSymbol(":")) {
        ErrorHere("part-select assignment target not supported in v0");
        return false;
      }
      if (!MatchSymbol("]")) {
        ErrorHere("expected ']' after assignment target");
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
    stmt.assign = SequentialAssign{lhs, std::move(lhs_index), std::move(rhs),
                                   nonblocking};
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
    module->instances.push_back(std::move(instance));
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
    return ParsePrimary();
  }

  std::unique_ptr<Expr> ParsePrimary() {
    std::unique_ptr<Expr> expr;
    if (MatchSymbol("$")) {
      char op = 0;
      if (MatchKeyword("signed")) {
        op = 'S';
      } else if (MatchKeyword("unsigned")) {
        op = 'U';
      } else {
        ErrorHere("unsupported system function");
        return nullptr;
      }
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
      uint64_t size = std::stoull(Peek().text);
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
    } else if (Peek().kind == TokenKind::kIdentifier) {
      std::string name = Peek().text;
      Advance();
      expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kIdentifier;
      expr->ident = std::move(name);
    }
    if (!expr) {
      ErrorHere("expected expression");
      return nullptr;
    }
    while (MatchSymbol("[")) {
      if (expr->kind != ExprKind::kIdentifier) {
        ErrorHere("bit/part select requires identifier");
        return nullptr;
      }
      std::unique_ptr<Expr> msb_expr = ParseExpr();
      if (!msb_expr) {
        return nullptr;
      }
      if (MatchSymbol(":")) {
        std::unique_ptr<Expr> lsb_expr = ParseExpr();
        if (!lsb_expr) {
          return nullptr;
        }
        int64_t msb = 0;
        int64_t lsb = 0;
        if (!EvalConstExpr(*msb_expr, &msb) ||
            !EvalConstExpr(*lsb_expr, &lsb)) {
          ErrorHere("part select requires constant expressions");
          return nullptr;
        }
        if (!MatchSymbol("]")) {
          ErrorHere("expected ']' after part select");
          return nullptr;
        }
        auto select = std::make_unique<Expr>();
        select->kind = ExprKind::kSelect;
        select->base = std::move(expr);
        select->msb = static_cast<int>(msb);
        select->lsb = static_cast<int>(lsb);
        select->has_range = true;
        select->msb_expr = std::move(msb_expr);
        select->lsb_expr = std::move(lsb_expr);
        expr = std::move(select);
        continue;
      }
      if (!MatchSymbol("]")) {
        ErrorHere("expected ']' after bit select");
        return nullptr;
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
                       int width, bool is_signed,
                       const std::shared_ptr<Expr>& msb_expr,
                       const std::shared_ptr<Expr>& lsb_expr) {
    for (auto& port : module->ports) {
      if (port.name == name) {
        port.dir = dir;
        port.width = width;
        port.is_signed = is_signed;
        port.msb_expr = msb_expr;
        port.lsb_expr = lsb_expr;
        return;
      }
    }
    module->ports.push_back(
        Port{dir, name, width, is_signed, msb_expr, lsb_expr});
  }

  void AddOrUpdateNet(Module* module, const std::string& name, NetType type,
                      int width, bool is_signed,
                      const std::shared_ptr<Expr>& msb_expr,
                      const std::shared_ptr<Expr>& lsb_expr, int array_size,
                      const std::shared_ptr<Expr>& array_msb_expr,
                      const std::shared_ptr<Expr>& array_lsb_expr) {
    for (auto& net : module->nets) {
      if (net.name == name) {
        net.type = type;
        net.width = width;
        net.is_signed = is_signed;
        net.msb_expr = msb_expr;
        net.lsb_expr = lsb_expr;
        net.array_size = array_size;
        net.array_msb_expr = array_msb_expr;
        net.array_lsb_expr = array_lsb_expr;
        return;
      }
    }
    Net net;
    net.type = type;
    net.name = name;
    net.width = width;
    net.is_signed = is_signed;
    net.msb_expr = msb_expr;
    net.lsb_expr = lsb_expr;
    net.array_size = array_size;
    net.array_msb_expr = array_msb_expr;
    net.array_lsb_expr = array_lsb_expr;
    module->nets.push_back(std::move(net));
  }

  void ErrorHere(const std::string& message) {
    const Token& token = Peek();
    diagnostics_->Add(Severity::kError, message,
                      SourceLocation{path_, token.line, token.column});
  }

  std::string path_;
  std::vector<Token> tokens_;
  Diagnostics* diagnostics_ = nullptr;
  size_t pos_ = 0;
  std::unordered_map<std::string, int64_t> current_params_;
  ParseOptions options_;

  bool EvalConstExpr(const Expr& expr, int64_t* out_value) {
    switch (expr.kind) {
      case ExprKind::kNumber:
        if (expr.x_bits != 0 || expr.z_bits != 0) {
          ErrorHere("x/z not allowed in constant expression");
          return false;
        }
        *out_value = static_cast<int64_t>(expr.number);
        return true;
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
      case ExprKind::kConcat:
        ErrorHere("concatenation not allowed in constant expression");
        return false;
    }
    return false;
  }

  bool TryEvalConstExpr(const Expr& expr, int64_t* out_value) const {
    switch (expr.kind) {
      case ExprKind::kNumber:
        if (expr.x_bits != 0 || expr.z_bits != 0) {
          return false;
        }
        *out_value = static_cast<int64_t>(expr.number);
        return true;
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
    return Peek(1).kind == TokenKind::kIdentifier &&
           Peek(2).kind == TokenKind::kSymbol && Peek(2).text == "(";
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
  out_program->modules.clear();

  std::ifstream file(path);
  if (!file) {
    diagnostics->Add(Severity::kError,
                     "failed to open input file",
                     SourceLocation{path});
    return false;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  const std::string text = buffer.str();
  if (text.empty() && !options.allow_empty) {
    diagnostics->Add(Severity::kError,
                     "input file is empty",
                     SourceLocation{path});
    return false;
  }

  Parser parser(path, Tokenize(text), diagnostics, options);
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
