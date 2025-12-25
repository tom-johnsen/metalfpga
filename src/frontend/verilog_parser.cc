#include "frontend/verilog_parser.hh"

#include <cctype>
#include <cstdint>
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
  struct GeneratedNetDecl {
    NetType type = NetType::kWire;
    std::string name;
    int width = 1;
    bool is_signed = false;
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
    std::unique_ptr<GenerateBlock> block;
  };

  struct GenerateBlock {
    std::string label;
    std::vector<GenerateItem> items;
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
    current_genvars_.clear();
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
      if (MatchKeyword("endmodule")) {
        current_module_ = nullptr;
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
      if (MatchKeyword("function")) {
        if (!ParseFunction(&module)) {
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
    current_module_ = nullptr;
    return false;
  }

  bool ParseFunction(Module* module) {
    Function func;
    bool is_signed = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    int width = 1;
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    bool had_range = false;
    if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
      return false;
    }
    if (!had_range) {
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
    func.msb_expr = msb_expr;
    func.lsb_expr = lsb_expr;

    bool saw_body = false;
    bool in_block = false;
    while (!IsAtEnd()) {
      if (MatchKeyword("endfunction")) {
        break;
      }
      if (MatchKeyword("input")) {
        if (!ParseFunctionInput(&func)) {
          return false;
        }
        continue;
      }
      if (MatchKeyword("begin")) {
        in_block = true;
        continue;
      }
      if (MatchKeyword("end")) {
        if (!in_block) {
          ErrorHere("unexpected 'end' in function");
          return false;
        }
        in_block = false;
        continue;
      }
      if (Peek().kind == TokenKind::kIdentifier && Peek().text == func.name) {
        Advance();
        if (!MatchSymbol("=")) {
          ErrorHere("expected '=' after function name");
          return false;
        }
        auto rhs = ParseExpr();
        if (!rhs) {
          return false;
        }
        if (!MatchSymbol(";")) {
          ErrorHere("expected ';' after function assignment");
          return false;
        }
        if (saw_body) {
          ErrorHere("multiple assignments to function name in v0");
          return false;
        }
        func.body_expr = std::move(rhs);
        saw_body = true;
        continue;
      }
      ErrorHere("unsupported function item '" + Peek().text + "'");
      return false;
    }

    if (!func.body_expr) {
      ErrorHere("function missing return assignment");
      return false;
    }
    if (in_block) {
      ErrorHere("missing 'end' before endfunction");
      return false;
    }
    module->functions.push_back(std::move(func));
    return true;
  }

  bool ParseFunctionInput(Function* func) {
    bool is_signed = false;
    if (MatchKeyword("signed")) {
      is_signed = true;
    }
    int width = 1;
    std::shared_ptr<Expr> msb_expr;
    std::shared_ptr<Expr> lsb_expr;
    bool had_range = false;
    if (!ParseRange(&width, &msb_expr, &lsb_expr, &had_range)) {
      return false;
    }
    if (!had_range) {
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
                       range_msb, range_lsb, {});
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
                       range_msb, range_lsb, {});
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
        init = ParseExpr();
        if (!init) {
          return false;
        }
      }
      AddOrUpdateNet(module, name, NetType::kWire, width, is_signed, range_msb,
                     range_lsb, array_dims);
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

  bool ParseLocalIntegerDecl() {
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in integer declaration");
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

  bool ParseGenvarDecl(std::unordered_set<std::string>* genvars) {
    while (true) {
      std::string name;
      if (!ConsumeIdentifier(&name)) {
        ErrorHere("expected identifier in genvar declaration");
        return false;
      }
      genvars->insert(name);
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

  std::unique_ptr<Expr> MakeNumberExpr(uint64_t value) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::kNumber;
    expr->number = value;
    expr->value_bits = value;
    return expr;
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
    std::string lhs;
    if (!ConsumeIdentifier(&lhs)) {
      ErrorHere("expected identifier after 'assign'");
      return false;
    }
    GenerateAssign assign;
    assign.lhs = lhs;
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

  bool CloneStatementGenerate(const Statement& statement,
                              const GenerateContext& ctx,
                              Statement* out_statement) {
    out_statement->kind = statement.kind;
    if (statement.kind == StatementKind::kAssign) {
      out_statement->assign.lhs =
          RenameIdent(statement.assign.lhs, ctx.renames);
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
      out_statement->assign.nonblocking = statement.assign.nonblocking;
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
    return true;
  }

  bool CloneAlwaysGenerate(const AlwaysBlock& block,
                           const GenerateContext& ctx,
                           AlwaysBlock* out_block) {
    out_block->edge = block.edge;
    out_block->clock = RenameIdent(block.clock, ctx.renames);
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
                         decl.msb_expr, decl.lsb_expr, decl.array_dims);
          break;
        }
        case GenerateItem::Kind::kAssign: {
          const auto& gen_assign = item.assign;
          Assign assign;
          assign.lhs = RenameIdent(gen_assign.lhs, ctx.renames);
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
          std::string child_prefix = prefix;
          if (!item.block->label.empty()) {
            child_prefix += item.block->label + "__";
          }
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
          std::string base_prefix =
              prefix + "gen" + std::to_string(gen_for.id) + "__";
          if (!gen_for.body->label.empty()) {
            base_prefix += gen_for.body->label + "__";
          }
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
                base_prefix + gen_for.var + std::to_string(current) + "__";
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
          int64_t cond_value = 0;
          if (!EvalConstExprWithContext(*gen_if.condition, ctx, &cond_value)) {
            ErrorHere("generate if condition must be constant");
            return false;
          }
          const GenerateBlock* chosen =
              (cond_value != 0) ? gen_if.then_block.get()
                                : (gen_if.has_else ? gen_if.else_block.get()
                                                   : nullptr);
          if (chosen) {
            std::string child_prefix = prefix;
            if (!chosen->label.empty()) {
              child_prefix += chosen->label + "__";
            }
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

  bool ParseGenerateBlockBody(GenerateBlock* out_block,
                              std::unordered_set<std::string>* genvars) {
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
                        std::unordered_set<std::string>* genvars) {
    if (!MatchSymbol("(")) {
      ErrorHere("expected '(' after 'for'");
      return false;
    }
    std::string var;
    if (!ConsumeIdentifier(&var)) {
      ErrorHere("expected loop variable in generate for");
      return false;
    }
    if (genvars->count(var) == 0) {
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
                       std::unordered_set<std::string>* genvars) {
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

  bool ParseGenerateItem(GenerateBlock* out_block,
                         std::unordered_set<std::string>* genvars) {
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
    if (MatchKeyword("begin")) {
      auto block = std::make_unique<GenerateBlock>();
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
    if (MatchKeyword("wire")) {
      std::vector<GeneratedNetDecl> decls;
      if (!ParseGenerateNetDecl(NetType::kWire, &decls)) {
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

    *out_block = std::move(block);
    return true;
  }

  bool ParseStatementBody(std::vector<Statement>* out_statements) {
    if (MatchKeyword("begin")) {
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
        if (MatchKeyword("reg")) {
          if (!ParseLocalRegDecl(current_module_)) {
            return false;
          }
          continue;
        }
        Statement stmt;
        if (!ParseStatement(&stmt)) {
          return false;
        }
        out_statements->push_back(std::move(stmt));
      }
      return true;
    }
    if (MatchKeyword("integer")) {
      return ParseLocalIntegerDecl();
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
      if (MatchKeyword("integer")) {
        if (!ParseLocalIntegerDecl()) {
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
    std::string lhs;
    if (!ConsumeIdentifier(&lhs)) {
      ErrorHere("expected identifier in sequential assignment");
      return false;
    }
    std::unique_ptr<Expr> lhs_index;
    std::vector<std::unique_ptr<Expr>> lhs_indices;
    while (MatchSymbol("[")) {
      auto index = ParseExpr();
      if (!index) {
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
      lhs_indices.push_back(std::move(index));
    }
    if (lhs_indices.size() == 1) {
      lhs_index = std::move(lhs_indices.front());
      lhs_indices.clear();
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
    stmt.assign.lhs = lhs;
    stmt.assign.lhs_index = std::move(lhs_index);
    stmt.assign.lhs_indices = std::move(lhs_indices);
    stmt.assign.rhs = std::move(rhs);
    stmt.assign.nonblocking = nonblocking;
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
      } else if (MatchKeyword("clog2")) {
        op = 'C';
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
      if (MatchSymbol("(")) {
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
                      const std::shared_ptr<Expr>& lsb_expr,
                      const std::vector<ArrayDim>& array_dims) {
    int array_size = 0;
    if (array_dims.size() == 1) {
      array_size = array_dims.front().size;
    }
    for (auto& net : module->nets) {
      if (net.name == name) {
        net.type = type;
        net.width = width;
        net.is_signed = is_signed;
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
    net.msb_expr = msb_expr;
    net.lsb_expr = lsb_expr;
    net.array_size = array_size;
    net.array_dims = array_dims;
    module->nets.push_back(std::move(net));
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

  std::string path_;
  std::vector<Token> tokens_;
  Diagnostics* diagnostics_ = nullptr;
  size_t pos_ = 0;
  std::unordered_map<std::string, int64_t> current_params_;
  std::unordered_set<std::string> current_genvars_;
  Module* current_module_ = nullptr;
  ParseOptions options_;
  int generate_id_ = 0;

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
      case ExprKind::kCall:
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
      case ExprKind::kCall:
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
