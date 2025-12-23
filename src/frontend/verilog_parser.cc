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
  Parser(std::string path, std::vector<Token> tokens, Diagnostics* diagnostics)
      : path_(std::move(path)),
        tokens_(std::move(tokens)),
        diagnostics_(diagnostics) {}

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
        if (!ParseParameterDecl(&module)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("localparam")) {
        if (!ParseParameterDecl(&module)) {
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
    while (true) {
      PortDir dir = current_dir;
      int width = current_width;
      bool is_reg = current_is_reg;
      if (MatchKeyword("input")) {
        dir = PortDir::kInput;
        width = 1;
        is_reg = false;
        if (MatchKeyword("wire")) {
          is_reg = false;
        }
        if (!ParseRange(&width)) {
          return false;
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
      } else if (MatchKeyword("output")) {
        dir = PortDir::kOutput;
        width = 1;
        is_reg = false;
        if (MatchKeyword("reg")) {
          is_reg = true;
        } else if (MatchKeyword("wire")) {
          is_reg = false;
        }
        if (!ParseRange(&width)) {
          return false;
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
      } else if (MatchKeyword("inout")) {
        dir = PortDir::kInout;
        width = 1;
        is_reg = false;
        if (MatchKeyword("wire")) {
          is_reg = false;
        }
        if (!ParseRange(&width)) {
          return false;
        }
        current_dir = dir;
        current_width = width;
        current_is_reg = is_reg;
      } else {
        ParseRange(&width);
      }
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected port name");
        return false;
      }
      AddOrUpdatePort(module, name, dir, width);
      if (dir == PortDir::kOutput && is_reg) {
        AddOrUpdateNet(module, name, NetType::kReg, width);
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
    int width = 1;
    if (!ParseRange(&width)) {
      return false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in declaration");
        return false;
      }
      AddOrUpdatePort(module, name, dir, width);
      if (dir == PortDir::kOutput && is_reg) {
        AddOrUpdateNet(module, name, NetType::kReg, width);
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
    int width = 1;
    if (!ParseRange(&width)) {
      return false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in wire declaration");
        return false;
      }
      std::unique_ptr<Expr> init;
      if (MatchSymbol("=")) {
        init = ParseExpr();
        if (!init) {
          return false;
        }
      }
      module->nets.push_back(Net{NetType::kWire, name, width});
      if (init) {
        module->assigns.push_back(Assign{name, std::move(init)});
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
    int width = 1;
    if (!ParseRange(&width)) {
      return false;
    }
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in reg declaration");
        return false;
      }
      AddOrUpdateNet(module, name, NetType::kReg, width);
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
      if (!ParseParameterItem(module)) {
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

  bool ParseParameterDecl(Module* module) {
    if (!ParseParameterItem(module)) {
      return false;
    }
    while (MatchSymbol(",")) {
      if (MatchKeyword("parameter")) {
        if (!ParseParameterItem(module)) {
          return false;
        }
      } else {
        if (!ParseParameterItem(module)) {
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

  bool ParseParameterItem(Module* module) {
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
    module->parameters.push_back(Parameter{name, std::move(expr)});
    current_params_[name] = value;
    return true;
  }

  bool ParseAssign(Module* module) {
    std::string lhs;
    if (!ConsumeIdentifier(&lhs)) {
      ErrorHere("expected identifier after 'assign'");
      return false;
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
    module->assigns.push_back(Assign{lhs, std::move(rhs)});
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
    if (MatchKeyword("posedge")) {
      edge = EdgeKind::kPosedge;
    } else if (MatchKeyword("negedge")) {
      edge = EdgeKind::kNegedge;
    } else {
      ErrorHere("expected 'posedge' or 'negedge' in sensitivity list");
      return false;
    }
    std::string clock;
    if (!ConsumeIdentifier(&clock)) {
      ErrorHere("expected clock identifier after edge");
      return false;
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
    if (MatchKeyword("begin")) {
      return ParseBlockStatement(out_statement);
    }
    return ParseSequentialAssign(out_statement);
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

  bool ParseSequentialAssign(Statement* out_statement) {
    std::string lhs;
    if (!ConsumeIdentifier(&lhs)) {
      ErrorHere("expected identifier in sequential assignment");
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
    stmt.assign = SequentialAssign{lhs, std::move(rhs), nonblocking};
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
          std::unique_ptr<Expr> expr = ParseExpr();
          if (!expr) {
            return false;
          }
          if (!MatchSymbol(")")) {
            ErrorHere("expected ')' after port expression");
            return false;
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
          std::unique_ptr<Expr> expr = ParseExpr();
          if (!expr) {
            return false;
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
    if (!MatchSymbol("[")) {
      return true;
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
    return true;
  }

  std::unique_ptr<Expr> ParseExpr() { return ParseConditional(); }

  std::unique_ptr<Expr> ParseConditional() {
    auto condition = ParseEquality();
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
    while (MatchSymbol("|")) {
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
    while (MatchSymbol("&")) {
      auto right = ParseShift();
      left = MakeBinary('&', std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<Expr> ParseShift() {
    auto left = ParseAddSub();
    while (true) {
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
      break;
    }
    return left;
  }

  std::unique_ptr<Expr> ParseUnary() {
    if (MatchSymbol("~")) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::kUnary;
      expr->unary_op = '~';
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
    if (MatchSymbol("{")) {
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
      std::unique_ptr<Expr> msb_expr;
      std::unique_ptr<Expr> lsb_expr;
      int64_t msb = 0;
      int64_t lsb = 0;
      if (!ParseConstExpr(&msb_expr, &msb, "bit select")) {
        return nullptr;
      }
      bool has_range = false;
      if (MatchSymbol(":")) {
        if (!ParseConstExpr(&lsb_expr, &lsb, "part select")) {
          return nullptr;
        }
        has_range = true;
      } else {
        lsb = msb;
      }
      if (!MatchSymbol("]")) {
        ErrorHere("expected ']' after bit select");
        return nullptr;
      }
      auto select = std::make_unique<Expr>();
      select->kind = ExprKind::kSelect;
      select->base = std::move(expr);
      select->msb = static_cast<int>(msb);
      select->lsb = static_cast<int>(lsb);
      select->has_range = has_range;
      expr = std::move(select);
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
    std::string token = Peek().text;
    Advance();
    if (token.empty()) {
      ErrorHere("invalid base literal");
      return nullptr;
    }
    char base_char = static_cast<char>(std::tolower(
        static_cast<unsigned char>(token[0])));
    std::string digits = token.substr(1);
    if (digits.empty() && Peek().kind == TokenKind::kNumber) {
      digits = Peek().text;
      Advance();
    }
    std::string cleaned;
    for (char c : digits) {
      if (c != '_') {
        cleaned.push_back(c);
      }
    }
    int base = 10;
    switch (base_char) {
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
        ErrorHere("unsupported base in literal");
        return nullptr;
    }
    uint64_t value = 0;
    for (char c : cleaned) {
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
      value = value * static_cast<uint64_t>(base) +
              static_cast<uint64_t>(digit);
    }
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kNumber;
    expr->number = value;
    expr->has_base = true;
    expr->base_char = base_char;
    if (size > 0) {
      expr->has_width = true;
      expr->number_width = static_cast<int>(size);
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
                       int width) {
    for (auto& port : module->ports) {
      if (port.name == name) {
        port.dir = dir;
        port.width = width;
        return;
      }
    }
    module->ports.push_back(Port{dir, name, width});
  }

  void AddOrUpdateNet(Module* module, const std::string& name, NetType type,
                      int width) {
    for (auto& net : module->nets) {
      if (net.name == name) {
        net.type = type;
        net.width = width;
        return;
      }
    }
    module->nets.push_back(Net{type, name, width});
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

  bool EvalConstExpr(const Expr& expr, int64_t* out_value) {
    switch (expr.kind) {
      case ExprKind::kNumber:
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
          case '&':
            *out_value = lhs & rhs;
            return true;
          case '|':
            *out_value = lhs | rhs;
            return true;
          case '^':
            *out_value = lhs ^ rhs;
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
      case ExprKind::kConcat:
        ErrorHere("concatenation not allowed in constant expression");
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

  Parser parser(path, Tokenize(text), diagnostics);
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
