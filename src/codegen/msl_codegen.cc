#include "codegen/msl_codegen.hh"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gpga {

namespace {

const Port* FindPort(const Module& module, const std::string& name) {
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return &port;
    }
  }
  return nullptr;
}

const Function* FindFunction(const Module& module, const std::string& name) {
  for (const auto& func : module.functions) {
    if (func.name == name) {
      return &func;
    }
  }
  return nullptr;
}

int SignalWidth(const Module& module, const std::string& name) {
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

NetType SignalNetType(const Module& module, const std::string& name) {
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return net.type;
    }
  }
  return NetType::kWire;
}

bool IsWireLikeNet(NetType type) { return type != NetType::kReg; }

bool IsTriregNet(NetType type) { return type == NetType::kTrireg; }

bool IsWiredAndNet(NetType type) {
  return type == NetType::kWand || type == NetType::kTriand;
}

bool IsWiredOrNet(NetType type) {
  return type == NetType::kWor || type == NetType::kTrior;
}

bool SignalSigned(const Module& module, const std::string& name) {
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

bool IsArrayNet(const Module& module, const std::string& name,
                int* element_width, int* array_size) {
  for (const auto& net : module.nets) {
    if (net.name == name &&
        (net.array_size > 0 || !net.array_dims.empty())) {
      if (element_width) {
        *element_width = net.width;
      }
      if (array_size) {
        *array_size = net.array_size;
      }
      return true;
    }
  }
  return false;
}

uint64_t MaskForWidth64(int width) {
  if (width >= 64) {
    return 0xFFFFFFFFFFFFFFFFull;
  }
  if (width <= 0) {
    return 0ull;
  }
  return (1ull << width) - 1ull;
}

std::string TypeForWidth(int width) {
  return (width > 32) ? "ulong" : "uint";
}

std::string SignedTypeForWidth(int width) {
  return (width > 32) ? "long" : "int";
}

std::string ZeroForWidth(int width) {
  return (width > 32) ? "0ul" : "0u";
}

std::string CastForWidth(int width) {
  return (width > 32) ? "(ulong)" : "";
}

std::string SignedCastForWidth(int width) {
  return (width > 32) ? "(long)" : "(int)";
}

bool HasOuterParens(const std::string& expr) {
  if (expr.size() < 2 || expr.front() != '(' || expr.back() != ')') {
    return false;
  }
  int depth = 0;
  for (size_t i = 0; i < expr.size(); ++i) {
    char c = expr[i];
    if (c == '(') {
      ++depth;
    } else if (c == ')') {
      --depth;
      if (depth == 0 && i + 1 != expr.size()) {
        return false;
      }
    }
    if (depth < 0) {
      return false;
    }
  }
  return depth == 0;
}

std::string StripOuterParens(std::string expr) {
  while (HasOuterParens(expr)) {
    expr = expr.substr(1, expr.size() - 2);
  }
  return expr;
}

bool ParseUIntLiteral(const std::string& text, uint64_t* value_out) {
  if (!value_out) {
    return false;
  }
  std::string trimmed = StripOuterParens(text);
  if (trimmed.empty()) {
    return false;
  }
  size_t i = 0;
  uint64_t value = 0;
  for (; i < trimmed.size(); ++i) {
    char c = trimmed[i];
    if (c < '0' || c > '9') {
      break;
    }
    uint64_t digit = static_cast<uint64_t>(c - '0');
    if (value > (std::numeric_limits<uint64_t>::max() - digit) / 10ull) {
      return false;
    }
    value = value * 10ull + digit;
  }
  if (i == 0) {
    return false;
  }
  if (i < trimmed.size()) {
    std::string suffix = trimmed.substr(i);
    for (char& c : suffix) {
      c = static_cast<char>(std::tolower(c));
    }
    if (!(suffix == "u" || suffix == "ul")) {
      return false;
    }
  }
  *value_out = value;
  return true;
}

std::string TrimWhitespace(const std::string& text) {
  size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(start, end - start);
}

bool SplitTopLevelBitwiseAnd(const std::string& expr, std::string* lhs_out,
                             std::string* rhs_out) {
  std::string trimmed = StripOuterParens(expr);
  int depth = 0;
  for (size_t i = 0; i < trimmed.size(); ++i) {
    char c = trimmed[i];
    if (c == '(') {
      ++depth;
      continue;
    }
    if (c == ')') {
      --depth;
      continue;
    }
    if (depth != 0 || c != '&') {
      continue;
    }
    if (i + 1 < trimmed.size() && trimmed[i + 1] == '&') {
      continue;
    }
    if (i > 0 && trimmed[i - 1] == '&') {
      continue;
    }
    std::string lhs = TrimWhitespace(trimmed.substr(0, i));
    std::string rhs = TrimWhitespace(trimmed.substr(i + 1));
    if (lhs.empty() || rhs.empty()) {
      continue;
    }
    if (lhs_out) {
      *lhs_out = lhs;
    }
    if (rhs_out) {
      *rhs_out = rhs;
    }
    return true;
  }
  return false;
}

bool IsWidthMaskLiteral(const std::string& expr, int width) {
  if (width >= 64) {
    return false;
  }
  uint64_t mask = MaskForWidth64(width);
  uint64_t value = 0;
  return ParseUIntLiteral(StripOuterParens(expr), &value) && value == mask;
}

bool IsMaskedByWidth(const std::string& expr, int width) {
  if (width >= 64) {
    return false;
  }
  std::string lhs;
  std::string rhs;
  if (!SplitTopLevelBitwiseAnd(expr, &lhs, &rhs)) {
    return false;
  }
  return IsWidthMaskLiteral(lhs, width) || IsWidthMaskLiteral(rhs, width);
}

std::string WrapIfNeeded(const std::string& expr) {
  if (HasOuterParens(expr)) {
    return expr;
  }
  return "(" + expr + ")";
}

bool IsZeroLiteral(const std::string& expr) {
  uint64_t value = 0;
  return ParseUIntLiteral(expr, &value) && value == 0;
}

std::string MaskForWidthExpr(const std::string& expr, int width) {
  if (width >= 64) {
    return expr;
  }
  uint64_t mask = MaskForWidth64(width);
  uint64_t literal = 0;
  std::string stripped = StripOuterParens(expr);
  if (ParseUIntLiteral(stripped, &literal) && (literal & ~mask) == 0) {
    return stripped;
  }
  if (IsMaskedByWidth(expr, width)) {
    return WrapIfNeeded(stripped);
  }
  std::string lhs;
  std::string rhs;
  if (SplitTopLevelBitwiseAnd(expr, &lhs, &rhs)) {
    if (IsWidthMaskLiteral(lhs, width) && IsMaskedByWidth(rhs, width)) {
      return WrapIfNeeded(rhs);
    }
    if (IsWidthMaskLiteral(rhs, width) && IsMaskedByWidth(lhs, width)) {
      return WrapIfNeeded(lhs);
    }
  }
  if (width == 32) {
    return WrapIfNeeded(expr);
  }
  std::string suffix = (width > 32) ? "ul" : "u";
  return "((" + expr + ") & " + std::to_string(mask) + suffix + ")";
}

std::string MaskLiteralForWidth(int width) {
  if (width >= 64) {
    return "0xFFFFFFFFFFFFFFFFul";
  }
  uint64_t mask = MaskForWidth64(width);
  std::string suffix = (width > 32) ? "ul" : "u";
  return std::to_string(mask) + suffix;
}

int StrengthRank(Strength strength) {
  switch (strength) {
    case Strength::kHighZ:
      return 0;
    case Strength::kWeak:
      return 1;
    case Strength::kPull:
      return 2;
    case Strength::kStrong:
      return 3;
    case Strength::kSupply:
      return 4;
  }
  return 0;
}

std::string StrengthLiteral(Strength strength) {
  return std::to_string(StrengthRank(strength)) + "u";
}

std::string ExtendExpr(const std::string& expr, int expr_width,
                       int target_width) {
  std::string masked = MaskForWidthExpr(expr, expr_width);
  if (target_width > 32 && expr_width <= 32) {
    return "(ulong)" + masked;
  }
  if (target_width <= 32 && expr_width > 32) {
    return "(uint)" + masked;
  }
  return masked;
}

std::string SignExtendExpr(const std::string& expr, int expr_width,
                           int target_width) {
  if (expr_width <= 0) {
    return SignedCastForWidth(target_width) + ZeroForWidth(target_width);
  }
  int width = std::max(expr_width, target_width);
  int shift = width - expr_width;
  std::string masked = MaskForWidthExpr(expr, expr_width);
  std::string cast = SignedCastForWidth(width);
  if (shift == 0) {
    return cast + masked;
  }
  std::string widened = cast + masked;
  return "(" + cast + "(" + widened + " << " + std::to_string(shift) + "u) >> " +
         std::to_string(shift) + "u)";
}

bool ExprSigned(const Expr& expr, const Module& module) {
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      return SignalSigned(module, expr.ident);
    case ExprKind::kNumber:
      return expr.is_signed || !expr.has_base;
    case ExprKind::kUnary:
      if (expr.unary_op == 'S') {
        return true;
      }
      if (expr.unary_op == 'U') {
        return false;
      }
      if (expr.unary_op == 'C') {
        return false;
      }
      if (expr.unary_op == '&' || expr.unary_op == '|' ||
          expr.unary_op == '^' || expr.unary_op == '!') {
        return false;
      }
      if (expr.unary_op == '-' && expr.operand &&
          expr.operand->kind == ExprKind::kNumber) {
        return true;
      }
      return expr.operand ? ExprSigned(*expr.operand, module) : false;
    case ExprKind::kBinary: {
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
    case ExprKind::kTernary: {
      bool t_signed =
          expr.then_expr ? ExprSigned(*expr.then_expr, module) : false;
      bool e_signed =
          expr.else_expr ? ExprSigned(*expr.else_expr, module) : false;
      return t_signed && e_signed;
    }
    case ExprKind::kCall: {
      if (expr.ident == "$time") {
        return false;
      }
      const Function* func = FindFunction(module, expr.ident);
      return func ? func->is_signed : false;
    }
    case ExprKind::kSelect:
    case ExprKind::kIndex:
    case ExprKind::kConcat:
      return false;
  }
  return false;
}

void CollectIdentifiers(const Expr& expr,
                        std::unordered_set<std::string>* out) {
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      out->insert(expr.ident);
      return;
    case ExprKind::kNumber:
      return;
    case ExprKind::kUnary:
      if (expr.operand) {
        CollectIdentifiers(*expr.operand, out);
      }
      return;
    case ExprKind::kBinary:
      if (expr.lhs) {
        CollectIdentifiers(*expr.lhs, out);
      }
      if (expr.rhs) {
        CollectIdentifiers(*expr.rhs, out);
      }
      return;
    case ExprKind::kTernary:
      if (expr.condition) {
        CollectIdentifiers(*expr.condition, out);
      }
      if (expr.then_expr) {
        CollectIdentifiers(*expr.then_expr, out);
      }
      if (expr.else_expr) {
        CollectIdentifiers(*expr.else_expr, out);
      }
      return;
    case ExprKind::kSelect:
      if (expr.base) {
        CollectIdentifiers(*expr.base, out);
      }
      if (expr.msb_expr) {
        CollectIdentifiers(*expr.msb_expr, out);
      }
      if (expr.lsb_expr) {
        CollectIdentifiers(*expr.lsb_expr, out);
      }
      return;
    case ExprKind::kIndex:
      if (expr.base) {
        CollectIdentifiers(*expr.base, out);
      }
      if (expr.index) {
        CollectIdentifiers(*expr.index, out);
      }
      return;
    case ExprKind::kCall:
      for (const auto& arg : expr.call_args) {
        CollectIdentifiers(*arg, out);
      }
      return;
    case ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        CollectIdentifiers(*element, out);
      }
      return;
  }
}

std::vector<size_t> OrderAssigns(const Module& module) {
  const size_t count = module.assigns.size();
  std::unordered_map<std::string, std::vector<size_t>> lhs_to_indices;
  lhs_to_indices.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    lhs_to_indices[module.assigns[i].lhs].push_back(i);
  }

  std::vector<int> indegree(count, 0);
  std::vector<std::vector<size_t>> edges(count);
  for (size_t i = 0; i < count; ++i) {
    const auto& assign = module.assigns[i];
    if (!assign.rhs) {
      continue;
    }
    std::unordered_set<std::string> deps;
    CollectIdentifiers(*assign.rhs, &deps);
    for (const auto& dep : deps) {
      if (dep == assign.lhs) {
        continue;
      }
      auto it = lhs_to_indices.find(dep);
      if (it == lhs_to_indices.end()) {
        continue;
      }
      for (size_t producer : it->second) {
        if (producer == i) {
          continue;
        }
        edges[producer].push_back(i);
        indegree[i]++;
      }
    }
  }

  std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>>
      ready;
  for (size_t i = 0; i < count; ++i) {
    if (indegree[i] == 0) {
      ready.push(i);
    }
  }

  std::vector<size_t> ordered;
  ordered.reserve(count);
  while (!ready.empty()) {
    size_t current = ready.top();
    ready.pop();
    ordered.push_back(current);
    for (size_t next : edges[current]) {
      indegree[next]--;
      if (indegree[next] == 0) {
        ready.push(next);
      }
    }
  }

  if (ordered.size() != count) {
    std::vector<bool> seen(count, false);
    for (size_t index : ordered) {
      seen[index] = true;
    }
    for (size_t i = 0; i < count; ++i) {
      if (!seen[i]) {
        ordered.push_back(i);
      }
    }
  }
  return ordered;
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

int ExprWidth(const Expr& expr, const Module& module) {
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      return SignalWidth(module, expr.ident);
    case ExprKind::kNumber:
      if (expr.has_width && expr.number_width > 0) {
        return expr.number_width;
      }
      return MinimalWidth(expr.number);
    case ExprKind::kUnary:
      if (expr.unary_op == '!' || expr.unary_op == '&' ||
          expr.unary_op == '|' || expr.unary_op == '^') {
        return 1;
      }
      if (expr.unary_op == 'C') {
        return 32;
      }
      return expr.operand ? ExprWidth(*expr.operand, module) : 32;
    case ExprKind::kBinary: {
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G' ||
          expr.op == 'A' || expr.op == 'O') {
        return 1;
      }
      if (expr.op == 'l' || expr.op == 'r' || expr.op == 'R') {
        return expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      }
      int lhs = expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      int rhs = expr.rhs ? ExprWidth(*expr.rhs, module) : 32;
      return std::max(lhs, rhs);
    }
    case ExprKind::kTernary: {
      int t = expr.then_expr ? ExprWidth(*expr.then_expr, module) : 32;
      int e = expr.else_expr ? ExprWidth(*expr.else_expr, module) : 32;
      return std::max(t, e);
    }
    case ExprKind::kSelect: {
      if (expr.indexed_range && expr.indexed_width > 0) {
        return expr.indexed_width;
      }
      int lo = std::min(expr.msb, expr.lsb);
      int hi = std::max(expr.msb, expr.lsb);
      return (hi - lo + 1);
    }
    case ExprKind::kIndex: {
      if (expr.base && expr.base->kind == ExprKind::kIdentifier) {
        int element_width = 0;
        if (IsArrayNet(module, expr.base->ident, &element_width, nullptr)) {
          return element_width;
        }
      }
      return 1;
    }
    case ExprKind::kCall: {
      if (expr.ident == "$time") {
        return 64;
      }
      const Function* func = FindFunction(module, expr.ident);
      return func ? func->width : 32;
    }
    case ExprKind::kConcat: {
      int total = 0;
      int base = 0;
      for (const auto& element : expr.elements) {
        base += ExprWidth(*element, module);
      }
      total = base * std::max(1, expr.repeat);
      return total;
    }
  }
  return 32;
}

std::string BinaryOpString(char op) {
  switch (op) {
    case 'E':
      return "==";
    case 'N':
      return "!=";
    case 'L':
      return "<=";
    case 'G':
      return ">=";
    case 'l':
      return "<<";
    case 'r':
      return ">>";
    default:
      return std::string(1, op);
  }
}

std::string EmitExpr(const Expr& expr, const Module& module,
                     const std::unordered_set<std::string>& locals,
                     const std::unordered_set<std::string>& regs);

std::string EmitExprSized(const Expr& expr, int target_width,
                          const Module& module,
                          const std::unordered_set<std::string>& locals,
                          const std::unordered_set<std::string>& regs) {
  std::string raw = EmitExpr(expr, module, locals, regs);
  int expr_width = ExprWidth(expr, module);
  if (expr_width == target_width) {
    return raw;
  }
  if (expr_width < target_width) {
    if (ExprSigned(expr, module)) {
      return MaskForWidthExpr(
          SignExtendExpr(raw, expr_width, target_width), target_width);
    }
    return ExtendExpr(raw, expr_width, target_width);
  }
  return MaskForWidthExpr(raw, target_width);
}

std::string EmitConcatExpr(const Expr& expr, const Module& module,
                           const std::unordered_set<std::string>& locals,
                           const std::unordered_set<std::string>& regs) {
  int element_width = 0;
  for (const auto& element : expr.elements) {
    element_width += ExprWidth(*element, module);
  }
  int total_width = element_width * std::max(1, expr.repeat);
  if (total_width <= 0) {
    return "0u";
  }
  bool wide = total_width > 32;
  if (total_width > 64) {
    return "/*concat_trunc*/0u";
  }
  int shift = total_width;
  std::string acc = wide ? "0ul" : "0u";
  int repeats = std::max(1, expr.repeat);
  for (int r = 0; r < repeats; ++r) {
    for (const auto& element : expr.elements) {
      int width = ExprWidth(*element, module);
      shift -= width;
      if (shift < 0) {
        shift = 0;
      }
      std::string part = EmitExpr(*element, module, locals, regs);
      if (IsZeroLiteral(part)) {
        continue;
      }
      uint64_t mask = MaskForWidth64(width);
      std::string mask_suffix = wide ? "ul" : "u";
      std::string cast = wide ? "(ulong)" : "";
      std::string part_expr = cast + part;
      if (width != 32 && width < 64) {
        part_expr = "(" + part_expr + " & " + std::to_string(mask) +
                    mask_suffix + ")";
      }
      acc = "(" + acc + " | (" + part_expr + " << " +
            std::to_string(shift) + "u))";
    }
  }
  return acc;
}

bool IsOutputPort(const Module& module, const std::string& name) {
  const Port* port = FindPort(module, name);
  return port && (port->dir == PortDir::kOutput || port->dir == PortDir::kInout);
}

void CollectAssignedSignals(const Statement& stmt,
                            std::unordered_set<std::string>* out) {
  if (stmt.kind == StatementKind::kAssign) {
    out->insert(stmt.assign.lhs);
    return;
  }
  if (stmt.kind == StatementKind::kIf) {
    for (const auto& inner : stmt.then_branch) {
      CollectAssignedSignals(inner, out);
    }
    for (const auto& inner : stmt.else_branch) {
      CollectAssignedSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectAssignedSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kDelay) {
    for (const auto& inner : stmt.delay_body) {
      CollectAssignedSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kEventControl) {
    for (const auto& inner : stmt.event_body) {
      CollectAssignedSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kWait) {
    for (const auto& inner : stmt.wait_body) {
      CollectAssignedSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectAssignedSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      CollectAssignedSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kCase) {
    for (const auto& item : stmt.case_items) {
      for (const auto& inner : item.body) {
        CollectAssignedSignals(inner, out);
      }
    }
    for (const auto& inner : stmt.default_branch) {
      CollectAssignedSignals(inner, out);
    }
  }
}

std::unordered_set<std::string> CollectDrivenSignals(const Module& module) {
  std::unordered_set<std::string> driven;
  for (const auto& assign : module.assigns) {
    driven.insert(assign.lhs);
  }
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectAssignedSignals(stmt, &driven);
    }
  }
  return driven;
}

std::string EmitExpr(const Expr& expr, const Module& module,
                     const std::unordered_set<std::string>& locals,
                     const std::unordered_set<std::string>& regs) {
  switch (expr.kind) {
    case ExprKind::kIdentifier: {
      const Port* port = FindPort(module, expr.ident);
      if (port) {
        return port->name + "[gid]";
      }
      if (regs.count(expr.ident) > 0) {
        return expr.ident + "[gid]";
      }
      if (locals.count(expr.ident) > 0) {
        return expr.ident;
      }
      return expr.ident;
    }
    case ExprKind::kNumber:
      if ((expr.has_width && expr.number_width > 32) ||
          expr.number > 0xFFFFFFFFull) {
        std::string literal = std::to_string(expr.number) + "ul";
        if (expr.has_width) {
          return MaskForWidthExpr(literal, expr.number_width);
        }
        return literal;
      }
      {
        std::string literal = std::to_string(expr.number) + "u";
        if (expr.has_width) {
          return MaskForWidthExpr(literal, expr.number_width);
        }
        return literal;
      }
    case ExprKind::kUnary: {
      int width = expr.operand ? ExprWidth(*expr.operand, module) : 32;
      std::string operand =
          expr.operand ? EmitExpr(*expr.operand, module, locals, regs)
                       : ZeroForWidth(width);
      operand = MaskForWidthExpr(operand, width);
      if (expr.unary_op == 'S' || expr.unary_op == 'U') {
        return operand;
      }
      if (expr.unary_op == '&' || expr.unary_op == '|' ||
          expr.unary_op == '^') {
        std::string mask = MaskLiteralForWidth(width);
        if (expr.unary_op == '&') {
          return "((" + operand + " == " + mask + ") ? 1u : 0u)";
        }
        if (expr.unary_op == '|') {
          return "((" + operand + " != 0u) ? 1u : 0u)";
        }
        if (width > 32) {
          std::string lo = "uint(" + operand + ")";
          std::string hi = "uint((" + operand + ") >> 32u)";
          return "((popcount(" + lo + ") + popcount(" + hi + ")) & 1u)";
        }
        return "(popcount(uint(" + operand + ")) & 1u)";
      }
      if (expr.unary_op == '!') {
        std::string zero = ZeroForWidth(width);
        return "((" + operand + " == " + zero + ") ? 1u : 0u)";
      }
      if (expr.unary_op == '+') {
        return operand;
      }
      std::string raw = "(" + std::string(1, expr.unary_op) + operand + ")";
      return MaskForWidthExpr(raw, width);
    }
    case ExprKind::kBinary: {
      std::string lhs = EmitExpr(*expr.lhs, module, locals, regs);
      std::string rhs = EmitExpr(*expr.rhs, module, locals, regs);
      int lhs_width = expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      int rhs_width = expr.rhs ? ExprWidth(*expr.rhs, module) : 32;
      int target_width = std::max(lhs_width, rhs_width);
      bool lhs_signed = expr.lhs ? ExprSigned(*expr.lhs, module) : false;
      bool rhs_signed = expr.rhs ? ExprSigned(*expr.rhs, module) : false;
      bool signed_op = lhs_signed && rhs_signed;
      if (expr.op == 'A' || expr.op == 'O') {
        std::string lhs_masked = MaskForWidthExpr(lhs, lhs_width);
        std::string rhs_masked = MaskForWidthExpr(rhs, rhs_width);
        std::string lhs_zero = ZeroForWidth(lhs_width);
        std::string rhs_zero = ZeroForWidth(rhs_width);
        std::string lhs_bool = "(" + lhs_masked + " != " + lhs_zero + ")";
        std::string rhs_bool = "(" + rhs_masked + " != " + rhs_zero + ")";
        std::string op = (expr.op == 'A') ? "&&" : "||";
        return "((" + lhs_bool + " " + op + " " + rhs_bool + ") ? 1u : 0u)";
      }
      if (expr.op == 'l' || expr.op == 'r' || expr.op == 'R') {
        int width = lhs_width;
        std::string zero = ZeroForWidth(width);
        std::string lhs_masked = MaskForWidthExpr(lhs, width);
        std::string cast = CastForWidth(width);
        std::string op = (expr.op == 'l') ? "<<" : ">>";
        if (expr.op == 'R' && lhs_signed) {
          std::string one = (width > 32) ? "1ul" : "1u";
          std::string sign_bit =
              "((" + lhs_masked + " >> " + std::to_string(width - 1) +
              "u) & " + one + ")";
          std::string fill = "(" + sign_bit + " ? " +
                             MaskLiteralForWidth(width) + " : " + zero + ")";
          std::string signed_lhs = SignExtendExpr(lhs, width, width);
          std::string shifted =
              "(" + signed_lhs + " " + op + " " + rhs + ")";
          return "((" + rhs + ") >= " + std::to_string(width) + "u ? " + fill +
                 " : " + MaskForWidthExpr(shifted, width) + ")";
        }
        return "((" + rhs + ") >= " + std::to_string(width) + "u ? " + zero +
               " : (" + cast + lhs_masked + " " + op + " " + rhs + "))";
      }
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G') {
        std::string lhs_ext = signed_op
                                  ? SignExtendExpr(lhs, lhs_width, target_width)
                                  : ExtendExpr(lhs, lhs_width, target_width);
        std::string rhs_ext = signed_op
                                  ? SignExtendExpr(rhs, rhs_width, target_width)
                                  : ExtendExpr(rhs, rhs_width, target_width);
        return "((" + lhs_ext + " " + BinaryOpString(expr.op) + " " + rhs_ext +
               ") ? 1u : 0u)";
      }
      std::string lhs_ext = signed_op
                                ? SignExtendExpr(lhs, lhs_width, target_width)
                                : ExtendExpr(lhs, lhs_width, target_width);
      std::string rhs_ext = signed_op
                                ? SignExtendExpr(rhs, rhs_width, target_width)
                                : ExtendExpr(rhs, rhs_width, target_width);
      std::string raw =
          "(" + lhs_ext + " " + BinaryOpString(expr.op) + " " + rhs_ext + ")";
      return MaskForWidthExpr(raw, target_width);
    }
    case ExprKind::kTernary: {
      std::string cond = expr.condition
                             ? EmitExpr(*expr.condition, module, locals, regs)
                             : "0u";
      std::string then_expr =
          expr.then_expr ? EmitExpr(*expr.then_expr, module, locals, regs) : "0u";
      std::string else_expr =
          expr.else_expr ? EmitExpr(*expr.else_expr, module, locals, regs) : "0u";
      return "((" + cond + ") ? (" + then_expr + ") : (" + else_expr + "))";
    }
    case ExprKind::kSelect: {
      std::string base = EmitExpr(*expr.base, module, locals, regs);
      if (expr.indexed_range && expr.indexed_width > 0 && expr.lsb_expr) {
        int width = expr.indexed_width;
        int base_width = ExprWidth(*expr.base, module);
        std::string shift =
            EmitExpr(*expr.lsb_expr, module, locals, regs);
        std::string shift_val = "uint(" + shift + ")";
        std::string shifted = "(" + base + " >> " + shift_val + ")";
        std::string masked = MaskForWidthExpr(shifted, width);
        std::string zero = ZeroForWidth(width);
        return "((" + shift_val + ") >= " + std::to_string(base_width) +
               "u ? " + zero + " : " + masked + ")";
      }
      int lo = std::min(expr.msb, expr.lsb);
      int hi = std::max(expr.msb, expr.lsb);
      int width = hi - lo + 1;
      int base_width = ExprWidth(*expr.base, module);
      if (width == 32) {
        std::string shifted =
            "(" + base + " >> " + std::to_string(lo) + "u)";
        if (base_width > 32) {
          return "uint" + shifted;
        }
        return shifted;
      }
      bool wide = base_width > 32 || width > 32;
      uint64_t mask = MaskForWidth64(width);
      std::string mask_suffix = wide ? "ul" : "u";
      return "((" + base + " >> " + std::to_string(lo) + "u) & " +
             std::to_string(mask) + mask_suffix + ")";
    }
    case ExprKind::kIndex: {
      if (!expr.base || !expr.index) {
        return "0u";
      }
      if (expr.base->kind == ExprKind::kIdentifier) {
        int element_width = 0;
        int array_size = 0;
        if (IsArrayNet(module, expr.base->ident, &element_width,
                       &array_size)) {
          std::string index =
              expr.index ? EmitExpr(*expr.index, module, locals, regs) : "0u";
          std::string idx = "uint(" + index + ")";
          std::string base =
              "((gid * " + std::to_string(array_size) + "u) + " + idx + ")";
          std::string bounds =
              "(" + idx + " < " + std::to_string(array_size) + "u)";
          return "((" + bounds + ") ? " + expr.base->ident + "[" + base +
                 "] : " + ZeroForWidth(element_width) + ")";
        }
      }
      std::string base = EmitExpr(*expr.base, module, locals, regs);
      std::string index = EmitExpr(*expr.index, module, locals, regs);
      int base_width = ExprWidth(*expr.base, module);
      std::string one = (base_width > 32) ? "1ul" : "1u";
      std::string cast = CastForWidth(base_width);
      std::string masked = MaskForWidthExpr(base, base_width);
      return "((" + cast + masked + " >> " + index + ") & " + one + ")";
    }
    case ExprKind::kCall:
      if (expr.ident == "$time") {
        return "0ul";
      }
      return "/*function_call*/0u";
    case ExprKind::kConcat:
      return EmitConcatExpr(expr, module, locals, regs);
  }
  return "0u";
}

struct LvalueInfo {
  std::string expr;
  std::string guard;
  int width = 0;
  bool ok = false;
  bool is_array = false;
};

LvalueInfo BuildLvalue(const SequentialAssign& assign, const Module& module,
                       const std::unordered_set<std::string>& locals,
                       const std::unordered_set<std::string>& regs,
                       bool use_next) {
  LvalueInfo out;
  if (assign.lhs_index) {
    int element_width = 0;
    int array_size = 0;
    if (!IsArrayNet(module, assign.lhs, &element_width, &array_size)) {
      return out;
    }
    std::string index =
        EmitExpr(*assign.lhs_index, module, locals, regs);
    std::string idx = "uint(" + index + ")";
    std::string base =
        "((gid * " + std::to_string(array_size) + "u) + " + idx + ")";
    std::string target = assign.lhs;
    if (use_next) {
      target += "_next";
    }
    out.expr = target + "[" + base + "]";
    out.guard = "(" + idx + " < " + std::to_string(array_size) + "u)";
    out.width = element_width;
    out.ok = true;
    out.is_array = true;
    return out;
  }
  if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
    out.expr = assign.lhs + "[gid]";
  } else if (locals.count(assign.lhs) > 0) {
    out.expr = assign.lhs;
  } else {
    return out;
  }
  out.width = SignalWidth(module, assign.lhs);
  out.ok = true;
  return out;
}

}  // namespace

std::string EmitMSLStub(const Module& module, bool four_state) {
  std::ostringstream out;
  out << "#include <metal_stdlib>\n";
  out << "using namespace metal;\n\n";
  out << "struct GpgaParams { uint count; };\n\n";
  out << "// Placeholder MSL emitted by GPGA.\n\n";
  if (four_state) {
    out << "struct FourState32 { uint val; uint xz; };\n";
    out << "struct FourState64 { ulong val; ulong xz; };\n";
    out << "inline uint fs_mask32(uint width) {\n";
    out << "  return (width >= 32u) ? 0xFFFFFFFFu : ((1u << width) - 1u);\n";
    out << "}\n";
    out << "inline ulong fs_mask64(uint width) {\n";
    out << "  return (width >= 64u) ? 0xFFFFFFFFFFFFFFFFul : ((1ul << width) - 1ul);\n";
    out << "}\n";
    out << "inline FourState32 fs_make32(uint val, uint xz, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  FourState32 out = {val & mask, xz & mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_make64(ulong val, ulong xz, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  FourState64 out = {val & mask, xz & mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_allx32(uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  FourState32 out = {0u, mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_allx64(uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  FourState64 out = {0ul, mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_resize32(FourState32 a, uint width) {\n";
    out << "  return fs_make32(a.val, a.xz, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_resize64(FourState64 a, uint width) {\n";
    out << "  return fs_make64(a.val, a.xz, width);\n";
    out << "}\n";
    out << "inline FourState32 fs_sext32(FourState32 a, uint src_width, uint target_width) {\n";
    out << "  if (target_width == 0u || src_width == 0u) return fs_make32(0u, 0u, target_width);\n";
    out << "  if (target_width <= src_width) return fs_make32(a.val, a.xz, target_width);\n";
    out << "  uint src_mask = fs_mask32(src_width);\n";
    out << "  uint tgt_mask = fs_mask32(target_width);\n";
    out << "  uint val = a.val & src_mask;\n";
    out << "  uint xz = a.xz & src_mask;\n";
    out << "  uint sign_mask = 1u << (src_width - 1u);\n";
    out << "  uint sign_xz = xz & sign_mask;\n";
    out << "  uint sign_val = val & sign_mask;\n";
    out << "  uint ext_mask = tgt_mask & ~src_mask;\n";
    out << "  uint ext_val = sign_val ? ext_mask : 0u;\n";
    out << "  uint ext_xz = sign_xz ? ext_mask : 0u;\n";
    out << "  return fs_make32(val | ext_val, xz | ext_xz, target_width);\n";
    out << "}\n";
    out << "inline FourState64 fs_sext64(FourState64 a, uint src_width, uint target_width) {\n";
    out << "  if (target_width == 0u || src_width == 0u) return fs_make64(0ul, 0ul, target_width);\n";
    out << "  if (target_width <= src_width) return fs_make64(a.val, a.xz, target_width);\n";
    out << "  ulong src_mask = fs_mask64(src_width);\n";
    out << "  ulong tgt_mask = fs_mask64(target_width);\n";
    out << "  ulong val = a.val & src_mask;\n";
    out << "  ulong xz = a.xz & src_mask;\n";
    out << "  ulong sign_mask = 1ul << (src_width - 1u);\n";
    out << "  ulong sign_xz = xz & sign_mask;\n";
    out << "  ulong sign_val = val & sign_mask;\n";
    out << "  ulong ext_mask = tgt_mask & ~src_mask;\n";
    out << "  ulong ext_val = sign_val ? ext_mask : 0ul;\n";
    out << "  ulong ext_xz = sign_xz ? ext_mask : 0ul;\n";
    out << "  return fs_make64(val | ext_val, xz | ext_xz, target_width);\n";
    out << "}\n";
    out << "inline FourState32 fs_merge32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint bx = b.xz & mask;\n";
    out << "  uint ak = (~ax) & mask;\n";
    out << "  uint bk = (~bx) & mask;\n";
    out << "  uint same = ~(a.val ^ b.val) & ak & bk & mask;\n";
    out << "  FourState32 out = {a.val & same, mask & ~same};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_merge64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong bx = b.xz & mask;\n";
    out << "  ulong ak = (~ax) & mask;\n";
    out << "  ulong bk = (~bx) & mask;\n";
    out << "  ulong same = ~(a.val ^ b.val) & ak & bk & mask;\n";
    out << "  FourState64 out = {a.val & same, mask & ~same};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_not32(FourState32 a, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  FourState32 out = {(~a.val) & mask, a.xz & mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_not64(FourState64 a, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  FourState64 out = {(~a.val) & mask, a.xz & mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_and32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint bx = b.xz & mask;\n";
    out << "  uint a0 = (~a.val) & ~ax & mask;\n";
    out << "  uint b0 = (~b.val) & ~bx & mask;\n";
    out << "  uint a1 = a.val & ~ax & mask;\n";
    out << "  uint b1 = b.val & ~bx & mask;\n";
    out << "  uint known0 = a0 | b0;\n";
    out << "  uint known1 = a1 & b1;\n";
    out << "  uint unknown = mask & ~(known0 | known1);\n";
    out << "  FourState32 out = {known1, unknown};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_and64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong bx = b.xz & mask;\n";
    out << "  ulong a0 = (~a.val) & ~ax & mask;\n";
    out << "  ulong b0 = (~b.val) & ~bx & mask;\n";
    out << "  ulong a1 = a.val & ~ax & mask;\n";
    out << "  ulong b1 = b.val & ~bx & mask;\n";
    out << "  ulong known0 = a0 | b0;\n";
    out << "  ulong known1 = a1 & b1;\n";
    out << "  ulong unknown = mask & ~(known0 | known1);\n";
    out << "  FourState64 out = {known1, unknown};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_or32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint bx = b.xz & mask;\n";
    out << "  uint a0 = (~a.val) & ~ax & mask;\n";
    out << "  uint b0 = (~b.val) & ~bx & mask;\n";
    out << "  uint a1 = a.val & ~ax & mask;\n";
    out << "  uint b1 = b.val & ~bx & mask;\n";
    out << "  uint known1 = a1 | b1;\n";
    out << "  uint known0 = a0 & b0;\n";
    out << "  uint unknown = mask & ~(known0 | known1);\n";
    out << "  FourState32 out = {known1, unknown};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_or64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong bx = b.xz & mask;\n";
    out << "  ulong a0 = (~a.val) & ~ax & mask;\n";
    out << "  ulong b0 = (~b.val) & ~bx & mask;\n";
    out << "  ulong a1 = a.val & ~ax & mask;\n";
    out << "  ulong b1 = b.val & ~bx & mask;\n";
    out << "  ulong known1 = a1 | b1;\n";
    out << "  ulong known0 = a0 & b0;\n";
    out << "  ulong unknown = mask & ~(known0 | known1);\n";
    out << "  FourState64 out = {known1, unknown};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_xor32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint unknown = (a.xz | b.xz) & mask;\n";
    out << "  FourState32 out = {(a.val ^ b.val) & ~unknown & mask, unknown};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_xor64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong unknown = (a.xz | b.xz) & mask;\n";
    out << "  FourState64 out = {(a.val ^ b.val) & ~unknown & mask, unknown};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_add32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(width);\n";
    out << "  return fs_make32(a.val + b.val, 0u, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_add64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);\n";
    out << "  return fs_make64(a.val + b.val, 0ul, width);\n";
    out << "}\n";
    out << "inline FourState32 fs_sub32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(width);\n";
    out << "  return fs_make32(a.val - b.val, 0u, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_sub64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);\n";
    out << "  return fs_make64(a.val - b.val, 0ul, width);\n";
    out << "}\n";
    out << "inline FourState32 fs_mul32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(width);\n";
    out << "  return fs_make32(a.val * b.val, 0u, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_mul64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);\n";
    out << "  return fs_make64(a.val * b.val, 0ul, width);\n";
    out << "}\n";
    out << "inline FourState32 fs_div32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u || b.val == 0u) return fs_allx32(width);\n";
    out << "  return fs_make32(a.val / b.val, 0u, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_div64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul || b.val == 0ul) return fs_allx64(width);\n";
    out << "  return fs_make64(a.val / b.val, 0ul, width);\n";
    out << "}\n";
    out << "inline FourState32 fs_mod32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u || b.val == 0u) return fs_allx32(width);\n";
    out << "  return fs_make32(a.val % b.val, 0u, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_mod64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul || b.val == 0ul) return fs_allx64(width);\n";
    out << "  return fs_make64(a.val % b.val, 0ul, width);\n";
    out << "}\n";
    out << "inline FourState32 fs_cmp32(uint value, bool pred) {\n";
    out << "  FourState32 out = {pred ? 1u : 0u, 0u};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_cmp64(ulong value, bool pred) {\n";
    out << "  FourState64 out = {pred ? 1ul : 0ul, 0ul};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_eq32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  return fs_make32((a.val == b.val) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_eq64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  return fs_make64((a.val == b.val) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_ne32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  return fs_make32((a.val != b.val) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_ne64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  return fs_make64((a.val != b.val) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_lt32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  return fs_make32((a.val < b.val) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_lt64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  return fs_make64((a.val < b.val) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_gt32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  return fs_make32((a.val > b.val) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_gt64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  return fs_make64((a.val > b.val) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_le32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  return fs_make32((a.val <= b.val) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_le64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  return fs_make64((a.val <= b.val) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_ge32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  return fs_make32((a.val >= b.val) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_ge64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  return fs_make64((a.val >= b.val) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_shl32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if (b.xz != 0u) return fs_allx32(width);\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint shift = b.val;\n";
    out << "  if (shift >= width) return fs_make32(0u, 0u, width);\n";
    out << "  FourState32 out = {(a.val << shift) & mask, (a.xz << shift) & mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_shl64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if (b.xz != 0ul) return fs_allx64(width);\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong shift = b.val;\n";
    out << "  if (shift >= width) return fs_make64(0ul, 0ul, width);\n";
    out << "  FourState64 out = {(a.val << shift) & mask, (a.xz << shift) & mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_shr32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  if (b.xz != 0u) return fs_allx32(width);\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint shift = b.val;\n";
    out << "  if (shift >= width) return fs_make32(0u, 0u, width);\n";
    out << "  FourState32 out = {(a.val >> shift) & mask, (a.xz >> shift) & mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState64 fs_shr64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  if (b.xz != 0ul) return fs_allx64(width);\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong shift = b.val;\n";
    out << "  if (shift >= width) return fs_make64(0ul, 0ul, width);\n";
    out << "  FourState64 out = {(a.val >> shift) & mask, (a.xz >> shift) & mask};\n";
    out << "  return out;\n";
    out << "}\n";
    out << "inline FourState32 fs_mux32(FourState32 cond, FourState32 t, FourState32 f, uint width) {\n";
    out << "  if (cond.xz != 0u) return fs_merge32(t, f, width);\n";
    out << "  return (cond.val != 0u) ? fs_resize32(t, width) : fs_resize32(f, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_mux64(FourState64 cond, FourState64 t, FourState64 f, uint width) {\n";
    out << "  if (cond.xz != 0ul) return fs_merge64(t, f, width);\n";
    out << "  return (cond.val != 0ul) ? fs_resize64(t, width) : fs_resize64(f, width);\n";
    out << "}\n\n";
    out << "inline FourState32 fs_red_and32(FourState32 a, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint a0 = (~a.val) & ~ax & mask;\n";
    out << "  uint a1 = a.val & ~ax & mask;\n";
    out << "  if (a0 != 0u) return fs_make32(0u, 0u, 1u);\n";
    out << "  if (a1 == mask) return fs_make32(1u, 0u, 1u);\n";
    out << "  return fs_allx32(1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_red_and64(FourState64 a, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong a0 = (~a.val) & ~ax & mask;\n";
    out << "  ulong a1 = a.val & ~ax & mask;\n";
    out << "  if (a0 != 0ul) return fs_make64(0ul, 0ul, 1u);\n";
    out << "  if (a1 == mask) return fs_make64(1ul, 0ul, 1u);\n";
    out << "  return fs_allx64(1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_red_or32(FourState32 a, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint a0 = (~a.val) & ~ax & mask;\n";
    out << "  uint a1 = a.val & ~ax & mask;\n";
    out << "  if (a1 != 0u) return fs_make32(1u, 0u, 1u);\n";
    out << "  if (a0 == mask) return fs_make32(0u, 0u, 1u);\n";
    out << "  return fs_allx32(1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_red_or64(FourState64 a, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong a0 = (~a.val) & ~ax & mask;\n";
    out << "  ulong a1 = a.val & ~ax & mask;\n";
    out << "  if (a1 != 0ul) return fs_make64(1ul, 0ul, 1u);\n";
    out << "  if (a0 == mask) return fs_make64(0ul, 0ul, 1u);\n";
    out << "  return fs_allx64(1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_red_xor32(FourState32 a, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  if ((a.xz & mask) != 0u) return fs_allx32(1u);\n";
    out << "  uint parity = popcount(a.val & mask) & 1u;\n";
    out << "  return fs_make32(parity, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_red_xor64(FourState64 a, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  if ((a.xz & mask) != 0ul) return fs_allx64(1u);\n";
    out << "  ulong val = a.val & mask;\n";
    out << "  uint lo = uint(val);\n";
    out << "  uint hi = uint(val >> 32u);\n";
    out << "  uint parity = (popcount(lo) + popcount(hi)) & 1u;\n";
    out << "  return fs_make64(ulong(parity), 0ul, 1u);\n";
    out << "}\n\n";
    out << "inline int fs_sign32(uint val, uint width) {\n";
    out << "  if (width >= 32u) return int(val);\n";
    out << "  uint shift = 32u - width;\n";
    out << "  return int(val << shift) >> shift;\n";
    out << "}\n";
    out << "inline long fs_sign64(ulong val, uint width) {\n";
    out << "  if (width >= 64u) return long(val);\n";
    out << "  uint shift = 64u - width;\n";
    out << "  return long(val << shift) >> shift;\n";
    out << "}\n";
    out << "inline FourState32 fs_slt32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  int sa = fs_sign32(a.val & mask, width);\n";
    out << "  int sb = fs_sign32(b.val & mask, width);\n";
    out << "  return fs_make32((sa < sb) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_slt64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  long sa = fs_sign64(a.val & mask, width);\n";
    out << "  long sb = fs_sign64(b.val & mask, width);\n";
    out << "  return fs_make64((sa < sb) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_sle32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  int sa = fs_sign32(a.val & mask, width);\n";
    out << "  int sb = fs_sign32(b.val & mask, width);\n";
    out << "  return fs_make32((sa <= sb) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_sle64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  long sa = fs_sign64(a.val & mask, width);\n";
    out << "  long sb = fs_sign64(b.val & mask, width);\n";
    out << "  return fs_make64((sa <= sb) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_sgt32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  int sa = fs_sign32(a.val & mask, width);\n";
    out << "  int sb = fs_sign32(b.val & mask, width);\n";
    out << "  return fs_make32((sa > sb) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_sgt64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  long sa = fs_sign64(a.val & mask, width);\n";
    out << "  long sb = fs_sign64(b.val & mask, width);\n";
    out << "  return fs_make64((sa > sb) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_sge32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);\n";
    out << "  int sa = fs_sign32(a.val & mask, width);\n";
    out << "  int sb = fs_sign32(b.val & mask, width);\n";
    out << "  return fs_make32((sa >= sb) ? 1u : 0u, 0u, 1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_sge64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);\n";
    out << "  long sa = fs_sign64(a.val & mask, width);\n";
    out << "  long sb = fs_sign64(b.val & mask, width);\n";
    out << "  return fs_make64((sa >= sb) ? 1ul : 0ul, 0ul, 1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_sdiv32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(width);\n";
    out << "  int sa = fs_sign32(a.val & mask, width);\n";
    out << "  int sb = fs_sign32(b.val & mask, width);\n";
    out << "  if (sb == 0) return fs_allx32(width);\n";
    out << "  int res = sa / sb;\n";
    out << "  return fs_make32(uint(res), 0u, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_sdiv64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);\n";
    out << "  long sa = fs_sign64(a.val & mask, width);\n";
    out << "  long sb = fs_sign64(b.val & mask, width);\n";
    out << "  if (sb == 0) return fs_allx64(width);\n";
    out << "  long res = sa / sb;\n";
    out << "  return fs_make64(ulong(res), 0ul, width);\n";
    out << "}\n";
    out << "inline FourState32 fs_smod32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  if ((a.xz | b.xz) != 0u) return fs_allx32(width);\n";
    out << "  int sa = fs_sign32(a.val & mask, width);\n";
    out << "  int sb = fs_sign32(b.val & mask, width);\n";
    out << "  if (sb == 0) return fs_allx32(width);\n";
    out << "  int res = sa % sb;\n";
    out << "  return fs_make32(uint(res), 0u, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_smod64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);\n";
    out << "  long sa = fs_sign64(a.val & mask, width);\n";
    out << "  long sb = fs_sign64(b.val & mask, width);\n";
    out << "  if (sb == 0) return fs_allx64(width);\n";
    out << "  long res = sa % sb;\n";
    out << "  return fs_make64(ulong(res), 0ul, width);\n";
    out << "}\n";
    out << "inline FourState32 fs_sar32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  if (b.xz != 0u) return fs_allx32(width);\n";
    out << "  uint shift = b.val;\n";
    out << "  if (width == 0u) return fs_make32(0u, 0u, 0u);\n";
    out << "  uint sign_mask = 1u << (width - 1u);\n";
    out << "  if ((a.xz & sign_mask) != 0u) return fs_allx32(width);\n";
    out << "  uint sign = (a.val & sign_mask) ? mask : 0u;\n";
    out << "  if (shift >= width) return fs_make32(sign, 0u, width);\n";
    out << "  uint fill_mask = (shift == 0u) ? 0u : (~0u << (width - shift));\n";
    out << "  uint shifted_val = (a.val >> shift) | (sign & fill_mask);\n";
    out << "  uint shifted_xz = (a.xz >> shift) & mask;\n";
    out << "  return fs_make32(shifted_val, shifted_xz, width);\n";
    out << "}\n";
    out << "inline FourState64 fs_sar64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  if (b.xz != 0ul) return fs_allx64(width);\n";
    out << "  ulong shift = b.val;\n";
    out << "  if (width == 0u) return fs_make64(0ul, 0ul, 0u);\n";
    out << "  ulong sign_mask = 1ul << (width - 1u);\n";
    out << "  if ((a.xz & sign_mask) != 0ul) return fs_allx64(width);\n";
    out << "  ulong sign = (a.val & sign_mask) ? mask : 0ul;\n";
    out << "  if (shift >= width) return fs_make64(sign, 0ul, width);\n";
    out << "  ulong fill_mask = (shift == 0u) ? 0ul : (~0ul << (width - shift));\n";
    out << "  ulong shifted_val = (a.val >> shift) | (sign & fill_mask);\n";
    out << "  ulong shifted_xz = (a.xz >> shift) & mask;\n";
    out << "  return fs_make64(shifted_val, shifted_xz, width);\n";
    out << "}\n\n";
    out << "inline FourState32 fs_log_not32(FourState32 a, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint known1 = a.val & ~ax & mask;\n";
    out << "  if (known1 != 0u) return fs_make32(0u, 0u, 1u);\n";
    out << "  if (ax == 0u && (a.val & mask) == 0u) return fs_make32(1u, 0u, 1u);\n";
    out << "  return fs_allx32(1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_log_not64(FourState64 a, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong known1 = a.val & ~ax & mask;\n";
    out << "  if (known1 != 0ul) return fs_make64(0ul, 0ul, 1u);\n";
    out << "  if (ax == 0ul && (a.val & mask) == 0ul) return fs_make64(1ul, 0ul, 1u);\n";
    out << "  return fs_allx64(1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_log_and32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint bx = b.xz & mask;\n";
    out << "  uint a_known1 = a.val & ~ax & mask;\n";
    out << "  uint b_known1 = b.val & ~bx & mask;\n";
    out << "  bool a_true = a_known1 != 0u;\n";
    out << "  bool b_true = b_known1 != 0u;\n";
    out << "  bool a_false = (ax == 0u && (a.val & mask) == 0u);\n";
    out << "  bool b_false = (bx == 0u && (b.val & mask) == 0u);\n";
    out << "  if (a_false || b_false) return fs_make32(0u, 0u, 1u);\n";
    out << "  if (a_true && b_true) return fs_make32(1u, 0u, 1u);\n";
    out << "  return fs_allx32(1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_log_and64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong bx = b.xz & mask;\n";
    out << "  ulong a_known1 = a.val & ~ax & mask;\n";
    out << "  ulong b_known1 = b.val & ~bx & mask;\n";
    out << "  bool a_true = a_known1 != 0ul;\n";
    out << "  bool b_true = b_known1 != 0ul;\n";
    out << "  bool a_false = (ax == 0ul && (a.val & mask) == 0ul);\n";
    out << "  bool b_false = (bx == 0ul && (b.val & mask) == 0ul);\n";
    out << "  if (a_false || b_false) return fs_make64(0ul, 0ul, 1u);\n";
    out << "  if (a_true && b_true) return fs_make64(1ul, 0ul, 1u);\n";
    out << "  return fs_allx64(1u);\n";
    out << "}\n";
    out << "inline FourState32 fs_log_or32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint bx = b.xz & mask;\n";
    out << "  uint a_known1 = a.val & ~ax & mask;\n";
    out << "  uint b_known1 = b.val & ~bx & mask;\n";
    out << "  bool a_true = a_known1 != 0u;\n";
    out << "  bool b_true = b_known1 != 0u;\n";
    out << "  bool a_false = (ax == 0u && (a.val & mask) == 0u);\n";
    out << "  bool b_false = (bx == 0u && (b.val & mask) == 0u);\n";
    out << "  if (a_true || b_true) return fs_make32(1u, 0u, 1u);\n";
    out << "  if (a_false && b_false) return fs_make32(0u, 0u, 1u);\n";
    out << "  return fs_allx32(1u);\n";
    out << "}\n";
    out << "inline FourState64 fs_log_or64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong bx = b.xz & mask;\n";
    out << "  ulong a_known1 = a.val & ~ax & mask;\n";
    out << "  ulong b_known1 = b.val & ~bx & mask;\n";
    out << "  bool a_true = a_known1 != 0ul;\n";
    out << "  bool b_true = b_known1 != 0ul;\n";
    out << "  bool a_false = (ax == 0ul && (a.val & mask) == 0ul);\n";
    out << "  bool b_false = (bx == 0ul && (b.val & mask) == 0ul);\n";
    out << "  if (a_true || b_true) return fs_make64(1ul, 0ul, 1u);\n";
    out << "  if (a_false && b_false) return fs_make64(0ul, 0ul, 1u);\n";
    out << "  return fs_allx64(1u);\n";
    out << "}\n\n";
    out << "inline bool fs_case_eq32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ax = a.xz & mask;\n";
    out << "  uint bx = b.xz & mask;\n";
    out << "  if ((ax ^ bx) != 0u) return false;\n";
    out << "  uint known = (~(ax | bx)) & mask;\n";
    out << "  return ((a.val ^ b.val) & known) == 0u;\n";
    out << "}\n";
    out << "inline bool fs_case_eq64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ax = a.xz & mask;\n";
    out << "  ulong bx = b.xz & mask;\n";
    out << "  if ((ax ^ bx) != 0ul) return false;\n";
    out << "  ulong known = (~(ax | bx)) & mask;\n";
    out << "  return ((a.val ^ b.val) & known) == 0ul;\n";
    out << "}\n";
    out << "inline bool fs_casez32(FourState32 a, FourState32 b, uint ignore_mask, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint ignore = ignore_mask & mask;\n";
    out << "  uint cared = (~ignore) & mask;\n";
    out << "  if ((a.xz & cared) != 0u) return false;\n";
    out << "  return ((a.val ^ b.val) & cared) == 0u;\n";
    out << "}\n";
    out << "inline bool fs_casez64(FourState64 a, FourState64 b, ulong ignore_mask, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong ignore = ignore_mask & mask;\n";
    out << "  ulong cared = (~ignore) & mask;\n";
    out << "  if ((a.xz & cared) != 0ul) return false;\n";
    out << "  return ((a.val ^ b.val) & cared) == 0ul;\n";
    out << "}\n";
    out << "inline bool fs_casex32(FourState32 a, FourState32 b, uint width) {\n";
    out << "  uint mask = fs_mask32(width);\n";
    out << "  uint cared = (~(a.xz | b.xz)) & mask;\n";
    out << "  return ((a.val ^ b.val) & cared) == 0u;\n";
    out << "}\n";
    out << "inline bool fs_casex64(FourState64 a, FourState64 b, uint width) {\n";
    out << "  ulong mask = fs_mask64(width);\n";
    out << "  ulong cared = (~(a.xz | b.xz)) & mask;\n";
    out << "  return ((a.val ^ b.val) & cared) == 0ul;\n";
    out << "}\n\n";
  }
  if (four_state) {
    auto suffix_for_width = [](int width) -> std::string {
      return (width > 32) ? "ul" : "u";
    };
    auto literal_for_width = [&](uint64_t value, int width) -> std::string {
      return std::to_string(value) + suffix_for_width(width);
    };
    auto mask_literal = [&](int width) -> std::string {
      uint64_t mask = MaskForWidth64(width);
      return std::to_string(mask) + suffix_for_width(width);
    };
    auto drive_full = [&](int width) -> std::string {
      return mask_literal(width);
    };
    auto drive_zero = [&](int width) -> std::string {
      return literal_for_width(0, width);
    };
    auto val_name = [](const std::string& name) { return name + "_val"; };
    auto xz_name = [](const std::string& name) { return name + "_xz"; };

    struct FsExpr {
      std::string val;
      std::string xz;
      std::string drive;
      int width = 0;
    };

    auto fs_make_expr = [&](const FsExpr& expr, int width) -> std::string {
      if (width > 32) {
        return "fs_make64(" + expr.val + ", " + expr.xz + ", " +
               std::to_string(width) + "u)";
      }
      return "fs_make32(" + expr.val + ", " + expr.xz + ", " +
             std::to_string(width) + "u)";
    };

    auto fs_resize_drive = [&](const FsExpr& expr, int width,
                               bool sign_extend) -> std::string {
      if (expr.width == width) {
        return expr.drive;
      }
      if (width < expr.width) {
        return MaskForWidthExpr(expr.drive, width);
      }
      std::string widened = ExtendExpr(expr.drive, expr.width, width);
      uint64_t upper_mask_value =
          MaskForWidth64(width) & ~MaskForWidth64(expr.width);
      std::string upper_mask = literal_for_width(upper_mask_value, width);
      if (!sign_extend || expr.width <= 0) {
        return "(" + widened + " | " + upper_mask + ")";
      }
      std::string sign_bit = "((" + widened + " >> " +
                             std::to_string(expr.width - 1) + "u) & 1u)";
      std::string upper_drive =
          "(" + sign_bit + " ? " + upper_mask + " : " + drive_zero(width) +
          ")";
      return "(" + widened + " | " + upper_drive + ")";
    };

    auto fs_resize_expr = [&](const FsExpr& expr, int width) -> FsExpr {
      if (expr.width == width) {
        return expr;
      }
      std::string func = (width > 32) ? "fs_resize64" : "fs_resize32";
      std::string base = func + "(" + fs_make_expr(expr, expr.width) + ", " +
                         std::to_string(width) + "u)";
      std::string drive = fs_resize_drive(expr, width, false);
      return FsExpr{base + ".val", base + ".xz", drive, width};
    };

    auto fs_sext_expr = [&](const FsExpr& expr, int width) -> FsExpr {
      if (expr.width >= width) {
        return fs_resize_expr(expr, width);
      }
      std::string func = (width > 32) ? "fs_sext64" : "fs_sext32";
      std::string base =
          func + "(" + fs_make_expr(expr, expr.width) + ", " +
          std::to_string(expr.width) + "u, " + std::to_string(width) + "u)";
      std::string drive = fs_resize_drive(expr, width, true);
      return FsExpr{base + ".val", base + ".xz", drive, width};
    };

    auto fs_extend_expr = [&](const FsExpr& expr, int width,
                              bool signed_op) -> FsExpr {
      return signed_op ? fs_sext_expr(expr, width) : fs_resize_expr(expr, width);
    };

    auto fs_allx_expr = [&](int width) -> FsExpr {
      std::string func = (width > 32) ? "fs_allx64" : "fs_allx32";
      std::string base = func + "(" + std::to_string(width) + "u)";
      return FsExpr{base + ".val", base + ".xz", drive_full(width), width};
    };

    auto fs_unary = [&](const char* op, const FsExpr& arg, int width) -> FsExpr {
      std::string func =
          std::string("fs_") + op + (width > 32 ? "64" : "32");
      std::string base =
          func + "(" + fs_make_expr(arg, width) + ", " +
          std::to_string(width) + "u)";
      return FsExpr{base + ".val", base + ".xz", drive_full(width), width};
    };

    auto fs_binary = [&](const char* op, FsExpr lhs, FsExpr rhs, int width,
                         bool signed_op) -> FsExpr {
      lhs = fs_extend_expr(lhs, width, signed_op);
      rhs = fs_extend_expr(rhs, width, signed_op);
      std::string func =
          std::string("fs_") + op + (width > 32 ? "64" : "32");
      std::string base =
          func + "(" + fs_make_expr(lhs, width) + ", " +
          fs_make_expr(rhs, width) + ", " + std::to_string(width) + "u)";
      return FsExpr{base + ".val", base + ".xz", drive_full(width), width};
    };

    auto fs_shift = [&](const char* op, FsExpr lhs, FsExpr rhs,
                        int width) -> FsExpr {
      if (lhs.width != width) {
        lhs = fs_resize_expr(lhs, width);
      }
      int rhs_width = rhs.width;
      if (width > 32) {
        rhs_width = std::min(rhs_width, 64);
      } else {
        rhs_width = std::min(rhs_width, 32);
      }
      if (rhs.width != rhs_width) {
        rhs = fs_resize_expr(rhs, rhs_width);
      }
      std::string func =
          std::string("fs_") + op + (width > 32 ? "64" : "32");
      std::string base =
          func + "(" + fs_make_expr(lhs, width) + ", " +
          fs_make_expr(rhs, rhs_width) + ", " + std::to_string(width) + "u)";
      return FsExpr{base + ".val", base + ".xz", drive_full(width), width};
    };

    std::function<FsExpr(const Expr&)> emit_expr4;
    std::function<FsExpr(const Expr&)> emit_concat4;

    emit_concat4 = [&](const Expr& expr) -> FsExpr {
      int total_width = ExprWidth(expr, module);
      std::string acc_val = (total_width > 32) ? "0ul" : "0u";
      std::string acc_xz = (total_width > 32) ? "0ul" : "0u";
      std::string acc_drive = (total_width > 32) ? "0ul" : "0u";
      int repeats = std::max(1, expr.repeat);
      int shift = total_width;
      for (int rep = 0; rep < repeats; ++rep) {
        for (const auto& element : expr.elements) {
          FsExpr part = emit_expr4(*element);
          int width = ExprWidth(*element, module);
          shift -= width;
          std::string masked_val = MaskForWidthExpr(part.val, width);
          std::string masked_xz = MaskForWidthExpr(part.xz, width);
          std::string masked_drive = MaskForWidthExpr(part.drive, width);
          std::string cast = (total_width > 32) ? "(ulong)" : "(uint)";
          acc_val = "(" + acc_val + " | (" + cast + masked_val + " << " +
                    std::to_string(shift) + "u))";
          acc_xz = "(" + acc_xz + " | (" + cast + masked_xz + " << " +
                   std::to_string(shift) + "u))";
          acc_drive = "(" + acc_drive + " | (" + cast + masked_drive + " << " +
                      std::to_string(shift) + "u))";
        }
      }
      return FsExpr{acc_val, acc_xz, acc_drive, total_width};
    };

    emit_expr4 = [&](const Expr& expr) -> FsExpr {
      switch (expr.kind) {
        case ExprKind::kIdentifier: {
          const Port* port = FindPort(module, expr.ident);
          if (port) {
            return FsExpr{val_name(port->name) + "[gid]",
                          xz_name(port->name) + "[gid]",
                          drive_full(port->width), port->width};
          }
          return FsExpr{val_name(expr.ident), xz_name(expr.ident),
                        drive_full(SignalWidth(module, expr.ident)),
                        SignalWidth(module, expr.ident)};
        }
        case ExprKind::kNumber: {
          int width = expr.has_width && expr.number_width > 0
                          ? expr.number_width
                          : ExprWidth(expr, module);
          uint64_t xz_bits = expr.x_bits | expr.z_bits;
          uint64_t drive_bits = MaskForWidth64(width) & ~expr.z_bits;
          FsExpr out;
          out.width = width;
          out.val = literal_for_width(expr.value_bits, width);
          out.xz = literal_for_width(xz_bits, width);
          out.drive = literal_for_width(drive_bits, width);
          return out;
        }
        case ExprKind::kUnary: {
          FsExpr operand = emit_expr4(*expr.operand);
          int width = operand.width;
          if (expr.unary_op == 'S' || expr.unary_op == 'U') {
            return operand;
          }
          if (expr.unary_op == '+') {
            return operand;
          }
          if (expr.unary_op == '-') {
            FsExpr zero{literal_for_width(0, width), literal_for_width(0, width),
                        drive_full(width), width};
            bool signed_op =
                expr.operand ? ExprSigned(*expr.operand, module) : false;
            return fs_binary("sub", zero, operand, width, signed_op);
          }
          if (expr.unary_op == '~') {
            return fs_unary("not", operand, width);
          }
          if (expr.unary_op == '!') {
            std::string func = (width > 32) ? "fs_log_not64" : "fs_log_not32";
            std::string base =
                func + "(" + fs_make_expr(operand, width) + ", " +
                std::to_string(width) + "u)";
            return FsExpr{base + ".val", base + ".xz", drive_full(1), 1};
          }
          if (expr.unary_op == '&' || expr.unary_op == '|' ||
              expr.unary_op == '^') {
            std::string func = "fs_red_and";
            if (expr.unary_op == '|') {
              func = "fs_red_or";
            } else if (expr.unary_op == '^') {
              func = "fs_red_xor";
            }
            func += (width > 32) ? "64" : "32";
            std::string base = func + "(" + fs_make_expr(operand, width) + ", " +
                               std::to_string(width) + "u)";
            return FsExpr{base + ".val", base + ".xz", drive_full(1), 1};
          }
          return fs_allx_expr(width);
        }
        case ExprKind::kBinary: {
          if (expr.op == 'l' || expr.op == 'r' || expr.op == 'R') {
            FsExpr lhs = emit_expr4(*expr.lhs);
            FsExpr rhs = emit_expr4(*expr.rhs);
            int width = lhs.width;
            bool signed_lhs =
                expr.op == 'R' ? ExprSigned(*expr.lhs, module) : false;
            const char* op = (expr.op == 'l')
                                 ? "shl"
                                 : (signed_lhs ? "sar" : "shr");
            return fs_shift(op, lhs, rhs, width);
          }
          if (expr.op == 'A' || expr.op == 'O') {
            FsExpr lhs = emit_expr4(*expr.lhs);
            FsExpr rhs = emit_expr4(*expr.rhs);
            int width = std::max(lhs.width, rhs.width);
            lhs = fs_resize_expr(lhs, width);
            rhs = fs_resize_expr(rhs, width);
            std::string func = (width > 32)
                                   ? (expr.op == 'A' ? "fs_log_and64"
                                                    : "fs_log_or64")
                                   : (expr.op == 'A' ? "fs_log_and32"
                                                    : "fs_log_or32");
            std::string base =
                func + "(" + fs_make_expr(lhs, width) + ", " +
                fs_make_expr(rhs, width) + ", " + std::to_string(width) + "u)";
            return FsExpr{base + ".val", base + ".xz", drive_full(1), 1};
          }
          if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
              expr.op == '>' || expr.op == 'L' || expr.op == 'G') {
            FsExpr lhs = emit_expr4(*expr.lhs);
            FsExpr rhs = emit_expr4(*expr.rhs);
            int width = std::max(lhs.width, rhs.width);
            bool signed_op =
                ExprSigned(*expr.lhs, module) && ExprSigned(*expr.rhs, module);
            const char* op = "eq";
            if (expr.op == 'N') {
              op = "ne";
            } else if (expr.op == '<') {
              op = signed_op ? "slt" : "lt";
            } else if (expr.op == '>') {
              op = signed_op ? "sgt" : "gt";
            } else if (expr.op == 'L') {
              op = signed_op ? "sle" : "le";
            } else if (expr.op == 'G') {
              op = signed_op ? "sge" : "ge";
            }
            FsExpr cmp = fs_binary(op, lhs, rhs, width, signed_op);
            return fs_resize_expr(cmp, 1);
          }
          FsExpr lhs = emit_expr4(*expr.lhs);
          FsExpr rhs = emit_expr4(*expr.rhs);
          int width = std::max(lhs.width, rhs.width);
          bool signed_op =
              ExprSigned(*expr.lhs, module) && ExprSigned(*expr.rhs, module);
          const char* op = nullptr;
          switch (expr.op) {
            case '+':
              op = "add";
              break;
            case '-':
              op = "sub";
              break;
            case '*':
              op = "mul";
              break;
            case '/':
              op = signed_op ? "sdiv" : "div";
              break;
            case '%':
              op = signed_op ? "smod" : "mod";
              break;
            case '&':
              op = "and";
              break;
            case '|':
              op = "or";
              break;
            case '^':
              op = "xor";
              break;
            default:
              op = "add";
              break;
          }
          return fs_binary(op, lhs, rhs, width, signed_op);
        }
        case ExprKind::kTernary: {
          FsExpr cond = emit_expr4(*expr.condition);
          FsExpr then_expr = emit_expr4(*expr.then_expr);
          FsExpr else_expr = emit_expr4(*expr.else_expr);
          int width = std::max(then_expr.width, else_expr.width);
          FsExpr then_resized = fs_resize_expr(then_expr, width);
          FsExpr else_resized = fs_resize_expr(else_expr, width);
          std::string func = (width > 32) ? "fs_mux64" : "fs_mux32";
          std::string base =
              func + "(" + fs_make_expr(cond, cond.width) + ", " +
              fs_make_expr(then_resized, width) + ", " +
              fs_make_expr(else_resized, width) + ", " +
              std::to_string(width) + "u)";
          std::string cond_zero = literal_for_width(0, cond.width);
          std::string cond_known = "(" + cond.xz + " == " + cond_zero + ")";
          std::string cond_true =
              "(" + cond_known + " && " + cond.val + " != " + cond_zero + ")";
          std::string cond_false =
              "(" + cond_known + " && " + cond.val + " == " + cond_zero + ")";
          std::string drive =
              "(" + cond_true + " ? " + then_resized.drive + " : (" +
              cond_false + " ? " + else_resized.drive + " : (" +
              then_resized.drive + " | " + else_resized.drive + ")))";
          return FsExpr{base + ".val", base + ".xz", drive, width};
        }
        case ExprKind::kSelect: {
          FsExpr base = emit_expr4(*expr.base);
          if (expr.indexed_range && expr.indexed_width > 0 && expr.lsb_expr) {
            int width = expr.indexed_width;
            FsExpr shift = emit_expr4(*expr.lsb_expr);
            std::string mask = mask_literal(width);
            std::string idx = "uint(" + shift.val + ")";
            std::string zero = literal_for_width(0, width);
            std::string xguard =
                "(" + shift.xz + " == " + literal_for_width(0, shift.width) +
                ")";
            std::string bounds =
                "(" + idx + " < " + std::to_string(base.width) + "u)";
            std::string val =
                "((" + xguard + ") ? ((" + bounds + ") ? ((" + base.val +
                " >> " + idx + ") & " + mask + ") : " + zero + ") : " + zero +
                ")";
            std::string xz =
                "((" + xguard + ") ? ((" + bounds + ") ? ((" + base.xz +
                " >> " + idx + ") & " + mask + ") : " + zero + ") : " + mask +
                ")";
            std::string drive =
                "((" + xguard + ") ? ((" + bounds + ") ? ((" + base.drive +
                " >> " + idx + ") & " + mask + ") : " + mask + ") : " + mask +
                ")";
            return FsExpr{val, xz, drive, width};
          }
          int lo = std::min(expr.msb, expr.lsb);
          int hi = std::max(expr.msb, expr.lsb);
          int width = hi - lo + 1;
          std::string mask = mask_literal(width);
          std::string val = "((" + base.val + " >> " +
                            std::to_string(lo) + "u) & " + mask + ")";
          std::string xz = "((" + base.xz + " >> " +
                           std::to_string(lo) + "u) & " + mask + ")";
          std::string drive = "((" + base.drive + " >> " +
                              std::to_string(lo) + "u) & " + mask + ")";
          return FsExpr{val, xz, drive, width};
        }
        case ExprKind::kIndex: {
          if (!expr.base || !expr.index) {
            return fs_allx_expr(1);
          }
          if (expr.base->kind == ExprKind::kIdentifier) {
            int element_width = 0;
            int array_size = 0;
            if (IsArrayNet(module, expr.base->ident, &element_width,
                           &array_size)) {
              FsExpr idx = emit_expr4(*expr.index);
              std::string idx_val = idx.val;
              std::string idx_xz = idx.xz;
              std::string guard = "(" + idx_val + " < " +
                                  std::to_string(array_size) + "u)";
              std::string xguard = "(" + idx_xz + " == " +
                                   literal_for_width(0, idx.width) + ")";
              std::string base = "(gid * " + std::to_string(array_size) +
                                 "u) + uint(" + idx_val + ")";
              std::string val =
                  "((" + xguard + ") ? ((" + guard + ") ? " +
                  val_name(expr.base->ident) + "[" + base +
                  "] : " + literal_for_width(0, element_width) + ") : " +
                  literal_for_width(0, element_width) + ")";
              std::string xz =
                  "((" + xguard + ") ? ((" + guard + ") ? " +
                  xz_name(expr.base->ident) + "[" + base +
                  "] : " + literal_for_width(0, element_width) + ") : " +
                  mask_literal(element_width) + ")";
              return FsExpr{val, xz, drive_full(element_width),
                            element_width};
            }
          }
          FsExpr base = emit_expr4(*expr.base);
          FsExpr index = emit_expr4(*expr.index);
          int width = 1;
          std::string cond = "(" + index.xz + " == " +
                             literal_for_width(0, index.width) + ")";
          std::string val = "((" + cond + ") ? (((" + base.val + " >> " +
                            index.val + ") & " + literal_for_width(1, 1) +
                            ")) : 0u)";
          std::string xz = "((" + cond + ") ? (((" + base.xz + " >> " +
                           index.val + ") & " + literal_for_width(1, 1) +
                           ")) : 1u)";
          std::string drive = "((" + cond + ") ? (((" + base.drive + " >> " +
                              index.val + ") & " + literal_for_width(1, 1) +
                              ")) : 1u)";
          return FsExpr{val, xz, drive, width};
        }
        case ExprKind::kCall:
          if (expr.ident == "$time") {
            int width = 64;
            return FsExpr{literal_for_width(0, width),
                          literal_for_width(0, width), drive_full(width),
                          width};
          }
          return fs_allx_expr(1);
        case ExprKind::kConcat:
          return emit_concat4(expr);
      }
      return fs_allx_expr(1);
    };

    auto emit_case_cond4 = [&](CaseKind case_kind, const FsExpr& case_expr,
                               const Expr& label_expr,
                               const Expr* case_expr_src) -> std::string {
      FsExpr label = emit_expr4(label_expr);
      int width = std::max(case_expr.width, label.width);
      FsExpr case_w = fs_resize_expr(case_expr, width);
      FsExpr label_w = fs_resize_expr(label, width);
      std::string func_suffix = (width > 32) ? "64" : "32";
      std::string func = "fs_case_eq" + func_suffix;
      if (case_kind == CaseKind::kCaseZ) {
        if (label_expr.kind != ExprKind::kNumber) {
          return func + "(" + fs_make_expr(case_w, width) + ", " +
                 fs_make_expr(label_w, width) + ", " +
                 std::to_string(width) + "u)";
        }
        uint64_t ignore_bits = label_expr.z_bits;
        if (label_expr.x_bits != 0) {
          return "false";
        }
        if (case_expr_src && case_expr_src->kind == ExprKind::kNumber) {
          ignore_bits |= case_expr_src->z_bits;
        }
        std::string ignore_mask = literal_for_width(ignore_bits, width);
        func = "fs_casez" + func_suffix;
        return func + "(" + fs_make_expr(case_w, width) + ", " +
               fs_make_expr(label_w, width) + ", " + ignore_mask + ", " +
               std::to_string(width) + "u)";
      }
      if (case_kind == CaseKind::kCaseX) {
        func = "fs_casex" + func_suffix;
      }
      return func + "(" + fs_make_expr(case_w, width) + ", " +
             fs_make_expr(label_w, width) + ", " + std::to_string(width) + "u)";
    };

    auto emit_expr4_sized = [&](const Expr& expr, int target_width) -> FsExpr {
      FsExpr out_expr = emit_expr4(expr);
      bool signed_expr = ExprSigned(expr, module);
      return fs_extend_expr(out_expr, target_width, signed_expr);
    };

    struct Lvalue4 {
      std::string val;
      std::string xz;
      std::string guard;
      int width = 0;
      bool ok = false;
      bool is_array = false;
    };

    auto build_lvalue4_assign =
        [&](const Assign& assign,
            const std::unordered_set<std::string>& locals,
            const std::unordered_set<std::string>& regs) -> Lvalue4 {
      Lvalue4 out;
      if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
        out.val = val_name(assign.lhs) + "[gid]";
        out.xz = xz_name(assign.lhs) + "[gid]";
      } else if (locals.count(assign.lhs) > 0) {
        out.val = val_name(assign.lhs);
        out.xz = xz_name(assign.lhs);
      } else {
        return out;
      }
      out.width = SignalWidth(module, assign.lhs);
      out.ok = true;
      return out;
    };

    auto build_lvalue4 = [&](const SequentialAssign& assign,
                             const std::unordered_set<std::string>& locals,
                             const std::unordered_set<std::string>& regs,
                             bool use_next) -> Lvalue4 {
      Lvalue4 out;
      if (assign.lhs_index) {
        int element_width = 0;
        int array_size = 0;
        if (!IsArrayNet(module, assign.lhs, &element_width, &array_size)) {
          return out;
        }
        FsExpr idx = emit_expr4(*assign.lhs_index);
        std::string idx_val = idx.val;
        std::string idx_xz = idx.xz;
        std::string guard = "(" + idx_xz + " == " +
                            literal_for_width(0, idx.width) + " && " +
                            idx_val + " < " + std::to_string(array_size) +
                            "u)";
        std::string base = "(gid * " + std::to_string(array_size) +
                           "u) + uint(" + idx_val + ")";
        std::string name = assign.lhs;
        if (use_next) {
          name += "_next";
        }
        out.val = val_name(name) + "[" + base + "]";
        out.xz = xz_name(name) + "[" + base + "]";
        out.guard = guard;
        out.width = element_width;
        out.ok = true;
        out.is_array = true;
        return out;
      }
      if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
        out.val = val_name(assign.lhs) + "[gid]";
        out.xz = xz_name(assign.lhs) + "[gid]";
      } else if (locals.count(assign.lhs) > 0) {
        out.val = val_name(assign.lhs);
        out.xz = xz_name(assign.lhs);
      } else {
        return out;
      }
      out.width = SignalWidth(module, assign.lhs);
      out.ok = true;
      return out;
    };

    std::unordered_set<std::string> sequential_regs;
    std::unordered_set<std::string> initial_regs;
    bool has_initial = false;
    for (const auto& block : module.always_blocks) {
      if (block.edge == EdgeKind::kCombinational ||
          block.edge == EdgeKind::kInitial) {
        continue;
      }
      for (const auto& stmt : block.statements) {
        CollectAssignedSignals(stmt, &sequential_regs);
      }
    }
    for (const auto& block : module.always_blocks) {
      if (block.edge != EdgeKind::kInitial) {
        continue;
      }
      has_initial = true;
      for (const auto& stmt : block.statements) {
        CollectAssignedSignals(stmt, &initial_regs);
      }
    }

    std::vector<std::string> reg_names;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (net.type == NetType::kReg && !IsOutputPort(module, net.name) &&
          (sequential_regs.count(net.name) > 0 ||
           initial_regs.count(net.name) > 0)) {
        reg_names.push_back(net.name);
      }
    }
    std::vector<std::string> trireg_names;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (net.type == NetType::kTrireg && !IsOutputPort(module, net.name)) {
        trireg_names.push_back(net.name);
      }
    }
    std::vector<std::string> init_reg_names;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (net.type == NetType::kReg && !IsOutputPort(module, net.name) &&
          initial_regs.count(net.name) > 0) {
        init_reg_names.push_back(net.name);
      }
    }

    std::vector<const Net*> array_nets;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        array_nets.push_back(&net);
      }
    }

    out << "kernel void gpga_" << module.name << "(";
    int buffer_index = 0;
    bool first = true;
    for (const auto& port : module.ports) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string qualifier =
          (port.dir == PortDir::kInput) ? "constant" : "device";
      std::string type = TypeForWidth(port.width);
      out << "  " << qualifier << " " << type << "* "
          << val_name(port.name) << " [[buffer(" << buffer_index++ << ")]]";
      out << ",\n";
      out << "  " << qualifier << " " << type << "* "
          << xz_name(port.name) << " [[buffer(" << buffer_index++ << ")]]";
    }
    for (const auto& reg : reg_names) {
      out << ",\n";
      std::string type = TypeForWidth(SignalWidth(module, reg));
      out << "  device " << type << "* " << val_name(reg) << " [[buffer("
          << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device " << type << "* " << xz_name(reg) << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto& reg : trireg_names) {
      out << ",\n";
      std::string type = TypeForWidth(SignalWidth(module, reg));
      out << "  device " << type << "* " << val_name(reg) << " [[buffer("
          << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device " << type << "* " << xz_name(reg) << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto* net : array_nets) {
      out << ",\n";
      std::string type = TypeForWidth(net->width);
      out << "  device " << type << "* " << val_name(net->name)
          << " [[buffer(" << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device " << type << "* " << xz_name(net->name)
          << " [[buffer(" << buffer_index++ << ")]]";
    }
    out << ",\n";
    out << "  constant GpgaParams& params [[buffer(" << buffer_index++
        << ")]],\n";
    out << "  uint gid [[thread_position_in_grid]]) {\n";
    out << "  if (gid >= params.count) {\n";
    out << "    return;\n";
    out << "  }\n";

    std::unordered_set<std::string> locals;
    std::unordered_set<std::string> regs;
    std::unordered_set<std::string> declared;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (net.type == NetType::kReg) {
        if (sequential_regs.count(net.name) > 0) {
          regs.insert(net.name);
        } else if (!IsOutputPort(module, net.name)) {
          locals.insert(net.name);
        }
        continue;
      }
      if (IsTriregNet(net.type)) {
        regs.insert(net.name);
        continue;
      }
      if (!IsOutputPort(module, net.name)) {
        locals.insert(net.name);
      }
    }

    auto driven = CollectDrivenSignals(module);
    for (const auto& net : module.nets) {
      if (net.array_size > 0 || net.type == NetType::kReg ||
          IsTriregNet(net.type)) {
        continue;
      }
      if (driven.count(net.name) > 0 || locals.count(net.name) == 0) {
        continue;
      }
      if (declared.insert(net.name).second) {
        std::string type = TypeForWidth(net.width);
        std::string zero = literal_for_width(0, net.width);
        std::string mask = mask_literal(net.width);
        out << "  " << type << " " << val_name(net.name) << " = " << zero
            << ";\n";
        out << "  " << type << " " << xz_name(net.name) << " = " << mask
            << ";\n";
      }
    }

    std::vector<size_t> ordered_assigns = OrderAssigns(module);
    std::unordered_map<std::string, std::vector<size_t>> assign_groups;
    assign_groups.reserve(module.assigns.size());
    for (size_t i = 0; i < module.assigns.size(); ++i) {
      assign_groups[module.assigns[i].lhs].push_back(i);
    }
    std::unordered_set<std::string> multi_driver;
    std::unordered_map<std::string, size_t> drivers_remaining;
    struct DriverInfo {
      std::string val;
      std::string xz;
      std::string drive;
      std::string strength0;
      std::string strength1;
    };
    std::unordered_map<size_t, DriverInfo> driver_info;
    std::unordered_map<std::string, std::vector<size_t>> drivers_for_net;
    for (const auto& entry : assign_groups) {
      bool force_resolve =
          IsTriregNet(SignalNetType(module, entry.first));
      if (entry.second.size() <= 1 && !force_resolve) {
        continue;
      }
      multi_driver.insert(entry.first);
      drivers_remaining[entry.first] = entry.second.size();
      drivers_for_net[entry.first] = entry.second;
      for (size_t idx = 0; idx < entry.second.size(); ++idx) {
        size_t assign_index = entry.second[idx];
        const Assign& assign = module.assigns[assign_index];
        DriverInfo info;
        info.val =
            "__gpga_drv_" + entry.first + "_" + std::to_string(idx) + "_val";
        info.xz =
            "__gpga_drv_" + entry.first + "_" + std::to_string(idx) + "_xz";
        info.drive =
            "__gpga_drv_" + entry.first + "_" + std::to_string(idx) + "_drive";
        info.strength0 = StrengthLiteral(assign.strength0);
        info.strength1 = StrengthLiteral(assign.strength1);
        driver_info[assign_index] = std::move(info);
      }
    }

    auto emit_driver = [&](const Assign& assign, const DriverInfo& info) {
      if (!assign.rhs) {
        return;
      }
      int lhs_width = SignalWidth(module, assign.lhs);
      std::string type = TypeForWidth(lhs_width);
      if (assign.lhs_has_range) {
        int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
        int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
        int slice_width = hi - lo + 1;
        FsExpr rhs = emit_expr4_sized(*assign.rhs, slice_width);
        std::string mask = mask_literal(slice_width);
        std::string cast = (lhs_width > 32) ? "(ulong)" : "(uint)";
        out << "  " << type << " " << info.val << " = ((" << cast << rhs.val
            << " & " << mask << ") << " << std::to_string(lo) << "u);\n";
        out << "  " << type << " " << info.xz << " = ((" << cast << rhs.xz
            << " & " << mask << ") << " << std::to_string(lo) << "u);\n";
        out << "  " << type << " " << info.drive << " = ((" << cast
            << rhs.drive << " & " << mask << ") << " << std::to_string(lo)
            << "u);\n";
        return;
      }
      FsExpr rhs = emit_expr4_sized(*assign.rhs, lhs_width);
      out << "  " << type << " " << info.val << " = " << rhs.val << ";\n";
      out << "  " << type << " " << info.xz << " = " << rhs.xz << ";\n";
      out << "  " << type << " " << info.drive << " = " << rhs.drive << ";\n";
    };

    auto emit_resolve = [&](const std::string& name,
                            const std::vector<size_t>& indices) {
      NetType net_type = SignalNetType(module, name);
      bool wired_and = IsWiredAndNet(net_type);
      bool wired_or = IsWiredOrNet(net_type);
      bool is_trireg = IsTriregNet(net_type);
      int lhs_width = SignalWidth(module, name);
      std::string type = TypeForWidth(lhs_width);
      std::string one = (lhs_width > 32) ? "1ul" : "1u";
      std::string zero = drive_zero(lhs_width);
      std::string resolved_val = "__gpga_res_" + name + "_val";
      std::string resolved_xz = "__gpga_res_" + name + "_xz";
      std::string resolved_drive = "__gpga_res_" + name + "_drive";
      out << "  " << type << " " << resolved_val << " = " << zero << ";\n";
      out << "  " << type << " " << resolved_xz << " = " << zero << ";\n";
      out << "  " << type << " " << resolved_drive << " = " << zero << ";\n";
      out << "  for (uint bit = 0u; bit < " << lhs_width << "u; ++bit) {\n";
      out << "    " << type << " mask = (" << one << " << bit);\n";
      if (wired_and || wired_or) {
        out << "    bool has0 = false;\n";
        out << "    bool has1 = false;\n";
        out << "    bool hasx = false;\n";
        for (size_t idx : indices) {
          const auto& info = driver_info[idx];
          out << "    if ((" << info.drive << " & mask) != " << zero << ") {\n";
          out << "      if ((" << info.xz << " & mask) != " << zero << ") {\n";
          out << "        hasx = true;\n";
          out << "      } else if ((" << info.val << " & mask) != " << zero
              << ") {\n";
          out << "        has1 = true;\n";
          out << "      } else {\n";
          out << "        has0 = true;\n";
          out << "      }\n";
          out << "    }\n";
        }
        out << "    if (!has0 && !has1 && !hasx) {\n";
        out << "      " << resolved_xz << " |= mask;\n";
        out << "      continue;\n";
        out << "    }\n";
        out << "    " << resolved_drive << " |= mask;\n";
        if (wired_and) {
          out << "    if (has0) {\n";
          out << "      // 0 dominates wired-AND\n";
          out << "    } else if (hasx) {\n";
          out << "      " << resolved_xz << " |= mask;\n";
          out << "    } else {\n";
          out << "      " << resolved_val << " |= mask;\n";
          out << "    }\n";
        } else {
          out << "    if (has1) {\n";
          out << "      " << resolved_val << " |= mask;\n";
          out << "    } else if (hasx) {\n";
          out << "      " << resolved_xz << " |= mask;\n";
          out << "    } else {\n";
          out << "      // 0 dominates wired-OR\n";
          out << "    }\n";
        }
        out << "    continue;\n";
      }
      out << "    uint best0 = 0u;\n";
      out << "    uint best1 = 0u;\n";
      out << "    uint bestx = 0u;\n";
      for (size_t idx : indices) {
        const auto& info = driver_info[idx];
        out << "    if ((" << info.drive << " & mask) != " << zero << ") {\n";
        out << "      if ((" << info.xz << " & mask) != " << zero << ") {\n";
        out << "        uint x_strength = (" << info.strength0 << " > "
            << info.strength1 << ") ? " << info.strength0 << " : "
            << info.strength1 << ";\n";
        out << "        bestx = (bestx > x_strength) ? bestx : x_strength;\n";
        out << "      } else if ((" << info.val << " & mask) != " << zero
            << ") {\n";
        out << "        best1 = (best1 > " << info.strength1 << ") ? best1 : "
            << info.strength1 << ";\n";
        out << "      } else {\n";
        out << "        best0 = (best0 > " << info.strength0 << ") ? best0 : "
            << info.strength0 << ";\n";
        out << "      }\n";
        out << "    }\n";
      }
      out << "    if (best0 == 0u && best1 == 0u && bestx == 0u) {\n";
      out << "      " << resolved_xz << " |= mask;\n";
      out << "      continue;\n";
      out << "    }\n";
      out << "    " << resolved_drive << " |= mask;\n";
      out << "    uint max01 = (best0 > best1) ? best0 : best1;\n";
      out << "    if ((bestx >= max01) && max01 != 0u) {\n";
      out << "      " << resolved_xz << " |= mask;\n";
      out << "    } else if (best0 > best1) {\n";
      out << "      // 0 wins\n";
      out << "    } else if (best1 > best0) {\n";
      out << "      " << resolved_val << " |= mask;\n";
      out << "    } else {\n";
      out << "      " << resolved_xz << " |= mask;\n";
      out << "    }\n";
      out << "  }\n";

      bool is_output = IsOutputPort(module, name) || regs.count(name) > 0;
      bool is_local = locals.count(name) > 0 && !is_output &&
                      regs.count(name) == 0;
      if (is_output) {
        if (is_trireg) {
          out << "  " << val_name(name) << "[gid] = ("
              << val_name(name) << "[gid] & ~" << resolved_drive << ") | ("
              << resolved_val << " & " << resolved_drive << ");\n";
          out << "  " << xz_name(name) << "[gid] = ("
              << xz_name(name) << "[gid] & ~" << resolved_drive << ") | ("
              << resolved_xz << " & " << resolved_drive << ");\n";
        } else {
          out << "  " << val_name(name) << "[gid] = " << resolved_val << ";\n";
          out << "  " << xz_name(name) << "[gid] = " << resolved_xz << ";\n";
        }
      } else if (is_local) {
        if (declared.count(name) == 0) {
          out << "  " << type << " " << val_name(name) << ";\n";
          out << "  " << type << " " << xz_name(name) << ";\n";
          declared.insert(name);
        }
        out << "  " << val_name(name) << " = " << resolved_val << ";\n";
        out << "  " << xz_name(name) << " = " << resolved_xz << ";\n";
      } else {
        out << "  // Unmapped resolved assign: " << name << "\n";
      }
    };
    std::unordered_map<std::string, std::vector<const Assign*>> partial_assigns;
    for (const auto& assign : module.assigns) {
      if (assign.lhs_has_range && multi_driver.count(assign.lhs) == 0) {
        partial_assigns[assign.lhs].push_back(&assign);
      }
    }
    for (size_t index : ordered_assigns) {
      const auto& assign = module.assigns[index];
      if (!assign.rhs) {
        continue;
      }
      if (multi_driver.count(assign.lhs) > 0) {
        auto info_it = driver_info.find(index);
        if (info_it != driver_info.end()) {
          emit_driver(assign, info_it->second);
        }
        auto remain_it = drivers_remaining.find(assign.lhs);
        if (remain_it != drivers_remaining.end()) {
          if (remain_it->second > 0) {
            remain_it->second -= 1;
          }
          if (remain_it->second == 0) {
            emit_resolve(assign.lhs, drivers_for_net[assign.lhs]);
          }
        }
        continue;
      }
      if (assign.lhs_has_range) {
        continue;
      }
      Lvalue4 lhs = build_lvalue4_assign(assign, locals, regs);
      if (!lhs.ok) {
        continue;
      }
      FsExpr rhs = emit_expr4_sized(*assign.rhs, lhs.width);
      if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
        out << "  " << lhs.val << " = " << rhs.val << ";\n";
        out << "  " << lhs.xz << " = " << rhs.xz << ";\n";
      } else if (locals.count(assign.lhs) > 0) {
        if (declared.count(assign.lhs) == 0) {
          std::string type = TypeForWidth(lhs.width);
          out << "  " << type << " " << lhs.val << " = " << rhs.val << ";\n";
          out << "  " << type << " " << lhs.xz << " = " << rhs.xz << ";\n";
          declared.insert(assign.lhs);
        } else {
          out << "  " << lhs.val << " = " << rhs.val << ";\n";
          out << "  " << lhs.xz << " = " << rhs.xz << ";\n";
        }
      }
    }
    for (const auto& entry : partial_assigns) {
      const std::string& name = entry.first;
      int lhs_width = SignalWidth(module, name);
      std::string type = TypeForWidth(lhs_width);
      bool target_is_local =
          locals.count(name) > 0 && !IsOutputPort(module, name) &&
          regs.count(name) == 0;
      std::string temp_val =
          target_is_local ? val_name(name) : ("__gpga_partial_" + name + "_val");
      std::string temp_xz =
          target_is_local ? xz_name(name) : ("__gpga_partial_" + name + "_xz");
      std::string zero = literal_for_width(0, lhs_width);
      if (target_is_local) {
        if (declared.count(name) == 0) {
          out << "  " << type << " " << temp_val << " = " << zero << ";\n";
          out << "  " << type << " " << temp_xz << " = " << zero << ";\n";
          declared.insert(name);
        } else {
          out << "  " << temp_val << " = " << zero << ";\n";
          out << "  " << temp_xz << " = " << zero << ";\n";
        }
      } else {
        out << "  " << type << " " << temp_val << " = " << zero << ";\n";
        out << "  " << type << " " << temp_xz << " = " << zero << ";\n";
      }
      for (const auto* assign : entry.second) {
        int lo = std::min(assign->lhs_msb, assign->lhs_lsb);
        int hi = std::max(assign->lhs_msb, assign->lhs_lsb);
        int slice_width = hi - lo + 1;
        FsExpr rhs = emit_expr4_sized(*assign->rhs, slice_width);
        std::string mask = mask_literal(slice_width);
        std::string shifted_mask =
            "(" + mask + " << " + std::to_string(lo) + "u)";
        std::string cast = (lhs_width > 32) ? "(ulong)" : "(uint)";
        out << "  " << temp_val << " = (" << temp_val << " & ~"
            << shifted_mask << ") | ((" << cast << rhs.val << " & " << mask
            << ") << " << std::to_string(lo) << "u);\n";
        out << "  " << temp_xz << " = (" << temp_xz << " & ~"
            << shifted_mask << ") | ((" << cast << rhs.xz << " & " << mask
            << ") << " << std::to_string(lo) << "u);\n";
      }
      if (!target_is_local) {
        if (IsOutputPort(module, name) || regs.count(name) > 0) {
          out << "  " << val_name(name) << "[gid] = " << temp_val << ";\n";
          out << "  " << xz_name(name) << "[gid] = " << temp_xz << ";\n";
        } else if (locals.count(name) > 0) {
          if (declared.count(name) == 0) {
            out << "  " << type << " " << val_name(name) << " = " << temp_val
                << ";\n";
            out << "  " << type << " " << xz_name(name) << " = " << temp_xz
                << ";\n";
            declared.insert(name);
          } else {
            out << "  " << val_name(name) << " = " << temp_val << ";\n";
            out << "  " << xz_name(name) << " = " << temp_xz << ";\n";
          }
        } else {
          out << "  // Unmapped assign: " << name << " = " << temp_val
              << ";\n";
        }
      }
    }

    std::unordered_set<std::string> comb_targets;
    for (const auto& block : module.always_blocks) {
      if (block.edge != EdgeKind::kCombinational) {
        continue;
      }
      for (const auto& stmt : block.statements) {
        CollectAssignedSignals(stmt, &comb_targets);
      }
    }
    for (const auto& target : comb_targets) {
      if (locals.count(target) == 0 || declared.count(target) > 0) {
        continue;
      }
      std::string type = TypeForWidth(SignalWidth(module, target));
      out << "  " << type << " " << val_name(target) << ";\n";
      out << "  " << type << " " << xz_name(target) << ";\n";
      declared.insert(target);
    }

    auto cond_bool = [&](const FsExpr& expr) -> std::string {
      return "(" + expr.xz + " == " + literal_for_width(0, expr.width) +
             " && " + expr.val + " != " + literal_for_width(0, expr.width) +
             ")";
    };

    auto fs_merge_expr = [&](FsExpr lhs, FsExpr rhs, int width) -> FsExpr {
      lhs = fs_resize_expr(lhs, width);
      rhs = fs_resize_expr(rhs, width);
      std::string func = (width > 32) ? "fs_merge64" : "fs_merge32";
      std::string base =
          func + "(" + fs_make_expr(lhs, width) + ", " +
          fs_make_expr(rhs, width) + ", " + std::to_string(width) + "u)";
      return FsExpr{base + ".val", base + ".xz", drive_full(width), width};
    };

    auto signal_lvalue4 = [&](const std::string& name, std::string* val,
                              std::string* xz, int* width) -> bool {
      if (!val || !xz || !width) {
        return false;
      }
      *width = SignalWidth(module, name);
      if (*width <= 0) {
        return false;
      }
      if (IsOutputPort(module, name) || regs.count(name) > 0) {
        *val = val_name(name) + "[gid]";
        *xz = xz_name(name) + "[gid]";
        return true;
      }
      if (locals.count(name) > 0) {
        *val = val_name(name);
        *xz = xz_name(name);
        return true;
      }
      return false;
    };

    std::function<void(const Statement&, int)> emit_comb_stmt;
    emit_comb_stmt = [&](const Statement& stmt, int indent) {
      std::string pad(indent, ' ');
      if (stmt.kind == StatementKind::kAssign) {
        if (!stmt.assign.rhs) {
          return;
        }
        Lvalue4 lhs = build_lvalue4(stmt.assign, locals, regs, false);
        if (!lhs.ok) {
          return;
        }
        FsExpr rhs = emit_expr4_sized(*stmt.assign.rhs, lhs.width);
        if (!lhs.guard.empty()) {
          out << pad << "if " << lhs.guard << " {\n";
          out << pad << "  " << lhs.val << " = " << rhs.val << ";\n";
          out << pad << "  " << lhs.xz << " = " << rhs.xz << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lhs.val << " = " << rhs.val << ";\n";
          out << pad << lhs.xz << " = " << rhs.xz << ";\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kIf) {
        FsExpr cond =
            stmt.condition ? emit_expr4(*stmt.condition)
                           : FsExpr{literal_for_width(0, 1),
                                    literal_for_width(0, 1),
                                    drive_full(1), 1};
        out << pad << "if (" << cond_bool(cond) << ") {\n";
        for (const auto& inner : stmt.then_branch) {
          emit_comb_stmt(inner, indent + 2);
        }
        if (!stmt.else_branch.empty()) {
          out << pad << "} else {\n";
          for (const auto& inner : stmt.else_branch) {
            emit_comb_stmt(inner, indent + 2);
          }
          out << pad << "}\n";
        } else {
          out << pad << "}\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kCase) {
        FsExpr case_expr = stmt.case_expr
                               ? emit_expr4(*stmt.case_expr)
                               : FsExpr{literal_for_width(0, 1),
                                        literal_for_width(0, 1),
                                        drive_full(1), 1};
        bool first_case = true;
        for (const auto& item : stmt.case_items) {
          std::string cond;
          for (const auto& label : item.labels) {
            std::string piece =
                emit_case_cond4(stmt.case_kind, case_expr, *label,
                                stmt.case_expr.get());
            if (!cond.empty()) {
              cond += " || ";
            }
            cond += piece;
          }
          if (cond.empty()) {
            continue;
          }
          if (first_case) {
            out << pad << "if (" << cond << ") {\n";
            first_case = false;
          } else {
            out << pad << "} else if (" << cond << ") {\n";
          }
          for (const auto& inner : item.body) {
            emit_comb_stmt(inner, indent + 2);
          }
        }
        if (!stmt.default_branch.empty()) {
          out << pad << "} else {\n";
          for (const auto& inner : stmt.default_branch) {
            emit_comb_stmt(inner, indent + 2);
          }
          out << pad << "}\n";
        } else if (!first_case) {
          out << pad << "}\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kBlock) {
        out << pad << "{\n";
        for (const auto& inner : stmt.block) {
          emit_comb_stmt(inner, indent + 2);
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == StatementKind::kDelay) {
        out << pad << "// delay control ignored in MSL v0\n";
        for (const auto& inner : stmt.delay_body) {
          emit_comb_stmt(inner, indent);
        }
        return;
      }
      if (stmt.kind == StatementKind::kEventControl) {
        out << pad << "// event control ignored in MSL v0\n";
        for (const auto& inner : stmt.event_body) {
          emit_comb_stmt(inner, indent);
        }
        return;
      }
      if (stmt.kind == StatementKind::kWait) {
        out << pad << "// wait ignored in MSL v0\n";
        for (const auto& inner : stmt.wait_body) {
          emit_comb_stmt(inner, indent);
        }
        return;
      }
      if (stmt.kind == StatementKind::kForever) {
        out << pad << "// forever ignored in MSL v0\n";
        return;
      }
      if (stmt.kind == StatementKind::kFork) {
        out << pad << "// fork/join executed sequentially in MSL v0\n";
        for (const auto& inner : stmt.fork_branches) {
          emit_comb_stmt(inner, indent);
        }
        return;
      }
      if (stmt.kind == StatementKind::kDisable) {
        out << pad << "// disable ignored in MSL v0\n";
        return;
      }
      if (stmt.kind == StatementKind::kEventTrigger) {
        out << pad << "// event trigger ignored in MSL v0\n";
        return;
      }
      if (stmt.kind == StatementKind::kTaskCall) {
        out << pad << "// task call ignored in MSL v0\n";
        return;
      }
    };

    for (const auto& block : module.always_blocks) {
      if (block.edge != EdgeKind::kCombinational) {
        continue;
      }
      for (const auto& stmt : block.statements) {
        emit_comb_stmt(stmt, 2);
      }
    }
    for (const auto& sw : module.switches) {
      std::string a_val;
      std::string a_xz;
      std::string b_val;
      std::string b_xz;
      int a_width = 0;
      int b_width = 0;
      if (!signal_lvalue4(sw.a, &a_val, &a_xz, &a_width) ||
          !signal_lvalue4(sw.b, &b_val, &b_xz, &b_width)) {
        continue;
      }
      int width = std::min(a_width, b_width);
      FsExpr a_expr{a_val, a_xz, drive_full(width), width};
      FsExpr b_expr{b_val, b_xz, drive_full(width), width};
      FsExpr merged = fs_merge_expr(a_expr, b_expr, width);

      std::string cond_false;
      if (sw.kind == SwitchKind::kTran) {
        cond_false = "false";
      } else if (sw.kind == SwitchKind::kTranif1 ||
                 sw.kind == SwitchKind::kTranif0) {
        FsExpr cond = sw.control ? emit_expr4(*sw.control)
                                 : FsExpr{literal_for_width(0, 1),
                                          literal_for_width(0, 1),
                                          drive_full(1), 1};
        std::string zero = literal_for_width(0, cond.width);
        std::string known =
            "(" + cond.xz + " == " + literal_for_width(0, cond.width) + ")";
        std::string is_zero = "(" + cond.val + " == " + zero + ")";
        std::string is_one = "(" + cond.val + " != " + zero + ")";
        if (sw.kind == SwitchKind::kTranif1) {
          cond_false = known + " && " + is_zero;
        } else {
          cond_false = known + " && " + is_one;
        }
      } else {
        FsExpr cond = sw.control ? emit_expr4(*sw.control)
                                 : FsExpr{literal_for_width(0, 1),
                                          literal_for_width(0, 1),
                                          drive_full(1), 1};
        FsExpr cond_n = sw.control_n ? emit_expr4(*sw.control_n)
                                     : FsExpr{literal_for_width(0, 1),
                                              literal_for_width(0, 1),
                                              drive_full(1), 1};
        std::string known =
            "(" + cond.xz + " == " + literal_for_width(0, cond.width) + " && " +
            cond_n.xz + " == " + literal_for_width(0, cond_n.width) + ")";
        std::string on =
            "(" + cond.val + " != " + literal_for_width(0, cond.width) +
            " && " + cond_n.val + " == " + literal_for_width(0, cond_n.width) +
            ")";
        cond_false = known + " && !" + on;
      }

      out << "  if (" << cond_false << ") {\n";
      out << "  } else {\n";
      out << "    " << a_val << " = " << merged.val << ";\n";
      out << "    " << a_xz << " = " << merged.xz << ";\n";
      out << "    " << b_val << " = " << merged.val << ";\n";
      out << "    " << b_xz << " = " << merged.xz << ";\n";
      out << "  }\n";
    }
    out << "}\n";

    if (has_initial) {
      out << "\n";
      out << "kernel void gpga_" << module.name << "_init(";
      buffer_index = 0;
      first = true;
      for (const auto& port : module.ports) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string qualifier =
            (port.dir == PortDir::kInput) ? "constant" : "device";
        std::string type = TypeForWidth(port.width);
        out << "  " << qualifier << " " << type << "* "
            << val_name(port.name) << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  " << qualifier << " " << type << "* "
            << xz_name(port.name) << " [[buffer(" << buffer_index++ << ")]]";
      }
      for (const auto& reg : init_reg_names) {
        out << ",\n";
        std::string type = TypeForWidth(SignalWidth(module, reg));
        out << "  device " << type << "* " << val_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
      }
      for (const auto& reg : trireg_names) {
        out << ",\n";
        std::string type = TypeForWidth(SignalWidth(module, reg));
        out << "  device " << type << "* " << val_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
      }
      for (const auto* net : array_nets) {
        out << ",\n";
        std::string type = TypeForWidth(net->width);
        out << "  device " << type << "* " << val_name(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
      }
      out << ",\n";
      out << "  constant GpgaParams& params [[buffer(" << buffer_index++
          << ")]],\n";
      out << "  uint gid [[thread_position_in_grid]]) {\n";
      out << "  if (gid >= params.count) {\n";
      out << "    return;\n";
      out << "  }\n";

      std::unordered_set<std::string> init_locals;
      std::unordered_set<std::string> init_regs;
      std::unordered_set<std::string> init_declared;
      for (const auto& net : module.nets) {
        if (net.array_size > 0) {
          continue;
        }
        if (net.type == NetType::kReg) {
          if (initial_regs.count(net.name) > 0) {
            init_regs.insert(net.name);
          } else if (!IsOutputPort(module, net.name)) {
            init_locals.insert(net.name);
          }
          continue;
        }
        if (IsTriregNet(net.type)) {
          init_regs.insert(net.name);
          continue;
        }
        if (!IsOutputPort(module, net.name)) {
          init_locals.insert(net.name);
        }
      }

      std::unordered_set<std::string> init_targets;
      for (const auto& block : module.always_blocks) {
        if (block.edge != EdgeKind::kInitial) {
          continue;
        }
        for (const auto& stmt : block.statements) {
          CollectAssignedSignals(stmt, &init_targets);
        }
      }
      for (const auto& target : init_targets) {
        if (init_locals.count(target) == 0 || init_declared.count(target) > 0) {
          continue;
        }
        std::string type = TypeForWidth(SignalWidth(module, target));
        out << "  " << type << " " << val_name(target) << ";\n";
        out << "  " << type << " " << xz_name(target) << ";\n";
        init_declared.insert(target);
      }

      std::function<void(const Statement&, int)> emit_init_stmt;
      emit_init_stmt = [&](const Statement& stmt, int indent) {
        std::string pad(indent, ' ');
        if (stmt.kind == StatementKind::kAssign) {
          if (!stmt.assign.rhs) {
            return;
          }
          Lvalue4 lhs = build_lvalue4(stmt.assign, init_locals, init_regs,
                                      false);
          if (!lhs.ok) {
            return;
          }
          FsExpr rhs = emit_expr4_sized(*stmt.assign.rhs, lhs.width);
          if (!lhs.guard.empty()) {
            out << pad << "if " << lhs.guard << " {\n";
            out << pad << "  " << lhs.val << " = " << rhs.val << ";\n";
            out << pad << "  " << lhs.xz << " = " << rhs.xz << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << lhs.val << " = " << rhs.val << ";\n";
            out << pad << lhs.xz << " = " << rhs.xz << ";\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          FsExpr cond = stmt.condition ? emit_expr4(*stmt.condition)
                                       : fs_allx_expr(1);
          out << pad << "if (" << cond_bool(cond) << ") {\n";
          for (const auto& inner : stmt.then_branch) {
            emit_init_stmt(inner, indent + 2);
          }
          if (!stmt.else_branch.empty()) {
            out << pad << "} else {\n";
            for (const auto& inner : stmt.else_branch) {
              emit_init_stmt(inner, indent + 2);
            }
            out << pad << "}\n";
          } else {
            out << pad << "}\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          if (!stmt.case_expr) {
            return;
          }
          FsExpr case_expr = emit_expr4(*stmt.case_expr);
          if (stmt.case_items.empty()) {
            for (const auto& inner : stmt.default_branch) {
              emit_init_stmt(inner, indent);
            }
            return;
          }
          bool first_case = true;
          for (const auto& item : stmt.case_items) {
            std::string cond;
            for (const auto& label : item.labels) {
              std::string piece = emit_case_cond4(
                  stmt.case_kind, case_expr, *label, stmt.case_expr.get());
              if (!cond.empty()) {
                cond += " || ";
              }
              cond += piece;
            }
            if (cond.empty()) {
              continue;
            }
            if (first_case) {
              out << pad << "if (" << cond << ") {\n";
              first_case = false;
            } else {
              out << pad << "} else if (" << cond << ") {\n";
            }
            for (const auto& inner : item.body) {
              emit_init_stmt(inner, indent + 2);
            }
          }
          if (!stmt.default_branch.empty()) {
            out << pad << "} else {\n";
            for (const auto& inner : stmt.default_branch) {
              emit_init_stmt(inner, indent + 2);
            }
            out << pad << "}\n";
          } else if (!first_case) {
            out << pad << "}\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          out << pad << "{\n";
          for (const auto& inner : stmt.block) {
            emit_init_stmt(inner, indent + 2);
          }
          out << pad << "}\n";
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          out << pad << "// delay control ignored in MSL v0\n";
          for (const auto& inner : stmt.delay_body) {
            emit_init_stmt(inner, indent);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          out << pad << "// event control ignored in MSL v0\n";
          for (const auto& inner : stmt.event_body) {
            emit_init_stmt(inner, indent);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          out << pad << "// wait ignored in MSL v0\n";
          for (const auto& inner : stmt.wait_body) {
            emit_init_stmt(inner, indent);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          out << pad << "// forever ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kFork) {
          out << pad << "// fork/join executed sequentially in MSL v0\n";
          for (const auto& inner : stmt.fork_branches) {
            emit_init_stmt(inner, indent);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDisable) {
          out << pad << "// disable ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kEventTrigger) {
          out << pad << "// event trigger ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kTaskCall) {
          out << pad << "// task call ignored in MSL v0\n";
          return;
        }
      };

      for (const auto& block : module.always_blocks) {
        if (block.edge != EdgeKind::kInitial) {
          continue;
        }
        for (const auto& stmt : block.statements) {
          emit_init_stmt(stmt, 2);
        }
      }
      out << "}\n";
    }

    bool has_sequential = false;
    for (const auto& block : module.always_blocks) {
      if (block.edge == EdgeKind::kPosedge ||
          block.edge == EdgeKind::kNegedge) {
        has_sequential = true;
        break;
      }
    }

    if (has_sequential) {
      out << "\n";
      out << "kernel void gpga_" << module.name << "_tick(";
      buffer_index = 0;
      first = true;
      for (const auto& port : module.ports) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string qualifier =
            (port.dir == PortDir::kInput) ? "constant" : "device";
        std::string type = TypeForWidth(port.width);
        out << "  " << qualifier << " " << type << "* "
            << val_name(port.name) << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  " << qualifier << " " << type << "* "
            << xz_name(port.name) << " [[buffer(" << buffer_index++ << ")]]";
      }
      for (const auto& reg : reg_names) {
        out << ",\n";
        std::string type = TypeForWidth(SignalWidth(module, reg));
        out << "  device " << type << "* " << val_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
      }
      for (const auto* net : array_nets) {
        out << ",\n";
        std::string type = TypeForWidth(net->width);
        out << "  device " << type << "* " << val_name(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
      }
      for (const auto* net : array_nets) {
        out << ",\n";
        std::string type = TypeForWidth(net->width);
        out << "  device " << type << "* " << val_name(net->name + "_next")
            << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(net->name + "_next")
            << " [[buffer(" << buffer_index++ << ")]]";
      }
      out << ",\n";
      out << "  constant GpgaParams& params [[buffer(" << buffer_index++
          << ")]],\n";
      out << "  uint gid [[thread_position_in_grid]]) {\n";
      out << "  if (gid >= params.count) {\n";
      out << "    return;\n";
      out << "  }\n";
      out << "  // Tick kernel: sequential logic (posedge/negedge in v0).\n";
      for (const auto* net : array_nets) {
        out << "  for (uint i = 0u; i < " << net->array_size << "u; ++i) {\n";
        out << "    " << val_name(net->name + "_next") << "[(gid * "
            << net->array_size << "u) + i] = " << val_name(net->name)
            << "[(gid * " << net->array_size << "u) + i];\n";
        out << "    " << xz_name(net->name + "_next") << "[(gid * "
            << net->array_size << "u) + i] = " << xz_name(net->name)
            << "[(gid * " << net->array_size << "u) + i];\n";
        out << "  }\n";
      }

      std::unordered_set<std::string> tick_locals;
      std::unordered_set<std::string> tick_regs;
      for (const auto& net : module.nets) {
        if (net.array_size > 0) {
          continue;
        }
        if (net.type == NetType::kWire) {
          tick_locals.insert(net.name);
        } else if (net.type == NetType::kReg) {
          if (sequential_regs.count(net.name) > 0 ||
              initial_regs.count(net.name) > 0) {
            tick_regs.insert(net.name);
          }
        }
      }

      struct NbTemp {
        std::string val;
        std::string xz;
        int width = 0;
      };
      std::unordered_map<std::string, NbTemp> nb_map;

      auto collect_nb_targets = [&](const Statement& stmt,
                                    std::unordered_set<std::string>* out_set,
                                    const auto& self) -> void {
        if (stmt.kind == StatementKind::kAssign && stmt.assign.nonblocking &&
            !stmt.assign.lhs_index) {
          out_set->insert(stmt.assign.lhs);
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            self(inner, out_set, self);
          }
          for (const auto& inner : stmt.else_branch) {
            self(inner, out_set, self);
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              self(inner, out_set, self);
            }
          }
          for (const auto& inner : stmt.default_branch) {
            self(inner, out_set, self);
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            self(inner, out_set, self);
          }
        }
      };

      std::function<void(const Statement&, int)> emit_stmt;
      emit_stmt = [&](const Statement& stmt, int indent) {
        std::string pad(indent, ' ');
        if (stmt.kind == StatementKind::kAssign) {
          if (!stmt.assign.rhs) {
            return;
          }
          Lvalue4 lhs =
              build_lvalue4(stmt.assign, tick_locals, tick_regs, false);
          if (!lhs.ok) {
            return;
          }
          FsExpr rhs = emit_expr4_sized(*stmt.assign.rhs, lhs.width);
          if (lhs.is_array) {
            if (stmt.assign.nonblocking) {
              Lvalue4 next =
                  build_lvalue4(stmt.assign, tick_locals, tick_regs, true);
              if (!next.ok) {
                return;
              }
              if (!next.guard.empty()) {
                out << pad << "if " << next.guard << " {\n";
                out << pad << "  " << next.val << " = " << rhs.val << ";\n";
                out << pad << "  " << next.xz << " = " << rhs.xz << ";\n";
                out << pad << "}\n";
              } else {
                out << pad << next.val << " = " << rhs.val << ";\n";
                out << pad << next.xz << " = " << rhs.xz << ";\n";
              }
              return;
            }
            Lvalue4 next =
                build_lvalue4(stmt.assign, tick_locals, tick_regs, true);
            if (!lhs.guard.empty()) {
              out << pad << "if " << lhs.guard << " {\n";
              out << pad << "  " << lhs.val << " = " << rhs.val << ";\n";
              out << pad << "  " << lhs.xz << " = " << rhs.xz << ";\n";
              out << pad << "}\n";
            } else {
              out << pad << lhs.val << " = " << rhs.val << ";\n";
              out << pad << lhs.xz << " = " << rhs.xz << ";\n";
            }
            if (next.ok) {
              if (!next.guard.empty()) {
                out << pad << "if " << next.guard << " {\n";
                out << pad << "  " << next.val << " = " << rhs.val << ";\n";
                out << pad << "  " << next.xz << " = " << rhs.xz << ";\n";
                out << pad << "}\n";
              } else {
                out << pad << next.val << " = " << rhs.val << ";\n";
                out << pad << next.xz << " = " << rhs.xz << ";\n";
              }
            }
            return;
          }
          if (stmt.assign.nonblocking && nb_map.count(stmt.assign.lhs) > 0) {
            NbTemp temp = nb_map[stmt.assign.lhs];
            out << pad << temp.val << " = " << rhs.val << ";\n";
            out << pad << temp.xz << " = " << rhs.xz << ";\n";
            return;
          }
          if (!lhs.guard.empty()) {
            out << pad << "if " << lhs.guard << " {\n";
            out << pad << "  " << lhs.val << " = " << rhs.val << ";\n";
            out << pad << "  " << lhs.xz << " = " << rhs.xz << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << lhs.val << " = " << rhs.val << ";\n";
            out << pad << lhs.xz << " = " << rhs.xz << ";\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          FsExpr cond =
              stmt.condition ? emit_expr4(*stmt.condition)
                             : FsExpr{literal_for_width(0, 1),
                                      literal_for_width(0, 1),
                                      drive_full(1), 1};
          out << pad << "if (" << cond_bool(cond) << ") {\n";
          for (const auto& inner : stmt.then_branch) {
            emit_stmt(inner, indent + 2);
          }
          if (!stmt.else_branch.empty()) {
            out << pad << "} else {\n";
            for (const auto& inner : stmt.else_branch) {
              emit_stmt(inner, indent + 2);
            }
            out << pad << "}\n";
          } else {
            out << pad << "}\n";
          }
          return;
        }
      if (stmt.kind == StatementKind::kCase) {
        FsExpr case_expr = stmt.case_expr
                                 ? emit_expr4(*stmt.case_expr)
                                 : FsExpr{literal_for_width(0, 1),
                                          literal_for_width(0, 1),
                                          drive_full(1), 1};
        bool first_case = true;
        for (const auto& item : stmt.case_items) {
          std::string cond;
          for (const auto& label : item.labels) {
            std::string piece =
                emit_case_cond4(stmt.case_kind, case_expr, *label,
                                stmt.case_expr.get());
            if (!cond.empty()) {
              cond += " || ";
            }
            cond += piece;
            }
            if (cond.empty()) {
              continue;
            }
            if (first_case) {
              out << pad << "if (" << cond << ") {\n";
              first_case = false;
            } else {
              out << pad << "} else if (" << cond << ") {\n";
            }
            for (const auto& inner : item.body) {
              emit_stmt(inner, indent + 2);
            }
          }
          if (!stmt.default_branch.empty()) {
            out << pad << "} else {\n";
            for (const auto& inner : stmt.default_branch) {
              emit_stmt(inner, indent + 2);
            }
            out << pad << "}\n";
          } else if (!first_case) {
            out << pad << "}\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          out << pad << "{\n";
          for (const auto& inner : stmt.block) {
            emit_stmt(inner, indent + 2);
          }
          out << pad << "}\n";
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          out << pad << "// delay control ignored in MSL v0\n";
          for (const auto& inner : stmt.delay_body) {
            emit_stmt(inner, indent);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          out << pad << "// event control ignored in MSL v0\n";
          for (const auto& inner : stmt.event_body) {
            emit_stmt(inner, indent);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          out << pad << "// wait ignored in MSL v0\n";
          for (const auto& inner : stmt.wait_body) {
            emit_stmt(inner, indent);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          out << pad << "// forever ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kFork) {
          out << pad << "// fork/join executed sequentially in MSL v0\n";
          for (const auto& inner : stmt.fork_branches) {
            emit_stmt(inner, indent);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDisable) {
          out << pad << "// disable ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kEventTrigger) {
          out << pad << "// event trigger ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kTaskCall) {
          out << pad << "// task call ignored in MSL v0\n";
          return;
        }
      };

      for (const auto& block : module.always_blocks) {
        if (block.edge == EdgeKind::kCombinational ||
            block.edge == EdgeKind::kInitial) {
          continue;
        }
        out << "  // always @(";
        if (!block.sensitivity.empty()) {
          out << block.sensitivity;
        } else {
          out << (block.edge == EdgeKind::kPosedge ? "posedge " : "negedge ");
          out << block.clock;
        }
        out << ")\n";

        nb_map.clear();
        std::unordered_set<std::string> nb_targets;
        for (const auto& stmt : block.statements) {
          collect_nb_targets(stmt, &nb_targets, collect_nb_targets);
        }
        for (const auto& target : nb_targets) {
          int width = SignalWidth(module, target);
          std::string type = TypeForWidth(width);
          NbTemp temp;
          temp.width = width;
          temp.val = "nb_" + val_name(target);
          temp.xz = "nb_" + xz_name(target);
          nb_map[target] = temp;
          out << "  " << type << " " << temp.val << " = "
              << val_name(target) << "[gid];\n";
          out << "  " << type << " " << temp.xz << " = "
              << xz_name(target) << "[gid];\n";
        }

        for (const auto& stmt : block.statements) {
          emit_stmt(stmt, 2);
        }

        for (const auto& entry : nb_map) {
          out << "  " << val_name(entry.first) << "[gid] = "
              << entry.second.val << ";\n";
          out << "  " << xz_name(entry.first) << "[gid] = "
              << entry.second.xz << ";\n";
        }
      }
      out << "}\n";
    }
    return out.str();
  }

  std::unordered_set<std::string> sequential_regs;
  std::unordered_set<std::string> initial_regs;
  bool has_initial = false;
  for (const auto& block : module.always_blocks) {
    if (block.edge == EdgeKind::kCombinational ||
        block.edge == EdgeKind::kInitial) {
      continue;
    }
    for (const auto& stmt : block.statements) {
      CollectAssignedSignals(stmt, &sequential_regs);
    }
  }
  for (const auto& block : module.always_blocks) {
    if (block.edge != EdgeKind::kInitial) {
      continue;
    }
    has_initial = true;
    for (const auto& stmt : block.statements) {
      CollectAssignedSignals(stmt, &initial_regs);
    }
  }

  std::vector<std::string> reg_names;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      continue;
    }
    if (net.type == NetType::kReg && !IsOutputPort(module, net.name) &&
        (sequential_regs.count(net.name) > 0 ||
         initial_regs.count(net.name) > 0)) {
      reg_names.push_back(net.name);
    }
  }
  std::vector<std::string> init_reg_names;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      continue;
    }
    if (net.type == NetType::kReg && !IsOutputPort(module, net.name) &&
        initial_regs.count(net.name) > 0) {
      init_reg_names.push_back(net.name);
    }
  }
  std::vector<const Net*> array_nets;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      array_nets.push_back(&net);
    }
  }

  out << "kernel void gpga_" << module.name << "(";
  int buffer_index = 0;
  bool first = true;
  for (const auto& port : module.ports) {
    if (!first) {
      out << ",\n";
    }
    first = false;
    std::string qualifier =
        (port.dir == PortDir::kInput) ? "constant" : "device";
    std::string type = TypeForWidth(port.width);
    out << "  " << qualifier << " " << type << "* " << port.name
        << " [[buffer(" << buffer_index++ << ")]]";
  }
  for (const auto& reg : reg_names) {
    if (!first) {
      out << ",\n";
    }
    first = false;
    std::string type = TypeForWidth(SignalWidth(module, reg));
    out << "  device " << type << "* " << reg << " [[buffer("
        << buffer_index++ << ")]]";
  }
  for (const auto* net : array_nets) {
    if (!first) {
      out << ",\n";
    }
    first = false;
    std::string type = TypeForWidth(net->width);
    out << "  device " << type << "* " << net->name << " [[buffer("
        << buffer_index++ << ")]]";
  }
  if (!first) {
    out << ",\n";
  }
  out << "  constant GpgaParams& params [[buffer(" << buffer_index++
      << ")]],\n";
  out << "  uint gid [[thread_position_in_grid]]) {\n";
  out << "  if (gid >= params.count) {\n";
  out << "    return;\n";
  out << "  }\n";

  std::unordered_set<std::string> locals;
  std::unordered_set<std::string> regs;
  std::unordered_set<std::string> declared;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      continue;
    }
    if (net.type == NetType::kReg) {
      if (sequential_regs.count(net.name) > 0) {
        regs.insert(net.name);
      } else if (!IsOutputPort(module, net.name)) {
        locals.insert(net.name);
      }
      continue;
    }
    if (!IsOutputPort(module, net.name)) {
      locals.insert(net.name);
    }
  }

  auto driven = CollectDrivenSignals(module);
  for (const auto& net : module.nets) {
    if (net.array_size > 0 || net.type == NetType::kReg) {
      continue;
    }
    if (driven.count(net.name) > 0 || locals.count(net.name) == 0) {
      continue;
    }
    if (declared.insert(net.name).second) {
      std::string type = TypeForWidth(net.width);
      out << "  " << type << " " << net.name << " = " << ZeroForWidth(net.width)
          << ";\n";
    }
  }

  std::vector<size_t> ordered_assigns = OrderAssigns(module);
  std::unordered_map<std::string, std::vector<const Assign*>> partial_assigns;
  for (const auto& assign : module.assigns) {
    if (assign.lhs_has_range) {
      partial_assigns[assign.lhs].push_back(&assign);
    }
  }
  for (size_t index : ordered_assigns) {
    const auto& assign = module.assigns[index];
    if (!assign.rhs) {
      continue;
    }
    if (assign.lhs_has_range) {
      continue;
    }
    std::string expr = EmitExpr(*assign.rhs, module, locals, regs);
    int lhs_width = SignalWidth(module, assign.lhs);
    std::string sized = EmitExprSized(*assign.rhs, lhs_width, module, locals, regs);
    if (IsOutputPort(module, assign.lhs)) {
      out << "  " << assign.lhs << "[gid] = " << sized << ";\n";
    } else if (regs.count(assign.lhs) > 0) {
      out << "  " << assign.lhs << "[gid] = " << sized << ";\n";
    } else if (locals.count(assign.lhs) > 0) {
      if (declared.count(assign.lhs) == 0) {
        std::string type = TypeForWidth(SignalWidth(module, assign.lhs));
        out << "  " << type << " " << assign.lhs << " = " << sized << ";\n";
        declared.insert(assign.lhs);
      } else {
        out << "  " << assign.lhs << " = " << sized << ";\n";
      }
    } else {
      out << "  // Unmapped assign: " << assign.lhs << " = " << expr << ";\n";
    }
  }
  for (const auto& entry : partial_assigns) {
    const std::string& name = entry.first;
    int lhs_width = SignalWidth(module, name);
    std::string type = TypeForWidth(lhs_width);
    bool target_is_local =
        locals.count(name) > 0 && !IsOutputPort(module, name) &&
        regs.count(name) == 0;
    std::string temp = target_is_local ? name : ("__gpga_partial_" + name);
    std::string zero = ZeroForWidth(lhs_width);
    if (target_is_local) {
      if (declared.count(name) == 0) {
        out << "  " << type << " " << temp << " = " << zero << ";\n";
        declared.insert(name);
      } else {
        out << "  " << temp << " = " << zero << ";\n";
      }
    } else {
      out << "  " << type << " " << temp << " = " << zero << ";\n";
    }
    for (const auto* assign : entry.second) {
      int lo = std::min(assign->lhs_msb, assign->lhs_lsb);
      int hi = std::max(assign->lhs_msb, assign->lhs_lsb);
      int slice_width = hi - lo + 1;
      std::string rhs = EmitExprSized(*assign->rhs, slice_width, module, locals,
                                      regs);
      uint64_t mask = MaskForWidth64(slice_width);
      std::string suffix = (lhs_width > 32) ? "ul" : "u";
      std::string mask_literal = std::to_string(mask) + suffix;
      std::string shifted_mask =
          "(" + mask_literal + " << " + std::to_string(lo) + "u)";
      std::string cast = (lhs_width > 32) ? "(ulong)" : "(uint)";
      out << "  " << temp << " = (" << temp << " & ~" << shifted_mask << ") | (("
          << cast << rhs << " & " << mask_literal << ") << "
          << std::to_string(lo) << "u);\n";
    }
    if (!target_is_local) {
      if (IsOutputPort(module, name) || regs.count(name) > 0) {
        out << "  " << name << "[gid] = " << temp << ";\n";
      } else if (locals.count(name) > 0) {
        if (declared.count(name) == 0) {
          out << "  " << type << " " << name << " = " << temp << ";\n";
          declared.insert(name);
        } else {
          out << "  " << name << " = " << temp << ";\n";
        }
      } else {
        out << "  // Unmapped assign: " << name << " = " << temp << ";\n";
      }
    }
  }

  std::unordered_set<std::string> comb_targets;
  for (const auto& block : module.always_blocks) {
    if (block.edge != EdgeKind::kCombinational) {
      continue;
    }
    for (const auto& stmt : block.statements) {
      CollectAssignedSignals(stmt, &comb_targets);
    }
  }
  for (const auto& target : comb_targets) {
    if (locals.count(target) == 0 || declared.count(target) > 0) {
      continue;
    }
    std::string type = TypeForWidth(SignalWidth(module, target));
    out << "  " << type << " " << target << ";\n";
    declared.insert(target);
  }

  std::function<void(const Statement&, int)> emit_comb_stmt;
  auto emit_case_cond = [&](const std::string& case_value, int case_width,
                            const Expr& label) -> std::string {
    int label_width = ExprWidth(label, module);
    int target = std::max(case_width, label_width);
    std::string lhs = ExtendExpr(case_value, case_width, target);
    std::string rhs = EmitExpr(label, module, locals, regs);
    std::string rhs_ext = ExtendExpr(rhs, label_width, target);
    return "(" + lhs + " == " + rhs_ext + ")";
  };
  emit_comb_stmt = [&](const Statement& stmt, int indent) {
    std::string pad(indent, ' ');
    if (stmt.kind == StatementKind::kAssign) {
      if (!stmt.assign.rhs) {
        return;
      }
      std::string expr = EmitExpr(*stmt.assign.rhs, module, locals, regs);
      LvalueInfo lvalue =
          BuildLvalue(stmt.assign, module, locals, regs, false);
      if (!lvalue.ok) {
        out << pad << "// Unmapped combinational assign: " << stmt.assign.lhs
            << " = " << expr << ";\n";
        return;
      }
      std::string sized =
          EmitExprSized(*stmt.assign.rhs, lvalue.width, module, locals, regs);
        if (!lvalue.guard.empty()) {
          out << pad << "if " << lvalue.guard << " {\n";
          out << pad << "  " << lvalue.expr << " = " << sized << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lvalue.expr << " = " << sized << ";\n";
      }
      return;
    }
    if (stmt.kind == StatementKind::kIf) {
      std::string cond = stmt.condition
                             ? EmitExpr(*stmt.condition, module, locals, regs)
                             : "0u";
      out << pad << "if (" << cond << ") {\n";
      for (const auto& inner : stmt.then_branch) {
        emit_comb_stmt(inner, indent + 2);
      }
      if (!stmt.else_branch.empty()) {
        out << pad << "} else {\n";
        for (const auto& inner : stmt.else_branch) {
          emit_comb_stmt(inner, indent + 2);
        }
        out << pad << "}\n";
      } else {
        out << pad << "}\n";
      }
      return;
    }
    if (stmt.kind == StatementKind::kCase) {
      if (!stmt.case_expr) {
        return;
      }
      std::string case_value =
          EmitExpr(*stmt.case_expr, module, locals, regs);
      int case_width = ExprWidth(*stmt.case_expr, module);
      if (stmt.case_items.empty()) {
        for (const auto& inner : stmt.default_branch) {
          emit_comb_stmt(inner, indent);
        }
        return;
      }
      bool first = true;
      for (const auto& item : stmt.case_items) {
        std::string cond;
        for (const auto& label : item.labels) {
          std::string piece = emit_case_cond(case_value, case_width, *label);
          if (!cond.empty()) {
            cond += " || ";
          }
          cond += piece;
        }
        if (cond.empty()) {
          continue;
        }
        if (first) {
          out << pad << "if (" << cond << ") {\n";
          first = false;
        } else {
          out << pad << "} else if (" << cond << ") {\n";
        }
        for (const auto& inner : item.body) {
          emit_comb_stmt(inner, indent + 2);
        }
      }
      if (!stmt.default_branch.empty()) {
        out << pad << "} else {\n";
        for (const auto& inner : stmt.default_branch) {
          emit_comb_stmt(inner, indent + 2);
        }
        out << pad << "}\n";
      } else if (!first) {
        out << pad << "}\n";
      }
      return;
    }
    if (stmt.kind == StatementKind::kBlock) {
      out << pad << "{\n";
      for (const auto& inner : stmt.block) {
        emit_comb_stmt(inner, indent + 2);
      }
      out << pad << "}\n";
    }
  };

  for (const auto& block : module.always_blocks) {
    if (block.edge != EdgeKind::kCombinational) {
      continue;
    }
    for (const auto& stmt : block.statements) {
      emit_comb_stmt(stmt, 2);
    }
  }
  out << "}\n";

  if (has_initial) {
    out << "\n";
    out << "kernel void gpga_" << module.name << "_init(";
    buffer_index = 0;
    first = true;
    for (const auto& port : module.ports) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string qualifier =
          (port.dir == PortDir::kInput) ? "constant" : "device";
      std::string type = TypeForWidth(port.width);
      out << "  " << qualifier << " " << type << "* " << port.name
          << " [[buffer(" << buffer_index++ << ")]]";
    }
    for (const auto& reg : init_reg_names) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg));
      out << "  device " << type << "* " << reg << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto* net : array_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(net->width);
      out << "  device " << type << "* " << net->name << " [[buffer("
          << buffer_index++ << ")]]";
    }
    if (!first) {
      out << ",\n";
    }
    out << "  constant GpgaParams& params [[buffer(" << buffer_index++
        << ")]],\n";
    out << "  uint gid [[thread_position_in_grid]]) {\n";
    out << "  if (gid >= params.count) {\n";
    out << "    return;\n";
    out << "  }\n";

    std::unordered_set<std::string> init_locals;
    std::unordered_set<std::string> init_regs;
    std::unordered_set<std::string> init_declared;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (net.type == NetType::kReg) {
        if (initial_regs.count(net.name) > 0) {
          init_regs.insert(net.name);
        } else if (!IsOutputPort(module, net.name)) {
          init_locals.insert(net.name);
        }
        continue;
      }
      if (!IsOutputPort(module, net.name)) {
        init_locals.insert(net.name);
      }
    }

    std::unordered_set<std::string> init_targets;
    for (const auto& block : module.always_blocks) {
      if (block.edge != EdgeKind::kInitial) {
        continue;
      }
      for (const auto& stmt : block.statements) {
        CollectAssignedSignals(stmt, &init_targets);
      }
    }
    for (const auto& target : init_targets) {
      if (init_locals.count(target) == 0 || init_declared.count(target) > 0) {
        continue;
      }
      std::string type = TypeForWidth(SignalWidth(module, target));
      out << "  " << type << " " << target << ";\n";
      init_declared.insert(target);
    }

    std::function<void(const Statement&, int)> emit_init_stmt;
    auto emit_case_cond_init = [&](const std::string& case_value,
                                   int case_width,
                                   const Expr& label) -> std::string {
      int label_width = ExprWidth(label, module);
      int target = std::max(case_width, label_width);
      std::string lhs = ExtendExpr(case_value, case_width, target);
      std::string rhs = EmitExpr(label, module, init_locals, init_regs);
      std::string rhs_ext = ExtendExpr(rhs, label_width, target);
      return "(" + lhs + " == " + rhs_ext + ")";
    };
    emit_init_stmt = [&](const Statement& stmt, int indent) {
      std::string pad(indent, ' ');
      if (stmt.kind == StatementKind::kAssign) {
        if (!stmt.assign.rhs) {
          return;
        }
        std::string expr =
            EmitExpr(*stmt.assign.rhs, module, init_locals, init_regs);
        LvalueInfo lvalue =
            BuildLvalue(stmt.assign, module, init_locals, init_regs, false);
        if (!lvalue.ok) {
          out << pad << "// Unmapped init assign: " << stmt.assign.lhs
              << " = " << expr << ";\n";
          return;
        }
        std::string sized =
            EmitExprSized(*stmt.assign.rhs, lvalue.width, module, init_locals,
                          init_regs);
        if (!lvalue.guard.empty()) {
          out << pad << "if " << lvalue.guard << " {\n";
          out << pad << "  " << lvalue.expr << " = " << sized << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lvalue.expr << " = " << sized << ";\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kIf) {
        std::string cond = stmt.condition
                               ? EmitExpr(*stmt.condition, module, init_locals,
                                          init_regs)
                               : "0u";
        out << pad << "if (" << cond << ") {\n";
        for (const auto& inner : stmt.then_branch) {
          emit_init_stmt(inner, indent + 2);
        }
        if (!stmt.else_branch.empty()) {
          out << pad << "} else {\n";
          for (const auto& inner : stmt.else_branch) {
            emit_init_stmt(inner, indent + 2);
          }
          out << pad << "}\n";
        } else {
          out << pad << "}\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kCase) {
        if (!stmt.case_expr) {
          return;
        }
        std::string case_value =
            EmitExpr(*stmt.case_expr, module, init_locals, init_regs);
        int case_width = ExprWidth(*stmt.case_expr, module);
        if (stmt.case_items.empty()) {
          for (const auto& inner : stmt.default_branch) {
            emit_init_stmt(inner, indent);
          }
          return;
        }
        bool first = true;
        for (const auto& item : stmt.case_items) {
          std::string cond;
          for (const auto& label : item.labels) {
            std::string piece =
                emit_case_cond_init(case_value, case_width, *label);
            if (!cond.empty()) {
              cond += " || ";
            }
            cond += piece;
          }
          if (cond.empty()) {
            continue;
          }
          if (first) {
            out << pad << "if (" << cond << ") {\n";
            first = false;
          } else {
            out << pad << "} else if (" << cond << ") {\n";
          }
          for (const auto& inner : item.body) {
            emit_init_stmt(inner, indent + 2);
          }
        }
        if (!stmt.default_branch.empty()) {
          out << pad << "} else {\n";
          for (const auto& inner : stmt.default_branch) {
            emit_init_stmt(inner, indent + 2);
          }
          out << pad << "}\n";
        } else if (!first) {
          out << pad << "}\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kBlock) {
        out << pad << "{\n";
        for (const auto& inner : stmt.block) {
          emit_init_stmt(inner, indent + 2);
        }
        out << pad << "}\n";
      }
    };

    for (const auto& block : module.always_blocks) {
      if (block.edge != EdgeKind::kInitial) {
        continue;
      }
      for (const auto& stmt : block.statements) {
        emit_init_stmt(stmt, 2);
      }
    }
    out << "}\n";
  }

  bool has_sequential = false;
  for (const auto& block : module.always_blocks) {
    if (block.edge == EdgeKind::kPosedge ||
        block.edge == EdgeKind::kNegedge) {
      has_sequential = true;
      break;
    }
  }

  if (has_sequential) {
    out << "\n";
    out << "kernel void gpga_" << module.name << "_tick(";
    buffer_index = 0;
    first = true;
    for (const auto& port : module.ports) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string qualifier =
          (port.dir == PortDir::kInput) ? "constant" : "device";
      std::string type = TypeForWidth(port.width);
      out << "  " << qualifier << " " << type << "* " << port.name
          << " [[buffer(" << buffer_index++ << ")]]";
    }
    for (const auto& reg : reg_names) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg));
      out << "  device " << type << "* " << reg << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto* net : array_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(net->width);
      out << "  device " << type << "* " << net->name << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto* net : array_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(net->width);
      out << "  device " << type << "* " << net->name << "_next [[buffer("
          << buffer_index++ << ")]]";
    }
    if (!first) {
      out << ",\n";
    }
    out << "  constant GpgaParams& params [[buffer(" << buffer_index++
        << ")]],\n";
    out << "  uint gid [[thread_position_in_grid]]) {\n";
    out << "  if (gid >= params.count) {\n";
    out << "    return;\n";
    out << "  }\n";
    out << "  // Tick kernel: sequential logic (posedge/negedge in v0).\n";
    for (const auto* net : array_nets) {
      out << "  for (uint i = 0u; i < " << net->array_size << "u; ++i) {\n";
      out << "    " << net->name << "_next[(gid * " << net->array_size
          << "u) + i] = " << net->name << "[(gid * " << net->array_size
          << "u) + i];\n";
      out << "  }\n";
    }

    std::unordered_set<std::string> tick_locals;
    std::unordered_set<std::string> tick_regs;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (net.type == NetType::kWire) {
        tick_locals.insert(net.name);
      } else if (net.type == NetType::kReg) {
        if (sequential_regs.count(net.name) > 0 ||
            initial_regs.count(net.name) > 0) {
          tick_regs.insert(net.name);
        }
      }
    }

    auto scalar_lvalue = [&](const std::string& name) -> std::string {
      if (IsOutputPort(module, name) || tick_regs.count(name) > 0) {
        return name + "[gid]";
      }
      return name;
    };

    std::function<void(const Statement&, int,
                       const std::unordered_map<std::string, std::string>&)>
        emit_stmt;
    auto emit_case_cond_tick = [&](const std::string& case_value,
                                   int case_width,
                                   const Expr& label) -> std::string {
      int label_width = ExprWidth(label, module);
      int target = std::max(case_width, label_width);
      std::string lhs = ExtendExpr(case_value, case_width, target);
      std::string rhs = EmitExpr(label, module, tick_locals, tick_regs);
      std::string rhs_ext = ExtendExpr(rhs, label_width, target);
      return "(" + lhs + " == " + rhs_ext + ")";
    };
    emit_stmt = [&](const Statement& stmt, int indent,
                    const std::unordered_map<std::string, std::string>& nb_map) {
      std::string pad(indent, ' ');
      if (stmt.kind == StatementKind::kAssign) {
        if (!stmt.assign.rhs) {
          return;
        }
        std::string expr =
            EmitExpr(*stmt.assign.rhs, module, tick_locals, tick_regs);
        LvalueInfo lvalue =
            BuildLvalue(stmt.assign, module, tick_locals, tick_regs, false);
        if (!lvalue.ok) {
          out << pad << "// Unmapped sequential assign: " << stmt.assign.lhs
              << " = " << expr << ";\n";
          return;
        }
        std::string sized =
            EmitExprSized(*stmt.assign.rhs, lvalue.width, module, tick_locals,
                          tick_regs);
        if (lvalue.is_array) {
          if (stmt.assign.nonblocking) {
            LvalueInfo next =
                BuildLvalue(stmt.assign, module, tick_locals, tick_regs, true);
            if (!next.ok) {
              out << pad << "// Unmapped sequential assign: "
                  << stmt.assign.lhs << " = " << expr << ";\n";
              return;
            }
            if (!next.guard.empty()) {
              out << pad << "if " << next.guard << " {\n";
              out << pad << "  " << next.expr << " = " << sized << ";\n";
              out << pad << "}\n";
            } else {
              out << pad << next.expr << " = " << sized << ";\n";
            }
            return;
          }
          LvalueInfo next =
              BuildLvalue(stmt.assign, module, tick_locals, tick_regs, true);
          if (!lvalue.guard.empty()) {
            out << pad << "if " << lvalue.guard << " {\n";
            out << pad << "  " << lvalue.expr << " = " << sized << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << lvalue.expr << " = " << sized << ";\n";
          }
          if (!next.ok) {
            return;
          }
          if (!next.guard.empty()) {
            out << pad << "if " << next.guard << " {\n";
            out << pad << "  " << next.expr << " = " << sized << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << next.expr << " = " << sized << ";\n";
          }
          return;
        }
        if (stmt.assign.nonblocking && !stmt.assign.lhs_index) {
          auto it = nb_map.find(stmt.assign.lhs);
          if (it != nb_map.end()) {
            out << pad << it->second << " = " << sized << ";\n";
            return;
          }
        }
        if (!lvalue.guard.empty()) {
          out << pad << "if " << lvalue.guard << " {\n";
          out << pad << "  " << lvalue.expr << " = " << sized << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lvalue.expr << " = " << sized << ";\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kIf) {
        std::string cond = stmt.condition
                               ? EmitExpr(*stmt.condition, module, tick_locals,
                                          tick_regs)
                               : "0u";
        out << pad << "if (" << cond << ") {\n";
        for (const auto& inner : stmt.then_branch) {
          emit_stmt(inner, indent + 2, nb_map);
        }
        if (!stmt.else_branch.empty()) {
          out << pad << "} else {\n";
          for (const auto& inner : stmt.else_branch) {
            emit_stmt(inner, indent + 2, nb_map);
          }
          out << pad << "}\n";
        } else {
          out << pad << "}\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kCase) {
        if (!stmt.case_expr) {
          return;
        }
        std::string case_value =
            EmitExpr(*stmt.case_expr, module, tick_locals, tick_regs);
        int case_width = ExprWidth(*stmt.case_expr, module);
        if (stmt.case_items.empty()) {
          for (const auto& inner : stmt.default_branch) {
            emit_stmt(inner, indent, nb_map);
          }
          return;
        }
        bool first = true;
        for (const auto& item : stmt.case_items) {
          std::string cond;
          for (const auto& label : item.labels) {
            std::string piece =
                emit_case_cond_tick(case_value, case_width, *label);
            if (!cond.empty()) {
              cond += " || ";
            }
            cond += piece;
          }
          if (cond.empty()) {
            continue;
          }
          if (first) {
            out << pad << "if (" << cond << ") {\n";
            first = false;
          } else {
            out << pad << "} else if (" << cond << ") {\n";
          }
          for (const auto& inner : item.body) {
            emit_stmt(inner, indent + 2, nb_map);
          }
        }
        if (!stmt.default_branch.empty()) {
          out << pad << "} else {\n";
          for (const auto& inner : stmt.default_branch) {
            emit_stmt(inner, indent + 2, nb_map);
          }
          out << pad << "}\n";
        } else if (!first) {
          out << pad << "}\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kBlock) {
        out << pad << "{\n";
        for (const auto& inner : stmt.block) {
          emit_stmt(inner, indent + 2, nb_map);
        }
        out << pad << "}\n";
      }
    };

    auto collect_nb_targets =
        [&](const Statement& stmt, std::unordered_set<std::string>* out_set,
            const auto& self) -> void {
      if (stmt.kind == StatementKind::kAssign && stmt.assign.nonblocking &&
          !stmt.assign.lhs_index) {
        out_set->insert(stmt.assign.lhs);
        return;
      }
      if (stmt.kind == StatementKind::kIf) {
        for (const auto& inner : stmt.then_branch) {
          self(inner, out_set, self);
        }
        for (const auto& inner : stmt.else_branch) {
          self(inner, out_set, self);
        }
        return;
      }
      if (stmt.kind == StatementKind::kBlock) {
        for (const auto& inner : stmt.block) {
          self(inner, out_set, self);
        }
        return;
      }
      if (stmt.kind == StatementKind::kCase) {
        for (const auto& item : stmt.case_items) {
          for (const auto& inner : item.body) {
            self(inner, out_set, self);
          }
        }
        for (const auto& inner : stmt.default_branch) {
          self(inner, out_set, self);
        }
      }
    };

    for (const auto& block : module.always_blocks) {
      if (block.edge == EdgeKind::kCombinational ||
          block.edge == EdgeKind::kInitial) {
        continue;
      }
      out << "  // always @(";
      if (!block.sensitivity.empty()) {
        out << block.sensitivity;
      } else {
        out << (block.edge == EdgeKind::kPosedge ? "posedge " : "negedge ");
        out << block.clock;
      }
      out << ")\n";

      std::unordered_set<std::string> nb_targets;
      for (const auto& stmt : block.statements) {
        collect_nb_targets(stmt, &nb_targets, collect_nb_targets);
      }
      std::unordered_map<std::string, std::string> nb_map;
      for (const auto& target : nb_targets) {
        if (!IsOutputPort(module, target) && tick_regs.count(target) == 0) {
          continue;
        }
        std::string temp = "nb_" + target;
        nb_map[target] = temp;
        std::string type = TypeForWidth(SignalWidth(module, target));
        out << "  " << type << " " << temp << " = " << scalar_lvalue(target)
            << ";\n";
      }

      for (const auto& stmt : block.statements) {
        emit_stmt(stmt, 2, nb_map);
      }

      for (const auto& entry : nb_map) {
        out << "  " << scalar_lvalue(entry.first) << " = " << entry.second
            << ";\n";
      }
    }
    out << "}\n";
  }
  return out.str();
}

}  // namespace gpga
