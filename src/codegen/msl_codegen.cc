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

const Task* FindTask(const Module& module, const std::string& name) {
  for (const auto& task : module.tasks) {
    if (task.name == name) {
      return &task;
    }
  }
  return nullptr;
}

const std::unordered_map<std::string, int>* g_task_arg_widths = nullptr;
const std::unordered_map<std::string, bool>* g_task_arg_signed = nullptr;
const std::unordered_map<std::string, bool>* g_task_arg_real = nullptr;

int SignalWidth(const Module& module, const std::string& name) {
  if (g_task_arg_widths) {
    auto it = g_task_arg_widths->find(name);
    if (it != g_task_arg_widths->end()) {
      return it->second;
    }
  }
  if (name == "__gpga_time") {
    return 64;
  }
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
  if (g_task_arg_signed) {
    auto it = g_task_arg_signed->find(name);
    if (it != g_task_arg_signed->end()) {
      return it->second;
    }
  }
  if (name == "__gpga_time") {
    return false;
  }
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

bool SignalIsReal(const Module& module, const std::string& name) {
  if (g_task_arg_real) {
    auto it = g_task_arg_real->find(name);
    if (it != g_task_arg_real->end()) {
      return it->second;
    }
  }
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return net.is_real;
    }
  }
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return port.is_real;
    }
  }
  return false;
}

bool IsRealLiteralExpr(const Expr& expr) {
  if (expr.kind != ExprKind::kNumber) {
    return false;
  }
  if (expr.is_real_literal) {
    return true;
  }
  if (!expr.has_width || expr.number_width != 64) {
    return false;
  }
  if (expr.has_base || expr.is_signed) {
    return false;
  }
  if (expr.x_bits != 0 || expr.z_bits != 0) {
    return false;
  }
  return true;
}

bool IsArrayNet(const Module& module, const std::string& name,
                int* element_width, int* array_size);

bool ExprIsRealValue(const Expr& expr, const Module& module) {
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      return SignalIsReal(module, expr.ident);
    case ExprKind::kNumber:
      return IsRealLiteralExpr(expr);
    case ExprKind::kString:
      return false;
    case ExprKind::kUnary:
      if (expr.unary_op == '+' || expr.unary_op == '-') {
        return expr.operand ? ExprIsRealValue(*expr.operand, module) : false;
      }
      return false;
    case ExprKind::kBinary:
      if (expr.op == '+' || expr.op == '-' || expr.op == '*' ||
          expr.op == '/' || expr.op == 'p') {
        return (expr.lhs && ExprIsRealValue(*expr.lhs, module)) ||
               (expr.rhs && ExprIsRealValue(*expr.rhs, module));
      }
      return false;
    case ExprKind::kTernary:
      return (expr.then_expr && ExprIsRealValue(*expr.then_expr, module)) ||
             (expr.else_expr && ExprIsRealValue(*expr.else_expr, module));
    case ExprKind::kIndex: {
      const Expr* base = expr.base.get();
      while (base && base->kind == ExprKind::kIndex) {
        base = base->base.get();
      }
      if (!base || base->kind != ExprKind::kIdentifier) {
        return false;
      }
      if (!SignalIsReal(module, base->ident)) {
        return false;
      }
      return IsArrayNet(module, base->ident, nullptr, nullptr);
    }
    case ExprKind::kSelect:
    case ExprKind::kConcat:
      return false;
    case ExprKind::kCall:
      return expr.ident == "$realtime" || expr.ident == "$itor" ||
             expr.ident == "$bitstoreal";
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

std::string UnsignedCastForWidth(int width) {
  return (width > 32) ? "(ulong)" : "(uint)";
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
  if (ExprIsRealValue(expr, module)) {
    return true;
  }
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      return SignalSigned(module, expr.ident);
    case ExprKind::kNumber:
      return expr.is_signed || !expr.has_base;
    case ExprKind::kString:
      return false;
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
          expr.unary_op == '^' || expr.unary_op == '!' ||
          expr.unary_op == 'B') {
        return false;
      }
      if (expr.unary_op == '-' && expr.operand &&
          expr.operand->kind == ExprKind::kNumber) {
        return true;
      }
      return expr.operand ? ExprSigned(*expr.operand, module) : false;
    case ExprKind::kBinary: {
      if (expr.op == 'E' || expr.op == 'N' || expr.op == 'C' ||
          expr.op == 'c' || expr.op == 'W' || expr.op == 'w' ||
          expr.op == '<' || expr.op == '>' || expr.op == 'L' ||
          expr.op == 'G' || expr.op == 'A' || expr.op == 'O') {
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
    case ExprKind::kString:
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

struct SystemTaskInfo {
  bool has_system_tasks = false;
  size_t max_args = 0;
  size_t monitor_max_args = 0;
  std::vector<const Statement*> monitor_stmts;
  std::unordered_map<const Statement*, uint32_t> monitor_ids;
  size_t strobe_max_args = 0;
  std::vector<const Statement*> strobe_stmts;
  std::unordered_map<const Statement*, uint32_t> strobe_ids;
  std::vector<std::string> string_table;
  std::unordered_map<std::string, uint32_t> string_ids;
};

bool IsSystemTaskName(const std::string& name) {
  return !name.empty() && name[0] == '$';
}

bool TaskTreatsIdentifierAsString(const std::string& name) {
  return name == "$dumpvars" || name == "$readmemh" || name == "$readmemb";
}

uint32_t AddSystemTaskString(SystemTaskInfo* info,
                             const std::string& value) {
  auto it = info->string_ids.find(value);
  if (it != info->string_ids.end()) {
    return it->second;
  }
  uint32_t id = static_cast<uint32_t>(info->string_table.size());
  info->string_table.push_back(value);
  info->string_ids[value] = id;
  return id;
}

void CollectSystemTaskInfo(const Statement& stmt, SystemTaskInfo* info) {
  if (!info) {
    return;
  }
  if (stmt.kind == StatementKind::kTaskCall &&
      IsSystemTaskName(stmt.task_name)) {
    info->has_system_tasks = true;
    info->max_args = std::max(info->max_args, stmt.task_args.size());
    if (stmt.task_name == "$monitor") {
      info->monitor_max_args =
          std::max(info->monitor_max_args, stmt.task_args.size());
      info->monitor_stmts.push_back(&stmt);
    }
    if (stmt.task_name == "$strobe") {
      info->strobe_max_args =
          std::max(info->strobe_max_args, stmt.task_args.size());
      info->strobe_stmts.push_back(&stmt);
    }
    bool ident_as_string = TaskTreatsIdentifierAsString(stmt.task_name);
    for (const auto& arg : stmt.task_args) {
      if (!arg) {
        continue;
      }
      if (arg->kind == ExprKind::kString) {
        AddSystemTaskString(info, arg->string_value);
      } else if (ident_as_string && arg->kind == ExprKind::kIdentifier) {
        AddSystemTaskString(info, arg->ident);
      }
    }
  }
  if (stmt.kind == StatementKind::kIf) {
    for (const auto& inner : stmt.then_branch) {
      CollectSystemTaskInfo(inner, info);
    }
    for (const auto& inner : stmt.else_branch) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kCase) {
    for (const auto& item : stmt.case_items) {
      for (const auto& inner : item.body) {
        CollectSystemTaskInfo(inner, info);
      }
    }
    for (const auto& inner : stmt.default_branch) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFor) {
    for (const auto& inner : stmt.for_body) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kWhile) {
    for (const auto& inner : stmt.while_body) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kRepeat) {
    for (const auto& inner : stmt.repeat_body) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kDelay) {
    for (const auto& inner : stmt.delay_body) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kEventControl) {
    for (const auto& inner : stmt.event_body) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kWait) {
    for (const auto& inner : stmt.wait_body) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectSystemTaskInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      CollectSystemTaskInfo(inner, info);
    }
  }
}

SystemTaskInfo BuildSystemTaskInfo(const Module& module) {
  SystemTaskInfo info;
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectSystemTaskInfo(stmt, &info);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      CollectSystemTaskInfo(stmt, &info);
    }
  }
  info.monitor_ids.reserve(info.monitor_stmts.size());
  for (size_t i = 0; i < info.monitor_stmts.size(); ++i) {
    info.monitor_ids[info.monitor_stmts[i]] = static_cast<uint32_t>(i);
  }
  info.strobe_ids.reserve(info.strobe_stmts.size());
  for (size_t i = 0; i < info.strobe_stmts.size(); ++i) {
    info.strobe_ids[info.strobe_stmts[i]] = static_cast<uint32_t>(i);
  }
  return info;
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
  if (ExprIsRealValue(expr, module)) {
    return 64;
  }
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      return SignalWidth(module, expr.ident);
    case ExprKind::kNumber:
      if (expr.has_width && expr.number_width > 0) {
        return expr.number_width;
      }
      return std::max(32, MinimalWidth(expr.number));
    case ExprKind::kString:
      return 0;
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
      if (expr.op == 'E' || expr.op == 'N' || expr.op == 'C' ||
          expr.op == 'c' || expr.op == 'W' || expr.op == 'w' ||
          expr.op == '<' || expr.op == '>' || expr.op == 'L' ||
          expr.op == 'G' || expr.op == 'A' || expr.op == 'O') {
        return 1;
      }
      if (expr.op == 'l' || expr.op == 'r' || expr.op == 'R') {
        return expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      }
      if (expr.op == 'p') {
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
      if (expr.ident == "$realtobits") {
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
    case 'C':
      return "==";
    case 'c':
      return "!=";
    case 'W':
      return "==";
    case 'w':
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
std::string EmitRealValueExpr(const Expr& expr, const Module& module,
                              const std::unordered_set<std::string>& locals,
                              const std::unordered_set<std::string>& regs);
std::string EmitRealBitsExpr(const Expr& expr, const Module& module,
                             const std::unordered_set<std::string>& locals,
                             const std::unordered_set<std::string>& regs);
std::string EmitRealToIntExpr(const Expr& expr, int target_width,
                              bool signed_target, const Module& module,
                              const std::unordered_set<std::string>& locals,
                              const std::unordered_set<std::string>& regs);
std::string EmitCondExpr(const Expr& expr, const Module& module,
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

std::string EmitRealValueExpr(const Expr& expr, const Module& module,
                              const std::unordered_set<std::string>& locals,
                              const std::unordered_set<std::string>& regs) {
  auto emit_int_as_real = [&](const Expr& value_expr) -> std::string {
    int width = ExprWidth(value_expr, module);
    bool signed_expr = ExprSigned(value_expr, module);
    std::string raw = EmitExpr(value_expr, module, locals, regs);
    std::string cast;
    if (width > 32) {
      cast = signed_expr ? "(double)(long)" : "(double)(ulong)";
    } else {
      cast = signed_expr ? "(double)(int)" : "(double)(uint)";
    }
    return cast + "(" + raw + ")";
  };

  switch (expr.kind) {
    case ExprKind::kIdentifier: {
      const Port* port = FindPort(module, expr.ident);
      if (SignalIsReal(module, expr.ident)) {
        if (port) {
          return "gpga_bits_to_real(" + port->name + "[gid])";
        }
        if (regs.count(expr.ident) > 0) {
          return "gpga_bits_to_real(" + expr.ident + "[gid])";
        }
        if (locals.count(expr.ident) > 0) {
          return "gpga_bits_to_real(" + expr.ident + ")";
        }
        return "gpga_bits_to_real(" + expr.ident + ")";
      }
      return emit_int_as_real(expr);
    }
    case ExprKind::kNumber:
      if (IsRealLiteralExpr(expr)) {
        return "gpga_bits_to_real(" + std::to_string(expr.value_bits) + "ul)";
      }
      return emit_int_as_real(expr);
    case ExprKind::kString:
      return "0.0";
    case ExprKind::kUnary: {
      std::string operand = expr.operand
                                ? EmitRealValueExpr(*expr.operand, module,
                                                    locals, regs)
                                : "0.0";
      if (expr.unary_op == '+') {
        return operand;
      }
      if (expr.unary_op == '-') {
        return "(-" + operand + ")";
      }
      return "0.0";
    }
    case ExprKind::kBinary: {
      std::string lhs = expr.lhs ? EmitRealValueExpr(*expr.lhs, module, locals,
                                                     regs)
                                 : "0.0";
      std::string rhs = expr.rhs ? EmitRealValueExpr(*expr.rhs, module, locals,
                                                     regs)
                                 : "0.0";
      if (expr.op == '+' || expr.op == '-' || expr.op == '*' ||
          expr.op == '/') {
        return "(" + lhs + " " + std::string(1, expr.op) + " " + rhs + ")";
      }
      if (expr.op == 'p') {
        return "pow(" + lhs + ", " + rhs + ")";
      }
      return "0.0";
    }
    case ExprKind::kTernary: {
      std::string cond = expr.condition
                             ? EmitCondExpr(*expr.condition, module, locals,
                                            regs)
                             : "false";
      std::string then_expr =
          expr.then_expr
              ? EmitRealValueExpr(*expr.then_expr, module, locals, regs)
              : "0.0";
      std::string else_expr =
          expr.else_expr
              ? EmitRealValueExpr(*expr.else_expr, module, locals, regs)
              : "0.0";
      return "((" + cond + ") ? (" + then_expr + ") : (" + else_expr + "))";
    }
    case ExprKind::kIndex: {
      if (!expr.base || !expr.index) {
        return "0.0";
      }
      if (expr.base->kind == ExprKind::kIdentifier) {
        int element_width = 0;
        int array_size = 0;
        if (IsArrayNet(module, expr.base->ident, &element_width, &array_size)) {
          std::string index =
              EmitExpr(*expr.index, module, locals, regs);
          std::string idx = "uint(" + index + ")";
          std::string base =
              "((gid * " + std::to_string(array_size) + "u) + " + idx + ")";
          std::string bounds =
              "(" + idx + " < " + std::to_string(array_size) + "u)";
          if (SignalIsReal(module, expr.base->ident)) {
            return "((" + bounds + ") ? gpga_bits_to_real(" +
                   expr.base->ident + "[" + base + "]) : 0.0)";
          }
        }
      }
      return emit_int_as_real(expr);
    }
    case ExprKind::kCall:
      if (expr.ident == "$realtime") {
        return "(double)__gpga_time";
      }
      if (expr.ident == "$itor") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          return emit_int_as_real(*expr.call_args.front());
        }
        return "0.0";
      }
      if (expr.ident == "$bitstoreal") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          std::string bits =
              EmitExprSized(*expr.call_args.front(), 64, module, locals, regs);
          return "gpga_bits_to_real(" + bits + ")";
        }
        return "0.0";
      }
      return "0.0";
    case ExprKind::kSelect:
    case ExprKind::kConcat:
      return "0.0";
  }
  return "0.0";
}

std::string EmitRealBitsExpr(const Expr& expr, const Module& module,
                             const std::unordered_set<std::string>& locals,
                             const std::unordered_set<std::string>& regs) {
  if (IsRealLiteralExpr(expr)) {
    return std::to_string(expr.value_bits) + "ul";
  }
  if (expr.kind == ExprKind::kNumber && expr.value_bits == 0 &&
      expr.x_bits == 0 && expr.z_bits == 0) {
    return "0ul";
  }
  return "gpga_real_to_bits(" +
         EmitRealValueExpr(expr, module, locals, regs) + ")";
}

std::string EmitRealToIntExpr(const Expr& expr, int target_width,
                              bool signed_target, const Module& module,
                              const std::unordered_set<std::string>& locals,
                              const std::unordered_set<std::string>& regs) {
  std::string real_value = EmitRealValueExpr(expr, module, locals, regs);
  std::string cast;
  if (target_width > 32) {
    cast = signed_target ? "(long)" : "(ulong)";
  } else {
    cast = signed_target ? "(int)" : "(uint)";
  }
  std::string raw = cast + "(" + real_value + ")";
  return MaskForWidthExpr(raw, target_width);
}

std::string EmitCondExpr(const Expr& expr, const Module& module,
                         const std::unordered_set<std::string>& locals,
                         const std::unordered_set<std::string>& regs) {
  if (ExprIsRealValue(expr, module)) {
    std::string real_val = EmitRealValueExpr(expr, module, locals, regs);
    return "(" + real_val + " != 0.0)";
  }
  std::string raw = EmitExpr(expr, module, locals, regs);
  int width = ExprWidth(expr, module);
  std::string masked = MaskForWidthExpr(raw, width);
  std::string zero = ZeroForWidth(width);
  return "(" + masked + " != " + zero + ")";
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
  if (stmt.kind == StatementKind::kAssign ||
      stmt.kind == StatementKind::kForce ||
      stmt.kind == StatementKind::kRelease) {
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

void CollectReadSignalsExpr(const Expr& expr,
                            std::unordered_set<std::string>* out) {
  if (!out) {
    return;
  }
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      out->insert(expr.ident);
      return;
    case ExprKind::kUnary:
      if (expr.operand) {
        CollectReadSignalsExpr(*expr.operand, out);
      }
      return;
    case ExprKind::kBinary:
      if (expr.lhs) {
        CollectReadSignalsExpr(*expr.lhs, out);
      }
      if (expr.rhs) {
        CollectReadSignalsExpr(*expr.rhs, out);
      }
      return;
    case ExprKind::kTernary:
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
    case ExprKind::kSelect:
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
    case ExprKind::kIndex:
      if (expr.base) {
        CollectReadSignalsExpr(*expr.base, out);
      }
      if (expr.index) {
        CollectReadSignalsExpr(*expr.index, out);
      }
      return;
    case ExprKind::kCall:
      for (const auto& arg : expr.call_args) {
        if (arg) {
          CollectReadSignalsExpr(*arg, out);
        }
      }
      return;
    case ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        if (element) {
          CollectReadSignalsExpr(*element, out);
        }
      }
      if (expr.repeat_expr) {
        CollectReadSignalsExpr(*expr.repeat_expr, out);
      }
      return;
    case ExprKind::kNumber:
    case ExprKind::kString:
      return;
  }
}

void CollectReadSignals(const Statement& stmt,
                        std::unordered_set<std::string>* out) {
  if (!out) {
    return;
  }
  if (stmt.kind == StatementKind::kAssign) {
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
  if (stmt.kind == StatementKind::kIf) {
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
  if (stmt.kind == StatementKind::kCase) {
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
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFor) {
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
  if (stmt.kind == StatementKind::kWhile) {
    if (stmt.while_condition) {
      CollectReadSignalsExpr(*stmt.while_condition, out);
    }
    for (const auto& inner : stmt.while_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kRepeat) {
    if (stmt.repeat_count) {
      CollectReadSignalsExpr(*stmt.repeat_count, out);
    }
    for (const auto& inner : stmt.repeat_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kDelay) {
    if (stmt.delay) {
      CollectReadSignalsExpr(*stmt.delay, out);
    }
    for (const auto& inner : stmt.delay_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kEventControl) {
    if (!stmt.event_items.empty()) {
      for (const auto& item : stmt.event_items) {
        if (item.expr) {
          CollectReadSignalsExpr(*item.expr, out);
        }
      }
    } else if (stmt.event_expr) {
      CollectReadSignalsExpr(*stmt.event_expr, out);
    }
    for (const auto& inner : stmt.event_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kWait) {
    if (stmt.wait_condition) {
      CollectReadSignalsExpr(*stmt.wait_condition, out);
    }
    for (const auto& inner : stmt.wait_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      CollectReadSignals(inner, out);
    }
    return;
  }
  if (stmt.kind == StatementKind::kTaskCall) {
    for (const auto& arg : stmt.task_args) {
      if (arg) {
        CollectReadSignalsExpr(*arg, out);
      }
    }
    return;
  }
}

bool ExprUsesPower(const Expr& expr) {
  switch (expr.kind) {
    case ExprKind::kUnary:
      return expr.operand && ExprUsesPower(*expr.operand);
    case ExprKind::kBinary:
      if (expr.op == 'p') {
        return true;
      }
      return (expr.lhs && ExprUsesPower(*expr.lhs)) ||
             (expr.rhs && ExprUsesPower(*expr.rhs));
    case ExprKind::kTernary:
      return (expr.condition && ExprUsesPower(*expr.condition)) ||
             (expr.then_expr && ExprUsesPower(*expr.then_expr)) ||
             (expr.else_expr && ExprUsesPower(*expr.else_expr));
    case ExprKind::kSelect:
      return (expr.base && ExprUsesPower(*expr.base)) ||
             (expr.msb_expr && ExprUsesPower(*expr.msb_expr)) ||
             (expr.lsb_expr && ExprUsesPower(*expr.lsb_expr));
    case ExprKind::kIndex:
      return (expr.base && ExprUsesPower(*expr.base)) ||
             (expr.index && ExprUsesPower(*expr.index));
    case ExprKind::kCall:
      for (const auto& arg : expr.call_args) {
        if (arg && ExprUsesPower(*arg)) {
          return true;
        }
      }
      return false;
    case ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        if (element && ExprUsesPower(*element)) {
          return true;
        }
      }
      if (expr.repeat_expr && ExprUsesPower(*expr.repeat_expr)) {
        return true;
      }
      return false;
    case ExprKind::kIdentifier:
    case ExprKind::kNumber:
    case ExprKind::kString:
      return false;
  }
  return false;
}

bool StatementUsesPower(const Statement& stmt) {
  if (stmt.kind == StatementKind::kAssign ||
      stmt.kind == StatementKind::kForce ||
      stmt.kind == StatementKind::kRelease) {
    if (stmt.assign.rhs && ExprUsesPower(*stmt.assign.rhs)) {
      return true;
    }
    if (stmt.assign.lhs_index && ExprUsesPower(*stmt.assign.lhs_index)) {
      return true;
    }
    for (const auto& index : stmt.assign.lhs_indices) {
      if (index && ExprUsesPower(*index)) {
        return true;
      }
    }
    if (stmt.assign.lhs_msb_expr && ExprUsesPower(*stmt.assign.lhs_msb_expr)) {
      return true;
    }
    if (stmt.assign.lhs_lsb_expr && ExprUsesPower(*stmt.assign.lhs_lsb_expr)) {
      return true;
    }
    if (stmt.assign.delay && ExprUsesPower(*stmt.assign.delay)) {
      return true;
    }
    return false;
  }
  if (stmt.kind == StatementKind::kIf) {
    if (stmt.condition && ExprUsesPower(*stmt.condition)) {
      return true;
    }
    for (const auto& inner : stmt.then_branch) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    for (const auto& inner : stmt.else_branch) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kCase) {
    if (stmt.case_expr && ExprUsesPower(*stmt.case_expr)) {
      return true;
    }
    for (const auto& item : stmt.case_items) {
      for (const auto& label : item.labels) {
        if (label && ExprUsesPower(*label)) {
          return true;
        }
      }
      for (const auto& inner : item.body) {
        if (StatementUsesPower(inner)) {
          return true;
        }
      }
    }
    for (const auto& inner : stmt.default_branch) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kFor) {
    if (stmt.for_init_rhs && ExprUsesPower(*stmt.for_init_rhs)) {
      return true;
    }
    if (stmt.for_condition && ExprUsesPower(*stmt.for_condition)) {
      return true;
    }
    if (stmt.for_step_rhs && ExprUsesPower(*stmt.for_step_rhs)) {
      return true;
    }
    for (const auto& inner : stmt.for_body) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kWhile) {
    if (stmt.while_condition && ExprUsesPower(*stmt.while_condition)) {
      return true;
    }
    for (const auto& inner : stmt.while_body) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kRepeat) {
    if (stmt.repeat_count && ExprUsesPower(*stmt.repeat_count)) {
      return true;
    }
    for (const auto& inner : stmt.repeat_body) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kDelay) {
    if (stmt.delay && ExprUsesPower(*stmt.delay)) {
      return true;
    }
    for (const auto& inner : stmt.delay_body) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kEventControl) {
    if (!stmt.event_items.empty()) {
      for (const auto& item : stmt.event_items) {
        if (item.expr && ExprUsesPower(*item.expr)) {
          return true;
        }
      }
    } else if (stmt.event_expr && ExprUsesPower(*stmt.event_expr)) {
      return true;
    }
    for (const auto& inner : stmt.event_body) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kWait) {
    if (stmt.wait_condition && ExprUsesPower(*stmt.wait_condition)) {
      return true;
    }
    for (const auto& inner : stmt.wait_body) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      if (StatementUsesPower(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kTaskCall) {
    for (const auto& arg : stmt.task_args) {
      if (arg && ExprUsesPower(*arg)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

bool ExprUsesReal(const Expr& expr, const Module& module) {
  if (ExprIsRealValue(expr, module)) {
    return true;
  }
  switch (expr.kind) {
    case ExprKind::kUnary:
      return expr.operand && ExprUsesReal(*expr.operand, module);
    case ExprKind::kBinary:
      return (expr.lhs && ExprUsesReal(*expr.lhs, module)) ||
             (expr.rhs && ExprUsesReal(*expr.rhs, module));
    case ExprKind::kTernary:
      return (expr.condition && ExprUsesReal(*expr.condition, module)) ||
             (expr.then_expr && ExprUsesReal(*expr.then_expr, module)) ||
             (expr.else_expr && ExprUsesReal(*expr.else_expr, module));
    case ExprKind::kSelect:
      return (expr.base && ExprUsesReal(*expr.base, module)) ||
             (expr.msb_expr && ExprUsesReal(*expr.msb_expr, module)) ||
             (expr.lsb_expr && ExprUsesReal(*expr.lsb_expr, module));
    case ExprKind::kIndex:
      return (expr.base && ExprUsesReal(*expr.base, module)) ||
             (expr.index && ExprUsesReal(*expr.index, module));
    case ExprKind::kCall:
      if (expr.ident == "$realtime" || expr.ident == "$itor" ||
          expr.ident == "$bitstoreal" || expr.ident == "$rtoi" ||
          expr.ident == "$realtobits") {
        return true;
      }
      for (const auto& arg : expr.call_args) {
        if (arg && ExprUsesReal(*arg, module)) {
          return true;
        }
      }
      return false;
    case ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        if (element && ExprUsesReal(*element, module)) {
          return true;
        }
      }
      if (expr.repeat_expr && ExprUsesReal(*expr.repeat_expr, module)) {
        return true;
      }
      return false;
    case ExprKind::kIdentifier:
    case ExprKind::kNumber:
    case ExprKind::kString:
      return false;
  }
  return false;
}

bool ExprHasSystemCall(const Expr& expr) {
  switch (expr.kind) {
    case ExprKind::kIdentifier:
    case ExprKind::kNumber:
    case ExprKind::kString:
      return false;
    case ExprKind::kUnary:
      return expr.operand && ExprHasSystemCall(*expr.operand);
    case ExprKind::kBinary:
      return (expr.lhs && ExprHasSystemCall(*expr.lhs)) ||
             (expr.rhs && ExprHasSystemCall(*expr.rhs));
    case ExprKind::kTernary:
      return (expr.condition && ExprHasSystemCall(*expr.condition)) ||
             (expr.then_expr && ExprHasSystemCall(*expr.then_expr)) ||
             (expr.else_expr && ExprHasSystemCall(*expr.else_expr));
    case ExprKind::kSelect:
      return (expr.base && ExprHasSystemCall(*expr.base)) ||
             (expr.msb_expr && ExprHasSystemCall(*expr.msb_expr)) ||
             (expr.lsb_expr && ExprHasSystemCall(*expr.lsb_expr));
    case ExprKind::kIndex:
      return (expr.base && ExprHasSystemCall(*expr.base)) ||
             (expr.index && ExprHasSystemCall(*expr.index));
    case ExprKind::kCall:
      if (!expr.ident.empty() && expr.ident.front() == '$') {
        return true;
      }
      for (const auto& arg : expr.call_args) {
        if (arg && ExprHasSystemCall(*arg)) {
          return true;
        }
      }
      return false;
    case ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        if (element && ExprHasSystemCall(*element)) {
          return true;
        }
      }
      if (expr.repeat_expr && ExprHasSystemCall(*expr.repeat_expr)) {
        return true;
      }
      return false;
  }
  return false;
}

bool StatementUsesReal(const Statement& stmt, const Module& module) {
  if (stmt.kind == StatementKind::kAssign ||
      stmt.kind == StatementKind::kForce ||
      stmt.kind == StatementKind::kRelease) {
    if (stmt.assign.rhs && ExprUsesReal(*stmt.assign.rhs, module)) {
      return true;
    }
    if (stmt.assign.lhs_index &&
        ExprUsesReal(*stmt.assign.lhs_index, module)) {
      return true;
    }
    for (const auto& index : stmt.assign.lhs_indices) {
      if (index && ExprUsesReal(*index, module)) {
        return true;
      }
    }
    if (stmt.assign.lhs_msb_expr &&
        ExprUsesReal(*stmt.assign.lhs_msb_expr, module)) {
      return true;
    }
    if (stmt.assign.lhs_lsb_expr &&
        ExprUsesReal(*stmt.assign.lhs_lsb_expr, module)) {
      return true;
    }
    if (stmt.assign.delay && ExprUsesReal(*stmt.assign.delay, module)) {
      return true;
    }
    return false;
  }
  if (stmt.kind == StatementKind::kIf) {
    if (stmt.condition && ExprUsesReal(*stmt.condition, module)) {
      return true;
    }
    for (const auto& inner : stmt.then_branch) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    for (const auto& inner : stmt.else_branch) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kCase) {
    if (stmt.case_expr && ExprUsesReal(*stmt.case_expr, module)) {
      return true;
    }
    for (const auto& item : stmt.case_items) {
      for (const auto& label : item.labels) {
        if (label && ExprUsesReal(*label, module)) {
          return true;
        }
      }
      for (const auto& inner : item.body) {
        if (StatementUsesReal(inner, module)) {
          return true;
        }
      }
    }
    for (const auto& inner : stmt.default_branch) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kFor) {
    if (stmt.for_init_rhs &&
        ExprUsesReal(*stmt.for_init_rhs, module)) {
      return true;
    }
    if (stmt.for_condition &&
        ExprUsesReal(*stmt.for_condition, module)) {
      return true;
    }
    if (stmt.for_step_rhs &&
        ExprUsesReal(*stmt.for_step_rhs, module)) {
      return true;
    }
    for (const auto& inner : stmt.for_body) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kWhile) {
    if (stmt.while_condition &&
        ExprUsesReal(*stmt.while_condition, module)) {
      return true;
    }
    for (const auto& inner : stmt.while_body) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kRepeat) {
    if (stmt.repeat_count && ExprUsesReal(*stmt.repeat_count, module)) {
      return true;
    }
    for (const auto& inner : stmt.repeat_body) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kDelay) {
    if (stmt.delay && ExprUsesReal(*stmt.delay, module)) {
      return true;
    }
    for (const auto& inner : stmt.delay_body) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kEventControl) {
    if (!stmt.event_items.empty()) {
      for (const auto& item : stmt.event_items) {
        if (item.expr && ExprUsesReal(*item.expr, module)) {
          return true;
        }
      }
    } else if (stmt.event_expr && ExprUsesReal(*stmt.event_expr, module)) {
      return true;
    }
    for (const auto& inner : stmt.event_body) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kWait) {
    if (stmt.wait_condition &&
        ExprUsesReal(*stmt.wait_condition, module)) {
      return true;
    }
    for (const auto& inner : stmt.wait_body) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      if (StatementUsesReal(inner, module)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kTaskCall) {
    for (const auto& arg : stmt.task_args) {
      if (arg && ExprUsesReal(*arg, module)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

bool ModuleUsesPower(const Module& module) {
  for (const auto& assign : module.assigns) {
    if (assign.rhs && ExprUsesPower(*assign.rhs)) {
      return true;
    }
  }
  for (const auto& sw : module.switches) {
    if (sw.control && ExprUsesPower(*sw.control)) {
      return true;
    }
    if (sw.control_n && ExprUsesPower(*sw.control_n)) {
      return true;
    }
  }
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      if (StatementUsesPower(stmt)) {
        return true;
      }
    }
  }
  for (const auto& func : module.functions) {
    if (func.body_expr && ExprUsesPower(*func.body_expr)) {
      return true;
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      if (StatementUsesPower(stmt)) {
        return true;
      }
    }
  }
  for (const auto& param : module.parameters) {
    if (param.value && ExprUsesPower(*param.value)) {
      return true;
    }
  }
  for (const auto& defparam : module.defparams) {
    if (defparam.expr && ExprUsesPower(*defparam.expr)) {
      return true;
    }
  }
  return false;
}

bool ModuleUsesReal(const Module& module) {
  for (const auto& net : module.nets) {
    if (net.is_real) {
      return true;
    }
  }
  for (const auto& assign : module.assigns) {
    if (assign.rhs && ExprUsesReal(*assign.rhs, module)) {
      return true;
    }
  }
  for (const auto& sw : module.switches) {
    if (sw.control && ExprUsesReal(*sw.control, module)) {
      return true;
    }
    if (sw.control_n && ExprUsesReal(*sw.control_n, module)) {
      return true;
    }
  }
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      if (StatementUsesReal(stmt, module)) {
        return true;
      }
    }
  }
  for (const auto& func : module.functions) {
    if (func.body_expr && ExprUsesReal(*func.body_expr, module)) {
      return true;
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      if (StatementUsesReal(stmt, module)) {
        return true;
      }
    }
  }
  for (const auto& param : module.parameters) {
    if (param.is_real) {
      return true;
    }
    if (param.value && ExprUsesReal(*param.value, module)) {
      return true;
    }
  }
  for (const auto& defparam : module.defparams) {
    if (defparam.expr && ExprUsesReal(*defparam.expr, module)) {
      return true;
    }
  }
  return false;
}

bool IsSchedulerStatementKind(StatementKind kind) {
  switch (kind) {
    case StatementKind::kDelay:
    case StatementKind::kEventControl:
    case StatementKind::kWait:
    case StatementKind::kForever:
    case StatementKind::kFork:
    case StatementKind::kDisable:
    case StatementKind::kEventTrigger:
    case StatementKind::kTaskCall:
      return true;
    default:
      return false;
  }
}

bool StatementNeedsScheduler(const Statement& stmt) {
  if (stmt.kind == StatementKind::kAssign && stmt.assign.delay) {
    return true;
  }
  if (IsSchedulerStatementKind(stmt.kind)) {
    return true;
  }
  if (stmt.kind == StatementKind::kIf) {
    for (const auto& inner : stmt.then_branch) {
      if (StatementNeedsScheduler(inner)) {
        return true;
      }
    }
    for (const auto& inner : stmt.else_branch) {
      if (StatementNeedsScheduler(inner)) {
        return true;
      }
    }
  }
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      if (StatementNeedsScheduler(inner)) {
        return true;
      }
    }
  }
  if (stmt.kind == StatementKind::kCase) {
    for (const auto& item : stmt.case_items) {
      for (const auto& inner : item.body) {
        if (StatementNeedsScheduler(inner)) {
          return true;
        }
      }
    }
    for (const auto& inner : stmt.default_branch) {
      if (StatementNeedsScheduler(inner)) {
        return true;
      }
    }
  }
  if (stmt.kind == StatementKind::kFor) {
    for (const auto& inner : stmt.for_body) {
      if (StatementNeedsScheduler(inner)) {
        return true;
      }
    }
  }
  if (stmt.kind == StatementKind::kWhile) {
    for (const auto& inner : stmt.while_body) {
      if (StatementNeedsScheduler(inner)) {
        return true;
      }
    }
  }
  if (stmt.kind == StatementKind::kRepeat) {
    for (const auto& inner : stmt.repeat_body) {
      if (StatementNeedsScheduler(inner)) {
        return true;
      }
    }
  }
  return false;
}

bool ModuleNeedsScheduler(const Module& module) {
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      if (StatementNeedsScheduler(stmt)) {
        return true;
      }
    }
  }
  return false;
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
  if (ExprIsRealValue(expr, module)) {
    return EmitRealToIntExpr(expr, ExprWidth(expr, module), true, module,
                             locals, regs);
  }
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
    case ExprKind::kString:
      return "0u";
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
        std::string cond =
            expr.operand ? EmitCondExpr(*expr.operand, module, locals, regs)
                         : "false";
        return "((" + cond + ") ? 0u : 1u)";
      }
      if (expr.unary_op == 'B') {
        std::string cond =
            expr.operand ? EmitCondExpr(*expr.operand, module, locals, regs)
                         : "false";
        return "((" + cond + ") ? 1u : 0u)";
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
        std::string lhs_bool =
            expr.lhs ? EmitCondExpr(*expr.lhs, module, locals, regs) : "false";
        std::string rhs_bool =
            expr.rhs ? EmitCondExpr(*expr.rhs, module, locals, regs) : "false";
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
      if (expr.op == 'p') {
        int target_width = lhs_width;
        bool signed_op = lhs_signed && rhs_signed;
        std::string lhs_ext = signed_op
                                  ? SignExtendExpr(lhs, lhs_width, target_width)
                                  : ExtendExpr(lhs, lhs_width, target_width);
        std::string rhs_ext = signed_op
                                  ? SignExtendExpr(rhs, rhs_width, target_width)
                                  : ExtendExpr(rhs, rhs_width, target_width);
        std::string cast =
            signed_op ? SignedCastForWidth(target_width)
                      : UnsignedCastForWidth(target_width);
        std::string func;
        if (target_width > 32) {
          func = signed_op ? "gpga_pow_s64" : "gpga_pow_u64";
        } else {
          func = signed_op ? "gpga_pow_s32" : "gpga_pow_u32";
        }
        std::string call =
            func + "(" + cast + lhs_ext + ", " + cast + rhs_ext + ")";
        return MaskForWidthExpr(call, target_width);
      }
      if (expr.op == 'E' || expr.op == 'N' || expr.op == 'C' ||
          expr.op == 'c' || expr.op == 'W' || expr.op == 'w' ||
          expr.op == '<' || expr.op == '>' || expr.op == 'L' ||
          expr.op == 'G') {
        if ((expr.lhs && ExprIsRealValue(*expr.lhs, module)) ||
            (expr.rhs && ExprIsRealValue(*expr.rhs, module))) {
          std::string lhs_real =
              expr.lhs ? EmitRealValueExpr(*expr.lhs, module, locals, regs)
                       : "0.0";
          std::string rhs_real =
              expr.rhs ? EmitRealValueExpr(*expr.rhs, module, locals, regs)
                       : "0.0";
          std::string op = BinaryOpString(expr.op);
          return "((" + lhs_real + " " + op + " " + rhs_real + ") ? 1u : 0u)";
        }
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
                             ? EmitCondExpr(*expr.condition, module, locals,
                                            regs)
                             : "false";
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
        return "__gpga_time";
      }
      if (expr.ident == "$rtoi") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          return EmitRealToIntExpr(*expr.call_args.front(), 32, true, module,
                                   locals, regs);
        }
        return "0u";
      }
      if (expr.ident == "$realtobits") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          return EmitRealBitsExpr(*expr.call_args.front(), module, locals,
                                  regs);
        }
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
  std::string bit_index;
  std::string range_index;
  int width = 0;
  int base_width = 0;
  int range_lsb = 0;
  bool ok = false;
  bool is_array = false;
  bool is_bit_select = false;
  bool is_range = false;
  bool is_indexed_range = false;
};

LvalueInfo BuildLvalue(const SequentialAssign& assign, const Module& module,
                       const std::unordered_set<std::string>& locals,
                       const std::unordered_set<std::string>& regs,
                       bool use_next) {
  LvalueInfo out;
  if (SignalIsReal(module, assign.lhs)) {
    if (assign.lhs_has_range) {
      return out;
    }
    if ((assign.lhs_index || !assign.lhs_indices.empty()) &&
        !IsArrayNet(module, assign.lhs, nullptr, nullptr)) {
      return out;
    }
  }
  if (assign.lhs_has_range) {
    int element_width = 0;
    int array_size = 0;
    if (assign.lhs_index &&
        IsArrayNet(module, assign.lhs, &element_width, &array_size)) {
      if (!assign.lhs_msb_expr || element_width <= 0 || array_size <= 0) {
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
      out.base_width = element_width;
      out.width = 1;
      out.ok = true;
      out.is_bit_select = true;
      out.bit_index =
          EmitExpr(*assign.lhs_msb_expr, module, locals, regs);
      out.guard = "(" + idx + " < " + std::to_string(array_size) + "u && "
                  "uint(" + out.bit_index + ") < " +
                  std::to_string(element_width) + "u)";
      return out;
    }
    if (IsArrayNet(module, assign.lhs, nullptr, nullptr)) {
      return out;
    }
    std::string base;
    if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
      base = assign.lhs + "[gid]";
    } else if (locals.count(assign.lhs) > 0) {
      base = assign.lhs;
    } else {
      return out;
    }
    out.expr = base;
    out.base_width = SignalWidth(module, assign.lhs);
    out.ok = true;
    out.is_range = true;
    if (assign.lhs_indexed_range) {
      if (!assign.lhs_lsb_expr || assign.lhs_indexed_width <= 0) {
        return LvalueInfo{};
      }
      std::string index =
          EmitExpr(*assign.lhs_lsb_expr, module, locals, regs);
      int width = assign.lhs_indexed_width;
      if (width <= 0) {
        return LvalueInfo{};
      }
      out.range_index = index;
      out.width = width;
      out.is_indexed_range = true;
      if (out.base_width >= width) {
        int limit = out.base_width - width;
        out.guard = "(uint(" + index + ") <= " + std::to_string(limit) + "u)";
      } else {
        out.guard = "false";
      }
      return out;
    }
    int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
    int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
    out.range_lsb = lo;
    out.width = hi - lo + 1;
    return out;
  }
  if (assign.lhs_index) {
    int element_width = 0;
    int array_size = 0;
    if (!IsArrayNet(module, assign.lhs, &element_width, &array_size)) {
      std::string base;
      if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
        base = assign.lhs + "[gid]";
      } else if (locals.count(assign.lhs) > 0) {
        base = assign.lhs;
      } else {
        return out;
      }
      std::string index =
          EmitExpr(*assign.lhs_index, module, locals, regs);
      int base_width = SignalWidth(module, assign.lhs);
      out.expr = base;
      out.bit_index = index;
      out.base_width = base_width;
      out.width = 1;
      out.guard = "(uint(" + index + ") < " + std::to_string(base_width) + "u)";
      out.ok = true;
      out.is_bit_select = true;
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

std::string EmitBitSelectUpdate(const std::string& base_expr,
                                const std::string& index_expr,
                                int base_width,
                                const std::string& rhs_expr) {
  std::string idx = "uint(" + index_expr + ")";
  std::string one = (base_width > 32) ? "1ul" : "1u";
  std::string cast = CastForWidth(base_width);
  std::string rhs_masked = MaskForWidthExpr(rhs_expr, 1);
  std::string clear = "~(" + one + " << " + idx + ")";
  std::string set = "((" + cast + rhs_masked + ") << " + idx + ")";
  return "(" + base_expr + " & " + clear + ") | " + set;
}

std::string EmitRangeSelectUpdate(const std::string& base_expr,
                                  const std::string& index_expr,
                                  int base_width, int slice_width,
                                  const std::string& rhs_expr) {
  std::string idx = "uint(" + index_expr + ")";
  std::string cast = CastForWidth(base_width);
  uint64_t slice_mask = MaskForWidth64(slice_width);
  uint64_t base_mask = MaskForWidth64(base_width);
  std::string suffix = (base_width > 32) ? "ul" : "u";
  std::string slice_literal = std::to_string(slice_mask) + suffix;
  std::string base_literal = std::to_string(base_mask) + suffix;
  std::string rhs_masked = MaskForWidthExpr(rhs_expr, slice_width);
  std::string shifted_mask =
      "((" + slice_literal + " << " + idx + ") & " + base_literal + ")";
  std::string clear = "~" + shifted_mask;
  std::string set =
      "((" + cast + rhs_masked + " & " + slice_literal + ") << " + idx + ")";
  return "(" + base_expr + " & " + clear + ") | " + set;
}

}  // namespace

std::string EmitMSLStub(const Module& module, bool four_state) {
  std::ostringstream out;
  out << "#include <metal_stdlib>\n";
  out << "using namespace metal;\n\n";
  if (four_state) {
    out << "#include \"gpga_4state.h\"\n\n";
  }
  const bool uses_power = ModuleUsesPower(module);
  const bool uses_real = ModuleUsesReal(module);
  if (!four_state && uses_power) {
    out << "inline uint gpga_pow_u32(uint base, uint exp) {\n";
    out << "  uint result = 1u;\n";
    out << "  while (exp != 0u) {\n";
    out << "    if (exp & 1u) {\n";
    out << "      result *= base;\n";
    out << "    }\n";
    out << "    base *= base;\n";
    out << "    exp >>= 1u;\n";
    out << "  }\n";
    out << "  return result;\n";
    out << "}\n";
    out << "inline ulong gpga_pow_u64(ulong base, ulong exp) {\n";
    out << "  ulong result = 1ul;\n";
    out << "  while (exp != 0ul) {\n";
    out << "    if (exp & 1ul) {\n";
    out << "      result *= base;\n";
    out << "    }\n";
    out << "    base *= base;\n";
    out << "    exp >>= 1ul;\n";
    out << "  }\n";
    out << "  return result;\n";
    out << "}\n";
    out << "inline uint gpga_pow_s32(int base, int exp) {\n";
    out << "  if (exp < 0) {\n";
    out << "    return 0u;\n";
    out << "  }\n";
    out << "  return gpga_pow_u32(uint(base), uint(exp));\n";
    out << "}\n";
    out << "inline ulong gpga_pow_s64(long base, long exp) {\n";
    out << "  if (exp < 0) {\n";
    out << "    return 0ul;\n";
    out << "  }\n";
    out << "  return gpga_pow_u64(ulong(base), ulong(exp));\n";
    out << "}\n\n";
  }
  if (uses_real) {
    out << "inline double gpga_bits_to_real(ulong bits) {\n";
    out << "  return as_type<double>(bits);\n";
    out << "}\n";
    out << "inline ulong gpga_real_to_bits(double value) {\n";
    out << "  return as_type<ulong>(value);\n";
    out << "}\n\n";
  }
  out << "struct GpgaParams { uint count; };\n\n";
  out << "constexpr ulong __gpga_time = 0ul;\n\n";
  out << "// Placeholder MSL emitted by GPGA.\n\n";
  const bool needs_scheduler = ModuleNeedsScheduler(module);
  const SystemTaskInfo system_task_info = BuildSystemTaskInfo(module);
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
    auto decay_name = [](const std::string& name) {
      return name + "_decay_time";
    };
    auto trireg_decay_delay = [&](const std::string& name) -> std::string {
      for (const auto& net : module.nets) {
        if (net.name != name) {
          continue;
        }
        switch (net.charge) {
          case ChargeStrength::kSmall:
            return "1ul";
          case ChargeStrength::kMedium:
            return "10ul";
          case ChargeStrength::kLarge:
            return "100ul";
          case ChargeStrength::kNone:
            return "10ul";
        }
      }
      return "10ul";
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
    std::unordered_set<std::string> scheduled_reads;
    for (const auto& block : module.always_blocks) {
      if (block.edge == EdgeKind::kCombinational) {
        continue;
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
    std::unordered_set<std::string> buffered_regs;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (IsTriregNet(net.type)) {
        buffered_regs.insert(net.name);
        continue;
      }
      if (net.type == NetType::kReg ||
          scheduled_reads.count(net.name) > 0) {
        buffered_regs.insert(net.name);
      }
    }

    struct FsExpr {
      std::string val;
      std::string xz;
      std::string drive;
      int width = 0;
      std::string full;
      bool is_const = false;
      uint64_t const_val = 0;
      uint64_t const_xz = 0;
      uint64_t const_drive = 0;
      bool is_real = false;
    };

    auto fs_expr_from_base = [&](const std::string& base,
                                 const std::string& drive,
                                 int width) -> FsExpr {
      return FsExpr{base + ".val", base + ".xz", drive, width, base};
    };

    auto fs_const_expr = [&](uint64_t val_bits, uint64_t xz_bits,
                             uint64_t drive_bits, int width) -> FsExpr {
      uint64_t mask = MaskForWidth64(width);
      FsExpr out;
      out.width = width;
      out.const_val = val_bits & mask;
      out.const_xz = xz_bits & mask;
      out.const_drive = drive_bits & mask;
      out.is_const = true;
      out.val = literal_for_width(out.const_val, width);
      out.xz = literal_for_width(out.const_xz, width);
      out.drive = literal_for_width(out.const_drive, width);
      return out;
    };

    auto fs_make_expr = [&](const FsExpr& expr, int width) -> std::string {
      if (expr.is_const && expr.width == width) {
        std::string type = (width > 32) ? "FourState64" : "FourState32";
        return type + "{" + literal_for_width(expr.const_val, width) + ", " +
               literal_for_width(expr.const_xz, width) + "}";
      }
      if (!expr.full.empty() && expr.width == width) {
        return expr.full;
      }
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

    auto fs_const_extend = [&](const FsExpr& expr, int width,
                               bool sign_extend) -> FsExpr {
      if (!expr.is_const) {
        return expr;
      }
      if (width <= 0) {
        return fs_const_expr(0u, 0u, 0u, width);
      }
      uint64_t src_mask = MaskForWidth64(expr.width);
      uint64_t dst_mask = MaskForWidth64(width);
      uint64_t val = expr.const_val & src_mask;
      uint64_t xz = expr.const_xz & src_mask;
      uint64_t drive = expr.const_drive & src_mask;
      if (width > expr.width) {
        uint64_t ext_mask = dst_mask & ~src_mask;
        if (sign_extend && expr.width > 0) {
          int sign_width = std::min(expr.width, 64);
          uint64_t sign_mask =
              (sign_width > 0) ? (1ull << (sign_width - 1)) : 0ull;
          uint64_t ext_val = (val & sign_mask) ? ext_mask : 0u;
          uint64_t ext_xz = (xz & sign_mask) ? ext_mask : 0u;
          uint64_t ext_drive = (drive & sign_mask) ? ext_mask : 0u;
          val |= ext_val;
          xz |= ext_xz;
          drive |= ext_drive;
        } else {
          drive |= ext_mask;
        }
      }
      return fs_const_expr(val, xz, drive, width);
    };

    auto fs_resize_expr = [&](const FsExpr& expr, int width) -> FsExpr {
      if (expr.is_real) {
        return expr;
      }
      if (expr.width == width) {
        return expr;
      }
      if (expr.is_const) {
        return fs_const_extend(expr, width, false);
      }
      std::string func = (width > 32) ? "fs_resize64" : "fs_resize32";
      std::string base = func + "(" + fs_make_expr(expr, expr.width) + ", " +
                         std::to_string(width) + "u)";
      std::string drive = fs_resize_drive(expr, width, false);
      return fs_expr_from_base(base, drive, width);
    };

    auto fs_sext_expr = [&](const FsExpr& expr, int width) -> FsExpr {
      if (expr.width >= width) {
        return fs_resize_expr(expr, width);
      }
      if (expr.is_const) {
        return fs_const_extend(expr, width, true);
      }
      std::string func = (width > 32) ? "fs_sext64" : "fs_sext32";
      std::string base =
          func + "(" + fs_make_expr(expr, expr.width) + ", " +
          std::to_string(expr.width) + "u, " + std::to_string(width) + "u)";
      std::string drive = fs_resize_drive(expr, width, true);
      return fs_expr_from_base(base, drive, width);
    };

    auto fs_extend_expr = [&](const FsExpr& expr, int width,
                              bool signed_op) -> FsExpr {
      if (expr.is_real) {
        return expr;
      }
      return signed_op ? fs_sext_expr(expr, width) : fs_resize_expr(expr, width);
    };

    auto fs_allx_expr = [&](int width) -> FsExpr {
      uint64_t mask = MaskForWidth64(width);
      return fs_const_expr(0u, mask, mask, width);
    };

    auto fs_unary = [&](const char* op, const FsExpr& arg, int width) -> FsExpr {
      std::string func =
          std::string("fs_") + op + (width > 32 ? "64" : "32");
      std::string base =
          func + "(" + fs_make_expr(arg, width) + ", " +
          std::to_string(width) + "u)";
      return fs_expr_from_base(base, drive_full(width), width);
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
      return fs_expr_from_base(base, drive_full(width), width);
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
      return fs_expr_from_base(base, drive_full(width), width);
    };

    struct ExprUse {
      int count = 0;
      int cost = 0;
    };

    struct CseState {
      std::unordered_map<std::string, ExprUse> uses;
      std::unordered_map<std::string, FsExpr> temps;
      int min_cost = 4;
      int indent = 0;
    };

    auto is_cse_candidate = [&](const Expr& expr) -> bool {
      switch (expr.kind) {
        case ExprKind::kIdentifier:
        case ExprKind::kNumber:
        case ExprKind::kString:
        case ExprKind::kCall:
          return false;
        default:
          return true;
      }
    };

    auto expr_key = [&](const Expr& expr, const auto& self) -> std::string {
      std::string key;
      switch (expr.kind) {
        case ExprKind::kIdentifier:
          key = "id:" + expr.ident;
          break;
        case ExprKind::kNumber: {
          int width = expr.has_width && expr.number_width > 0
                          ? expr.number_width
                          : ExprWidth(expr, module);
          key = "num:" + std::to_string(expr.value_bits) + ":" +
                std::to_string(expr.x_bits) + ":" +
                std::to_string(expr.z_bits) + ":" + std::to_string(width);
          break;
        }
        case ExprKind::kString:
          key = "str";
          break;
        case ExprKind::kUnary:
          key = std::string("un:") + expr.unary_op + "(" +
                (expr.operand ? self(*expr.operand, self) : "") + ")";
          break;
        case ExprKind::kBinary:
          key = std::string("bin:") + expr.op + "(" +
                (expr.lhs ? self(*expr.lhs, self) : "") + "," +
                (expr.rhs ? self(*expr.rhs, self) : "") + ")";
          break;
        case ExprKind::kTernary:
          key = "ter(" +
                (expr.condition ? self(*expr.condition, self) : "") + "?" +
                (expr.then_expr ? self(*expr.then_expr, self) : "") + ":" +
                (expr.else_expr ? self(*expr.else_expr, self) : "") + ")";
          break;
        case ExprKind::kSelect: {
          key =
              "sel(" + (expr.base ? self(*expr.base, self) : "") + ",";
          if (expr.indexed_range && expr.lsb_expr) {
            key += "idx:" + std::to_string(expr.indexed_width) + ":" +
                   (expr.indexed_desc ? "d:" : "a:") +
                   self(*expr.lsb_expr, self) + ")";
          } else {
            key += std::to_string(expr.msb) + ":" +
                   std::to_string(expr.lsb) + ")";
          }
          break;
        }
        case ExprKind::kIndex:
          key = "idx(" +
                (expr.base ? self(*expr.base, self) : "") + "," +
                (expr.index ? self(*expr.index, self) : "") + ")";
          break;
        case ExprKind::kCall: {
          key = "call:" + expr.ident + "(";
          for (const auto& arg : expr.call_args) {
            if (!arg) {
              continue;
            }
            if (key.back() != '(') {
              key += ",";
            }
            key += self(*arg, self);
          }
          key += ")";
          break;
        }
        case ExprKind::kConcat: {
          key = "cat:" + std::to_string(expr.repeat) + "(";
          for (const auto& element : expr.elements) {
            if (!element) {
              continue;
            }
            if (key.back() != '(') {
              key += ",";
            }
            key += self(*element, self);
          }
          key += ")";
          break;
        }
      }
      if (key.empty()) {
        key = "unknown";
      }
      int width = ExprWidth(expr, module);
      key += ":w" + std::to_string(width);
      key += ExprSigned(expr, module) ? ":s" : ":u";
      return key;
    };

    auto collect_expr_uses = [&](const Expr& expr, const auto& self,
                                 CseState* state) -> int {
      if (!state) {
        return 1;
      }
      int cost = 1;
      switch (expr.kind) {
        case ExprKind::kUnary:
          if (expr.operand) {
            cost += self(*expr.operand, self, state);
          }
          break;
        case ExprKind::kBinary:
          if (expr.lhs) {
            cost += self(*expr.lhs, self, state);
          }
          if (expr.rhs) {
            cost += self(*expr.rhs, self, state);
          }
          break;
        case ExprKind::kTernary:
          if (expr.condition) {
            cost += self(*expr.condition, self, state);
          }
          if (expr.then_expr) {
            cost += self(*expr.then_expr, self, state);
          }
          if (expr.else_expr) {
            cost += self(*expr.else_expr, self, state);
          }
          break;
        case ExprKind::kSelect:
          if (expr.base) {
            cost += self(*expr.base, self, state);
          }
          if (expr.indexed_range && expr.lsb_expr) {
            cost += self(*expr.lsb_expr, self, state);
          }
          break;
        case ExprKind::kIndex:
          if (expr.base) {
            cost += self(*expr.base, self, state);
          }
          if (expr.index) {
            cost += self(*expr.index, self, state);
          }
          break;
        case ExprKind::kCall:
          for (const auto& arg : expr.call_args) {
            if (!arg) {
              continue;
            }
            cost += self(*arg, self, state);
          }
          break;
        case ExprKind::kConcat:
          for (const auto& element : expr.elements) {
            if (!element) {
              continue;
            }
            cost += self(*element, self, state);
          }
          if (expr.repeat_expr) {
            cost += self(*expr.repeat_expr, self, state);
          }
          break;
        default:
          break;
      }
      std::string key = expr_key(expr, expr_key);
      ExprUse& entry = state->uses[key];
      entry.count += 1;
      if (entry.cost < cost) {
        entry.cost = cost;
      }
      return cost;
    };

    int fs_temp_index = 0;
    int switch_temp_index = 0;
    CseState* active_cse = nullptr;

    auto should_cse = [&](const Expr& expr, const std::string& key,
                          const CseState* state) -> bool {
      if (!state || !is_cse_candidate(expr)) {
        return false;
      }
      auto it = state->uses.find(key);
      if (it == state->uses.end()) {
        return false;
      }
      return it->second.count > 1 && it->second.cost >= state->min_cost;
    };

    auto emit_cse_temp = [&](const FsExpr& expr,
                             CseState* state) -> FsExpr {
      if (!state || expr.width <= 0) {
        return expr;
      }
      std::string name = "__gpga_fs_tmp" + std::to_string(fs_temp_index++);
      std::string type = (expr.width > 32) ? "FourState64" : "FourState32";
      std::string dtype = (expr.width > 32) ? "ulong" : "uint";
      std::string pad(state->indent, ' ');
      out << pad << type << " " << name << " = "
          << fs_make_expr(expr, expr.width) << ";\n";
      out << pad << dtype << " " << name << "_drive = " << expr.drive << ";\n";
      return FsExpr{name + ".val", name + ".xz", name + "_drive", expr.width,
                    name};
    };

    std::function<FsExpr(const Expr&)> emit_expr4;
    std::function<FsExpr(const Expr&)> emit_expr4_impl;
    std::function<FsExpr(const Expr&)> emit_concat4;
    std::function<std::string(const Expr&)> emit_real_value4;
    std::function<FsExpr(const FsExpr&, int, bool, bool)> maybe_hoist_full;

    emit_expr4 = [&](const Expr& expr) -> FsExpr {
      if (ExprIsRealValue(expr, module)) {
        return emit_expr4_impl(expr);
      }
      if (!active_cse) {
        return emit_expr4_impl(expr);
      }
      std::string key = expr_key(expr, expr_key);
      bool use_cse = should_cse(expr, key, active_cse);
      if (use_cse) {
        auto it = active_cse->temps.find(key);
        if (it != active_cse->temps.end()) {
          return it->second;
        }
      }
      FsExpr value = emit_expr4_impl(expr);
      if (use_cse) {
        FsExpr temp = emit_cse_temp(value, active_cse);
        active_cse->temps.emplace(key, temp);
        return temp;
      }
      return value;
    };

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
          std::string cast = CastForWidth(total_width);
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

    auto emit_real_bits4 = [&](const Expr& expr) -> std::string {
      if (IsRealLiteralExpr(expr)) {
        return std::to_string(expr.value_bits) + "ul";
      }
      if (expr.kind == ExprKind::kNumber && expr.value_bits == 0 &&
          expr.x_bits == 0 && expr.z_bits == 0) {
        return "0ul";
      }
      return "gpga_real_to_bits(" + emit_real_value4(expr) + ")";
    };

    auto emit_real_expr4 = [&](const Expr& expr) -> FsExpr {
      FsExpr out;
      out.width = 64;
      out.val = emit_real_bits4(expr);
      out.xz = literal_for_width(0, 64);
      out.drive = drive_full(64);
      out.is_real = true;
      return out;
    };

    emit_real_value4 = [&](const Expr& expr) -> std::string {
      if (!ExprIsRealValue(expr, module)) {
        FsExpr int_expr = emit_expr4(expr);
        std::string mask =
            literal_for_width(MaskForWidth64(int_expr.width), 64);
        std::string known_val =
            "((" + int_expr.val + " & ~" + int_expr.xz + ") & " + mask + ")";
        bool signed_expr = ExprSigned(expr, module);
        std::string cast;
        if (int_expr.width > 32) {
          cast = signed_expr ? "(long)" : "(ulong)";
        } else {
          cast = signed_expr ? "(int)" : "(uint)";
        }
        return "double(" + cast + "(" + known_val + "))";
      }
      switch (expr.kind) {
        case ExprKind::kIdentifier: {
          const Port* port = FindPort(module, expr.ident);
          std::string ref;
          if (port) {
            ref = val_name(port->name) + "[gid]";
          } else if (buffered_regs.count(expr.ident) > 0) {
            ref = val_name(expr.ident) + "[gid]";
          } else {
            ref = val_name(expr.ident);
          }
          return "gpga_bits_to_real(" + ref + ")";
        }
        case ExprKind::kNumber:
          if (IsRealLiteralExpr(expr)) {
            return "gpga_bits_to_real(" + std::to_string(expr.value_bits) +
                   "ul)";
          }
          return "0.0";
        case ExprKind::kString:
          return "0.0";
        case ExprKind::kUnary: {
          std::string operand =
              expr.operand ? emit_real_value4(*expr.operand) : "0.0";
          if (expr.unary_op == '+') {
            return operand;
          }
          if (expr.unary_op == '-') {
            return "(-" + operand + ")";
          }
          return "0.0";
        }
        case ExprKind::kBinary: {
          std::string lhs =
              expr.lhs ? emit_real_value4(*expr.lhs) : "0.0";
          std::string rhs =
              expr.rhs ? emit_real_value4(*expr.rhs) : "0.0";
          if (expr.op == '+' || expr.op == '-' || expr.op == '*' ||
              expr.op == '/') {
            return "(" + lhs + " " + std::string(1, expr.op) + " " + rhs + ")";
          }
          if (expr.op == 'p') {
            return "pow(" + lhs + ", " + rhs + ")";
          }
          return "0.0";
        }
        case ExprKind::kTernary: {
          std::string cond = expr.condition
                                 ? "(" + emit_real_value4(*expr.condition) +
                                       " != 0.0)"
                                 : "false";
          std::string then_expr =
              expr.then_expr ? emit_real_value4(*expr.then_expr) : "0.0";
          std::string else_expr =
              expr.else_expr ? emit_real_value4(*expr.else_expr) : "0.0";
          return "((" + cond + ") ? (" + then_expr + ") : (" + else_expr + "))";
        }
        case ExprKind::kCall:
          if (expr.ident == "$realtime") {
            return "(double)__gpga_time";
          }
          if (expr.ident == "$itor") {
            if (!expr.call_args.empty() && expr.call_args.front()) {
              return emit_real_value4(*expr.call_args.front());
            }
            return "0.0";
          }
          if (expr.ident == "$bitstoreal") {
            if (!expr.call_args.empty() && expr.call_args.front()) {
              FsExpr bits_expr = emit_expr4(*expr.call_args.front());
              std::string mask =
                  literal_for_width(MaskForWidth64(bits_expr.width), 64);
              std::string bits = "((" + bits_expr.val + ") & " + mask + ")";
              return "gpga_bits_to_real(" + bits + ")";
            }
            return "0.0";
          }
          return "0.0";
        case ExprKind::kSelect:
          return "0.0";
        case ExprKind::kIndex: {
          if (!expr.base || !expr.index) {
            return "0.0";
          }
          if (expr.base->kind == ExprKind::kIdentifier) {
            int element_width = 0;
            int array_size = 0;
            if (IsArrayNet(module, expr.base->ident, &element_width,
                           &array_size) &&
                SignalIsReal(module, expr.base->ident)) {
              FsExpr idx = emit_expr4(*expr.index);
              if (active_cse) {
                idx = maybe_hoist_full(idx, active_cse->indent, false, false);
              }
              std::string idx_val = idx.val;
              std::string idx_xz = idx.xz;
              std::string bounds = "(" + idx_val + " < " +
                                   std::to_string(array_size) + "u)";
              std::string xguard =
                  "(" + idx_xz + " == " +
                  literal_for_width(0, idx.width) + ")";
              std::string base =
                  "(gid * " + std::to_string(array_size) + "u) + uint(" +
                  idx_val + ")";
              return "((" + xguard + ") ? ((" + bounds + ") ? " +
                     "gpga_bits_to_real(" + val_name(expr.base->ident) + "[" +
                     base + "]) : 0.0) : 0.0)";
            }
          }
          return "0.0";
        }
        case ExprKind::kConcat:
          return "0.0";
      }
      return "0.0";
    };

    const std::unordered_map<std::string, int64_t> kEmptyParams;
    auto try_eval_const_expr4 = [&](const Expr& expr, FsExpr* out) -> bool {
      if (!out) {
        return false;
      }
      if (ExprUsesReal(expr, module)) {
        return false;
      }
      FourStateValue value;
      if (!EvalConstExpr4State(expr, kEmptyParams, &value, nullptr)) {
        return false;
      }
      int width = ExprWidth(expr, module);
      uint64_t mask = MaskForWidth64(width);
      uint64_t val_bits = value.value_bits & mask;
      uint64_t x_bits = value.x_bits & mask;
      uint64_t z_bits = value.z_bits & mask;
      uint64_t xz_bits = (x_bits | z_bits) & mask;
      uint64_t drive_bits = mask & ~z_bits;
      *out = fs_const_expr(val_bits, xz_bits, drive_bits, width);
      return true;
    };

    emit_expr4_impl = [&](const Expr& expr) -> FsExpr {
      FsExpr const_expr;
      if (try_eval_const_expr4(expr, &const_expr)) {
        return const_expr;
      }
      if (ExprIsRealValue(expr, module)) {
        return emit_real_expr4(expr);
      }
      switch (expr.kind) {
        case ExprKind::kIdentifier: {
          const Port* port = FindPort(module, expr.ident);
          if (port) {
            return FsExpr{val_name(port->name) + "[gid]",
                          xz_name(port->name) + "[gid]",
                          drive_full(port->width), port->width};
          }
          if (buffered_regs.count(expr.ident) > 0) {
            int width = SignalWidth(module, expr.ident);
            return FsExpr{val_name(expr.ident) + "[gid]",
                          xz_name(expr.ident) + "[gid]",
                          drive_full(width), width};
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
          return fs_const_expr(expr.value_bits, xz_bits, drive_bits, width);
        }
        case ExprKind::kString:
          return fs_allx_expr(1);
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
            if (expr.operand && ExprIsRealValue(*expr.operand, module)) {
              std::string real_val = emit_real_value4(*expr.operand);
              std::string pred = "(" + real_val + " == 0.0)";
              std::string val = "(" + pred + " ? 1u : 0u)";
              return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
            }
            std::string func = (width > 32) ? "fs_log_not64" : "fs_log_not32";
            std::string base =
                func + "(" + fs_make_expr(operand, width) + ", " +
                std::to_string(width) + "u)";
            return fs_expr_from_base(base, drive_full(1), 1);
          }
          if (expr.unary_op == 'B') {
            if (expr.operand && ExprIsRealValue(*expr.operand, module)) {
              std::string real_val = emit_real_value4(*expr.operand);
              std::string pred = "(" + real_val + " != 0.0)";
              std::string val = "(" + pred + " ? 1u : 0u)";
              return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
            }
            std::string zero = literal_for_width(0, width);
            std::string val = "((" + operand.xz + " == " + zero + " && " +
                              operand.val + " != " + zero + ") ? 1u : 0u)";
            return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
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
            return fs_expr_from_base(base, drive_full(1), 1);
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
            auto bool_expr = [&](const FsExpr& value) -> std::string {
              if (value.is_real) {
                return "(gpga_bits_to_real(" + value.val + ") != 0.0)";
              }
              return "(" + value.xz + " == " +
                     literal_for_width(0, value.width) + " && " + value.val +
                     " != " + literal_for_width(0, value.width) + ")";
            };
            if (lhs.is_real || rhs.is_real) {
              std::string lhs_bool = bool_expr(lhs);
              std::string rhs_bool = bool_expr(rhs);
              std::string op = (expr.op == 'A') ? "&&" : "||";
              std::string val =
                  "((" + lhs_bool + " " + op + " " + rhs_bool +
                  ") ? 1u : 0u)";
              return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
            }
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
            return fs_expr_from_base(base, drive_full(1), 1);
          }
          if (expr.op == 'C' || expr.op == 'c' || expr.op == 'W' ||
              expr.op == 'w') {
            if ((expr.lhs && ExprIsRealValue(*expr.lhs, module)) ||
                (expr.rhs && ExprIsRealValue(*expr.rhs, module))) {
              std::string lhs_real =
                  expr.lhs ? emit_real_value4(*expr.lhs) : "0.0";
              std::string rhs_real =
                  expr.rhs ? emit_real_value4(*expr.rhs) : "0.0";
              std::string op = (expr.op == 'c' || expr.op == 'w') ? "!="
                                                                 : "==";
              std::string pred = "(" + lhs_real + " " + op + " " + rhs_real +
                                 ")";
              std::string val = "(" + pred + " ? 1u : 0u)";
              return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
            }
            FsExpr lhs = emit_expr4(*expr.lhs);
            FsExpr rhs = emit_expr4(*expr.rhs);
            int width = std::max(lhs.width, rhs.width);
            bool signed_op =
                ExprSigned(*expr.lhs, module) && ExprSigned(*expr.rhs, module);
            lhs = fs_extend_expr(lhs, width, signed_op);
            rhs = fs_extend_expr(rhs, width, signed_op);
            std::string func;
            std::string pred;
            if (expr.op == 'C' || expr.op == 'c') {
              func = (width > 32) ? "fs_case_eq64" : "fs_case_eq32";
              pred = func + "(" + fs_make_expr(lhs, width) + ", " +
                     fs_make_expr(rhs, width) + ", " +
                     std::to_string(width) + "u)";
            } else {
              func = (width > 32) ? "fs_casez64" : "fs_casez32";
              std::string ignore = MaskForWidthExpr(rhs.xz, width);
              pred = func + "(" + fs_make_expr(lhs, width) + ", " +
                     fs_make_expr(rhs, width) + ", " + ignore + ", " +
                     std::to_string(width) + "u)";
            }
            if (expr.op == 'c' || expr.op == 'w') {
              pred = "(!(" + pred + "))";
            }
            std::string val = "(" + pred + " ? 1u : 0u)";
            return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
          }
          if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
              expr.op == '>' || expr.op == 'L' || expr.op == 'G') {
            if ((expr.lhs && ExprIsRealValue(*expr.lhs, module)) ||
                (expr.rhs && ExprIsRealValue(*expr.rhs, module))) {
              std::string lhs_real =
                  expr.lhs ? emit_real_value4(*expr.lhs) : "0.0";
              std::string rhs_real =
                  expr.rhs ? emit_real_value4(*expr.rhs) : "0.0";
              std::string op = BinaryOpString(expr.op);
              std::string pred = "(" + lhs_real + " " + op + " " + rhs_real +
                                 ")";
              std::string val = "(" + pred + " ? 1u : 0u)";
              return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
            }
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
          if (expr.op == 'p') {
            width = lhs.width;
          }
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
            case 'p':
              op = signed_op ? "spow" : "pow";
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
          if (active_cse) {
            cond = maybe_hoist_full(cond, active_cse->indent, false, true);
          }
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
          return fs_expr_from_base(base, drive, width);
        }
        case ExprKind::kSelect: {
          FsExpr base = emit_expr4(*expr.base);
          if (expr.indexed_range && expr.indexed_width > 0 && expr.lsb_expr) {
            int width = expr.indexed_width;
            FsExpr shift = emit_expr4(*expr.lsb_expr);
            if (active_cse) {
              shift = maybe_hoist_full(shift, active_cse->indent, false, false);
            }
            std::string mask = mask_literal(width);
            if (shift.is_const) {
              if (shift.const_xz != 0) {
                return fs_allx_expr(width);
              }
              uint32_t idx_val =
                  static_cast<uint32_t>(shift.const_val);
              if (idx_val >= static_cast<uint32_t>(base.width)) {
                return fs_const_expr(0u, 0u, MaskForWidth64(width), width);
              }
              std::string idx = std::to_string(idx_val) + "u";
              if (base.is_const && base.width <= 64 && idx_val < 64) {
                uint64_t mask_value = MaskForWidth64(width);
                uint64_t val_bits = (base.const_val >> idx_val) & mask_value;
                uint64_t xz_bits = (base.const_xz >> idx_val) & mask_value;
                uint64_t drive_bits =
                    (base.const_drive >> idx_val) & mask_value;
                return fs_const_expr(val_bits, xz_bits, drive_bits, width);
              }
              std::string val =
                  "((" + base.val + " >> " + idx + ") & " + mask + ")";
              std::string xz =
                  "((" + base.xz + " >> " + idx + ") & " + mask + ")";
              std::string drive =
                  "((" + base.drive + " >> " + idx + ") & " + mask + ")";
              return FsExpr{val, xz, drive, width};
            }
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
              if (active_cse) {
                idx = maybe_hoist_full(idx, active_cse->indent, false, false);
              }
              std::string idx_val = idx.val;
              std::string idx_xz = idx.xz;
              if (idx.is_const) {
                if (idx.const_xz != 0) {
                  return fs_allx_expr(element_width);
                }
                if (idx.const_val >= static_cast<uint64_t>(array_size)) {
                  return fs_const_expr(0u, 0u, MaskForWidth64(element_width),
                                       element_width);
                }
                std::string base = "(gid * " + std::to_string(array_size) +
                                   "u) + uint(" + idx_val + ")";
                return FsExpr{val_name(expr.base->ident) + "[" + base + "]",
                              xz_name(expr.base->ident) + "[" + base + "]",
                              drive_full(element_width), element_width};
              }
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
          if (active_cse) {
            index = maybe_hoist_full(index, active_cse->indent, false, false);
          }
          int width = 1;
          if (index.is_const) {
            if (index.const_xz != 0) {
              return fs_allx_expr(width);
            }
            std::string idx = index.val;
            std::string val = "(((" + base.val + " >> " + idx + ") & " +
                              literal_for_width(1, 1) + "))";
            std::string xz = "(((" + base.xz + " >> " + idx + ") & " +
                             literal_for_width(1, 1) + "))";
            std::string drive = "(((" + base.drive + " >> " + idx + ") & " +
                                 literal_for_width(1, 1) + "))";
            return FsExpr{val, xz, drive, width};
          }
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
            return FsExpr{"__gpga_time", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$rtoi") {
            int width = ExprWidth(expr, module);
            std::string real_val =
                (!expr.call_args.empty() && expr.call_args.front())
                    ? emit_real_value4(*expr.call_args.front())
                    : "0.0";
            std::string cast = (width > 32) ? "(long)" : "(int)";
            std::string raw = cast + "(" + real_val + ")";
            std::string val = MaskForWidthExpr(raw, width);
            return FsExpr{val, literal_for_width(0, width), drive_full(width),
                          width};
          }
          if (expr.ident == "$realtobits") {
            std::string bits =
                (!expr.call_args.empty() && expr.call_args.front())
                    ? emit_real_bits4(*expr.call_args.front())
                    : "0ul";
            int width = 64;
            return FsExpr{bits, literal_for_width(0, width),
                          drive_full(width), width};
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
      if (ExprIsRealValue(expr, module)) {
        if (target_width == 64) {
          return emit_real_expr4(expr);
        }
        bool signed_expr = ExprSigned(expr, module);
        std::string real_val = emit_real_value4(expr);
        std::string cast;
        if (target_width > 32) {
          cast = signed_expr ? "(long)" : "(ulong)";
        } else {
          cast = signed_expr ? "(int)" : "(uint)";
        }
        std::string raw = cast + "(" + real_val + ")";
        FsExpr out;
        out.width = target_width;
        out.val = MaskForWidthExpr(raw, target_width);
        out.xz = literal_for_width(0, target_width);
        out.drive = drive_full(target_width);
        return out;
      }
      FsExpr out_expr = emit_expr4(expr);
      bool signed_expr = ExprSigned(expr, module);
      return fs_extend_expr(out_expr, target_width, signed_expr);
    };

    auto emit_expr4_with_cse = [&](const Expr& expr, int indent) -> FsExpr {
      CseState cse;
      cse.indent = indent;
      collect_expr_uses(expr, collect_expr_uses, &cse);
      active_cse = &cse;
      FsExpr out_expr = emit_expr4(expr);
      active_cse = nullptr;
      return out_expr;
    };

    auto emit_expr4_sized_with_cse = [&](const Expr& expr, int target_width,
                                         int indent) -> FsExpr {
      if (ExprIsRealValue(expr, module)) {
        return emit_expr4_sized(expr, target_width);
      }
      CseState cse;
      cse.indent = indent;
      collect_expr_uses(expr, collect_expr_uses, &cse);
      active_cse = &cse;
      FsExpr out_expr = emit_expr4(expr);
      active_cse = nullptr;
      bool signed_expr = ExprSigned(expr, module);
      return fs_extend_expr(out_expr, target_width, signed_expr);
    };

    maybe_hoist_full = [&](const FsExpr& expr, int indent,
                           bool need_drive, bool force_small) -> FsExpr {
      if (expr.is_real) {
        return expr;
      }
      if (expr.full.empty() || expr.width <= 0) {
        return expr;
      }
      const size_t kMinHoist = 120;
      size_t min_len = force_small ? 0 : kMinHoist;
      if (expr.full.size() < min_len) {
        return expr;
      }
      if (expr.full.rfind("__gpga_fs_tmp", 0) == 0) {
        return expr;
      }
      std::string name = "__gpga_fs_tmp" + std::to_string(fs_temp_index++);
      std::string type = (expr.width > 32) ? "FourState64" : "FourState32";
      std::string dtype = (expr.width > 32) ? "ulong" : "uint";
      std::string pad(indent, ' ');
      out << pad << type << " " << name << " = " << expr.full << ";\n";
      std::string drive_expr = expr.drive;
      if (need_drive) {
        out << pad << dtype << " " << name << "_drive = " << expr.drive
            << ";\n";
        drive_expr = name + "_drive";
      }
      return FsExpr{name + ".val", name + ".xz", drive_expr, expr.width, name};
    };

    struct ExprCacheEntry {
      FsExpr expr;
      std::unordered_set<std::string> deps;
    };

    struct ExprCache {
      ExprCache* parent = nullptr;
      std::unordered_map<std::string, ExprCacheEntry> entries;
      std::unordered_set<std::string> blocked;
    };

    auto expr_cache_key = [&](const Expr& expr, int target_width) {
      int width = target_width > 0 ? target_width : ExprWidth(expr, module);
      std::string key = expr_key(expr, expr_key) + ":w" +
                        std::to_string(width);
      key += ExprSigned(expr, module) ? ":s" : ":u";
      return key;
    };

    auto cache_entry_blocked = [&](const ExprCache* cache,
                                   const ExprCacheEntry& entry) -> bool {
      for (const auto& dep : entry.deps) {
        for (const ExprCache* cur = cache; cur; cur = cur->parent) {
          if (cur->blocked.count(dep) > 0) {
            return true;
          }
        }
      }
      return false;
    };

    auto cache_lookup = [&](ExprCache* cache,
                            const std::string& key) -> const ExprCacheEntry* {
      for (ExprCache* cur = cache; cur; cur = cur->parent) {
        auto it = cur->entries.find(key);
        if (it == cur->entries.end()) {
          continue;
        }
        if (!cache_entry_blocked(cache, it->second)) {
          return &it->second;
        }
      }
      return nullptr;
    };

    auto emit_expr4_cached = [&](const Expr& expr, int target_width,
                                 int indent, ExprCache* cache) -> FsExpr {
      int width = target_width > 0 ? target_width : ExprWidth(expr, module);
      std::string key = expr_cache_key(expr, width);
      if (cache) {
        if (const ExprCacheEntry* entry = cache_lookup(cache, key)) {
          return entry->expr;
        }
      }
      FsExpr out_expr = emit_expr4_sized_with_cse(expr, width, indent);
      out_expr = maybe_hoist_full(out_expr, indent, false, true);
      if (cache) {
        ExprCacheEntry entry;
        entry.expr = out_expr;
        CollectReadSignalsExpr(expr, &entry.deps);
        cache->entries.emplace(key, std::move(entry));
      }
      return out_expr;
    };

    struct Lvalue4 {
      std::string val;
      std::string xz;
      std::string guard;
      std::string bit_index_val;
      std::string bit_index_xz;
      std::string range_index_val;
      std::string range_index_xz;
      int width = 0;
      int base_width = 0;
      int range_lsb = 0;
      bool ok = false;
      bool is_array = false;
      bool is_bit_select = false;
      bool is_range = false;
      bool is_indexed_range = false;
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
                             bool use_next, int indent) -> Lvalue4 {
      Lvalue4 out;
      if (SignalIsReal(module, assign.lhs)) {
        if (assign.lhs_has_range) {
          return out;
        }
        if ((assign.lhs_index || !assign.lhs_indices.empty()) &&
            !IsArrayNet(module, assign.lhs, nullptr, nullptr)) {
          return out;
        }
      }
      if (assign.lhs_has_range) {
        if (IsArrayNet(module, assign.lhs, nullptr, nullptr)) {
          return out;
        }
        std::string base_val;
        std::string base_xz;
        if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
          base_val = val_name(assign.lhs) + "[gid]";
          base_xz = xz_name(assign.lhs) + "[gid]";
        } else if (locals.count(assign.lhs) > 0) {
          base_val = val_name(assign.lhs);
          base_xz = xz_name(assign.lhs);
        } else {
          return out;
        }
        out.val = base_val;
        out.xz = base_xz;
        out.base_width = SignalWidth(module, assign.lhs);
        out.ok = true;
        out.is_range = true;
        if (assign.lhs_indexed_range) {
          if (!assign.lhs_lsb_expr || assign.lhs_indexed_width <= 0) {
            return Lvalue4{};
          }
          FsExpr idx = emit_expr4(*assign.lhs_lsb_expr);
          idx = maybe_hoist_full(idx, indent, false, false);
          int width = assign.lhs_indexed_width;
          out.range_index_val = idx.val;
          out.range_index_xz = idx.xz;
          out.width = width;
          out.is_indexed_range = true;
          if (idx.is_const) {
            if (idx.const_xz != 0) {
              return Lvalue4{};
            }
            if (idx.const_val + static_cast<uint64_t>(width) >
                static_cast<uint64_t>(out.base_width)) {
              return Lvalue4{};
            }
          } else {
            if (out.base_width >= width) {
              int limit = out.base_width - width;
              out.guard = "(" + idx.xz + " == " +
                          literal_for_width(0, idx.width) + " && " + idx.val +
                          " <= " + std::to_string(limit) + "u)";
            } else {
              out.guard = "false";
            }
          }
          return out;
        }
        int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
        int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
        out.range_lsb = lo;
        out.width = hi - lo + 1;
        return out;
      }
      if (assign.lhs_index) {
        int element_width = 0;
        int array_size = 0;
        if (!IsArrayNet(module, assign.lhs, &element_width, &array_size)) {
          std::string base_val;
          std::string base_xz;
          if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
            base_val = val_name(assign.lhs) + "[gid]";
            base_xz = xz_name(assign.lhs) + "[gid]";
          } else if (locals.count(assign.lhs) > 0) {
            base_val = val_name(assign.lhs);
            base_xz = xz_name(assign.lhs);
          } else {
            return out;
          }
          FsExpr idx = emit_expr4(*assign.lhs_index);
          idx = maybe_hoist_full(idx, indent, false, false);
          std::string idx_val = idx.val;
          std::string idx_xz = idx.xz;
          int base_width = SignalWidth(module, assign.lhs);
          if (idx.is_const) {
            if (idx.const_xz != 0) {
              return out;
            }
            if (idx.const_val >= static_cast<uint64_t>(base_width)) {
              return out;
            }
          } else {
            out.guard = "(" + idx_xz + " == " +
                        literal_for_width(0, idx.width) + " && " + idx_val +
                        " < " + std::to_string(base_width) + "u)";
          }
          out.val = base_val;
          out.xz = base_xz;
          out.bit_index_val = idx_val;
          out.bit_index_xz = idx_xz;
          out.width = 1;
          out.base_width = base_width;
          out.ok = true;
          out.is_bit_select = true;
          return out;
        }
        FsExpr idx = emit_expr4(*assign.lhs_index);
        idx = maybe_hoist_full(idx, indent, false, false);
        std::string idx_val = idx.val;
        std::string idx_xz = idx.xz;
        if (idx.is_const) {
          if (idx.const_xz != 0) {
            return out;
          }
          if (idx.const_val >= static_cast<uint64_t>(array_size)) {
            return out;
          }
        } else {
          out.guard = "(" + idx_xz + " == " +
                      literal_for_width(0, idx.width) + " && " + idx_val +
                      " < " + std::to_string(array_size) + "u)";
        }
        std::string base = "(gid * " + std::to_string(array_size) +
                           "u) + uint(" + idx_val + ")";
        std::string name = assign.lhs;
        if (use_next) {
          name += "_next";
        }
        out.val = val_name(name) + "[" + base + "]";
        out.xz = xz_name(name) + "[" + base + "]";
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

    std::vector<std::string> reg_names;
    std::vector<std::string> export_wires;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (port_names.count(net.name) > 0 || IsTriregNet(net.type)) {
        continue;
      }
      if (net.type == NetType::kReg) {
        reg_names.push_back(net.name);
        continue;
      }
      if (scheduled_reads.count(net.name) > 0) {
        reg_names.push_back(net.name);
        export_wires.push_back(net.name);
      }
    }
    std::unordered_set<std::string> export_wire_set(export_wires.begin(),
                                                     export_wires.end());
    std::vector<const Net*> trireg_nets;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (net.type == NetType::kTrireg && !IsOutputPort(module, net.name)) {
        trireg_nets.push_back(&net);
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

    std::unordered_set<std::string> switch_nets;
    for (const auto& sw : module.switches) {
      switch_nets.insert(sw.a);
      switch_nets.insert(sw.b);
    }
    std::unordered_set<std::string> drive_declared;
    std::unordered_map<std::string, std::string> drive_vars;
    auto drive_var_name = [&](const std::string& name) -> std::string {
      return "__gpga_drive_" + name;
    };
    auto drive_init_for = [&](const std::string& name, int width) -> std::string {
      const Port* port = FindPort(module, name);
      if (port && (port->dir == PortDir::kInput ||
                   port->dir == PortDir::kInout)) {
        return drive_full(width);
      }
      NetType net_type = SignalNetType(module, name);
      if (net_type == NetType::kReg || IsTriregNet(net_type)) {
        return drive_full(width);
      }
      return drive_zero(width);
    };
    auto ensure_drive_declared =
        [&](const std::string& name, int width,
            const std::string& init) -> std::string {
          std::string var = drive_var_name(name);
          drive_vars[name] = var;
          if (drive_declared.insert(name).second) {
            std::string type = TypeForWidth(width);
            out << "  " << type << " " << var << " = " << init << ";\n";
          }
          return var;
        };

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
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg));
      out << "  device " << type << "* " << val_name(reg) << " [[buffer("
          << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device " << type << "* " << xz_name(reg) << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto* reg : trireg_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg->name));
      out << "  device " << type << "* " << val_name(reg->name)
          << " [[buffer("
          << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device " << type << "* " << xz_name(reg->name)
          << " [[buffer("
          << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device ulong* " << decay_name(reg->name) << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto* net : array_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(net->width);
      out << "  device " << type << "* " << val_name(net->name)
          << " [[buffer(" << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device " << type << "* " << xz_name(net->name)
          << " [[buffer(" << buffer_index++ << ")]]";
    }
    if (!first) {
      out << ",\n";
    }
    first = false;
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
      if (net.type == NetType::kReg || IsTriregNet(net.type) ||
          export_wire_set.count(net.name) > 0) {
        if (port_names.count(net.name) == 0) {
          regs.insert(net.name);
        }
        continue;
      }
      if (port_names.count(net.name) == 0) {
        locals.insert(net.name);
      }
    }

    auto driven = CollectDrivenSignals(module);
    for (const auto& net : module.nets) {
      if (net.array_size > 0 || IsTriregNet(net.type)) {
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
    std::unordered_map<std::string, size_t> drivers_remaining_template;
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
      drivers_remaining_template[entry.first] = entry.second.size();
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
      bool lhs_real = SignalIsReal(module, assign.lhs);
      int lhs_width = SignalWidth(module, assign.lhs);
      std::string type = TypeForWidth(lhs_width);
      if (assign.lhs_has_range) {
        if (lhs_real) {
          out << "  // Unsupported real range driver: " << assign.lhs << "\n";
          return;
        }
        int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
        int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
        int slice_width = hi - lo + 1;
        FsExpr rhs = emit_expr4_sized_with_cse(*assign.rhs, slice_width, 2);
        rhs = maybe_hoist_full(rhs, 2, true, true);
        std::string mask = mask_literal(slice_width);
        std::string cast = CastForWidth(lhs_width);
        out << "  " << type << " " << info.val << " = ((" << cast << rhs.val
            << " & " << mask << ") << " << std::to_string(lo) << "u);\n";
        out << "  " << type << " " << info.xz << " = ((" << cast << rhs.xz
            << " & " << mask << ") << " << std::to_string(lo) << "u);\n";
        out << "  " << type << " " << info.drive << " = ((" << cast
            << rhs.drive << " & " << mask << ") << " << std::to_string(lo)
            << "u);\n";
        return;
      }
      FsExpr rhs = lhs_real ? emit_real_expr4(*assign.rhs)
                            : emit_expr4_sized_with_cse(*assign.rhs, lhs_width,
                                                        2);
      rhs = maybe_hoist_full(rhs, 2, true, true);
      out << "  " << type << " " << info.val << " = " << rhs.val << ";\n";
      out << "  " << type << " " << info.xz << " = " << rhs.xz << ";\n";
      out << "  " << type << " " << info.drive << " = " << rhs.drive << ";\n";
    };

    auto emit_resolve = [&](const std::string& name,
                            const std::vector<size_t>& indices,
                            const std::unordered_set<std::string>& locals_ctx,
                            const std::unordered_set<std::string>& regs_ctx,
                            std::unordered_set<std::string>* declared_ctx) {
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
      } else {
        out << "    uint best0 = 0u;\n";
        out << "    uint best1 = 0u;\n";
        out << "    uint bestx = 0u;\n";
        for (size_t idx : indices) {
          const auto& info = driver_info[idx];
          out << "    if ((" << info.drive << " & mask) != " << zero << ") {\n";
          out << "      if ((" << info.xz << " & mask) != " << zero << ") {\n";
          if (info.strength0 == info.strength1) {
            out << "        uint x_strength = " << info.strength0 << ";\n";
          } else {
            out << "        uint x_strength = (" << info.strength0 << " > "
                << info.strength1 << ") ? " << info.strength0 << " : "
                << info.strength1 << ";\n";
          }
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
      }
      out << "  }\n";

      if (switch_nets.count(name) > 0) {
        ensure_drive_declared(name, lhs_width, drive_zero(lhs_width));
        out << "  " << drive_var_name(name) << " = " << resolved_drive << ";\n";
      }

      bool is_output = IsOutputPort(module, name) || regs_ctx.count(name) > 0;
      bool is_local = locals_ctx.count(name) > 0 && !is_output &&
                      regs_ctx.count(name) == 0;
      if (is_output) {
        if (is_trireg) {
          std::string decay_ref = decay_name(name) + "[gid]";
          std::string decay_delay = trireg_decay_delay(name);
          std::string drive_flag = "__gpga_trireg_drive_" + name;
          std::string decay_flag = "__gpga_trireg_decay_" + name;
          out << "  bool " << drive_flag << " = (" << resolved_drive
              << " != " << zero << ");\n";
          out << "  if (" << drive_flag << ") {\n";
          out << "    " << decay_ref << " = __gpga_time + " << decay_delay
              << ";\n";
          out << "  }\n";
          out << "  if (!" << drive_flag << " && " << decay_ref
              << " == 0ul) {\n";
          out << "    " << decay_ref << " = __gpga_time + " << decay_delay
              << ";\n";
          out << "  }\n";
          out << "  bool " << decay_flag << " = (!" << drive_flag << " && "
              << decay_ref << " != 0ul && __gpga_time >= " << decay_ref
              << ");\n";
          out << "  " << val_name(name) << "[gid] = ("
              << val_name(name) << "[gid] & ~" << resolved_drive << ") | ("
              << resolved_val << " & " << resolved_drive << ");\n";
          out << "  " << xz_name(name) << "[gid] = ("
              << xz_name(name) << "[gid] & ~" << resolved_drive << ") | ("
              << resolved_xz << " & " << resolved_drive << ");\n";
          out << "  if (" << decay_flag << ") {\n";
          out << "    " << xz_name(name) << "[gid] |= "
              << drive_full(lhs_width) << ";\n";
          out << "  }\n";
        } else {
          out << "  " << val_name(name) << "[gid] = " << resolved_val << ";\n";
          out << "  " << xz_name(name) << "[gid] = " << resolved_xz << ";\n";
        }
      } else if (is_local) {
        if (declared_ctx && declared_ctx->count(name) == 0) {
          out << "  " << type << " " << val_name(name) << ";\n";
          out << "  " << type << " " << xz_name(name) << ";\n";
          if (declared_ctx) {
            declared_ctx->insert(name);
          }
        }
        out << "  " << val_name(name) << " = " << resolved_val << ";\n";
        out << "  " << xz_name(name) << " = " << resolved_xz << ";\n";
      } else {
        out << "  // Unmapped resolved assign: " << name << "\n";
      }
    };
    auto emit_continuous_assigns =
        [&](const std::unordered_set<std::string>& locals_ctx,
            const std::unordered_set<std::string>& regs_ctx,
            std::unordered_set<std::string>* declared_ctx) {
          std::unordered_map<std::string, size_t> drivers_remaining =
              drivers_remaining_template;
          std::unordered_map<std::string, std::vector<const Assign*>>
              partial_assigns;
          for (const auto& assign : module.assigns) {
            if (assign.lhs_has_range && multi_driver.count(assign.lhs) == 0) {
              if (SignalIsReal(module, assign.lhs)) {
                continue;
              }
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
                  emit_resolve(assign.lhs, drivers_for_net[assign.lhs],
                               locals_ctx, regs_ctx, declared_ctx);
                }
              }
              continue;
            }
            if (assign.lhs_has_range) {
              continue;
            }
            Lvalue4 lhs = build_lvalue4_assign(assign, locals_ctx, regs_ctx);
            if (!lhs.ok) {
              continue;
            }
            bool lhs_real = SignalIsReal(module, assign.lhs);
            FsExpr rhs = lhs_real
                             ? emit_real_expr4(*assign.rhs)
                             : emit_expr4_sized_with_cse(*assign.rhs, lhs.width,
                                                         2);
            rhs = maybe_hoist_full(rhs, 2, false, true);
            if (IsOutputPort(module, assign.lhs) ||
                regs_ctx.count(assign.lhs) > 0) {
              out << "  " << lhs.val << " = " << rhs.val << ";\n";
              out << "  " << lhs.xz << " = " << rhs.xz << ";\n";
            } else if (locals_ctx.count(assign.lhs) > 0) {
              if (declared_ctx && declared_ctx->count(assign.lhs) == 0) {
                std::string type = TypeForWidth(lhs.width);
                out << "  " << type << " " << lhs.val << " = " << rhs.val
                    << ";\n";
                out << "  " << type << " " << lhs.xz << " = " << rhs.xz
                    << ";\n";
                declared_ctx->insert(assign.lhs);
              } else {
                out << "  " << lhs.val << " = " << rhs.val << ";\n";
                out << "  " << lhs.xz << " = " << rhs.xz << ";\n";
              }
            }
            if (switch_nets.count(assign.lhs) > 0) {
              std::string drive_var =
                  ensure_drive_declared(assign.lhs, lhs.width,
                                        drive_zero(lhs.width));
              out << "  " << drive_var << " = " << rhs.drive << ";\n";
            }
          }
          for (const auto& entry : partial_assigns) {
            const std::string& name = entry.first;
            int lhs_width = SignalWidth(module, name);
            std::string type = TypeForWidth(lhs_width);
            bool target_is_local =
                locals_ctx.count(name) > 0 && !IsOutputPort(module, name) &&
                regs_ctx.count(name) == 0;
            std::string temp_val =
                target_is_local ? val_name(name)
                                : ("__gpga_partial_" + name + "_val");
            std::string temp_xz =
                target_is_local ? xz_name(name)
                                : ("__gpga_partial_" + name + "_xz");
            bool track_drive = switch_nets.count(name) > 0;
            std::string temp_drive = "__gpga_partial_" + name + "_drive";
            std::string zero = literal_for_width(0, lhs_width);
            if (target_is_local) {
              if (declared_ctx && declared_ctx->count(name) == 0) {
                out << "  " << type << " " << temp_val << " = " << zero
                    << ";\n";
                out << "  " << type << " " << temp_xz << " = " << zero
                    << ";\n";
                if (track_drive) {
                  out << "  " << type << " " << temp_drive << " = " << zero
                      << ";\n";
                }
                declared_ctx->insert(name);
              } else {
                out << "  " << temp_val << " = " << zero << ";\n";
                out << "  " << temp_xz << " = " << zero << ";\n";
                if (track_drive) {
                  out << "  " << temp_drive << " = " << zero << ";\n";
                }
              }
            } else {
              out << "  " << type << " " << temp_val << " = " << zero
                  << ";\n";
              out << "  " << type << " " << temp_xz << " = " << zero << ";\n";
              if (track_drive) {
                out << "  " << type << " " << temp_drive << " = " << zero
                    << ";\n";
              }
            }
            for (const auto* assign : entry.second) {
              int lo = std::min(assign->lhs_msb, assign->lhs_lsb);
              int hi = std::max(assign->lhs_msb, assign->lhs_lsb);
              int slice_width = hi - lo + 1;
              FsExpr rhs =
                  emit_expr4_sized_with_cse(*assign->rhs, slice_width, 2);
              rhs = maybe_hoist_full(rhs, 2, false, true);
              std::string mask = mask_literal(slice_width);
              std::string shifted_mask =
                  "(" + mask + " << " + std::to_string(lo) + "u)";
              std::string cast = CastForWidth(lhs_width);
              out << "  " << temp_val << " = (" << temp_val << " & ~"
                  << shifted_mask << ") | ((" << cast << rhs.val << " & "
                  << mask << ") << " << std::to_string(lo) << "u);\n";
              out << "  " << temp_xz << " = (" << temp_xz << " & ~"
                  << shifted_mask << ") | ((" << cast << rhs.xz << " & "
                  << mask << ") << " << std::to_string(lo) << "u);\n";
              if (track_drive) {
                out << "  " << temp_drive << " = (" << temp_drive << " & ~"
                    << shifted_mask << ") | ((" << cast << rhs.drive << " & "
                    << mask << ") << " << std::to_string(lo) << "u);\n";
              }
            }
            if (!target_is_local) {
              if (IsOutputPort(module, name) || regs_ctx.count(name) > 0) {
                out << "  " << val_name(name) << "[gid] = " << temp_val
                    << ";\n";
                out << "  " << xz_name(name) << "[gid] = " << temp_xz
                    << ";\n";
              } else if (locals_ctx.count(name) > 0) {
                if (declared_ctx && declared_ctx->count(name) == 0) {
                  out << "  " << type << " " << val_name(name) << " = "
                      << temp_val << ";\n";
                  out << "  " << type << " " << xz_name(name) << " = "
                      << temp_xz << ";\n";
                  declared_ctx->insert(name);
                } else {
                  out << "  " << val_name(name) << " = " << temp_val << ";\n";
                  out << "  " << xz_name(name) << " = " << temp_xz << ";\n";
                }
              } else {
                out << "  // Unmapped assign: " << name << " = " << temp_val
                    << ";\n";
              }
            }
            if (track_drive) {
              std::string drive_var =
                  ensure_drive_declared(name, lhs_width,
                                        drive_zero(lhs_width));
              out << "  " << drive_var << " = " << temp_drive << ";\n";
            }
          }
        };
    emit_continuous_assigns(locals, regs, &declared);

    for (const auto& name : switch_nets) {
      if (drive_declared.count(name) > 0) {
        continue;
      }
      int width = SignalWidth(module, name);
      ensure_drive_declared(name, width, drive_init_for(name, width));
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
      if (expr.is_real) {
        return "(gpga_bits_to_real(" + expr.val + ") != 0.0)";
      }
      return "(" + expr.xz + " == " + literal_for_width(0, expr.width) +
             " && " + expr.val + " != " + literal_for_width(0, expr.width) +
             ")";
    };

    auto eval_const_bool = [&](const FsExpr& expr, bool* value) -> bool {
      if (!value || !expr.is_const || expr.width > 64) {
        return false;
      }
      if (expr.const_xz != 0) {
        *value = false;
        return true;
      }
      *value = (expr.const_val != 0);
      return true;
    };

    auto fs_merge_expr = [&](FsExpr lhs, FsExpr rhs, int width) -> FsExpr {
      lhs = fs_resize_expr(lhs, width);
      rhs = fs_resize_expr(rhs, width);
      std::string func = (width > 32) ? "fs_merge64" : "fs_merge32";
      std::string base =
          func + "(" + fs_make_expr(lhs, width) + ", " +
          fs_make_expr(rhs, width) + ", " + std::to_string(width) + "u)";
      return fs_expr_from_base(base, drive_full(width), width);
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

    std::function<void(const Statement&, int, ExprCache*)> emit_comb_stmt;
    auto emit_bit_select4 = [&](const Lvalue4& lhs, const FsExpr& rhs,
                                const std::string& target_val,
                                const std::string& target_xz, int indent) {
      std::string pad(indent, ' ');
      std::string idx = "uint(" + lhs.bit_index_val + ")";
      std::string one = (lhs.base_width > 32) ? "1ul" : "1u";
      std::string cast = CastForWidth(lhs.base_width);
      std::string mask = "(" + one + " << " + idx + ")";
      std::string rhs_val_masked = MaskForWidthExpr(rhs.val, 1);
      std::string rhs_xz_masked = MaskForWidthExpr(rhs.xz, 1);
      std::string update_val =
          "(" + target_val + " & ~" + mask + ") | ((" + cast + rhs_val_masked +
          ") << " + idx + ")";
      std::string update_xz =
          "(" + target_xz + " & ~" + mask + ") | ((" + cast + rhs_xz_masked +
          ") << " + idx + ")";
      if (!lhs.guard.empty()) {
        out << pad << "if " << lhs.guard << " {\n";
        out << pad << "  " << target_val << " = " << update_val << ";\n";
        out << pad << "  " << target_xz << " = " << update_xz << ";\n";
        out << pad << "}\n";
      } else {
        out << pad << target_val << " = " << update_val << ";\n";
        out << pad << target_xz << " = " << update_xz << ";\n";
      }
    };
    auto emit_range_select4 = [&](const Lvalue4& lhs, const FsExpr& rhs,
                                  const std::string& target_val,
                                  const std::string& target_xz, int indent) {
      std::string pad(indent, ' ');
      std::string idx = lhs.is_indexed_range
                            ? "uint(" + lhs.range_index_val + ")"
                            : std::to_string(lhs.range_lsb) + "u";
      uint64_t slice_mask = MaskForWidth64(lhs.width);
      uint64_t base_mask = MaskForWidth64(lhs.base_width);
      std::string suffix = (lhs.base_width > 32) ? "ul" : "u";
      std::string slice_literal = std::to_string(slice_mask) + suffix;
      std::string base_literal = std::to_string(base_mask) + suffix;
      std::string cast = CastForWidth(lhs.base_width);
      std::string rhs_val_masked = MaskForWidthExpr(rhs.val, lhs.width);
      std::string rhs_xz_masked = MaskForWidthExpr(rhs.xz, lhs.width);
      std::string mask =
          "((" + slice_literal + " << " + idx + ") & " + base_literal + ")";
      std::string update_val =
          "(" + target_val + " & ~" + mask + ") | ((" + cast +
          rhs_val_masked + " & " + slice_literal + ") << " + idx + ")";
      std::string update_xz =
          "(" + target_xz + " & ~" + mask + ") | ((" + cast +
          rhs_xz_masked + " & " + slice_literal + ") << " + idx + ")";
      if (!lhs.guard.empty()) {
        out << pad << "if " << lhs.guard << " {\n";
        out << pad << "  " << target_val << " = " << update_val << ";\n";
        out << pad << "  " << target_xz << " = " << update_xz << ";\n";
        out << pad << "}\n";
      } else {
        out << pad << target_val << " = " << update_val << ";\n";
        out << pad << target_xz << " = " << update_xz << ";\n";
      }
    };
    emit_comb_stmt = [&](const Statement& stmt, int indent,
                         ExprCache* cache) {
      std::string pad(indent, ' ');
      if (stmt.kind == StatementKind::kAssign) {
        if (!stmt.assign.rhs) {
          return;
        }
        Lvalue4 lhs = build_lvalue4(stmt.assign, locals, regs, false, indent);
        if (!lhs.ok) {
          return;
        }
        bool lhs_real = SignalIsReal(module, stmt.assign.lhs);
        FsExpr rhs = lhs_real
                         ? emit_real_expr4(*stmt.assign.rhs)
                         : emit_expr4_cached(*stmt.assign.rhs, lhs.width,
                                             indent, cache);
        if (lhs.is_bit_select) {
          emit_bit_select4(lhs, rhs, lhs.val, lhs.xz, indent);
          if (cache) {
            cache->blocked.insert(stmt.assign.lhs);
          }
          return;
        }
        if (lhs.is_range) {
          emit_range_select4(lhs, rhs, lhs.val, lhs.xz, indent);
          if (cache) {
            cache->blocked.insert(stmt.assign.lhs);
          }
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
        if (cache) {
          cache->blocked.insert(stmt.assign.lhs);
        }
        return;
      }
      if (stmt.kind == StatementKind::kIf) {
        FsExpr cond =
            stmt.condition
                ? emit_expr4_cached(*stmt.condition,
                                    ExprWidth(*stmt.condition, module), indent,
                                    cache)
                           : FsExpr{literal_for_width(0, 1),
                                    literal_for_width(0, 1),
                                    drive_full(1), 1};
        bool cond_value = false;
        if (eval_const_bool(cond, &cond_value)) {
          const auto& branch =
              cond_value ? stmt.then_branch : stmt.else_branch;
          for (const auto& inner : branch) {
            emit_comb_stmt(inner, indent, cache);
          }
          return;
        }
        out << pad << "if (" << cond_bool(cond) << ") {\n";
        ExprCache then_cache;
        then_cache.parent = cache;
        for (const auto& inner : stmt.then_branch) {
          emit_comb_stmt(inner, indent + 2, &then_cache);
        }
        if (!stmt.else_branch.empty()) {
          out << pad << "} else {\n";
          ExprCache else_cache;
          else_cache.parent = cache;
          for (const auto& inner : stmt.else_branch) {
            emit_comb_stmt(inner, indent + 2, &else_cache);
          }
          if (cache) {
            for (const auto& name : then_cache.blocked) {
              cache->blocked.insert(name);
            }
            for (const auto& name : else_cache.blocked) {
              cache->blocked.insert(name);
            }
          }
          out << pad << "}\n";
        } else {
          if (cache) {
            for (const auto& name : then_cache.blocked) {
              cache->blocked.insert(name);
            }
          }
          out << pad << "}\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kCase) {
        FsExpr case_expr =
            stmt.case_expr
                ? emit_expr4_cached(*stmt.case_expr,
                                    ExprWidth(*stmt.case_expr, module), indent,
                                    cache)
                : FsExpr{literal_for_width(0, 1), literal_for_width(0, 1),
                         drive_full(1), 1};
        bool first_case = true;
        std::unordered_set<std::string> case_blocked;
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
          ExprCache branch_cache;
          branch_cache.parent = cache;
          for (const auto& inner : item.body) {
            emit_comb_stmt(inner, indent + 2, &branch_cache);
          }
          for (const auto& name : branch_cache.blocked) {
            case_blocked.insert(name);
          }
        }
        if (!stmt.default_branch.empty()) {
          out << pad << "} else {\n";
          ExprCache branch_cache;
          branch_cache.parent = cache;
          for (const auto& inner : stmt.default_branch) {
            emit_comb_stmt(inner, indent + 2, &branch_cache);
          }
          for (const auto& name : branch_cache.blocked) {
            case_blocked.insert(name);
          }
          out << pad << "}\n";
        } else if (!first_case) {
          out << pad << "}\n";
        }
        if (cache) {
          for (const auto& name : case_blocked) {
            cache->blocked.insert(name);
          }
        }
        return;
      }
      if (stmt.kind == StatementKind::kBlock) {
        out << pad << "{\n";
        for (const auto& inner : stmt.block) {
          emit_comb_stmt(inner, indent + 2, cache);
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == StatementKind::kDelay) {
        out << pad << "// delay control ignored in MSL v0\n";
        for (const auto& inner : stmt.delay_body) {
          emit_comb_stmt(inner, indent, cache);
        }
        return;
      }
      if (stmt.kind == StatementKind::kEventControl) {
        out << pad << "// event control ignored in MSL v0\n";
        for (const auto& inner : stmt.event_body) {
          emit_comb_stmt(inner, indent, cache);
        }
        return;
      }
      if (stmt.kind == StatementKind::kWait) {
        out << pad << "// wait ignored in MSL v0\n";
        for (const auto& inner : stmt.wait_body) {
          emit_comb_stmt(inner, indent, cache);
        }
        return;
      }
      if (stmt.kind == StatementKind::kForever) {
        out << pad << "// forever ignored in MSL v0\n";
        return;
      }
      if (stmt.kind == StatementKind::kFork) {
        out << pad << "// fork/join executed sequentially in MSL v0\n";
        std::unordered_set<std::string> fork_blocked;
        for (const auto& inner : stmt.fork_branches) {
          ExprCache branch_cache;
          branch_cache.parent = cache;
          emit_comb_stmt(inner, indent, &branch_cache);
          for (const auto& name : branch_cache.blocked) {
            fork_blocked.insert(name);
          }
        }
        if (cache) {
          for (const auto& name : fork_blocked) {
            cache->blocked.insert(name);
          }
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
      if (stmt.kind == StatementKind::kForce ||
          stmt.kind == StatementKind::kRelease) {
        out << pad << "// force/release ignored in MSL v0\n";
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
      ExprCache block_cache;
      for (const auto& stmt : block.statements) {
        emit_comb_stmt(stmt, 2, &block_cache);
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
      int temp_index = switch_temp_index++;
      std::string fs_type = (width > 32) ? "FourState64" : "FourState32";
      std::string type = TypeForWidth(width);
      std::string zero = literal_for_width(0, width);
      std::string one = (width > 32) ? "1ul" : "1u";
      std::string strength0 = StrengthLiteral(sw.strength0);
      std::string strength1 = StrengthLiteral(sw.strength1);
      std::string x_strength = (strength0 == strength1)
                                   ? strength0
                                   : ("(" + strength0 + " > " + strength1 +
                                      ") ? " + strength0 + " : " + strength1);
      std::string a_tmp = "__gpga_sw_a" + std::to_string(temp_index);
      std::string b_tmp = "__gpga_sw_b" + std::to_string(temp_index);
      std::string m_val = "__gpga_sw_val" + std::to_string(temp_index);
      std::string m_xz = "__gpga_sw_xz" + std::to_string(temp_index);
      std::string m_drive = "__gpga_sw_drive" + std::to_string(temp_index);
      std::string a_drive = drive_var_name(sw.a);
      std::string b_drive = drive_var_name(sw.b);
      out << "    " << fs_type << " " << a_tmp << " = "
          << fs_make_expr(a_expr, width) << ";\n";
      out << "    " << fs_type << " " << b_tmp << " = "
          << fs_make_expr(b_expr, width) << ";\n";
      out << "    " << type << " " << m_val << " = " << zero << ";\n";
      out << "    " << type << " " << m_xz << " = " << zero << ";\n";
      out << "    " << type << " " << m_drive << " = " << zero << ";\n";
      out << "    for (uint bit = 0u; bit < " << width << "u; ++bit) {\n";
      out << "      " << type << " mask = (" << one << " << bit);\n";
      out << "      uint best0 = 0u;\n";
      out << "      uint best1 = 0u;\n";
      out << "      uint bestx = 0u;\n";
      out << "      if ((" << a_drive << " & mask) != " << zero << ") {\n";
      out << "        if ((" << a_tmp << ".xz & mask) != " << zero << ") {\n";
      out << "          bestx = (bestx > " << x_strength << ") ? bestx : "
          << x_strength << ";\n";
      out << "        } else if ((" << a_tmp << ".val & mask) != " << zero
          << ") {\n";
      out << "          best1 = (best1 > " << strength1 << ") ? best1 : "
          << strength1 << ";\n";
      out << "        } else {\n";
      out << "          best0 = (best0 > " << strength0 << ") ? best0 : "
          << strength0 << ";\n";
      out << "        }\n";
      out << "      }\n";
      out << "      if ((" << b_drive << " & mask) != " << zero << ") {\n";
      out << "        if ((" << b_tmp << ".xz & mask) != " << zero << ") {\n";
      out << "          bestx = (bestx > " << x_strength << ") ? bestx : "
          << x_strength << ";\n";
      out << "        } else if ((" << b_tmp << ".val & mask) != " << zero
          << ") {\n";
      out << "          best1 = (best1 > " << strength1 << ") ? best1 : "
          << strength1 << ";\n";
      out << "        } else {\n";
      out << "          best0 = (best0 > " << strength0 << ") ? best0 : "
          << strength0 << ";\n";
      out << "        }\n";
      out << "      }\n";
      out << "      if (best0 == 0u && best1 == 0u && bestx == 0u) {\n";
      out << "        " << m_xz << " |= mask;\n";
      out << "        continue;\n";
      out << "      }\n";
      out << "      " << m_drive << " |= mask;\n";
      out << "      uint max01 = (best0 > best1) ? best0 : best1;\n";
      out << "      if ((bestx >= max01) && max01 != 0u) {\n";
      out << "        " << m_xz << " |= mask;\n";
      out << "      } else if (best0 > best1) {\n";
      out << "        // 0 wins\n";
      out << "      } else if (best1 > best0) {\n";
      out << "        " << m_val << " |= mask;\n";
      out << "      } else {\n";
      out << "        " << m_xz << " |= mask;\n";
      out << "      }\n";
      out << "    }\n";
      out << "    " << a_val << " = " << m_val << ";\n";
      out << "    " << a_xz << " = " << m_xz << ";\n";
      out << "    " << b_val << " = " << m_val << ";\n";
      out << "    " << b_xz << " = " << m_xz << ";\n";
      out << "    " << a_drive << " = " << m_drive << ";\n";
      out << "    " << b_drive << " = " << m_drive << ";\n";
      out << "  }\n";
    }
    out << "}\n";

    if (has_initial && !needs_scheduler) {
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
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(SignalWidth(module, reg));
        out << "  device " << type << "* " << val_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
      }
      for (const auto* reg : trireg_nets) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(SignalWidth(module, reg->name));
        out << "  device " << type << "* " << val_name(reg->name)
            << " [[buffer("
            << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(reg->name)
            << " [[buffer("
            << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device ulong* " << decay_name(reg->name) << " [[buffer("
            << buffer_index++ << ")]]";
      }
      for (const auto* net : array_nets) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(net->width);
        out << "  device " << type << "* " << val_name(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
      }
      if (!first) {
        out << ",\n";
      }
      first = false;
      out << "  constant GpgaParams& params [[buffer(" << buffer_index++
          << ")]],\n";
      out << "  uint gid [[thread_position_in_grid]]) {\n";
      out << "  if (gid >= params.count) {\n";
      out << "    return;\n";
      out << "  }\n";
      for (const auto* reg : trireg_nets) {
        out << "  " << decay_name(reg->name) << "[gid] = 0ul;\n";
      }

      std::unordered_set<std::string> init_locals;
      std::unordered_set<std::string> init_regs;
      std::unordered_set<std::string> init_declared;
      for (const auto& net : module.nets) {
        if (net.array_size > 0) {
          continue;
        }
        if (net.type == NetType::kReg || IsTriregNet(net.type) ||
            export_wire_set.count(net.name) > 0) {
          if (port_names.count(net.name) == 0) {
            init_regs.insert(net.name);
          }
          continue;
        }
        if (port_names.count(net.name) == 0) {
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
      for (const auto& net : module.nets) {
        if (net.array_size > 0) {
          continue;
        }
        if (init_locals.count(net.name) == 0 ||
            init_declared.count(net.name) > 0) {
          continue;
        }
        std::string type = TypeForWidth(net.width);
        std::string zero = literal_for_width(0, net.width);
        std::string mask = mask_literal(net.width);
        out << "  " << type << " " << val_name(net.name) << " = " << zero
            << ";\n";
        out << "  " << type << " " << xz_name(net.name) << " = " << mask
            << ";\n";
        init_declared.insert(net.name);
      }

      emit_continuous_assigns(init_locals, init_regs, &init_declared);

      std::function<void(const Statement&, int, ExprCache*)> emit_init_stmt;
      std::function<void(const std::vector<Statement>&, int, ExprCache*)>
          emit_init_block;
      auto emit_init_bit_select4 =
          [&](const Lvalue4& lhs, const FsExpr& rhs,
              const std::string& target_val, const std::string& target_xz,
              int indent) {
            emit_bit_select4(lhs, rhs, target_val, target_xz, indent);
          };
      auto emit_init_range_select4 =
          [&](const Lvalue4& lhs, const FsExpr& rhs,
              const std::string& target_val, const std::string& target_xz,
              int indent) {
            emit_range_select4(lhs, rhs, target_val, target_xz, indent);
          };
      emit_init_stmt = [&](const Statement& stmt, int indent,
                           ExprCache* cache) {
        std::string pad(indent, ' ');
        if (stmt.kind == StatementKind::kAssign) {
          if (!stmt.assign.rhs) {
            return;
          }
          Lvalue4 lhs = build_lvalue4(stmt.assign, init_locals, init_regs,
                                      false, indent);
          if (!lhs.ok) {
            return;
          }
          bool lhs_real = SignalIsReal(module, stmt.assign.lhs);
          FsExpr rhs = lhs_real
                           ? emit_real_expr4(*stmt.assign.rhs)
                           : emit_expr4_cached(*stmt.assign.rhs, lhs.width,
                                               indent, cache);
          if (lhs.is_bit_select) {
            emit_init_bit_select4(lhs, rhs, lhs.val, lhs.xz, indent);
            if (cache) {
              cache->blocked.insert(stmt.assign.lhs);
            }
            return;
          }
          if (lhs.is_range) {
            emit_init_range_select4(lhs, rhs, lhs.val, lhs.xz, indent);
            if (cache) {
              cache->blocked.insert(stmt.assign.lhs);
            }
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
          if (cache) {
            cache->blocked.insert(stmt.assign.lhs);
          }
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          FsExpr cond = stmt.condition
                            ? emit_expr4_cached(*stmt.condition,
                                                ExprWidth(*stmt.condition,
                                                          module),
                                                indent, cache)
                            : fs_allx_expr(1);
          bool cond_value = false;
          if (eval_const_bool(cond, &cond_value)) {
            const auto& branch =
                cond_value ? stmt.then_branch : stmt.else_branch;
            emit_init_block(branch, indent, cache);
            return;
          }
          out << pad << "if (" << cond_bool(cond) << ") {\n";
          ExprCache then_cache;
          then_cache.parent = cache;
          emit_init_block(stmt.then_branch, indent + 2, &then_cache);
          if (!stmt.else_branch.empty()) {
            out << pad << "} else {\n";
            ExprCache else_cache;
            else_cache.parent = cache;
            emit_init_block(stmt.else_branch, indent + 2, &else_cache);
            if (cache) {
              for (const auto& name : then_cache.blocked) {
                cache->blocked.insert(name);
              }
              for (const auto& name : else_cache.blocked) {
                cache->blocked.insert(name);
              }
            }
            out << pad << "}\n";
          } else {
            if (cache) {
              for (const auto& name : then_cache.blocked) {
                cache->blocked.insert(name);
              }
            }
            out << pad << "}\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          if (!stmt.case_expr) {
            return;
          }
          FsExpr case_expr =
              emit_expr4_cached(*stmt.case_expr,
                                ExprWidth(*stmt.case_expr, module), indent,
                                cache);
          if (stmt.case_items.empty()) {
            emit_init_block(stmt.default_branch, indent, cache);
            return;
          }
          bool first_case = true;
          std::unordered_set<std::string> case_blocked;
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
            ExprCache branch_cache;
            branch_cache.parent = cache;
            emit_init_block(item.body, indent + 2, &branch_cache);
            for (const auto& name : branch_cache.blocked) {
              case_blocked.insert(name);
            }
          }
          if (!stmt.default_branch.empty()) {
            out << pad << "} else {\n";
            ExprCache branch_cache;
            branch_cache.parent = cache;
            emit_init_block(stmt.default_branch, indent + 2, &branch_cache);
            for (const auto& name : branch_cache.blocked) {
              case_blocked.insert(name);
            }
            out << pad << "}\n";
          } else if (!first_case) {
            out << pad << "}\n";
          }
          if (cache) {
            for (const auto& name : case_blocked) {
              cache->blocked.insert(name);
            }
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          out << pad << "{\n";
          emit_init_block(stmt.block, indent + 2, cache);
          out << pad << "}\n";
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          out << pad << "// delay control ignored in MSL v0\n";
          emit_init_block(stmt.delay_body, indent, cache);
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          out << pad << "// event control ignored in MSL v0\n";
          emit_init_block(stmt.event_body, indent, cache);
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          out << pad << "// wait ignored in MSL v0\n";
          emit_init_block(stmt.wait_body, indent, cache);
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          out << pad << "// forever ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kFork) {
          out << pad << "// fork/join executed sequentially in MSL v0\n";
          std::unordered_set<std::string> fork_blocked;
          for (const auto& inner : stmt.fork_branches) {
            ExprCache branch_cache;
            branch_cache.parent = cache;
            emit_init_stmt(inner, indent, &branch_cache);
            for (const auto& name : branch_cache.blocked) {
              fork_blocked.insert(name);
            }
          }
          if (cache) {
            for (const auto& name : fork_blocked) {
              cache->blocked.insert(name);
            }
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
        if (stmt.kind == StatementKind::kForce ||
            stmt.kind == StatementKind::kRelease) {
          out << pad << "// force/release ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kTaskCall) {
          out << pad << "// task call ignored in MSL v0\n";
          return;
        }
      };
      emit_init_block = [&](const std::vector<Statement>& statements,
                            int indent, ExprCache* cache) {
        std::unordered_map<std::string, size_t> last_assign;
        std::vector<char> drop(statements.size(), 0);
        std::vector<char> has_syscall(statements.size(), 0);
        for (size_t i = 0; i < statements.size(); ++i) {
          std::unordered_set<std::string> reads;
          CollectReadSignals(statements[i], &reads);
          for (const auto& name : reads) {
            last_assign.erase(name);
          }
          const auto& stmt = statements[i];
          bool simple_assign = stmt.kind == StatementKind::kAssign &&
                               !stmt.assign.lhs_index &&
                               stmt.assign.lhs_indices.empty() &&
                               !stmt.assign.lhs_has_range;
          if (simple_assign) {
            const std::string& lhs = stmt.assign.lhs;
            if (stmt.assign.rhs && ExprHasSystemCall(*stmt.assign.rhs)) {
              has_syscall[i] = 1;
            }
            auto it = last_assign.find(lhs);
            if (it != last_assign.end()) {
              if (!has_syscall[it->second]) {
                drop[it->second] = 1;
              }
            }
            last_assign[lhs] = i;
          }
          if (stmt.kind == StatementKind::kTaskCall ||
              stmt.kind == StatementKind::kDisable ||
              stmt.kind == StatementKind::kEventTrigger) {
            last_assign.clear();
          }
        }
        for (size_t i = 0; i < statements.size(); ++i) {
          if (drop[i]) {
            continue;
          }
          emit_init_stmt(statements[i], indent, cache);
        }
      };

      for (const auto& block : module.always_blocks) {
        if (block.edge != EdgeKind::kInitial) {
          continue;
        }
        ExprCache block_cache;
        emit_init_block(block.statements, 2, &block_cache);
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
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(SignalWidth(module, reg));
        out << "  device " << type << "* " << val_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(reg) << " [[buffer("
            << buffer_index++ << ")]]";
      }
      for (const auto* net : array_nets) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(net->width);
        out << "  device " << type << "* " << val_name(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
      }
      for (const auto* net : array_nets) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(net->width);
        out << "  device " << type << "* " << val_name(net->name + "_next")
            << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << xz_name(net->name + "_next")
            << " [[buffer(" << buffer_index++ << ")]]";
      }
      if (!first) {
        out << ",\n";
      }
      first = false;
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
          if (export_wire_set.count(net.name) > 0) {
            tick_regs.insert(net.name);
          } else {
            tick_locals.insert(net.name);
          }
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

      std::function<void(const Statement&, int, ExprCache*)> emit_stmt;
      emit_stmt = [&](const Statement& stmt, int indent, ExprCache* cache) {
        std::string pad(indent, ' ');
        if (stmt.kind == StatementKind::kAssign) {
          if (!stmt.assign.rhs) {
            return;
          }
          Lvalue4 lhs =
              build_lvalue4(stmt.assign, tick_locals, tick_regs, false, indent);
          if (!lhs.ok) {
            return;
          }
          bool lhs_real = SignalIsReal(module, stmt.assign.lhs);
          FsExpr rhs = lhs_real
                           ? emit_real_expr4(*stmt.assign.rhs)
                           : emit_expr4_cached(*stmt.assign.rhs, lhs.width,
                                               indent, cache);
          if (lhs.is_array) {
            if (stmt.assign.nonblocking) {
              Lvalue4 next =
                  build_lvalue4(stmt.assign, tick_locals, tick_regs, true,
                                indent);
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
                build_lvalue4(stmt.assign, tick_locals, tick_regs, true,
                              indent);
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
          if (lhs.is_bit_select) {
            std::string target_val = lhs.val;
            std::string target_xz = lhs.xz;
            if (stmt.assign.nonblocking) {
              auto it = nb_map.find(stmt.assign.lhs);
              if (it != nb_map.end()) {
                target_val = it->second.val;
                target_xz = it->second.xz;
              }
            }
            emit_bit_select4(lhs, rhs, target_val, target_xz, indent);
            if (!stmt.assign.nonblocking && cache) {
              cache->blocked.insert(stmt.assign.lhs);
            }
            return;
          }
          if (lhs.is_range) {
            std::string target_val = lhs.val;
            std::string target_xz = lhs.xz;
            if (stmt.assign.nonblocking) {
              auto it = nb_map.find(stmt.assign.lhs);
              if (it != nb_map.end()) {
                target_val = it->second.val;
                target_xz = it->second.xz;
              }
            }
            emit_range_select4(lhs, rhs, target_val, target_xz, indent);
            if (!stmt.assign.nonblocking && cache) {
              cache->blocked.insert(stmt.assign.lhs);
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
          if (!stmt.assign.nonblocking && cache) {
            cache->blocked.insert(stmt.assign.lhs);
          }
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          FsExpr cond =
              stmt.condition
                  ? emit_expr4_cached(*stmt.condition,
                                      ExprWidth(*stmt.condition, module),
                                      indent, cache)
                             : FsExpr{literal_for_width(0, 1),
                                      literal_for_width(0, 1),
                                      drive_full(1), 1};
          bool cond_value = false;
          if (eval_const_bool(cond, &cond_value)) {
            const auto& branch =
                cond_value ? stmt.then_branch : stmt.else_branch;
            for (const auto& inner : branch) {
              emit_stmt(inner, indent, cache);
            }
            return;
          }
          out << pad << "if (" << cond_bool(cond) << ") {\n";
          ExprCache then_cache;
          then_cache.parent = cache;
          for (const auto& inner : stmt.then_branch) {
            emit_stmt(inner, indent + 2, &then_cache);
          }
          if (!stmt.else_branch.empty()) {
            out << pad << "} else {\n";
            ExprCache else_cache;
            else_cache.parent = cache;
            for (const auto& inner : stmt.else_branch) {
              emit_stmt(inner, indent + 2, &else_cache);
            }
            if (cache) {
              for (const auto& name : then_cache.blocked) {
                cache->blocked.insert(name);
              }
              for (const auto& name : else_cache.blocked) {
                cache->blocked.insert(name);
              }
            }
            out << pad << "}\n";
          } else {
            if (cache) {
              for (const auto& name : then_cache.blocked) {
                cache->blocked.insert(name);
              }
            }
            out << pad << "}\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          FsExpr case_expr =
              stmt.case_expr
                  ? emit_expr4_cached(*stmt.case_expr,
                                      ExprWidth(*stmt.case_expr, module),
                                      indent, cache)
                  : FsExpr{literal_for_width(0, 1),
                           literal_for_width(0, 1), drive_full(1), 1};
          bool first_case = true;
          std::unordered_set<std::string> case_blocked;
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
            ExprCache branch_cache;
            branch_cache.parent = cache;
            for (const auto& inner : item.body) {
              emit_stmt(inner, indent + 2, &branch_cache);
            }
            for (const auto& name : branch_cache.blocked) {
              case_blocked.insert(name);
            }
          }
          if (!stmt.default_branch.empty()) {
            out << pad << "} else {\n";
            ExprCache branch_cache;
            branch_cache.parent = cache;
            for (const auto& inner : stmt.default_branch) {
              emit_stmt(inner, indent + 2, &branch_cache);
            }
            for (const auto& name : branch_cache.blocked) {
              case_blocked.insert(name);
            }
            out << pad << "}\n";
          } else if (!first_case) {
            out << pad << "}\n";
          }
          if (cache) {
            for (const auto& name : case_blocked) {
              cache->blocked.insert(name);
            }
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          out << pad << "{\n";
          for (const auto& inner : stmt.block) {
            emit_stmt(inner, indent + 2, cache);
          }
          out << pad << "}\n";
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          out << pad << "// delay control ignored in MSL v0\n";
          for (const auto& inner : stmt.delay_body) {
            emit_stmt(inner, indent, cache);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          out << pad << "// event control ignored in MSL v0\n";
          for (const auto& inner : stmt.event_body) {
            emit_stmt(inner, indent, cache);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          out << pad << "// wait ignored in MSL v0\n";
          for (const auto& inner : stmt.wait_body) {
            emit_stmt(inner, indent, cache);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          out << pad << "// forever ignored in MSL v0\n";
          return;
        }
        if (stmt.kind == StatementKind::kFork) {
          out << pad << "// fork/join executed sequentially in MSL v0\n";
          std::unordered_set<std::string> fork_blocked;
          for (const auto& inner : stmt.fork_branches) {
            ExprCache branch_cache;
            branch_cache.parent = cache;
            emit_stmt(inner, indent, &branch_cache);
            for (const auto& name : branch_cache.blocked) {
              fork_blocked.insert(name);
            }
          }
          if (cache) {
            for (const auto& name : fork_blocked) {
              cache->blocked.insert(name);
            }
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
        if (stmt.kind == StatementKind::kForce ||
            stmt.kind == StatementKind::kRelease) {
          out << pad << "// force/release ignored in MSL v0\n";
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

        ExprCache block_cache;
        for (const auto& stmt : block.statements) {
          emit_stmt(stmt, 2, &block_cache);
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
    if (needs_scheduler) {
      struct ProcDef {
        int pid = 0;
        const std::vector<Statement>* body = nullptr;
        const Statement* single = nullptr;
      };

      std::vector<const AlwaysBlock*> initial_blocks;
      for (const auto& block : module.always_blocks) {
        if (block.edge == EdgeKind::kInitial) {
          initial_blocks.push_back(&block);
        }
      }

      if (!initial_blocks.empty()) {
        std::unordered_map<std::string, int> event_ids;
        for (size_t i = 0; i < module.events.size(); ++i) {
          event_ids[module.events[i].name] = static_cast<int>(i);
        }

        struct ForkInfo {
          int tag = 0;
          std::vector<int> children;
        };

        std::unordered_map<const Statement*, ForkInfo> fork_info;
        std::unordered_map<int, std::unordered_map<std::string, int>>
            fork_child_labels;
        std::vector<ProcDef> procs;
        std::vector<int> proc_parent;
        std::vector<int> proc_join_tag;

        int next_pid = 0;
        for (const auto* block : initial_blocks) {
          procs.push_back(ProcDef{next_pid, &block->statements, nullptr});
          proc_parent.push_back(-1);
          proc_join_tag.push_back(-1);
          ++next_pid;
        }
        const int root_proc_count = next_pid;
        int next_fork_tag = 0;

        std::function<void(const Statement&, int)> collect_forks;
        std::function<void(const std::vector<Statement>&, int)>
            collect_forks_in_list;
        collect_forks = [&](const Statement& stmt, int parent_pid) {
          if (stmt.kind == StatementKind::kFork) {
            ForkInfo info;
            info.tag = next_fork_tag++;
            for (const auto& branch : stmt.fork_branches) {
              int child_pid = next_pid++;
              info.children.push_back(child_pid);
              procs.push_back(ProcDef{child_pid, nullptr, &branch});
              proc_parent.push_back(parent_pid);
              proc_join_tag.push_back(info.tag);
              if (branch.kind == StatementKind::kBlock &&
                  !branch.block_label.empty()) {
                fork_child_labels[parent_pid][branch.block_label] = child_pid;
              }
              collect_forks(branch, child_pid);
            }
            fork_info[&stmt] = info;
            return;
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_forks(inner, parent_pid);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            for (const auto& inner : stmt.event_body) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_forks(inner, parent_pid);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_forks(inner, parent_pid);
            }
            return;
          }
        };
        collect_forks_in_list = [&](const std::vector<Statement>& stmts,
                                    int parent_pid) {
          for (const auto& stmt : stmts) {
            collect_forks(stmt, parent_pid);
          }
        };
        for (int i = 0; i < root_proc_count; ++i) {
          collect_forks_in_list(*procs[i].body, procs[i].pid);
        }

        std::unordered_map<const Statement*, int> wait_ids;
        std::vector<const Expr*> wait_exprs;
        std::function<void(const Statement&)> collect_waits;
        collect_waits = [&](const Statement& stmt) -> void {
          if (stmt.kind == StatementKind::kWait && stmt.wait_condition) {
            if (wait_ids.find(&stmt) == wait_ids.end()) {
              wait_ids[&stmt] = static_cast<int>(wait_exprs.size());
              wait_exprs.push_back(stmt.wait_condition.get());
            }
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_waits(inner);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            for (const auto& inner : stmt.event_body) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_waits(inner);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_waits(inner);
            }
            return;
          }
        };
        for (const auto& block : module.always_blocks) {
          for (const auto& stmt : block.statements) {
            collect_waits(stmt);
          }
        }

        struct EdgeWaitItem {
          const Expr* expr = nullptr;
          EventEdgeKind edge = EventEdgeKind::kAny;
        };
        struct EdgeWaitInfo {
          const Statement* stmt = nullptr;
          const Expr* expr = nullptr;
          std::vector<EdgeWaitItem> items;
          std::vector<std::string> star_signals;
          size_t star_offset = 0;
          size_t item_offset = 0;
        };
        std::unordered_map<const Statement*, int> edge_wait_ids;
        std::vector<EdgeWaitInfo> edge_waits;
        size_t edge_star_count = 0;
        size_t edge_item_count = 0;
        std::function<void(const Statement&)> collect_edge_waits;
        collect_edge_waits = [&](const Statement& stmt) -> void {
          if (stmt.kind == StatementKind::kEventControl) {
            bool named_event = false;
            const Expr* named_expr = nullptr;
            if (!stmt.event_items.empty()) {
              if (stmt.event_items.size() == 1 &&
                  stmt.event_items[0].edge == EventEdgeKind::kAny &&
                  stmt.event_items[0].expr) {
                named_expr = stmt.event_items[0].expr.get();
              }
            } else if (stmt.event_expr &&
                       stmt.event_edge == EventEdgeKind::kAny) {
              named_expr = stmt.event_expr.get();
            }
            if (named_expr && named_expr->kind == ExprKind::kIdentifier) {
              auto it = event_ids.find(named_expr->ident);
              if (it != event_ids.end()) {
                named_event = true;
              }
            }
            if (!named_event &&
                edge_wait_ids.find(&stmt) == edge_wait_ids.end()) {
              EdgeWaitInfo info;
              info.stmt = &stmt;
              if (!stmt.event_items.empty()) {
                for (const auto& item : stmt.event_items) {
                  if (!item.expr) {
                    continue;
                  }
                  info.items.push_back(
                      EdgeWaitItem{item.expr.get(), item.edge});
                }
              } else {
                info.expr = stmt.event_expr.get();
              }
              if (!info.items.empty()) {
                info.item_offset = edge_item_count;
                edge_item_count += info.items.size();
              } else if (info.expr) {
                info.item_offset = edge_item_count;
                edge_item_count += 1;
              } else {
                std::unordered_set<std::string> signals;
                for (const auto& inner : stmt.event_body) {
                  CollectReadSignals(inner, &signals);
                }
                info.star_signals.assign(signals.begin(), signals.end());
                std::sort(info.star_signals.begin(), info.star_signals.end());
                info.star_offset = edge_star_count;
                edge_star_count += info.star_signals.size();
              }
              edge_wait_ids[&stmt] = static_cast<int>(edge_waits.size());
              edge_waits.push_back(std::move(info));
            }
            for (const auto& inner : stmt.event_body) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_edge_waits(inner);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_edge_waits(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_edge_waits(inner);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_edge_waits(inner);
            }
            return;
          }
        };
        for (const auto& block : module.always_blocks) {
          for (const auto& stmt : block.statements) {
            collect_edge_waits(stmt);
          }
        }

        std::unordered_map<const Statement*, uint32_t> monitor_pid;
        std::unordered_map<const Statement*, uint32_t> strobe_pid;
        std::function<void(const Statement&, uint32_t)> collect_monitor_pids;
        collect_monitor_pids = [&](const Statement& stmt, uint32_t pid) {
          if (stmt.kind == StatementKind::kTaskCall &&
              stmt.task_name == "$monitor") {
            monitor_pid[&stmt] = pid;
          }
          if (stmt.kind == StatementKind::kTaskCall &&
              stmt.task_name == "$strobe") {
            strobe_pid[&stmt] = pid;
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_monitor_pids(inner, pid);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            for (const auto& inner : stmt.event_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_monitor_pids(inner, pid);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFork) {
            for (const auto& inner : stmt.fork_branches) {
              collect_monitor_pids(inner, pid);
            }
          }
        };
        for (const auto& proc : procs) {
          if (proc.body) {
            for (const auto& stmt : *proc.body) {
              collect_monitor_pids(stmt,
                                   static_cast<uint32_t>(proc.pid));
            }
          } else if (proc.single) {
            collect_monitor_pids(*proc.single,
                                 static_cast<uint32_t>(proc.pid));
          }
        }

        struct DelayAssignInfo {
          const Statement* stmt = nullptr;
          std::string lhs;
          bool nonblocking = false;
          bool lhs_real = false;
          bool is_array = false;
          bool is_bit_select = false;
          bool is_range = false;
          bool is_indexed_range = false;
          int width = 0;
          int base_width = 0;
          int range_lsb = 0;
          int array_size = 0;
          int element_width = 0;
        };

        std::unordered_map<const Statement*, uint32_t> delay_assign_ids;
        std::vector<DelayAssignInfo> delay_assigns;
        size_t delayed_nba_count = 0;
        std::function<void(const Statement&)> collect_delay_assigns;
        collect_delay_assigns = [&](const Statement& stmt) -> void {
          if (stmt.kind == StatementKind::kAssign && stmt.assign.delay) {
            DelayAssignInfo info;
            info.stmt = &stmt;
            info.lhs = stmt.assign.lhs;
            info.nonblocking = stmt.assign.nonblocking;
            info.lhs_real = SignalIsReal(module, stmt.assign.lhs);
            info.base_width = SignalWidth(module, stmt.assign.lhs);
            int element_width = 0;
            int array_size = 0;
            bool is_array = stmt.assign.lhs_index &&
                            IsArrayNet(module, stmt.assign.lhs, &element_width,
                                       &array_size);
            info.is_array = is_array;
            info.element_width = element_width;
            info.array_size = array_size;
            if (is_array) {
              info.width = element_width;
            } else if (stmt.assign.lhs_index) {
              info.is_bit_select = true;
              info.width = 1;
            } else if (stmt.assign.lhs_has_range) {
              info.is_range = true;
              info.base_width = SignalWidth(module, stmt.assign.lhs);
              if (stmt.assign.lhs_indexed_range) {
                info.is_indexed_range = true;
                info.width = stmt.assign.lhs_indexed_width;
              } else {
                int lo = std::min(stmt.assign.lhs_msb, stmt.assign.lhs_lsb);
                int hi = std::max(stmt.assign.lhs_msb, stmt.assign.lhs_lsb);
                info.range_lsb = lo;
                info.width = hi - lo + 1;
              }
            } else {
              info.width = SignalWidth(module, stmt.assign.lhs);
            }
            if (info.width <= 0) {
              info.width = info.base_width > 0 ? info.base_width : 1;
            }
            delay_assign_ids[&stmt] =
                static_cast<uint32_t>(delay_assigns.size());
            delay_assigns.push_back(info);
            if (info.nonblocking) {
              delayed_nba_count += 1;
            }
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_delay_assigns(inner);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            for (const auto& inner : stmt.event_body) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_delay_assigns(inner);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_delay_assigns(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFork) {
            for (const auto& inner : stmt.fork_branches) {
              collect_delay_assigns(inner);
            }
          }
        };
        for (const auto& block : module.always_blocks) {
          for (const auto& stmt : block.statements) {
            collect_delay_assigns(stmt);
          }
        }

        std::unordered_set<std::string> nb_targets;
        std::unordered_set<std::string> nb_array_targets;
        std::function<void(const Statement&)> collect_nb_targets;
        collect_nb_targets = [&](const Statement& stmt) -> void {
          if (stmt.kind == StatementKind::kAssign && stmt.assign.nonblocking) {
            if (stmt.assign.lhs_index) {
              nb_array_targets.insert(stmt.assign.lhs);
            } else {
              nb_targets.insert(stmt.assign.lhs);
            }
            return;
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_nb_targets(inner);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            for (const auto& inner : stmt.event_body) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_nb_targets(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_nb_targets(inner);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_nb_targets(inner);
            }
            return;
          }
        };
        for (const auto& block : module.always_blocks) {
          for (const auto& stmt : block.statements) {
            collect_nb_targets(stmt);
          }
        }

        std::vector<std::string> nb_targets_sorted(nb_targets.begin(),
                                                   nb_targets.end());
        std::sort(nb_targets_sorted.begin(), nb_targets_sorted.end());
        std::vector<const Net*> nb_array_nets;
        for (const auto& net : module.nets) {
          if (net.array_size <= 0) {
            continue;
          }
          if (nb_array_targets.count(net.name) > 0) {
            nb_array_nets.push_back(&net);
          }
        }
        std::sort(nb_array_nets.begin(), nb_array_nets.end(),
                  [](const Net* a, const Net* b) { return a->name < b->name; });

        const bool has_delayed_assigns = !delay_assigns.empty();
        const bool has_delayed_nba = delayed_nba_count > 0;
        size_t delayed_nba_capacity =
            has_delayed_nba ? std::max<size_t>(1, delayed_nba_count * 4) : 0;

        std::unordered_set<std::string> sched_locals;
        std::unordered_set<std::string> sched_regs;
        for (const auto& net : module.nets) {
          if (net.array_size > 0) {
            continue;
          }
          if (port_names.count(net.name) > 0) {
            continue;
          }
          if (net.type == NetType::kReg || IsTriregNet(net.type) ||
              scheduled_reads.count(net.name) > 0) {
            sched_regs.insert(net.name);
            continue;
          }
          if (!IsOutputPort(module, net.name)) {
            sched_locals.insert(net.name);
          }
        }

        std::unordered_set<std::string> sched_reg_set;
        for (const auto& net : module.nets) {
          if (net.array_size > 0 || port_names.count(net.name) > 0) {
            continue;
          }
          if (net.type == NetType::kReg || IsTriregNet(net.type) ||
              scheduled_reads.count(net.name) > 0) {
            sched_reg_set.insert(net.name);
          }
        }
        std::vector<std::string> sched_reg_names(sched_reg_set.begin(),
                                                 sched_reg_set.end());
        std::sort(sched_reg_names.begin(), sched_reg_names.end());

        out << "\n";
        out << "struct GpgaSchedParams { uint max_steps; uint max_proc_steps; uint service_capacity; };\n";
        out << "constexpr uint GPGA_SCHED_PROC_COUNT = " << procs.size()
            << "u;\n";
        out << "constexpr uint GPGA_SCHED_ROOT_COUNT = " << root_proc_count
            << "u;\n";
        out << "constexpr uint GPGA_SCHED_EVENT_COUNT = "
            << module.events.size() << "u;\n";
        out << "constexpr uint GPGA_SCHED_EDGE_COUNT = " << edge_item_count
            << "u;\n";
        out << "constexpr uint GPGA_SCHED_EDGE_STAR_COUNT = "
            << edge_star_count << "u;\n";
        out << "constexpr uint GPGA_SCHED_MAX_READY = " << procs.size()
            << "u;\n";
        out << "constexpr uint GPGA_SCHED_MAX_TIME = " << procs.size() << "u;\n";
        out << "constexpr uint GPGA_SCHED_MAX_NBA = "
            << nb_targets_sorted.size() << "u;\n";
        if (has_delayed_assigns) {
          out << "constexpr uint GPGA_SCHED_DELAY_COUNT = "
              << delay_assigns.size() << "u;\n";
        }
        if (has_delayed_nba) {
          out << "constexpr uint GPGA_SCHED_MAX_DNBA = "
              << delayed_nba_capacity << "u;\n";
        }
        out << "constexpr uint GPGA_SCHED_NO_PARENT = 0xFFFFFFFFu;\n";
        out << "constexpr uint GPGA_SCHED_WAIT_NONE = 0u;\n";
        out << "constexpr uint GPGA_SCHED_WAIT_TIME = 1u;\n";
        out << "constexpr uint GPGA_SCHED_WAIT_EVENT = 2u;\n";
        out << "constexpr uint GPGA_SCHED_WAIT_COND = 3u;\n";
        out << "constexpr uint GPGA_SCHED_WAIT_JOIN = 4u;\n";
        out << "constexpr uint GPGA_SCHED_WAIT_DELTA = 5u;\n";
        out << "constexpr uint GPGA_SCHED_WAIT_EDGE = 6u;\n";
        out << "constexpr uint GPGA_SCHED_EDGE_ANY = 0u;\n";
        out << "constexpr uint GPGA_SCHED_EDGE_POSEDGE = 1u;\n";
        out << "constexpr uint GPGA_SCHED_EDGE_NEGEDGE = 2u;\n";
        out << "constexpr uint GPGA_SCHED_EDGE_LIST = 3u;\n";
        out << "constexpr uint GPGA_SCHED_PROC_READY = 0u;\n";
        out << "constexpr uint GPGA_SCHED_PROC_BLOCKED = 1u;\n";
        out << "constexpr uint GPGA_SCHED_PROC_DONE = 2u;\n";
        out << "constexpr uint GPGA_SCHED_PHASE_ACTIVE = 0u;\n";
        out << "constexpr uint GPGA_SCHED_PHASE_NBA = 1u;\n";
        out << "constexpr uint GPGA_SCHED_STATUS_RUNNING = 0u;\n";
        out << "constexpr uint GPGA_SCHED_STATUS_IDLE = 1u;\n";
        out << "constexpr uint GPGA_SCHED_STATUS_FINISHED = 2u;\n";
        out << "constexpr uint GPGA_SCHED_STATUS_ERROR = 3u;\n";
        out << "constexpr uint GPGA_SCHED_STATUS_STOPPED = 4u;\n";
        if (!system_task_info.monitor_stmts.empty()) {
          size_t max_args =
              std::max<size_t>(1, system_task_info.monitor_max_args);
          out << "constexpr uint GPGA_SCHED_MONITOR_COUNT = "
              << system_task_info.monitor_stmts.size() << "u;\n";
          out << "constexpr uint GPGA_SCHED_MONITOR_MAX_ARGS = " << max_args
              << "u;\n";
        }
        if (!system_task_info.strobe_stmts.empty()) {
          out << "constexpr uint GPGA_SCHED_STROBE_COUNT = "
              << system_task_info.strobe_stmts.size() << "u;\n";
        }
        if (system_task_info.has_system_tasks) {
          size_t max_args = std::max<size_t>(1, system_task_info.max_args);
          out << "constexpr uint GPGA_SCHED_SERVICE_MAX_ARGS = " << max_args
              << "u;\n";
          out << "constexpr uint GPGA_SCHED_STRING_COUNT = "
              << system_task_info.string_table.size() << "u;\n";
          out << "constexpr uint GPGA_SERVICE_INVALID_ID = 0xFFFFFFFFu;\n";
          out << "constexpr uint GPGA_SERVICE_ARG_VALUE = 0u;\n";
          out << "constexpr uint GPGA_SERVICE_ARG_IDENT = 1u;\n";
          out << "constexpr uint GPGA_SERVICE_ARG_STRING = 2u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_DISPLAY = 0u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_MONITOR = 1u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_FINISH = 2u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_DUMPFILE = 3u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_DUMPVARS = 4u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_READMEMH = 5u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_READMEMB = 6u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_STOP = 7u;\n";
          out << "constexpr uint GPGA_SERVICE_KIND_STROBE = 8u;\n";
          out << "struct GpgaServiceRecord {\n";
          out << "  uint kind;\n";
          out << "  uint pid;\n";
          out << "  uint format_id;\n";
          out << "  uint arg_count;\n";
          out << "  uint arg_kind[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
          out << "  uint arg_width[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
          out << "  ulong arg_val[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
          out << "  ulong arg_xz[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
          out << "};\n";
        }
        out << "inline uint gpga_sched_index(uint gid, uint pid) {\n";
        out << "  return (gid * GPGA_SCHED_PROC_COUNT) + pid;\n";
        out << "}\n";
        out << "constant uint gpga_proc_parent[GPGA_SCHED_PROC_COUNT] = {";
        for (size_t i = 0; i < procs.size(); ++i) {
          uint32_t parent =
              proc_parent[i] < 0 ? 0xFFFFFFFFu
                                 : static_cast<uint32_t>(proc_parent[i]);
          if (i > 0) {
            out << ", ";
          }
          out << parent << "u";
        }
        out << "};\n";
        out << "constant uint gpga_proc_join_tag[GPGA_SCHED_PROC_COUNT] = {";
        for (size_t i = 0; i < procs.size(); ++i) {
          uint32_t tag = proc_join_tag[i] < 0
                             ? 0xFFFFFFFFu
                             : static_cast<uint32_t>(proc_join_tag[i]);
          if (i > 0) {
            out << ", ";
          }
          out << tag << "u";
        }
        out << "};\n";

        out << "\n";
        out << "kernel void gpga_" << module.name << "_sched_step(";
        int buffer_index = 0;
        bool first = true;
        auto emit_param = [&](const std::string& text) {
          if (!first) {
            out << ",\n";
          }
          first = false;
          out << text;
        };
        for (const auto& port : module.ports) {
          std::string qualifier =
              (port.dir == PortDir::kInput) ? "constant" : "device";
          std::string type = TypeForWidth(port.width);
          emit_param("  " + qualifier + " " + type + "* " +
                     val_name(port.name) + " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  " + qualifier + " " + type + "* " +
                     xz_name(port.name) + " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        for (const auto& reg : sched_reg_names) {
          std::string type = TypeForWidth(SignalWidth(module, reg));
          emit_param("  device " + type + "* " + val_name(reg) +
                     " [[buffer(" + std::to_string(buffer_index++) + ")]]");
          emit_param("  device " + type + "* " + xz_name(reg) +
                     " [[buffer(" + std::to_string(buffer_index++) + ")]]");
          if (IsTriregNet(SignalNetType(module, reg))) {
            emit_param("  device ulong* " + decay_name(reg) + " [[buffer(" +
                       std::to_string(buffer_index++) + ")]]");
          }
        }
        for (const auto* net : array_nets) {
          std::string type = TypeForWidth(net->width);
          emit_param("  device " + type + "* " + val_name(net->name) +
                     " [[buffer(" + std::to_string(buffer_index++) + ")]]");
          emit_param("  device " + type + "* " + xz_name(net->name) +
                     " [[buffer(" + std::to_string(buffer_index++) + ")]]");
        }
        for (const auto& target : nb_targets_sorted) {
          std::string type = TypeForWidth(SignalWidth(module, target));
          emit_param("  device " + type + "* nb_" + val_name(target) +
                     " [[buffer(" + std::to_string(buffer_index++) + ")]]");
          emit_param("  device " + type + "* nb_" + xz_name(target) +
                     " [[buffer(" + std::to_string(buffer_index++) + ")]]");
        }
        for (const auto* net : nb_array_nets) {
          std::string type = TypeForWidth(net->width);
          emit_param("  device " + type + "* " +
                     val_name(net->name + "_next") + " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device " + type + "* " +
                     xz_name(net->name + "_next") + " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        emit_param("  device uint* sched_pc [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_state [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_wait_kind [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_wait_edge_kind [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_wait_id [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_wait_event [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_edge_prev_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_edge_prev_xz [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_edge_star_prev_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_edge_star_prev_xz [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_wait_time [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_join_count [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_parent [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_join_tag [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_time [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_phase [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_active_init [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_initialized [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_event_pending [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_error [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_status [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        if (has_delayed_assigns) {
          emit_param("  device ulong* sched_delay_val [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device ulong* sched_delay_xz [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device uint* sched_delay_index_val [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device uint* sched_delay_index_xz [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        if (has_delayed_nba) {
          emit_param("  device uint* sched_dnba_count [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device ulong* sched_dnba_time [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device uint* sched_dnba_id [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device ulong* sched_dnba_val [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device ulong* sched_dnba_xz [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device uint* sched_dnba_index_val [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device uint* sched_dnba_index_xz [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
      if (!system_task_info.monitor_stmts.empty()) {
        emit_param("  device uint* sched_monitor_active [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_monitor_enable [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_monitor_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_monitor_xz [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (!system_task_info.strobe_stmts.empty()) {
        emit_param("  device uint* sched_strobe_pending [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
        if (!system_task_info.strobe_stmts.empty()) {
          emit_param("  device uint* sched_strobe_pending [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        if (system_task_info.has_system_tasks) {
          emit_param("  device uint* sched_service_count [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device GpgaServiceRecord* sched_service [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        emit_param("  constant GpgaSchedParams& sched [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  constant GpgaParams& params [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  uint gid [[thread_position_in_grid]]) {\n");
        out << "  if (gid >= params.count) {\n";
        out << "    return;\n";
        out << "  }\n";
        if (system_task_info.has_system_tasks) {
          out << "  sched_service_count[gid] = 0u;\n";
        }
        out << "  ulong __gpga_time = sched_time[gid];\n";
        out << "  if (sched_initialized[gid] == 0u) {\n";
        out << "    sched_time[gid] = 0ul;\n";
        out << "    __gpga_time = 0ul;\n";
        out << "    sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
        out << "    sched_active_init[gid] = 1u;\n";
        out << "    sched_error[gid] = 0u;\n";
        for (const auto* reg : trireg_nets) {
          out << "    " << decay_name(reg->name) << "[gid] = 0ul;\n";
        }
        if (has_delayed_nba) {
          out << "    sched_dnba_count[gid] = 0u;\n";
        }
        out << "    for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
        out << "      sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
        out << "    }\n";
        out << "    for (uint e = 0u; e < GPGA_SCHED_EDGE_COUNT; ++e) {\n";
        out << "      uint eidx = (gid * GPGA_SCHED_EDGE_COUNT) + e;\n";
        out << "      sched_edge_prev_val[eidx] = 0ul;\n";
        out << "      sched_edge_prev_xz[eidx] = 0ul;\n";
        out << "    }\n";
        out << "    for (uint s = 0u; s < GPGA_SCHED_EDGE_STAR_COUNT; ++s) {\n";
        out << "      uint sidx = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + s;\n";
        out << "      sched_edge_star_prev_val[sidx] = 0ul;\n";
        out << "      sched_edge_star_prev_xz[sidx] = 0ul;\n";
        out << "    }\n";
      if (!system_task_info.monitor_stmts.empty()) {
        out << "    sched_monitor_enable[gid] = 1u;\n";
        out << "    for (uint m = 0u; m < GPGA_SCHED_MONITOR_COUNT; ++m) {\n";
        out << "      sched_monitor_active[(gid * GPGA_SCHED_MONITOR_COUNT) + m] = 0u;\n";
        out << "      for (uint a = 0u; a < GPGA_SCHED_MONITOR_MAX_ARGS; ++a) {\n";
        out << "        uint offset = ((gid * GPGA_SCHED_MONITOR_COUNT) + m) * GPGA_SCHED_MONITOR_MAX_ARGS + a;\n";
        out << "        sched_monitor_val[offset] = 0ul;\n";
        out << "        sched_monitor_xz[offset] = 0ul;\n";
        out << "      }\n";
        out << "    }\n";
      }
      if (!system_task_info.strobe_stmts.empty()) {
        out << "    for (uint s = 0u; s < GPGA_SCHED_STROBE_COUNT; ++s) {\n";
        out << "      sched_strobe_pending[(gid * GPGA_SCHED_STROBE_COUNT) + s] = 0u;\n";
        out << "    }\n";
      }
        if (!system_task_info.strobe_stmts.empty()) {
          out << "    for (uint s = 0u; s < GPGA_SCHED_STROBE_COUNT; ++s) {\n";
          out << "      sched_strobe_pending[(gid * GPGA_SCHED_STROBE_COUNT) + s] = 0u;\n";
          out << "    }\n";
        }
        out << "    for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
        out << "      uint idx = gpga_sched_index(gid, pid);\n";
        out << "      sched_pc[idx] = 0u;\n";
        out << "      sched_state[idx] = (pid < GPGA_SCHED_ROOT_COUNT)\n";
        out << "          ? GPGA_SCHED_PROC_READY : GPGA_SCHED_PROC_BLOCKED;\n";
        out << "      sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "      sched_wait_edge_kind[idx] = GPGA_SCHED_EDGE_ANY;\n";
        out << "      sched_wait_id[idx] = 0u;\n";
        out << "      sched_wait_event[idx] = 0u;\n";
        out << "      sched_wait_time[idx] = 0ul;\n";
        out << "      sched_join_count[idx] = 0u;\n";
        out << "      sched_parent[idx] = gpga_proc_parent[pid];\n";
        out << "      sched_join_tag[idx] = gpga_proc_join_tag[pid];\n";
        out << "    }\n";
        out << "    sched_initialized[gid] = 1u;\n";
        out << "  }\n";
        out << "  if (sched_error[gid] != 0u) {\n";
        out << "    sched_status[gid] = GPGA_SCHED_STATUS_ERROR;\n";
        out << "    return;\n";
        out << "  }\n";

        auto delay_base_expr = [&](const std::string& name) -> Lvalue4 {
          Lvalue4 out_lhs;
          out_lhs.width = SignalWidth(module, name);
          out_lhs.base_width = out_lhs.width;
          if (IsOutputPort(module, name) || sched_regs.count(name) > 0) {
            out_lhs.val = val_name(name) + "[gid]";
            out_lhs.xz = xz_name(name) + "[gid]";
            out_lhs.ok = true;
            return out_lhs;
          }
          if (sched_locals.count(name) > 0) {
            out_lhs.val = val_name(name);
            out_lhs.xz = xz_name(name);
            out_lhs.ok = true;
            return out_lhs;
          }
          return out_lhs;
        };

        auto emit_delay_assign_apply =
            [&](const std::string& id_expr, const std::string& val_expr,
                const std::string& xz_expr, const std::string& idx_val_expr,
                const std::string& idx_xz_expr, bool use_nb,
                int indent) -> void {
          std::string pad(indent, ' ');
          out << pad << "switch (" << id_expr << ") {\n";
          for (size_t i = 0; i < delay_assigns.size(); ++i) {
            const DelayAssignInfo& info = delay_assigns[i];
            std::string pad2(indent + 2, ' ');
            out << pad2 << "case " << i << "u: {\n";
            if (info.lhs_real && (info.is_bit_select || info.is_range)) {
              out << pad2 << "  sched_error[gid] = 1u;\n";
              out << pad2 << "  break;\n";
              out << pad2 << "}\n";
              continue;
            }
            std::string target_val;
            std::string target_xz;
            if (info.is_array) {
              std::string name = info.lhs;
              if (use_nb) {
                name += "_next";
              }
              std::string base = "(gid * " + std::to_string(info.array_size) +
                                 "u) + uint(" + idx_val_expr + ")";
              std::string guard =
                  "(" + idx_xz_expr + " == 0u && " + idx_val_expr + " < " +
                  std::to_string(info.array_size) + "u)";
              target_val = val_name(name) + "[" + base + "]";
              target_xz = xz_name(name) + "[" + base + "]";
              out << pad2 << "  if " << guard << " {\n";
              out << pad2 << "    " << target_val << " = "
                  << MaskForWidthExpr(val_expr, info.width) << ";\n";
              out << pad2 << "    " << target_xz << " = "
                  << MaskForWidthExpr(xz_expr, info.width) << ";\n";
              out << pad2 << "  }\n";
              out << pad2 << "  break;\n";
              out << pad2 << "}\n";
              continue;
            }
            if (use_nb) {
              target_val = "nb_" + val_name(info.lhs) + "[gid]";
              target_xz = "nb_" + xz_name(info.lhs) + "[gid]";
            } else {
              Lvalue4 base = delay_base_expr(info.lhs);
              if (!base.ok) {
                out << pad2 << "  sched_error[gid] = 1u;\n";
                out << pad2 << "  break;\n";
                out << pad2 << "}\n";
                continue;
              }
              target_val = base.val;
              target_xz = base.xz;
            }
            if (info.is_bit_select) {
              Lvalue4 lhs;
              lhs.ok = true;
              lhs.val = target_val;
              lhs.xz = target_xz;
              lhs.width = info.width;
              lhs.base_width = info.base_width;
              lhs.bit_index_val = idx_val_expr;
              lhs.guard =
                  "(" + idx_xz_expr + " == 0u && " + idx_val_expr + " < " +
                  std::to_string(info.base_width) + "u)";
              FsExpr rhs;
              rhs.val = val_expr;
              rhs.xz = xz_expr;
              rhs.width = info.width;
              rhs.is_real = info.lhs_real;
              emit_bit_select4(lhs, rhs, target_val, target_xz, indent + 2);
              out << pad2 << "  break;\n";
              out << pad2 << "}\n";
              continue;
            }
            if (info.is_range) {
              Lvalue4 lhs;
              lhs.ok = true;
              lhs.val = target_val;
              lhs.xz = target_xz;
              lhs.width = info.width;
              lhs.base_width = info.base_width;
              lhs.is_range = true;
              lhs.is_indexed_range = info.is_indexed_range;
              lhs.range_lsb = info.range_lsb;
              lhs.range_index_val = idx_val_expr;
              if (info.is_indexed_range) {
                if (info.base_width >= info.width) {
                  int limit = info.base_width - info.width;
                  lhs.guard =
                      "(" + idx_xz_expr + " == 0u && " + idx_val_expr + " <= " +
                      std::to_string(limit) + "u)";
                } else {
                  lhs.guard = "false";
                }
              }
              FsExpr rhs;
              rhs.val = val_expr;
              rhs.xz = xz_expr;
              rhs.width = info.width;
              rhs.is_real = info.lhs_real;
              emit_range_select4(lhs, rhs, target_val, target_xz, indent + 2);
              out << pad2 << "  break;\n";
              out << pad2 << "}\n";
              continue;
            }
            out << pad2 << "  " << target_val << " = "
                << MaskForWidthExpr(val_expr, info.width) << ";\n";
            out << pad2 << "  " << target_xz << " = "
                << MaskForWidthExpr(xz_expr, info.width) << ";\n";
            out << pad2 << "  break;\n";
            out << pad2 << "}\n";
          }
          out << pad << "}\n";
        };

        auto emit_delay_value4 = [&](const Expr& expr) -> std::string {
          if (ExprIsRealValue(expr, module)) {
            std::string real = emit_real_value4(expr);
            return "ulong(" + real + ")";
          }
          FsExpr delay_expr = emit_expr4_sized(expr, 64);
          std::string zero = literal_for_width(0, delay_expr.width);
          return "(" + delay_expr.xz + " == " + zero + " ? " + delay_expr.val +
                 " : 0ul)";
        };

        out << "  sched_status[gid] = GPGA_SCHED_STATUS_RUNNING;\n";
        out << "  bool finished = false;\n";
        out << "  bool stopped = false;\n";
        out << "  uint steps = sched.max_steps;\n";
        out << "  while (steps > 0u) {\n";
        out << "    bool did_work = false;\n";
        out << "    if (sched_phase[gid] == GPGA_SCHED_PHASE_ACTIVE) {\n";
        out << "      if (sched_active_init[gid] != 0u) {\n";
        out << "        sched_active_init[gid] = 0u;\n";
        if (!nb_targets_sorted.empty()) {
          out << "        // Initialize NBA buffers for this delta.\n";
          for (const auto& target : nb_targets_sorted) {
            out << "        nb_" << val_name(target) << "[gid] = "
                << val_name(target) << "[gid];\n";
            out << "        nb_" << xz_name(target) << "[gid] = "
                << xz_name(target) << "[gid];\n";
          }
        }
        if (!nb_array_nets.empty()) {
          out << "        // Initialize array NBA buffers.\n";
          for (const auto* net : nb_array_nets) {
            out << "        for (uint i = 0u; i < " << net->array_size
                << "u; ++i) {\n";
            out << "          " << val_name(net->name + "_next") << "[(gid * "
                << net->array_size << "u) + i] = " << val_name(net->name)
                << "[(gid * " << net->array_size << "u) + i];\n";
            out << "          " << xz_name(net->name + "_next") << "[(gid * "
                << net->array_size << "u) + i] = " << xz_name(net->name)
                << "[(gid * " << net->array_size << "u) + i];\n";
            out << "        }\n";
          }
        }
        if (has_delayed_nba) {
          out << "        if (sched_dnba_count[gid] != 0u) {\n";
          out << "          uint __gpga_dnba_base = gid * GPGA_SCHED_MAX_DNBA;\n";
          out << "          uint __gpga_dnba_count = sched_dnba_count[gid];\n";
          out << "          uint __gpga_dnba_write = 0u;\n";
          out << "          for (uint __gpga_dnba_i = 0u; __gpga_dnba_i < __gpga_dnba_count; ++__gpga_dnba_i) {\n";
          out << "            uint __gpga_dnba_idx = __gpga_dnba_base + __gpga_dnba_i;\n";
          out << "            ulong __gpga_dnba_time = sched_dnba_time[__gpga_dnba_idx];\n";
          out << "            if (__gpga_dnba_time <= __gpga_time) {\n";
          out << "              uint __gpga_dnba_id = sched_dnba_id[__gpga_dnba_idx];\n";
          out << "              ulong __gpga_dval = sched_dnba_val[__gpga_dnba_idx];\n";
          out << "              ulong __gpga_dxz = sched_dnba_xz[__gpga_dnba_idx];\n";
          out << "              uint __gpga_didx_val = sched_dnba_index_val[__gpga_dnba_idx];\n";
          out << "              uint __gpga_didx_xz = sched_dnba_index_xz[__gpga_dnba_idx];\n";
          emit_delay_assign_apply("__gpga_dnba_id", "__gpga_dval", "__gpga_dxz",
                                  "__gpga_didx_val", "__gpga_didx_xz", true,
                                  14);
          out << "            } else {\n";
          out << "              uint __gpga_dnba_out = __gpga_dnba_base + __gpga_dnba_write;\n";
          out << "              if (__gpga_dnba_out != __gpga_dnba_idx) {\n";
          out << "                sched_dnba_time[__gpga_dnba_out] = __gpga_dnba_time;\n";
          out << "                sched_dnba_id[__gpga_dnba_out] = sched_dnba_id[__gpga_dnba_idx];\n";
          out << "                sched_dnba_val[__gpga_dnba_out] = sched_dnba_val[__gpga_dnba_idx];\n";
          out << "                sched_dnba_xz[__gpga_dnba_out] = sched_dnba_xz[__gpga_dnba_idx];\n";
          out << "                sched_dnba_index_val[__gpga_dnba_out] = sched_dnba_index_val[__gpga_dnba_idx];\n";
          out << "                sched_dnba_index_xz[__gpga_dnba_out] = sched_dnba_index_xz[__gpga_dnba_idx];\n";
          out << "              }\n";
          out << "              __gpga_dnba_write += 1u;\n";
          out << "            }\n";
          out << "          }\n";
          out << "          sched_dnba_count[gid] = __gpga_dnba_write;\n";
          out << "        }\n";
        }
        out << "      }\n";
        out << "      for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
        out << "        uint idx = gpga_sched_index(gid, pid);\n";
        out << "        while (steps > 0u && sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
        out << "          did_work = true;\n";
        out << "          steps--;\n";
        out << "          switch (pid) {\n";

        auto emit_inline_assign =
            [&](const SequentialAssign& assign, int indent,
                const std::unordered_set<std::string>& locals_override) -> void {
          if (!assign.rhs) {
            return;
          }
          std::string pad(indent, ' ');
          Lvalue4 lhs =
              build_lvalue4(assign, locals_override, sched_regs, false, indent);
          if (!lhs.ok) {
            return;
          }
          bool lhs_real = SignalIsReal(module, assign.lhs);
          FsExpr rhs = lhs_real
                           ? emit_real_expr4(*assign.rhs)
                           : emit_expr4_sized_with_cse(*assign.rhs, lhs.width,
                                                       indent);
          rhs = maybe_hoist_full(rhs, indent, false, true);
          if (assign.nonblocking) {
            if (assign.lhs_index) {
              Lvalue4 next =
                  build_lvalue4(assign, locals_override, sched_regs, true,
                                indent);
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
            if (lhs.is_bit_select) {
              if (lhs_real) {
                return;
              }
              std::string target_val = "nb_" + val_name(assign.lhs) + "[gid]";
              std::string target_xz = "nb_" + xz_name(assign.lhs) + "[gid]";
              emit_bit_select4(lhs, rhs, target_val, target_xz, indent);
              return;
            }
            if (lhs.is_range) {
              if (lhs_real) {
                return;
              }
              std::string target_val = "nb_" + val_name(assign.lhs) + "[gid]";
              std::string target_xz = "nb_" + xz_name(assign.lhs) + "[gid]";
              emit_range_select4(lhs, rhs, target_val, target_xz, indent);
              return;
            }
            out << pad << "nb_" << val_name(assign.lhs) << "[gid] = "
                << rhs.val << ";\n";
            out << pad << "nb_" << xz_name(assign.lhs) << "[gid] = "
                << rhs.xz << ";\n";
            return;
          }
          if (lhs.is_bit_select) {
            emit_bit_select4(lhs, rhs, lhs.val, lhs.xz, indent);
            return;
          }
          if (lhs.is_range) {
            emit_range_select4(lhs, rhs, lhs.val, lhs.xz, indent);
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
        };

        auto emit_lvalue_store =
            [&](const std::string& name, const FsExpr& rhs, int indent,
                const std::unordered_set<std::string>& locals_override) -> void {
          SequentialAssign temp;
          temp.lhs = name;
          temp.nonblocking = false;
          Lvalue4 lhs =
              build_lvalue4(temp, locals_override, sched_regs, false, indent);
          if (!lhs.ok) {
            return;
          }
          std::string pad(indent, ' ');
          if (!lhs.guard.empty()) {
            out << pad << "if " << lhs.guard << " {\n";
            out << pad << "  " << lhs.val << " = " << rhs.val << ";\n";
            out << pad << "  " << lhs.xz << " = " << rhs.xz << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << lhs.val << " = " << rhs.val << ";\n";
            out << pad << lhs.xz << " = " << rhs.xz << ";\n";
          }
        };

        struct ServiceArg {
          std::string kind;
          int width = 0;
          std::string val;
          std::string xz;
        };

        auto string_id_for = [&](const std::string& value,
                                 uint32_t* out_id) -> bool {
          auto it = system_task_info.string_ids.find(value);
          if (it == system_task_info.string_ids.end()) {
            return false;
          }
          if (out_id) {
            *out_id = it->second;
          }
          return true;
        };

        auto to_ulong = [&](const std::string& expr, int width) -> std::string {
          return (width > 32) ? expr : "(ulong)(" + expr + ")";
        };

        auto build_service_args =
            [&](const Statement& stmt, const std::string& name,
                std::string* format_id_expr,
                std::vector<ServiceArg>* args) -> bool {
          if (!format_id_expr || !args) {
            return false;
          }
          *format_id_expr = "GPGA_SERVICE_INVALID_ID";
          if (!stmt.task_args.empty() &&
              stmt.task_args[0]->kind == ExprKind::kString) {
            uint32_t id = 0;
            if (!string_id_for(stmt.task_args[0]->string_value, &id)) {
              return false;
            }
            *format_id_expr = std::to_string(id) + "u";
          }

          bool requires_string =
              name == "$dumpfile" || name == "$readmemh" || name == "$readmemb";
          if (requires_string &&
              *format_id_expr == "GPGA_SERVICE_INVALID_ID") {
            return false;
          }

          bool ident_as_string = TaskTreatsIdentifierAsString(name);
          args->clear();
          args->reserve(stmt.task_args.size());
          for (const auto& arg : stmt.task_args) {
            if (!arg) {
              continue;
            }
            if (arg->kind == ExprKind::kString) {
              uint32_t id = 0;
              if (!string_id_for(arg->string_value, &id)) {
                return false;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                          std::to_string(id) + "ul", "0ul"});
              continue;
            }
            if (ident_as_string && arg->kind == ExprKind::kIdentifier) {
              uint32_t id = 0;
              if (!string_id_for(arg->ident, &id)) {
                return false;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", 0,
                                          std::to_string(id) + "ul", "0ul"});
              continue;
            }
            if (arg->kind == ExprKind::kCall && arg->ident == "$time") {
              args->push_back(
                  ServiceArg{"GPGA_SERVICE_ARG_VALUE", 64, "__gpga_time",
                             "0ul"});
              continue;
            }
            int width = ExprWidth(*arg, module);
            if (width <= 0) {
              width = 1;
            }
            FsExpr value = emit_expr4_sized(*arg, width);
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_VALUE", width,
                                        to_ulong(value.val, width),
                                        to_ulong(value.xz, width)});
          }
          return true;
        };

        auto emit_service_record =
            [&](const char* kind_expr, const std::string& format_id_expr,
                const std::vector<ServiceArg>& args, int indent) -> void {
          std::string pad(indent, ' ');
          out << pad << "{\n";
          out << pad << "  uint __gpga_svc_index = sched_service_count[gid];\n";
          out << pad << "  if (__gpga_svc_index >= sched.service_capacity) {\n";
          out << pad << "    sched_error[gid] = 1u;\n";
          out << pad << "    sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          out << pad << "  } else {\n";
          out << pad
              << "    uint __gpga_svc_offset = (gid * sched.service_capacity) + "
                 "__gpga_svc_index;\n";
          out << pad
              << "    sched_service_count[gid] = __gpga_svc_index + 1u;\n";
          out << pad << "    sched_service[__gpga_svc_offset].kind = "
              << kind_expr << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].pid = pid;\n";
          out << pad << "    sched_service[__gpga_svc_offset].format_id = "
              << format_id_expr << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_count = "
              << args.size() << "u;\n";
          for (size_t i = 0; i < args.size(); ++i) {
            out << pad << "    sched_service[__gpga_svc_offset].arg_kind[" << i
                << "] = " << args[i].kind << ";\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_width[" << i
                << "] = " << args[i].width << "u;\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
                << "] = " << args[i].val << ";\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_xz[" << i
                << "] = " << args[i].xz << ";\n";
          }
          out << pad << "  }\n";
          out << pad << "}\n";
        };

        auto emit_monitor_record =
            [&](const std::string& pid_expr, const std::string& format_id_expr,
                const std::vector<ServiceArg>& args, int indent) -> void {
          std::string pad(indent, ' ');
          out << pad << "{\n";
          out << pad << "  uint __gpga_svc_index = sched_service_count[gid];\n";
          out << pad << "  if (__gpga_svc_index >= sched.service_capacity) {\n";
          out << pad << "    sched_error[gid] = 1u;\n";
          out << pad << "    steps = 0u;\n";
          out << pad << "  } else {\n";
          out << pad
              << "    uint __gpga_svc_offset = (gid * sched.service_capacity) + "
                 "__gpga_svc_index;\n";
          out << pad
              << "    sched_service_count[gid] = __gpga_svc_index + 1u;\n";
          out << pad << "    sched_service[__gpga_svc_offset].kind = "
              << "GPGA_SERVICE_KIND_MONITOR" << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].pid = "
              << pid_expr << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].format_id = "
              << format_id_expr << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_count = "
              << args.size() << "u;\n";
          for (size_t i = 0; i < args.size(); ++i) {
            out << pad << "    sched_service[__gpga_svc_offset].arg_kind[" << i
                << "] = " << args[i].kind << ";\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_width[" << i
                << "] = " << args[i].width << "u;\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
                << "] = " << args[i].val << ";\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_xz[" << i
                << "] = " << args[i].xz << ";\n";
          }
          out << pad << "  }\n";
          out << pad << "}\n";
        };

        auto emit_service_record_with_pid =
            [&](const char* kind_expr, const std::string& pid_expr,
                const std::string& format_id_expr,
                const std::vector<ServiceArg>& args, int indent) -> void {
          std::string pad(indent, ' ');
          out << pad << "{\n";
          out << pad << "  uint __gpga_svc_index = sched_service_count[gid];\n";
          out << pad << "  if (__gpga_svc_index >= sched.service_capacity) {\n";
          out << pad << "    sched_error[gid] = 1u;\n";
          out << pad << "    steps = 0u;\n";
          out << pad << "  } else {\n";
          out << pad
              << "    uint __gpga_svc_offset = (gid * sched.service_capacity) + "
                 "__gpga_svc_index;\n";
          out << pad
              << "    sched_service_count[gid] = __gpga_svc_index + 1u;\n";
          out << pad << "    sched_service[__gpga_svc_offset].kind = "
              << kind_expr << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].pid = "
              << pid_expr << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].format_id = "
              << format_id_expr << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_count = "
              << args.size() << "u;\n";
          for (size_t i = 0; i < args.size(); ++i) {
            out << pad << "    sched_service[__gpga_svc_offset].arg_kind[" << i
                << "] = " << args[i].kind << ";\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_width[" << i
                << "] = " << args[i].width << "u;\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
                << "] = " << args[i].val << ";\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_xz[" << i
                << "] = " << args[i].xz << ";\n";
          }
          out << pad << "  }\n";
          out << pad << "}\n";
        };

        auto emit_monitor_snapshot =
            [&](uint32_t monitor_id, const std::vector<ServiceArg>& args,
                int indent, bool force_emit) -> std::string {
          std::string pad(indent, ' ');
          std::string prefix =
              "__gpga_mon_" + std::to_string(monitor_id);
          std::string changed = prefix + "_changed";
          out << pad << "uint " << prefix << "_base = ((gid * "
              << "GPGA_SCHED_MONITOR_COUNT) + " << monitor_id
              << "u) * GPGA_SCHED_MONITOR_MAX_ARGS;\n";
          out << pad << "bool " << changed << " = "
              << (force_emit ? "true" : "false") << ";\n";
          for (size_t i = 0; i < args.size(); ++i) {
            if (args[i].kind != "GPGA_SERVICE_ARG_VALUE") {
              continue;
            }
            int width = args[i].width;
            if (width <= 0) {
              width = 1;
            }
            uint64_t mask = MaskForWidth64(width);
            std::string mask_literal = std::to_string(mask) + "ul";
            out << pad << "ulong " << prefix << "_val" << i << " = ("
                << args[i].val << ") & " << mask_literal << ";\n";
            out << pad << "ulong " << prefix << "_xz" << i << " = ("
                << args[i].xz << ") & " << mask_literal << ";\n";
            out << pad << "uint " << prefix << "_slot" << i << " = "
                << prefix << "_base + " << i << "u;\n";
            out << pad << "if ((((sched_monitor_val[" << prefix << "_slot" << i
                << "] ^ " << prefix << "_val" << i << ") | (sched_monitor_xz["
                << prefix << "_slot" << i << "] ^ " << prefix << "_xz" << i
                << ")) & " << mask_literal << ") != 0ul) {\n";
            out << pad << "  " << changed << " = true;\n";
            out << pad << "}\n";
            out << pad << "sched_monitor_val[" << prefix << "_slot" << i
                << "] = " << prefix << "_val" << i << ";\n";
            out << pad << "sched_monitor_xz[" << prefix << "_slot" << i
                << "] = " << prefix << "_xz" << i << ";\n";
          }
          return changed;
        };

      auto emit_system_task = [&](const Statement& stmt, int indent) -> void {
        if (!system_task_info.has_system_tasks) {
          out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
          out << std::string(indent, ' ')
              << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          return;
        }
          const std::string& name = stmt.task_name;
          if (name == "$monitoron") {
            if (!system_task_info.monitor_stmts.empty()) {
              out << std::string(indent, ' ') << "sched_monitor_enable[gid] = 1u;\n";
            }
            return;
          }
          if (name == "$monitoroff") {
            if (!system_task_info.monitor_stmts.empty()) {
              out << std::string(indent, ' ') << "sched_monitor_enable[gid] = 0u;\n";
            }
            return;
          }
          if (name == "$strobe") {
            auto it = system_task_info.strobe_ids.find(&stmt);
            if (it == system_task_info.strobe_ids.end()) {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            uint32_t strobe_id = it->second;
            std::string pad(indent, ' ');
            out << pad << "sched_strobe_pending[(gid * "
                << "GPGA_SCHED_STROBE_COUNT) + " << strobe_id << "u] += 1u;\n";
            return;
          }
          const char* kind_expr = nullptr;
          if (name == "$display") {
            kind_expr = "GPGA_SERVICE_KIND_DISPLAY";
          } else if (name == "$monitor") {
            kind_expr = "GPGA_SERVICE_KIND_MONITOR";
          } else if (name == "$finish") {
            kind_expr = "GPGA_SERVICE_KIND_FINISH";
          } else if (name == "$stop") {
            kind_expr = "GPGA_SERVICE_KIND_STOP";
          } else if (name == "$dumpfile") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPFILE";
          } else if (name == "$dumpvars") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPVARS";
          } else if (name == "$readmemh") {
            kind_expr = "GPGA_SERVICE_KIND_READMEMH";
          } else if (name == "$readmemb") {
            kind_expr = "GPGA_SERVICE_KIND_READMEMB";
          } else {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }

          std::string format_id_expr;
          std::vector<ServiceArg> args;
          if (!build_service_args(stmt, name, &format_id_expr, &args)) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }

          if (name == "$monitor") {
            auto it = system_task_info.monitor_ids.find(&stmt);
            if (it == system_task_info.monitor_ids.end()) {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            uint32_t monitor_id = it->second;
            std::string pad(indent, ' ');
            out << pad << "sched_monitor_active[(gid * "
                << "GPGA_SCHED_MONITOR_COUNT) + " << monitor_id << "u] = 1u;\n";
            std::string changed =
                emit_monitor_snapshot(monitor_id, args, indent, true);
            out << pad << "if (sched_monitor_enable[gid] != 0u && " << changed
                << ") {\n";
            emit_service_record(kind_expr, format_id_expr, args, indent + 2);
            out << pad << "}\n";
          } else {
            emit_service_record(kind_expr, format_id_expr, args, indent);
          }

          if (name == "$finish") {
            out << std::string(indent, ' ') << "finished = true;\n";
            out << std::string(indent, ' ') << "steps = 0u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          } else if (name == "$stop") {
            out << std::string(indent, ' ') << "stopped = true;\n";
            out << std::string(indent, ' ') << "steps = 0u;\n";
        }
      };

      std::function<bool(const Statement&)> inline_needs_scheduler;
      inline_needs_scheduler = [&](const Statement& stmt) -> bool {
        if (stmt.kind == StatementKind::kAssign && stmt.assign.delay) {
          return true;
        }
        if (stmt.kind == StatementKind::kTaskCall) {
          return !IsSystemTaskName(stmt.task_name);
        }
        if (stmt.kind == StatementKind::kEventTrigger) {
          return false;
        }
        switch (stmt.kind) {
          case StatementKind::kDelay:
          case StatementKind::kEventControl:
          case StatementKind::kWait:
          case StatementKind::kForever:
          case StatementKind::kFork:
          case StatementKind::kDisable:
            return true;
          default:
            break;
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            if (inline_needs_scheduler(inner)) {
              return true;
            }
          }
          for (const auto& inner : stmt.else_branch) {
            if (inline_needs_scheduler(inner)) {
              return true;
            }
          }
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            if (inline_needs_scheduler(inner)) {
              return true;
            }
          }
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              if (inline_needs_scheduler(inner)) {
                return true;
              }
            }
          }
          for (const auto& inner : stmt.default_branch) {
            if (inline_needs_scheduler(inner)) {
              return true;
            }
          }
        }
        if (stmt.kind == StatementKind::kFor) {
          for (const auto& inner : stmt.for_body) {
            if (inline_needs_scheduler(inner)) {
              return true;
            }
          }
        }
        if (stmt.kind == StatementKind::kWhile) {
          for (const auto& inner : stmt.while_body) {
            if (inline_needs_scheduler(inner)) {
              return true;
            }
          }
        }
        if (stmt.kind == StatementKind::kRepeat) {
          for (const auto& inner : stmt.repeat_body) {
            if (inline_needs_scheduler(inner)) {
              return true;
            }
          }
        }
        return false;
      };

      auto emit_inline_stmt =
          [&](const Statement& stmt, int indent,
              const std::unordered_set<std::string>& locals_override,
              const auto& self) -> void {
          std::string pad(indent, ' ');
          if (stmt.kind == StatementKind::kTaskCall &&
              IsSystemTaskName(stmt.task_name)) {
            emit_system_task(stmt, indent);
            return;
          }
          if (stmt.kind == StatementKind::kEventTrigger) {
            auto it = event_ids.find(stmt.trigger_target);
            if (it == event_ids.end()) {
              out << pad << "sched_error[gid] = 1u;\n";
              out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            out << pad << "sched_event_pending[(gid * "
                << "GPGA_SCHED_EVENT_COUNT) + " << it->second << "u] = 1u;\n";
            return;
          }
          if (stmt.kind == StatementKind::kAssign) {
            if (stmt.assign.delay) {
              out << pad << "sched_error[gid] = 1u;\n";
              out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            emit_inline_assign(stmt.assign, indent, locals_override);
            return;
          }
          if (inline_needs_scheduler(stmt)) {
            out << pad << "sched_error[gid] = 1u;\n";
            out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          if (stmt.kind == StatementKind::kIf) {
            FsExpr cond = stmt.condition
                               ? emit_expr4(*stmt.condition)
                               : FsExpr{literal_for_width(0, 1),
                                        literal_for_width(0, 1),
                                        drive_full(1), 1};
            out << pad << "if (" << cond_bool(cond) << ") {\n";
            for (const auto& inner : stmt.then_branch) {
              self(inner, indent + 2, locals_override, self);
            }
            if (!stmt.else_branch.empty()) {
              out << pad << "} else {\n";
              for (const auto& inner : stmt.else_branch) {
                self(inner, indent + 2, locals_override, self);
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
            bool first = true;
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
              if (first) {
                out << pad << "if (" << cond << ") {\n";
                first = false;
              } else {
                out << pad << "} else if (" << cond << ") {\n";
              }
              for (const auto& inner : item.body) {
                self(inner, indent + 2, locals_override, self);
              }
            }
            if (!stmt.default_branch.empty()) {
              out << pad << "} else {\n";
              for (const auto& inner : stmt.default_branch) {
                self(inner, indent + 2, locals_override, self);
              }
              out << pad << "}\n";
            } else if (!first) {
              out << pad << "}\n";
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            int width = SignalWidth(module, stmt.for_init_lhs);
            FsExpr init =
                stmt.for_init_rhs ? emit_expr4_sized(*stmt.for_init_rhs, width)
                                  : FsExpr{literal_for_width(0, width),
                                           literal_for_width(0, width),
                                           drive_full(width), width};
            emit_lvalue_store(stmt.for_init_lhs, init, indent, locals_override);
            FsExpr cond =
                stmt.for_condition ? emit_expr4(*stmt.for_condition)
                                   : FsExpr{literal_for_width(0, 1),
                                            literal_for_width(0, 1),
                                            drive_full(1), 1};
            out << pad << "while (" << cond_bool(cond) << ") {\n";
            for (const auto& inner : stmt.for_body) {
              self(inner, indent + 2, locals_override, self);
            }
            int step_width = SignalWidth(module, stmt.for_step_lhs);
            FsExpr step =
                stmt.for_step_rhs ? emit_expr4_sized(*stmt.for_step_rhs, step_width)
                                  : FsExpr{literal_for_width(0, step_width),
                                           literal_for_width(0, step_width),
                                           drive_full(step_width), step_width};
            emit_lvalue_store(stmt.for_step_lhs, step, indent + 2,
                              locals_override);
            out << pad << "}\n";
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            FsExpr cond =
                stmt.while_condition ? emit_expr4(*stmt.while_condition)
                                     : FsExpr{literal_for_width(0, 1),
                                              literal_for_width(0, 1),
                                              drive_full(1), 1};
            out << pad << "while (" << cond_bool(cond) << ") {\n";
            for (const auto& inner : stmt.while_body) {
              self(inner, indent + 2, locals_override, self);
            }
            out << pad << "}\n";
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            FsExpr count =
                stmt.repeat_count
                    ? emit_expr4_sized(*stmt.repeat_count, 32)
                    : FsExpr{literal_for_width(0, 32), literal_for_width(0, 32),
                             drive_full(32), 32};
            out << pad << "for (uint __gpga_rep = 0u; __gpga_rep < " << count.val
                << "; ++__gpga_rep) {\n";
            for (const auto& inner : stmt.repeat_body) {
              self(inner, indent + 2, locals_override, self);
            }
            out << pad << "}\n";
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            out << pad << "{\n";
            for (const auto& inner : stmt.block) {
              self(inner, indent + 2, locals_override, self);
            }
            out << pad << "}\n";
            return;
          }
        };

        auto emit_task_call =
            [&](const Statement& stmt, int indent,
                const auto& emit_inline_stmt_fn) -> void {
          if (IsSystemTaskName(stmt.task_name)) {
            emit_system_task(stmt, indent);
            return;
          }
          const Task* task = FindTask(module, stmt.task_name);
          if (!task) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          std::unordered_set<std::string> task_locals = sched_locals;
          std::unordered_map<std::string, int> task_widths;
          std::unordered_map<std::string, bool> task_signed;
          std::unordered_map<std::string, bool> task_real;
          struct TaskOutArg {
            std::string name;
            Lvalue4 target;
            int target_width = 0;
          };
          std::vector<TaskOutArg> task_outs;
          task_widths.reserve(task->args.size());
          task_signed.reserve(task->args.size());
          task_real.reserve(task->args.size());
          task_outs.reserve(task->args.size());

          for (const auto& arg : task->args) {
            task_widths[arg.name] = arg.width;
            task_signed[arg.name] = arg.is_signed;
            task_real[arg.name] = arg.is_real;
          }

          for (size_t i = 0; i < task->args.size(); ++i) {
            const auto& arg = task->args[i];
            const Expr* call_arg = nullptr;
            if (i < stmt.task_args.size()) {
              call_arg = stmt.task_args[i].get();
            }
            std::string type = TypeForWidth(arg.width);
            if (arg.dir == TaskArgDir::kInput) {
              FsExpr expr = call_arg ? emit_expr4_sized(*call_arg, arg.width)
                                     : FsExpr{literal_for_width(0, arg.width),
                                              literal_for_width(0, arg.width),
                                              drive_full(arg.width),
                                              arg.width};
              out << std::string(indent, ' ') << type << " "
                  << val_name(arg.name) << " = " << expr.val << ";\n";
              out << std::string(indent, ' ') << type << " "
                  << xz_name(arg.name) << " = " << expr.xz << ";\n";
              task_locals.insert(arg.name);
              continue;
            }
            if (!call_arg || call_arg->kind != ExprKind::kIdentifier) {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            FsExpr init = emit_expr4_sized(*call_arg, arg.width);
            out << std::string(indent, ' ') << type << " "
                << val_name(arg.name) << " = " << init.val << ";\n";
            out << std::string(indent, ' ') << type << " "
                << xz_name(arg.name) << " = " << init.xz << ";\n";
            task_locals.insert(arg.name);
            SequentialAssign target_assign;
            target_assign.lhs = call_arg->ident;
            target_assign.nonblocking = false;
            Lvalue4 target =
                build_lvalue4(target_assign, sched_locals, sched_regs, false,
                              indent);
            int target_width = ExprWidth(*call_arg, module);
            task_outs.push_back(TaskOutArg{arg.name, target, target_width});
          }

          const auto* prev_widths = g_task_arg_widths;
          const auto* prev_signed = g_task_arg_signed;
          const auto* prev_real = g_task_arg_real;
          g_task_arg_widths = &task_widths;
          g_task_arg_signed = &task_signed;
          g_task_arg_real = &task_real;
          for (const auto& inner : task->body) {
            emit_inline_stmt_fn(inner, indent, task_locals,
                                emit_inline_stmt_fn);
          }
          for (const auto& out_arg : task_outs) {
            if (!out_arg.target.ok) {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              continue;
            }
            Expr arg_expr;
            arg_expr.kind = ExprKind::kIdentifier;
            arg_expr.ident = out_arg.name;
            FsExpr value =
                emit_expr4_sized(arg_expr, out_arg.target_width);
            if (!out_arg.target.guard.empty()) {
              out << std::string(indent, ' ') << "if "
                  << out_arg.target.guard << " {\n";
              out << std::string(indent, ' ') << "  " << out_arg.target.val
                  << " = " << value.val << ";\n";
              out << std::string(indent, ' ') << "  " << out_arg.target.xz
                  << " = " << value.xz << ";\n";
              out << std::string(indent, ' ') << "}\n";
            } else {
              out << std::string(indent, ' ') << out_arg.target.val << " = "
                  << value.val << ";\n";
              out << std::string(indent, ' ') << out_arg.target.xz << " = "
                  << value.xz << ";\n";
            }
          }
          g_task_arg_widths = prev_widths;
          g_task_arg_signed = prev_signed;
          g_task_arg_real = prev_real;
        };

        for (const auto& proc : procs) {
          std::vector<const Statement*> stmts;
          if (proc.body) {
            for (const auto& stmt : *proc.body) {
              stmts.push_back(&stmt);
            }
          } else if (proc.single) {
            stmts.push_back(proc.single);
          }
          std::unordered_map<const Statement*, int> pc_for_stmt;
          int pc_counter = 0;
          for (const auto* stmt : stmts) {
            pc_for_stmt[stmt] = pc_counter++;
          }
          const int pc_done = pc_counter++;
          struct BodyCase {
            int pc = 0;
            const Statement* owner = nullptr;
            std::vector<const Statement*> body;
            int next_pc = 0;
            int loop_pc = -1;
            bool is_forever_body = false;
            bool is_assign_delay = false;
            int delay_id = -1;
          };
          std::vector<BodyCase> body_cases;

          std::unordered_map<std::string, int> block_end_pc;
          for (size_t i = 0; i < stmts.size(); ++i) {
            const auto* stmt = stmts[i];
            if (stmt->kind == StatementKind::kBlock &&
                !stmt->block_label.empty()) {
              int next_pc = (i + 1 < stmts.size()) ? pc_for_stmt[stmts[i + 1]]
                                                   : pc_done;
              block_end_pc[stmt->block_label] = next_pc;
            }
          }

          out << "            case " << proc.pid << ": {\n";
          out << "              uint pc = sched_pc[idx];\n";
          out << "              switch (pc) {\n";
          for (size_t i = 0; i < stmts.size(); ++i) {
            const Statement& stmt = *stmts[i];
            int pc = pc_for_stmt[&stmt];
            int next_pc =
                (i + 1 < stmts.size()) ? pc_for_stmt[stmts[i + 1]] : pc_done;
            out << "                case " << pc << ": {\n";
            if (stmt.kind == StatementKind::kAssign) {
              if (!stmt.assign.rhs) {
                out << "                  sched_pc[idx] = " << next_pc
                    << "u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              if (stmt.assign.delay) {
                auto it = delay_assign_ids.find(&stmt);
                if (it == delay_assign_ids.end()) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                uint32_t delay_id = it->second;
                const DelayAssignInfo& info = delay_assigns[delay_id];
                FsExpr rhs = info.lhs_real
                                 ? emit_real_expr4(*stmt.assign.rhs)
                                 : emit_expr4_sized_with_cse(
                                       *stmt.assign.rhs, info.width, 18);
                rhs = maybe_hoist_full(rhs, 18, false, true);
                std::string mask =
                    literal_for_width(MaskForWidth64(info.width), 64);
                out << "                  ulong __gpga_dval = ((ulong)("
                    << rhs.val << ")) & " << mask << ";\n";
                out << "                  ulong __gpga_dxz = ((ulong)("
                    << rhs.xz << ")) & " << mask << ";\n";
                std::string idx_val = "0u";
                std::string idx_xz = "0u";
                if (info.is_array || info.is_bit_select ||
                    info.is_indexed_range) {
                  const Expr* idx_expr = nullptr;
                  if (info.is_indexed_range) {
                    idx_expr = stmt.assign.lhs_lsb_expr.get();
                  } else {
                    idx_expr = stmt.assign.lhs_index.get();
                  }
                  if (!idx_expr) {
                    out << "                  sched_error[gid] = 1u;\n";
                    out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                    out << "                  break;\n";
                    out << "                }\n";
                    continue;
                  }
                  FsExpr idx = emit_expr4(*idx_expr);
                  idx = maybe_hoist_full(idx, 18, false, false);
                  idx_val = idx.val;
                  idx_xz = idx.xz;
                }
                out << "                  uint __gpga_didx_val = uint("
                    << idx_val << ");\n";
                out << "                  uint __gpga_didx_xz = uint("
                    << idx_xz << ");\n";
                out << "                  ulong __gpga_delay = "
                    << emit_delay_value4(*stmt.assign.delay) << ";\n";
                if (stmt.assign.nonblocking) {
                  out << "                  if (__gpga_delay == 0ul) {\n";
                  emit_delay_assign_apply(std::to_string(delay_id) + "u",
                                          "__gpga_dval", "__gpga_dxz",
                                          "__gpga_didx_val", "__gpga_didx_xz",
                                          true, 20);
                  out << "                  } else {\n";
                  out << "                    uint __gpga_dnba_count = sched_dnba_count[gid];\n";
                  out << "                    if (__gpga_dnba_count >= GPGA_SCHED_MAX_DNBA) {\n";
                  out << "                      sched_error[gid] = 1u;\n";
                  out << "                    } else {\n";
                  out << "                      uint __gpga_dnba_slot = (gid * GPGA_SCHED_MAX_DNBA) + __gpga_dnba_count;\n";
                  out << "                      sched_dnba_count[gid] = __gpga_dnba_count + 1u;\n";
                  out << "                      sched_dnba_time[__gpga_dnba_slot] = __gpga_time + __gpga_delay;\n";
                  out << "                      sched_dnba_id[__gpga_dnba_slot] = "
                      << delay_id << "u;\n";
                  out << "                      sched_dnba_val[__gpga_dnba_slot] = __gpga_dval;\n";
                  out << "                      sched_dnba_xz[__gpga_dnba_slot] = __gpga_dxz;\n";
                  out << "                      sched_dnba_index_val[__gpga_dnba_slot] = __gpga_didx_val;\n";
                  out << "                      sched_dnba_index_xz[__gpga_dnba_slot] = __gpga_didx_xz;\n";
                  out << "                    }\n";
                  out << "                  }\n";
                  out << "                  sched_pc[idx] = " << next_pc
                      << "u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                }
                int body_pc = pc_counter++;
                BodyCase body_case;
                body_case.pc = body_pc;
                body_case.owner = &stmt;
                body_case.next_pc = next_pc;
                body_case.is_assign_delay = true;
                body_case.delay_id = static_cast<int>(delay_id);
                body_cases.push_back(std::move(body_case));
                out << "                  uint __gpga_delay_slot = (gid * GPGA_SCHED_DELAY_COUNT) + "
                    << delay_id << "u;\n";
                out << "                  sched_delay_val[__gpga_delay_slot] = __gpga_dval;\n";
                out << "                  sched_delay_xz[__gpga_delay_slot] = __gpga_dxz;\n";
                out << "                  sched_delay_index_val[__gpga_delay_slot] = __gpga_didx_val;\n";
                out << "                  sched_delay_index_xz[__gpga_delay_slot] = __gpga_didx_xz;\n";
                out << "                  sched_wait_kind[idx] = "
                       "(__gpga_delay == 0ul) ? GPGA_SCHED_WAIT_DELTA : "
                       "GPGA_SCHED_WAIT_TIME;\n";
                out << "                  sched_wait_time[idx] = __gpga_time + "
                       "__gpga_delay;\n";
                out << "                  sched_pc[idx] = " << body_pc << "u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              emit_inline_stmt(stmt, 18, sched_locals, emit_inline_stmt);
              out << "                  sched_pc[idx] = " << next_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kDelay) {
              int body_pc = -1;
              if (!stmt.delay_body.empty()) {
                body_pc = pc_counter++;
                BodyCase body_case;
                body_case.pc = body_pc;
                body_case.owner = &stmt;
                body_case.next_pc = next_pc;
                for (const auto& inner : stmt.delay_body) {
                  body_case.body.push_back(&inner);
                }
                body_cases.push_back(std::move(body_case));
              }
              std::string delay_val =
                  stmt.delay ? emit_delay_value4(*stmt.delay) : "0ul";
              out << "                  ulong __gpga_delay = " << delay_val
                  << ";\n";
              out << "                  sched_wait_kind[idx] = "
                     "(__gpga_delay == 0ul) ? GPGA_SCHED_WAIT_DELTA : "
                     "GPGA_SCHED_WAIT_TIME;\n";
              out << "                  sched_wait_time[idx] = __gpga_time + "
                     "__gpga_delay;\n";
              out << "                  sched_pc[idx] = "
                  << (body_pc >= 0 ? std::to_string(body_pc) + "u"
                                   : std::to_string(next_pc) + "u")
                  << ";\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kEventControl) {
              int body_pc = -1;
              if (!stmt.event_body.empty()) {
                body_pc = pc_counter++;
                BodyCase body_case;
                body_case.pc = body_pc;
                body_case.owner = &stmt;
                body_case.next_pc = next_pc;
                for (const auto& inner : stmt.event_body) {
                  body_case.body.push_back(&inner);
                }
                body_cases.push_back(std::move(body_case));
              }
              int event_id = -1;
              bool named_event = false;
              const Expr* named_expr = nullptr;
              if (!stmt.event_items.empty()) {
                if (stmt.event_items.size() == 1 &&
                    stmt.event_items[0].edge == EventEdgeKind::kAny &&
                    stmt.event_items[0].expr) {
                  named_expr = stmt.event_items[0].expr.get();
                }
              } else if (stmt.event_expr &&
                         stmt.event_edge == EventEdgeKind::kAny) {
                named_expr = stmt.event_expr.get();
              }
              if (named_expr && named_expr->kind == ExprKind::kIdentifier) {
                auto it = event_ids.find(named_expr->ident);
                if (it != event_ids.end()) {
                  event_id = it->second;
                  named_event = true;
                }
              }
              if (named_event) {
                out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_EVENT;\n";
                out << "                  sched_wait_event[idx] = " << event_id
                    << "u;\n";
              } else {
                int edge_id = -1;
                auto it = edge_wait_ids.find(&stmt);
                if (it != edge_wait_ids.end()) {
                  edge_id = it->second;
                }
                if (edge_id < 0) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                const EdgeWaitInfo& info = edge_waits[edge_id];
                const char* edge_kind = "GPGA_SCHED_EDGE_ANY";
                if (!info.items.empty()) {
                  edge_kind = "GPGA_SCHED_EDGE_LIST";
                } else if (stmt.event_edge == EventEdgeKind::kPosedge) {
                  edge_kind = "GPGA_SCHED_EDGE_POSEDGE";
                } else if (stmt.event_edge == EventEdgeKind::kNegedge) {
                  edge_kind = "GPGA_SCHED_EDGE_NEGEDGE";
                }
                out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_EDGE;\n";
                out << "                  sched_wait_id[idx] = " << edge_id
                    << "u;\n";
                out << "                  sched_wait_edge_kind[idx] = " << edge_kind
                    << ";\n";
                if (!info.items.empty()) {
                  out << "                  uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_COUNT) + "
                      << info.item_offset << "u;\n";
                  for (size_t i = 0; i < info.items.size(); ++i) {
                    FsExpr edge_expr = emit_expr4(*info.items[i].expr);
                    std::string mask =
                        literal_for_width(MaskForWidth64(edge_expr.width), 64);
                    out << "                  ulong __gpga_edge_val = ((ulong)("
                        << edge_expr.val << ")) & " << mask << ";\n";
                    out << "                  ulong __gpga_edge_xz = ((ulong)("
                        << edge_expr.xz << ")) & " << mask << ";\n";
                    out << "                  sched_edge_prev_val[__gpga_edge_base + "
                        << i << "u] = __gpga_edge_val;\n";
                    out << "                  sched_edge_prev_xz[__gpga_edge_base + "
                        << i << "u] = __gpga_edge_xz;\n";
                  }
                } else if (info.expr) {
                  FsExpr edge_expr = emit_expr4(*info.expr);
                  std::string mask =
                      literal_for_width(MaskForWidth64(edge_expr.width), 64);
                  out << "                  uint __gpga_edge_idx = (gid * GPGA_SCHED_EDGE_COUNT) + "
                      << info.item_offset << "u;\n";
                  out << "                  ulong __gpga_edge_val = ((ulong)("
                      << edge_expr.val << ")) & " << mask << ";\n";
                  out << "                  ulong __gpga_edge_xz = ((ulong)("
                      << edge_expr.xz << ")) & " << mask << ";\n";
                  out << "                  sched_edge_prev_val[__gpga_edge_idx] = __gpga_edge_val;\n";
                  out << "                  sched_edge_prev_xz[__gpga_edge_idx] = __gpga_edge_xz;\n";
                } else {
                  out << "                  uint __gpga_edge_star_base = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + "
                      << info.star_offset << "u;\n";
                  for (size_t i = 0; i < info.star_signals.size(); ++i) {
                    Expr ident_expr;
                    ident_expr.kind = ExprKind::kIdentifier;
                    ident_expr.ident = info.star_signals[i];
                    FsExpr sig = emit_expr4(ident_expr);
                    std::string mask =
                        literal_for_width(MaskForWidth64(sig.width), 64);
                    out << "                  sched_edge_star_prev_val[__gpga_edge_star_base + "
                        << i << "u] = ((ulong)(" << sig.val << ")) & " << mask
                        << ";\n";
                    out << "                  sched_edge_star_prev_xz[__gpga_edge_star_base + "
                        << i << "u] = ((ulong)(" << sig.xz << ")) & " << mask
                        << ";\n";
                  }
                }
              }
              out << "                  sched_pc[idx] = "
                  << (body_pc >= 0 ? std::to_string(body_pc) + "u"
                                   : std::to_string(next_pc) + "u")
                  << ";\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kWait) {
              int body_pc = -1;
              if (!stmt.wait_body.empty()) {
                body_pc = pc_counter++;
                BodyCase body_case;
                body_case.pc = body_pc;
                body_case.owner = &stmt;
                body_case.next_pc = next_pc;
                for (const auto& inner : stmt.wait_body) {
                  body_case.body.push_back(&inner);
                }
                body_cases.push_back(std::move(body_case));
              }
              int wait_id = -1;
              auto it = wait_ids.find(&stmt);
              if (it != wait_ids.end()) {
                wait_id = it->second;
              }
              if (!stmt.wait_condition || wait_id < 0) {
                out << "                  sched_pc[idx] = " << next_pc << "u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              FsExpr cond = emit_expr4(*stmt.wait_condition);
              out << "                  if (" << cond_bool(cond) << ") {\n";
              if (body_pc >= 0) {
                out << "                    sched_pc[idx] = " << body_pc << "u;\n";
                out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              } else {
                out << "                    sched_pc[idx] = " << next_pc << "u;\n";
                out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              }
              out << "                    break;\n";
              out << "                  }\n";
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_COND;\n";
              out << "                  sched_wait_id[idx] = " << wait_id << "u;\n";
              out << "                  sched_pc[idx] = "
                  << (body_pc >= 0 ? std::to_string(body_pc) + "u"
                                   : std::to_string(next_pc) + "u")
                  << ";\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kForever) {
              if (stmt.forever_body.empty()) {
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              const Statement& body_stmt = stmt.forever_body.front();
              if (body_stmt.kind != StatementKind::kDelay) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              int body_pc = pc_counter++;
              BodyCase body_case;
              body_case.pc = body_pc;
              body_case.owner = &stmt;
              body_case.next_pc = pc;
              body_case.loop_pc = pc;
              body_case.is_forever_body = true;
              for (const auto& inner : body_stmt.delay_body) {
                body_case.body.push_back(&inner);
              }
              body_cases.push_back(std::move(body_case));
              std::string delay_val =
                  body_stmt.delay ? emit_delay_value4(*body_stmt.delay) : "0ul";
              out << "                  ulong __gpga_delay = " << delay_val
                  << ";\n";
              out << "                  sched_wait_kind[idx] = "
                     "(__gpga_delay == 0ul) ? GPGA_SCHED_WAIT_DELTA : "
                     "GPGA_SCHED_WAIT_TIME;\n";
              out << "                  sched_wait_time[idx] = __gpga_time + "
                     "__gpga_delay;\n";
              out << "                  sched_pc[idx] = " << body_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kFork) {
              auto it = fork_info.find(&stmt);
              if (it == fork_info.end()) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              const ForkInfo& info = it->second;
              for (int child : info.children) {
                out << "                  {\n";
                out << "                    uint cidx = gpga_sched_index(gid, "
                    << child << "u);\n";
                out << "                    sched_pc[cidx] = 0u;\n";
                out << "                    sched_state[cidx] = GPGA_SCHED_PROC_READY;\n";
                out << "                    sched_wait_kind[cidx] = GPGA_SCHED_WAIT_NONE;\n";
                out << "                    sched_wait_id[cidx] = 0u;\n";
                out << "                    sched_wait_event[cidx] = 0u;\n";
                out << "                    sched_wait_time[cidx] = 0ul;\n";
                out << "                    sched_join_count[cidx] = 0u;\n";
                out << "                  }\n";
              }
              out << "                  sched_join_count[idx] = "
                  << info.children.size() << "u;\n";
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_JOIN;\n";
              out << "                  sched_wait_id[idx] = " << info.tag << "u;\n";
              out << "                  sched_pc[idx] = " << next_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kDisable) {
              auto it = block_end_pc.find(stmt.disable_target);
              if (it == block_end_pc.end()) {
                int disable_pid = -1;
                auto fork_it = fork_child_labels.find(proc.pid);
                if (fork_it != fork_child_labels.end()) {
                  auto label_it = fork_it->second.find(stmt.disable_target);
                  if (label_it != fork_it->second.end()) {
                    disable_pid = label_it->second;
                  }
                }
                if (disable_pid < 0) {
                  int parent_pid = proc_parent[proc.pid];
                  if (parent_pid >= 0) {
                    auto parent_it = fork_child_labels.find(parent_pid);
                    if (parent_it != fork_child_labels.end()) {
                      auto label_it =
                          parent_it->second.find(stmt.disable_target);
                      if (label_it != parent_it->second.end()) {
                        disable_pid = label_it->second;
                      }
                    }
                  }
                }
                if (disable_pid < 0) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                out << "                  {\n";
                out << "                    uint __gpga_didx = gpga_sched_index(gid, "
                    << disable_pid << "u);\n";
                out << "                    if (sched_state[__gpga_didx] != GPGA_SCHED_PROC_DONE) {\n";
                out << "                      sched_state[__gpga_didx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                      uint parent = sched_parent[__gpga_didx];\n";
                out << "                      if (parent != GPGA_SCHED_NO_PARENT) {\n";
                out << "                        uint pidx = gpga_sched_index(gid, parent);\n";
                out << "                        if (sched_wait_kind[pidx] == GPGA_SCHED_WAIT_JOIN &&\n";
                out << "                            sched_wait_id[pidx] == sched_join_tag[__gpga_didx]) {\n";
                out << "                          if (sched_join_count[pidx] > 0u) {\n";
                out << "                            sched_join_count[pidx] -= 1u;\n";
                out << "                          }\n";
                out << "                          if (sched_join_count[pidx] == 0u) {\n";
                out << "                            sched_wait_kind[pidx] = GPGA_SCHED_WAIT_NONE;\n";
                out << "                            sched_state[pidx] = GPGA_SCHED_PROC_READY;\n";
                out << "                          }\n";
                out << "                        }\n";
                out << "                      }\n";
                out << "                    }\n";
                out << "                  }\n";
                out << "                  sched_pc[idx] = " << next_pc << "u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              out << "                  sched_pc[idx] = " << it->second << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kEventTrigger) {
              int event_id = -1;
              auto it = event_ids.find(stmt.trigger_target);
              if (it != event_ids.end()) {
                event_id = it->second;
              }
              if (event_id < 0) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              out << "                  sched_event_pending[(gid * "
                  << "GPGA_SCHED_EVENT_COUNT) + " << event_id << "u] = 1u;\n";
              out << "                  sched_pc[idx] = " << next_pc << "u;\n";
              if (next_pc == pc_done) {
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              } else {
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              }
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kTaskCall) {
              emit_task_call(stmt, 18, emit_inline_stmt);
              out << "                  if (sched_state[idx] != GPGA_SCHED_PROC_DONE) {\n";
              out << "                    sched_pc[idx] = " << next_pc << "u;\n";
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              out << "                  }\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            emit_inline_stmt(stmt, 18, sched_locals, emit_inline_stmt);
            out << "                  if (sched_state[idx] != GPGA_SCHED_PROC_DONE) {\n";
            out << "                    sched_pc[idx] = " << next_pc << "u;\n";
            out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            out << "                  }\n";
            out << "                  break;\n";
            out << "                }\n";
          }
          for (const auto& body_case : body_cases) {
            out << "                case " << body_case.pc << ": {\n";
            if (body_case.is_assign_delay) {
              out << "                  uint __gpga_delay_slot = (gid * "
                  << "GPGA_SCHED_DELAY_COUNT) + " << body_case.delay_id
                  << "u;\n";
              out << "                  ulong __gpga_dval = "
                  << "sched_delay_val[__gpga_delay_slot];\n";
              out << "                  ulong __gpga_dxz = "
                  << "sched_delay_xz[__gpga_delay_slot];\n";
              out << "                  uint __gpga_didx_val = "
                  << "sched_delay_index_val[__gpga_delay_slot];\n";
              out << "                  uint __gpga_didx_xz = "
                  << "sched_delay_index_xz[__gpga_delay_slot];\n";
              emit_delay_assign_apply(
                  std::to_string(body_case.delay_id) + "u", "__gpga_dval",
                  "__gpga_dxz", "__gpga_didx_val", "__gpga_didx_xz", false, 18);
              out << "                  sched_pc[idx] = " << body_case.next_pc
                  << "u;\n";
              if (body_case.next_pc == pc_done) {
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              } else {
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              }
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            for (const auto* inner : body_case.body) {
              emit_inline_stmt(*inner, 18, sched_locals, emit_inline_stmt);
            }
            out << "                  if (sched_state[idx] != GPGA_SCHED_PROC_DONE) {\n";
            out << "                    sched_pc[idx] = " << body_case.next_pc
                << "u;\n";
            if (body_case.next_pc == pc_done) {
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            } else {
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            }
            out << "                  }\n";
            out << "                  break;\n";
            out << "                }\n";
          }
          out << "                case " << pc_done << ": {\n";
          out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          out << "                  break;\n";
          out << "                }\n";
          out << "                default: {\n";
          out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          out << "                  break;\n";
          out << "                }\n";
          out << "              }\n";
          out << "              break;\n";
          out << "            }\n";
        }
        out << "            default: {\n";
        out << "              sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
        out << "              break;\n";
        out << "            }\n";
        out << "          }\n";
        out << "          if (sched_state[idx] == GPGA_SCHED_PROC_DONE) {\n";
        out << "            uint parent = sched_parent[idx];\n";
        out << "            if (parent != GPGA_SCHED_NO_PARENT) {\n";
        out << "              uint pidx = gpga_sched_index(gid, parent);\n";
        out << "              if (sched_wait_kind[pidx] == GPGA_SCHED_WAIT_JOIN &&\n";
        out << "                  sched_wait_id[pidx] == sched_join_tag[idx]) {\n";
        out << "                if (sched_join_count[pidx] > 0u) {\n";
        out << "                  sched_join_count[pidx] -= 1u;\n";
        out << "                }\n";
        out << "                if (sched_join_count[pidx] == 0u) {\n";
        out << "                  sched_wait_kind[pidx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "                  sched_state[pidx] = GPGA_SCHED_PROC_READY;\n";
        out << "                }\n";
        out << "              }\n";
        out << "            }\n";
        out << "          }\n";
        out << "        }\n";
        out << "      }\n";
        out << "      if (!did_work) {\n";
        out << "        bool any_ready = false;\n";
        out << "        for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
        out << "          uint idx = gpga_sched_index(gid, pid);\n";
        out << "          if (sched_state[idx] != GPGA_SCHED_PROC_BLOCKED) {\n";
        out << "            continue;\n";
        out << "          }\n";
        out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_DELTA) {\n";
        out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
        out << "            any_ready = true;\n";
        out << "            continue;\n";
        out << "          }\n";
        out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_EVENT) {\n";
        out << "            uint ev = sched_wait_event[idx];\n";
        out << "            uint eidx = (gid * GPGA_SCHED_EVENT_COUNT) + ev;\n";
        out << "            if (ev < GPGA_SCHED_EVENT_COUNT &&\n";
        out << "                sched_event_pending[eidx] != 0u) {\n";
        out << "              sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "              sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
        out << "              any_ready = true;\n";
        out << "            }\n";
        out << "            continue;\n";
        out << "          }\n";
        out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_EDGE) {\n";
        out << "            bool ready = false;\n";
        out << "            uint edge_kind = sched_wait_edge_kind[idx];\n";
        out << "            switch (sched_wait_id[idx]) {\n";
        for (size_t i = 0; i < edge_waits.size(); ++i) {
          const EdgeWaitInfo& info = edge_waits[i];
          out << "              case " << i << "u: {\n";
          if (!info.items.empty()) {
            out << "                uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_COUNT) + "
                << info.item_offset << "u;\n";
            out << "                bool __gpga_any = false;\n";
            for (size_t j = 0; j < info.items.size(); ++j) {
              FsExpr curr = emit_expr4(*info.items[j].expr);
              std::string mask =
                  literal_for_width(MaskForWidth64(curr.width), 64);
              out << "                ulong __gpga_prev_val = sched_edge_prev_val[__gpga_edge_base + "
                  << j << "u];\n";
              out << "                ulong __gpga_prev_xz = sched_edge_prev_xz[__gpga_edge_base + "
                  << j << "u];\n";
              out << "                ulong __gpga_curr_val = ((ulong)(" << curr.val
                  << ")) & " << mask << ";\n";
              out << "                ulong __gpga_curr_xz = ((ulong)(" << curr.xz
                  << ")) & " << mask << ";\n";
              if (info.items[j].edge == EventEdgeKind::kAny) {
                out << "                if (__gpga_curr_val != __gpga_prev_val || __gpga_curr_xz != __gpga_prev_xz) {\n";
                out << "                  __gpga_any = true;\n";
                out << "                }\n";
              } else {
                out << "                {\n";
                out << "                  ulong __gpga_prev_zero = (~__gpga_prev_val) & (~__gpga_prev_xz) & "
                    << mask << ";\n";
                out << "                  ulong __gpga_prev_one = __gpga_prev_val & (~__gpga_prev_xz) & "
                    << mask << ";\n";
                out << "                  ulong __gpga_prev_unk = __gpga_prev_xz & " << mask
                    << ";\n";
                out << "                  ulong __gpga_curr_zero = (~__gpga_curr_val) & (~__gpga_curr_xz) & "
                    << mask << ";\n";
                out << "                  ulong __gpga_curr_one = __gpga_curr_val & (~__gpga_curr_xz) & "
                    << mask << ";\n";
                out << "                  ulong __gpga_curr_unk = __gpga_curr_xz & " << mask
                    << ";\n";
                if (info.items[j].edge == EventEdgeKind::kPosedge) {
                  out << "                  ulong __gpga_edge_mask = (__gpga_prev_zero & (__gpga_curr_one | __gpga_curr_unk)) | (__gpga_prev_unk & __gpga_curr_one);\n";
                  out << "                  if (__gpga_edge_mask != 0ul) { __gpga_any = true; }\n";
                } else {
                  out << "                  ulong __gpga_edge_mask = (__gpga_prev_one & (__gpga_curr_zero | __gpga_curr_unk)) | (__gpga_prev_unk & __gpga_curr_zero);\n";
                  out << "                  if (__gpga_edge_mask != 0ul) { __gpga_any = true; }\n";
                }
                out << "                }\n";
              }
              out << "                sched_edge_prev_val[__gpga_edge_base + " << j
                  << "u] = __gpga_curr_val;\n";
              out << "                sched_edge_prev_xz[__gpga_edge_base + " << j
                  << "u] = __gpga_curr_xz;\n";
            }
            out << "                ready = __gpga_any;\n";
          } else if (info.expr) {
            FsExpr curr = emit_expr4(*info.expr);
            std::string mask =
                literal_for_width(MaskForWidth64(curr.width), 64);
            out << "                uint __gpga_edge_idx = (gid * GPGA_SCHED_EDGE_COUNT) + "
                << info.item_offset << "u;\n";
            out << "                ulong __gpga_prev_val = sched_edge_prev_val[__gpga_edge_idx];\n";
            out << "                ulong __gpga_prev_xz = sched_edge_prev_xz[__gpga_edge_idx];\n";
            out << "                ulong __gpga_curr_val = ((ulong)(" << curr.val
                << ")) & " << mask << ";\n";
            out << "                ulong __gpga_curr_xz = ((ulong)(" << curr.xz
                << ")) & " << mask << ";\n";
            out << "                if (edge_kind == GPGA_SCHED_EDGE_ANY) {\n";
            out << "                  ready = (__gpga_curr_val != __gpga_prev_val || __gpga_curr_xz != __gpga_prev_xz);\n";
            out << "                } else {\n";
            out << "                  ulong __gpga_prev_zero = (~__gpga_prev_val) & (~__gpga_prev_xz) & "
                << mask << ";\n";
            out << "                  ulong __gpga_prev_one = __gpga_prev_val & (~__gpga_prev_xz) & "
                << mask << ";\n";
            out << "                  ulong __gpga_prev_unk = __gpga_prev_xz & " << mask
                << ";\n";
            out << "                  ulong __gpga_curr_zero = (~__gpga_curr_val) & (~__gpga_curr_xz) & "
                << mask << ";\n";
            out << "                  ulong __gpga_curr_one = __gpga_curr_val & (~__gpga_curr_xz) & "
                << mask << ";\n";
            out << "                  ulong __gpga_curr_unk = __gpga_curr_xz & " << mask
                << ";\n";
            out << "                  if (edge_kind == GPGA_SCHED_EDGE_POSEDGE) {\n";
            out << "                    ulong __gpga_edge_mask = (__gpga_prev_zero & (__gpga_curr_one | __gpga_curr_unk)) | (__gpga_prev_unk & __gpga_curr_one);\n";
            out << "                    ready = (__gpga_edge_mask != 0ul);\n";
            out << "                  } else if (edge_kind == GPGA_SCHED_EDGE_NEGEDGE) {\n";
            out << "                    ulong __gpga_edge_mask = (__gpga_prev_one & (__gpga_curr_zero | __gpga_curr_unk)) | (__gpga_prev_unk & __gpga_curr_zero);\n";
            out << "                    ready = (__gpga_edge_mask != 0ul);\n";
            out << "                  }\n";
            out << "                }\n";
            out << "                sched_edge_prev_val[__gpga_edge_idx] = __gpga_curr_val;\n";
            out << "                sched_edge_prev_xz[__gpga_edge_idx] = __gpga_curr_xz;\n";
          } else {
            out << "                uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + "
                << info.star_offset << "u;\n";
            out << "                bool __gpga_changed = false;\n";
            for (size_t s = 0; s < info.star_signals.size(); ++s) {
              Expr ident_expr;
              ident_expr.kind = ExprKind::kIdentifier;
              ident_expr.ident = info.star_signals[s];
              FsExpr sig = emit_expr4(ident_expr);
              std::string mask =
                  literal_for_width(MaskForWidth64(sig.width), 64);
              out << "                {\n";
              out << "                  ulong __gpga_curr_val = ((ulong)(" << sig.val
                  << ")) & " << mask << ";\n";
              out << "                  ulong __gpga_curr_xz = ((ulong)(" << sig.xz
                  << ")) & " << mask << ";\n";
              out << "                  ulong __gpga_prev_val = sched_edge_star_prev_val[__gpga_edge_base + "
                  << s << "u];\n";
              out << "                  ulong __gpga_prev_xz = sched_edge_star_prev_xz[__gpga_edge_base + "
                  << s << "u];\n";
              out << "                  if (__gpga_curr_val != __gpga_prev_val || __gpga_curr_xz != __gpga_prev_xz) {\n";
              out << "                    __gpga_changed = true;\n";
              out << "                  }\n";
              out << "                  sched_edge_star_prev_val[__gpga_edge_base + "
                  << s << "u] = __gpga_curr_val;\n";
              out << "                  sched_edge_star_prev_xz[__gpga_edge_base + "
                  << s << "u] = __gpga_curr_xz;\n";
              out << "                }\n";
            }
            out << "                ready = __gpga_changed;\n";
          }
          out << "                break;\n";
          out << "              }\n";
        }
        out << "              default:\n";
        out << "                ready = false;\n";
        out << "                break;\n";
        out << "            }\n";
        out << "            if (ready) {\n";
        out << "              sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "              sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
        out << "              any_ready = true;\n";
        out << "            }\n";
        out << "            continue;\n";
        out << "          }\n";
        out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_COND) {\n";
        out << "            bool ready = false;\n";
        out << "            switch (sched_wait_id[idx]) {\n";
        for (size_t i = 0; i < wait_exprs.size(); ++i) {
          FsExpr cond = emit_expr4(*wait_exprs[i]);
          out << "              case " << i << "u:\n";
          out << "                ready = (" << cond_bool(cond) << ");\n";
          out << "                break;\n";
        }
        out << "              default:\n";
        out << "                ready = false;\n";
        out << "                break;\n";
        out << "            }\n";
        out << "            if (ready) {\n";
        out << "              sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "              sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
        out << "              any_ready = true;\n";
        out << "            }\n";
        out << "            continue;\n";
        out << "          }\n";
        out << "        }\n";
        out << "        for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
        out << "          sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
        out << "        }\n";
        out << "        if (any_ready) {\n";
        out << "          sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
        out << "          continue;\n";
        out << "        }\n";
        out << "        sched_phase[gid] = GPGA_SCHED_PHASE_NBA;\n";
        out << "      }\n";
        out << "      continue;\n";
        out << "    }\n";
        out << "    if (sched_phase[gid] == GPGA_SCHED_PHASE_NBA) {\n";
        if (!nb_targets_sorted.empty()) {
          out << "      // Commit scalar NBAs.\n";
          for (const auto& target : nb_targets_sorted) {
            out << "      " << val_name(target) << "[gid] = nb_"
                << val_name(target) << "[gid];\n";
            out << "      " << xz_name(target) << "[gid] = nb_"
                << xz_name(target) << "[gid];\n";
          }
        }
        if (!nb_array_nets.empty()) {
          out << "      // Commit array NBAs.\n";
          for (const auto* net : nb_array_nets) {
            out << "      for (uint i = 0u; i < " << net->array_size << "u; ++i) {\n";
            out << "        " << val_name(net->name) << "[(gid * "
                << net->array_size << "u) + i] = "
                << val_name(net->name + "_next") << "[(gid * "
                << net->array_size << "u) + i];\n";
            out << "        " << xz_name(net->name) << "[(gid * "
                << net->array_size << "u) + i] = "
                << xz_name(net->name + "_next") << "[(gid * "
                << net->array_size << "u) + i];\n";
            out << "      }\n";
          }
        }
        if (!system_task_info.monitor_stmts.empty()) {
          out << "      // Monitor change detection.\n";
          for (size_t i = 0; i < system_task_info.monitor_stmts.size(); ++i) {
            const Statement* monitor_stmt = system_task_info.monitor_stmts[i];
            std::string format_id_expr;
            std::vector<ServiceArg> args;
            build_service_args(*monitor_stmt, monitor_stmt->task_name,
                               &format_id_expr, &args);
            uint32_t monitor_pid_value = 0u;
            auto pid_it = monitor_pid.find(monitor_stmt);
            if (pid_it != monitor_pid.end()) {
              monitor_pid_value = pid_it->second;
            }
            std::string pid_expr = std::to_string(monitor_pid_value) + "u";
            out << "      if (sched_monitor_active[(gid * "
                << "GPGA_SCHED_MONITOR_COUNT) + " << i << "u] != 0u) {\n";
            std::string changed =
                emit_monitor_snapshot(static_cast<uint32_t>(i), args, 8, false);
            out << "        if (sched_monitor_enable[gid] != 0u && " << changed
                << ") {\n";
            emit_monitor_record(pid_expr, format_id_expr, args, 10);
            out << "        }\n";
            out << "      }\n";
          }
        }
        if (!system_task_info.strobe_stmts.empty()) {
          out << "      // Strobe emissions.\n";
          for (size_t i = 0; i < system_task_info.strobe_stmts.size(); ++i) {
            const Statement* strobe_stmt = system_task_info.strobe_stmts[i];
            std::string format_id_expr;
            std::vector<ServiceArg> args;
            build_service_args(*strobe_stmt, strobe_stmt->task_name,
                               &format_id_expr, &args);
            uint32_t strobe_pid_value = 0u;
            auto pid_it = strobe_pid.find(strobe_stmt);
            if (pid_it != strobe_pid.end()) {
              strobe_pid_value = pid_it->second;
            }
            std::string pid_expr = std::to_string(strobe_pid_value) + "u";
            out << "      uint __gpga_strobe_count = sched_strobe_pending[(gid * "
                << "GPGA_SCHED_STROBE_COUNT) + " << i << "u];\n";
            out << "      while (__gpga_strobe_count > 0u) {\n";
            emit_service_record_with_pid("GPGA_SERVICE_KIND_STROBE", pid_expr,
                                         format_id_expr, args, 8);
            out << "        __gpga_strobe_count -= 1u;\n";
            out << "      }\n";
            out << "      sched_strobe_pending[(gid * GPGA_SCHED_STROBE_COUNT) + "
                << i << "u] = 0u;\n";
          }
        }
        out << "      bool any_ready = false;\n";
        out << "      for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
        out << "        uint idx = gpga_sched_index(gid, pid);\n";
        out << "        if (sched_state[idx] != GPGA_SCHED_PROC_BLOCKED) {\n";
        out << "          continue;\n";
        out << "        }\n";
        out << "        if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_EVENT) {\n";
        out << "          uint ev = sched_wait_event[idx];\n";
        out << "          uint eidx = (gid * GPGA_SCHED_EVENT_COUNT) + ev;\n";
        out << "          if (ev < GPGA_SCHED_EVENT_COUNT &&\n";
        out << "              sched_event_pending[eidx] != 0u) {\n";
        out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
        out << "            any_ready = true;\n";
        out << "          }\n";
        out << "          continue;\n";
        out << "        }\n";
        out << "        if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_EDGE) {\n";
        out << "          bool ready = false;\n";
        out << "          uint edge_kind = sched_wait_edge_kind[idx];\n";
        out << "          switch (sched_wait_id[idx]) {\n";
        for (size_t i = 0; i < edge_waits.size(); ++i) {
          const EdgeWaitInfo& info = edge_waits[i];
          out << "            case " << i << "u: {\n";
          if (!info.items.empty()) {
            out << "              uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_COUNT) + "
                << info.item_offset << "u;\n";
            out << "              bool __gpga_any = false;\n";
            for (size_t j = 0; j < info.items.size(); ++j) {
              FsExpr curr = emit_expr4(*info.items[j].expr);
              std::string mask =
                  literal_for_width(MaskForWidth64(curr.width), 64);
              out << "              ulong __gpga_prev_val = sched_edge_prev_val[__gpga_edge_base + "
                  << j << "u];\n";
              out << "              ulong __gpga_prev_xz = sched_edge_prev_xz[__gpga_edge_base + "
                  << j << "u];\n";
              out << "              ulong __gpga_curr_val = ((ulong)(" << curr.val
                  << ")) & " << mask << ";\n";
              out << "              ulong __gpga_curr_xz = ((ulong)(" << curr.xz
                  << ")) & " << mask << ";\n";
              if (info.items[j].edge == EventEdgeKind::kAny) {
                out << "              if (__gpga_curr_val != __gpga_prev_val || __gpga_curr_xz != __gpga_prev_xz) {\n";
                out << "                __gpga_any = true;\n";
                out << "              }\n";
              } else {
                out << "              {\n";
                out << "                ulong __gpga_prev_zero = (~__gpga_prev_val) & (~__gpga_prev_xz) & "
                    << mask << ";\n";
                out << "                ulong __gpga_prev_one = __gpga_prev_val & (~__gpga_prev_xz) & "
                    << mask << ";\n";
                out << "                ulong __gpga_prev_unk = __gpga_prev_xz & " << mask
                    << ";\n";
                out << "                ulong __gpga_curr_zero = (~__gpga_curr_val) & (~__gpga_curr_xz) & "
                    << mask << ";\n";
                out << "                ulong __gpga_curr_one = __gpga_curr_val & (~__gpga_curr_xz) & "
                    << mask << ";\n";
                out << "                ulong __gpga_curr_unk = __gpga_curr_xz & " << mask
                    << ";\n";
                if (info.items[j].edge == EventEdgeKind::kPosedge) {
                  out << "                ulong __gpga_edge_mask = (__gpga_prev_zero & (__gpga_curr_one | __gpga_curr_unk)) | (__gpga_prev_unk & __gpga_curr_one);\n";
                  out << "                if (__gpga_edge_mask != 0ul) { __gpga_any = true; }\n";
                } else {
                  out << "                ulong __gpga_edge_mask = (__gpga_prev_one & (__gpga_curr_zero | __gpga_curr_unk)) | (__gpga_prev_unk & __gpga_curr_zero);\n";
                  out << "                if (__gpga_edge_mask != 0ul) { __gpga_any = true; }\n";
                }
                out << "              }\n";
              }
              out << "              sched_edge_prev_val[__gpga_edge_base + " << j
                  << "u] = __gpga_curr_val;\n";
              out << "              sched_edge_prev_xz[__gpga_edge_base + " << j
                  << "u] = __gpga_curr_xz;\n";
            }
            out << "              ready = __gpga_any;\n";
          } else if (info.expr) {
            FsExpr curr = emit_expr4(*info.expr);
            std::string mask =
                literal_for_width(MaskForWidth64(curr.width), 64);
            out << "              uint __gpga_edge_idx = (gid * GPGA_SCHED_EDGE_COUNT) + "
                << info.item_offset << "u;\n";
            out << "              ulong __gpga_prev_val = sched_edge_prev_val[__gpga_edge_idx];\n";
            out << "              ulong __gpga_prev_xz = sched_edge_prev_xz[__gpga_edge_idx];\n";
            out << "              ulong __gpga_curr_val = ((ulong)(" << curr.val
                << ")) & " << mask << ";\n";
            out << "              ulong __gpga_curr_xz = ((ulong)(" << curr.xz
                << ")) & " << mask << ";\n";
            out << "              if (edge_kind == GPGA_SCHED_EDGE_ANY) {\n";
            out << "                ready = (__gpga_curr_val != __gpga_prev_val || __gpga_curr_xz != __gpga_prev_xz);\n";
            out << "              } else {\n";
            out << "                ulong __gpga_prev_zero = (~__gpga_prev_val) & (~__gpga_prev_xz) & "
                << mask << ";\n";
            out << "                ulong __gpga_prev_one = __gpga_prev_val & (~__gpga_prev_xz) & "
                << mask << ";\n";
            out << "                ulong __gpga_prev_unk = __gpga_prev_xz & " << mask
                << ";\n";
            out << "                ulong __gpga_curr_zero = (~__gpga_curr_val) & (~__gpga_curr_xz) & "
                << mask << ";\n";
            out << "                ulong __gpga_curr_one = __gpga_curr_val & (~__gpga_curr_xz) & "
                << mask << ";\n";
            out << "                ulong __gpga_curr_unk = __gpga_curr_xz & " << mask
                << ";\n";
            out << "                if (edge_kind == GPGA_SCHED_EDGE_POSEDGE) {\n";
            out << "                  ulong __gpga_edge_mask = (__gpga_prev_zero & (__gpga_curr_one | __gpga_curr_unk)) | (__gpga_prev_unk & __gpga_curr_one);\n";
            out << "                  ready = (__gpga_edge_mask != 0ul);\n";
            out << "                } else if (edge_kind == GPGA_SCHED_EDGE_NEGEDGE) {\n";
            out << "                  ulong __gpga_edge_mask = (__gpga_prev_one & (__gpga_curr_zero | __gpga_curr_unk)) | (__gpga_prev_unk & __gpga_curr_zero);\n";
            out << "                  ready = (__gpga_edge_mask != 0ul);\n";
            out << "                }\n";
            out << "              }\n";
            out << "              sched_edge_prev_val[__gpga_edge_idx] = __gpga_curr_val;\n";
            out << "              sched_edge_prev_xz[__gpga_edge_idx] = __gpga_curr_xz;\n";
          } else {
            out << "              uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + "
                << info.star_offset << "u;\n";
            out << "              bool __gpga_changed = false;\n";
            for (size_t s = 0; s < info.star_signals.size(); ++s) {
              Expr ident_expr;
              ident_expr.kind = ExprKind::kIdentifier;
              ident_expr.ident = info.star_signals[s];
              FsExpr sig = emit_expr4(ident_expr);
              std::string mask =
                  literal_for_width(MaskForWidth64(sig.width), 64);
              out << "              {\n";
              out << "                ulong __gpga_curr_val = ((ulong)(" << sig.val
                  << ")) & " << mask << ";\n";
              out << "                ulong __gpga_curr_xz = ((ulong)(" << sig.xz
                  << ")) & " << mask << ";\n";
              out << "                ulong __gpga_prev_val = sched_edge_star_prev_val[__gpga_edge_base + "
                  << s << "u];\n";
              out << "                ulong __gpga_prev_xz = sched_edge_star_prev_xz[__gpga_edge_base + "
                  << s << "u];\n";
              out << "                if (__gpga_curr_val != __gpga_prev_val || __gpga_curr_xz != __gpga_prev_xz) {\n";
              out << "                  __gpga_changed = true;\n";
              out << "                }\n";
              out << "                sched_edge_star_prev_val[__gpga_edge_base + "
                  << s << "u] = __gpga_curr_val;\n";
              out << "                sched_edge_star_prev_xz[__gpga_edge_base + "
                  << s << "u] = __gpga_curr_xz;\n";
              out << "              }\n";
            }
            out << "              ready = __gpga_changed;\n";
          }
          out << "              break;\n";
          out << "            }\n";
        }
        out << "            default:\n";
        out << "              ready = false;\n";
        out << "              break;\n";
        out << "          }\n";
        out << "          if (ready) {\n";
        out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
        out << "            any_ready = true;\n";
        out << "          }\n";
        out << "          continue;\n";
        out << "        }\n";
        out << "        if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_COND) {\n";
        out << "          bool ready = false;\n";
        out << "          switch (sched_wait_id[idx]) {\n";
        for (size_t i = 0; i < wait_exprs.size(); ++i) {
          FsExpr cond = emit_expr4(*wait_exprs[i]);
          out << "            case " << i << "u:\n";
          out << "              ready = (" << cond_bool(cond) << ");\n";
          out << "              break;\n";
        }
        out << "            default:\n";
        out << "              ready = false;\n";
        out << "              break;\n";
        out << "          }\n";
        out << "          if (ready) {\n";
        out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
        out << "            any_ready = true;\n";
        out << "          }\n";
        out << "          continue;\n";
        out << "        }\n";
        out << "      }\n";
        out << "      for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
        out << "        sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
        out << "      }\n";
        out << "      if (any_ready) {\n";
        out << "        sched_active_init[gid] = 1u;\n";
        out << "        sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
        out << "        continue;\n";
        out << "      }\n";
        out << "      // Advance time to next wakeup.\n";
        out << "      bool have_time = false;\n";
        out << "      ulong next_time = ~0ul;\n";
        out << "      for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
        out << "        uint idx = gpga_sched_index(gid, pid);\n";
        out << "        if (sched_wait_kind[idx] != GPGA_SCHED_WAIT_TIME) {\n";
        out << "          continue;\n";
        out << "        }\n";
        out << "        ulong t = sched_wait_time[idx];\n";
        out << "        if (!have_time || t < next_time) {\n";
        out << "          have_time = true;\n";
        out << "          next_time = t;\n";
        out << "        }\n";
        out << "      }\n";
        out << "      if (have_time) {\n";
        out << "        sched_time[gid] = next_time;\n";
        out << "        __gpga_time = next_time;\n";
        out << "        for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
        out << "          uint idx = gpga_sched_index(gid, pid);\n";
        out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_TIME &&\n";
        out << "              sched_wait_time[idx] == next_time) {\n";
        out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
        out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
        out << "          }\n";
        out << "        }\n";
        out << "        sched_active_init[gid] = 1u;\n";
        out << "        sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
        out << "        continue;\n";
        out << "      }\n";
        out << "      finished = true;\n";
        out << "      break;\n";
        out << "    }\n";
        out << "  }\n";
        out << "  if (sched_error[gid] != 0u) {\n";
        out << "    sched_status[gid] = GPGA_SCHED_STATUS_ERROR;\n";
        out << "  } else if (finished) {\n";
        out << "    sched_status[gid] = GPGA_SCHED_STATUS_FINISHED;\n";
        out << "  } else if (stopped) {\n";
        out << "    sched_status[gid] = GPGA_SCHED_STATUS_STOPPED;\n";
        out << "  } else {\n";
        out << "    sched_status[gid] = GPGA_SCHED_STATUS_IDLE;\n";
        out << "  }\n";
        out << "}\n";
      }
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
  std::unordered_set<std::string> scheduled_reads;
  for (const auto& block : module.always_blocks) {
    if (block.edge == EdgeKind::kCombinational) {
      continue;
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

  auto literal_for_width = [](uint64_t value, int width) -> std::string {
    std::string suffix = (width > 32) ? "ul" : "u";
    return std::to_string(value) + suffix;
  };
  auto decay_name = [](const std::string& name) {
    return name + "_decay_time";
  };
  auto trireg_decay_delay = [&](const std::string& name) -> std::string {
    for (const auto& net : module.nets) {
      if (net.name != name) {
        continue;
      }
      switch (net.charge) {
        case ChargeStrength::kSmall:
          return "1ul";
        case ChargeStrength::kMedium:
          return "10ul";
        case ChargeStrength::kLarge:
          return "100ul";
        case ChargeStrength::kNone:
          return "10ul";
      }
    }
    return "10ul";
  };

  std::vector<std::string> reg_names;
  std::vector<std::string> export_wires;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      continue;
    }
    if (port_names.count(net.name) > 0) {
      continue;
    }
    if (net.type == NetType::kReg) {
      reg_names.push_back(net.name);
      continue;
    }
    if (scheduled_reads.count(net.name) > 0) {
      reg_names.push_back(net.name);
      export_wires.push_back(net.name);
    }
  }
  std::unordered_set<std::string> export_wire_set(export_wires.begin(),
                                                   export_wires.end());
  std::vector<const Net*> trireg_nets;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      continue;
    }
    if (net.type == NetType::kTrireg && !IsOutputPort(module, net.name)) {
      trireg_nets.push_back(&net);
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

  std::unordered_set<std::string> switch_nets;
  for (const auto& sw : module.switches) {
    switch_nets.insert(sw.a);
    switch_nets.insert(sw.b);
  }
  std::unordered_set<std::string> drive_declared;
  std::unordered_map<std::string, std::string> drive_vars;
  auto drive_var_name = [&](const std::string& name) -> std::string {
    return "__gpga_drive_" + name;
  };
  auto drive_init_for = [&](const std::string& name, int width) -> std::string {
    const Port* port = FindPort(module, name);
    if (port && (port->dir == PortDir::kInput ||
                 port->dir == PortDir::kInout)) {
      return MaskLiteralForWidth(width);
    }
    NetType net_type = SignalNetType(module, name);
    if (net_type == NetType::kReg || IsTriregNet(net_type)) {
      return MaskLiteralForWidth(width);
    }
    return ZeroForWidth(width);
  };
  auto ensure_drive_declared =
      [&](const std::string& name, int width,
          const std::string& init) -> std::string {
        std::string var = drive_var_name(name);
        drive_vars[name] = var;
        if (drive_declared.insert(name).second) {
          std::string type = TypeForWidth(width);
          out << "  " << type << " " << var << " = " << init << ";\n";
        }
        return var;
      };

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
  for (const auto* reg : trireg_nets) {
    if (!first) {
      out << ",\n";
    }
    first = false;
    std::string type = TypeForWidth(SignalWidth(module, reg->name));
    out << "  device " << type << "* " << reg->name << " [[buffer("
        << buffer_index++ << ")]]";
    out << ",\n";
    out << "  device ulong* " << decay_name(reg->name) << " [[buffer("
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
    if (net.type == NetType::kReg || IsTriregNet(net.type) ||
        export_wire_set.count(net.name) > 0) {
      if (port_names.count(net.name) == 0) {
        regs.insert(net.name);
      }
      continue;
    }
    if (port_names.count(net.name) == 0) {
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
  std::unordered_map<std::string, std::vector<size_t>> assign_groups;
  assign_groups.reserve(module.assigns.size());
  for (size_t i = 0; i < module.assigns.size(); ++i) {
    assign_groups[module.assigns[i].lhs].push_back(i);
  }

  std::unordered_set<std::string> multi_driver;
  std::unordered_map<std::string, size_t> drivers_remaining_template;
  struct DriverInfo {
    std::string val;
    std::string drive;
    std::string strength0;
    std::string strength1;
  };
  std::unordered_map<size_t, DriverInfo> driver_info;
  std::unordered_map<std::string, std::vector<size_t>> drivers_for_net;
  for (const auto& entry : assign_groups) {
    bool force_resolve = IsTriregNet(SignalNetType(module, entry.first));
    if (entry.second.size() <= 1 && !force_resolve) {
      continue;
    }
    multi_driver.insert(entry.first);
    drivers_remaining_template[entry.first] = entry.second.size();
    drivers_for_net[entry.first] = entry.second;
    for (size_t idx = 0; idx < entry.second.size(); ++idx) {
      size_t assign_index = entry.second[idx];
      const Assign& assign = module.assigns[assign_index];
      DriverInfo info;
      info.val =
          "__gpga_drv_" + entry.first + "_" + std::to_string(idx) + "_val";
      info.drive =
          "__gpga_drv_" + entry.first + "_" + std::to_string(idx) + "_drive";
      info.strength0 = StrengthLiteral(assign.strength0);
      info.strength1 = StrengthLiteral(assign.strength1);
      driver_info[assign_index] = std::move(info);
    }
  }
  for (const auto* net : trireg_nets) {
    if (assign_groups.count(net->name) > 0) {
      continue;
    }
    multi_driver.insert(net->name);
    drivers_remaining_template[net->name] = 0u;
    drivers_for_net[net->name] = {};
  }

  std::function<std::string(const Expr&, int)> emit_drive_expr;
  emit_drive_expr = [&](const Expr& expr, int width) -> std::string {
    uint64_t mask = MaskForWidth64(width);
    if (expr.kind == ExprKind::kNumber) {
      uint64_t drive_bits = mask & ~expr.z_bits;
      return literal_for_width(drive_bits, width);
    }
    if (expr.kind == ExprKind::kTernary && expr.condition &&
        expr.then_expr && expr.else_expr) {
      std::string cond =
          EmitCondExpr(*expr.condition, module, locals, regs);
      std::string then_drive = emit_drive_expr(*expr.then_expr, width);
      std::string else_drive = emit_drive_expr(*expr.else_expr, width);
      return "((" + cond + ") ? (" + then_drive + ") : (" + else_drive + "))";
    }
    return MaskLiteralForWidth(width);
  };

  auto emit_driver = [&](const Assign& assign, const DriverInfo& info) {
    if (!assign.rhs) {
      return;
    }
    bool lhs_real = SignalIsReal(module, assign.lhs);
    int lhs_width = SignalWidth(module, assign.lhs);
    std::string type = TypeForWidth(lhs_width);
    if (assign.lhs_has_range) {
      if (lhs_real) {
        out << "  // Unsupported real range driver: " << assign.lhs << "\n";
        return;
      }
      int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
      int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
      int slice_width = hi - lo + 1;
      std::string rhs =
          EmitExprSized(*assign.rhs, slice_width, module, locals, regs);
      std::string drive = emit_drive_expr(*assign.rhs, slice_width);
      uint64_t mask = MaskForWidth64(slice_width);
      std::string mask_literal = literal_for_width(mask, lhs_width);
      std::string cast = CastForWidth(lhs_width);
      out << "  " << type << " " << info.val << " = ((" << cast << rhs
          << " & " << mask_literal << ") << " << std::to_string(lo)
          << "u);\n";
      out << "  " << type << " " << info.drive << " = ((" << cast << drive
          << " & " << mask_literal << ") << " << std::to_string(lo)
          << "u);\n";
      return;
    }
    std::string rhs = lhs_real ? EmitRealBitsExpr(*assign.rhs, module, locals,
                                                  regs)
                               : EmitExprSized(*assign.rhs, lhs_width, module,
                                               locals, regs);
    std::string drive = lhs_real ? MaskLiteralForWidth(lhs_width)
                                 : emit_drive_expr(*assign.rhs, lhs_width);
    out << "  " << type << " " << info.val << " = " << rhs << ";\n";
    out << "  " << type << " " << info.drive << " = "
        << MaskForWidthExpr(drive, lhs_width) << ";\n";
  };

  auto emit_resolve = [&](const std::string& name,
                          const std::vector<size_t>& indices,
                          const std::unordered_set<std::string>& locals_ctx,
                          const std::unordered_set<std::string>& regs_ctx,
                          std::unordered_set<std::string>* declared_ctx) {
    NetType net_type = SignalNetType(module, name);
    bool wired_and = IsWiredAndNet(net_type);
    bool wired_or = IsWiredOrNet(net_type);
    bool is_trireg = IsTriregNet(net_type);
    int lhs_width = SignalWidth(module, name);
    std::string type = TypeForWidth(lhs_width);
    std::string one = (lhs_width > 32) ? "1ul" : "1u";
    std::string zero = ZeroForWidth(lhs_width);
    std::string resolved_val = "__gpga_res_" + name + "_val";
    std::string resolved_drive = "__gpga_res_" + name + "_drive";
    out << "  " << type << " " << resolved_val << " = " << zero << ";\n";
    out << "  " << type << " " << resolved_drive << " = " << zero << ";\n";
    out << "  for (uint bit = 0u; bit < " << lhs_width << "u; ++bit) {\n";
    out << "    " << type << " mask = (" << one << " << bit);\n";
    if (wired_and || wired_or) {
      out << "    bool has0 = false;\n";
      out << "    bool has1 = false;\n";
      for (size_t idx : indices) {
        const auto& info = driver_info[idx];
        out << "    if ((" << info.drive << " & mask) != " << zero << ") {\n";
        out << "      if ((" << info.val << " & mask) != " << zero << ") {\n";
        out << "        has1 = true;\n";
        out << "      } else {\n";
        out << "        has0 = true;\n";
        out << "      }\n";
        out << "    }\n";
      }
      out << "    if (!has0 && !has1) {\n";
      out << "      continue;\n";
      out << "    }\n";
      out << "    " << resolved_drive << " |= mask;\n";
      if (wired_and) {
        out << "    if (!has0) {\n";
        out << "      " << resolved_val << " |= mask;\n";
        out << "    }\n";
      } else {
        out << "    if (has1) {\n";
        out << "      " << resolved_val << " |= mask;\n";
        out << "    }\n";
      }
      out << "    continue;\n";
    } else {
      out << "    uint best0 = 0u;\n";
      out << "    uint best1 = 0u;\n";
      for (size_t idx : indices) {
        const auto& info = driver_info[idx];
        out << "    if ((" << info.drive << " & mask) != " << zero << ") {\n";
        out << "      if ((" << info.val << " & mask) != " << zero << ") {\n";
        out << "        best1 = (best1 > " << info.strength1 << ") ? best1 : "
            << info.strength1 << ";\n";
        out << "      } else {\n";
        out << "        best0 = (best0 > " << info.strength0 << ") ? best0 : "
            << info.strength0 << ";\n";
        out << "      }\n";
        out << "    }\n";
      }
      out << "    if (best0 == 0u && best1 == 0u) {\n";
      out << "      continue;\n";
      out << "    }\n";
      out << "    " << resolved_drive << " |= mask;\n";
      out << "    if (best1 > best0) {\n";
      out << "      " << resolved_val << " |= mask;\n";
      out << "    }\n";
    }
    out << "  }\n";

    if (switch_nets.count(name) > 0) {
      ensure_drive_declared(name, lhs_width, ZeroForWidth(lhs_width));
      out << "  " << drive_var_name(name) << " = " << resolved_drive << ";\n";
    }

    bool is_output = IsOutputPort(module, name) || regs_ctx.count(name) > 0;
    bool is_local = locals_ctx.count(name) > 0 && !is_output &&
                    regs_ctx.count(name) == 0;
    if (is_output) {
      if (is_trireg) {
        std::string decay_ref = decay_name(name) + "[gid]";
        std::string decay_delay = trireg_decay_delay(name);
        std::string drive_flag = "__gpga_trireg_drive_" + name;
        std::string decay_flag = "__gpga_trireg_decay_" + name;
        out << "  bool " << drive_flag << " = (" << resolved_drive
            << " != " << zero << ");\n";
        out << "  if (" << drive_flag << ") {\n";
        out << "    " << decay_ref << " = __gpga_time + " << decay_delay
            << ";\n";
        out << "  }\n";
        out << "  if (!" << drive_flag << " && " << decay_ref
            << " == 0ul) {\n";
        out << "    " << decay_ref << " = __gpga_time + " << decay_delay
            << ";\n";
        out << "  }\n";
        out << "  bool " << decay_flag << " = (!" << drive_flag << " && "
            << decay_ref << " != 0ul && __gpga_time >= " << decay_ref
            << ");\n";
        out << "  " << name << "[gid] = (" << name << "[gid] & ~"
            << resolved_drive << ") | (" << resolved_val << " & "
            << resolved_drive << ");\n";
        out << "  if (" << decay_flag << ") {\n";
        out << "    " << name << "[gid] = " << zero << ";\n";
        out << "  }\n";
      } else {
        out << "  " << name << "[gid] = " << resolved_val << ";\n";
      }
    } else if (is_local) {
      if (declared_ctx && declared_ctx->count(name) == 0) {
        out << "  " << type << " " << name << ";\n";
        if (declared_ctx) {
          declared_ctx->insert(name);
        }
      }
      out << "  " << name << " = " << resolved_val << ";\n";
    } else {
      out << "  // Unmapped resolved assign: " << name << "\n";
    }
  };

  auto emit_continuous_assigns =
      [&](const std::unordered_set<std::string>& locals_ctx,
          const std::unordered_set<std::string>& regs_ctx,
          std::unordered_set<std::string>* declared_ctx) {
        std::unordered_map<std::string, size_t> drivers_remaining =
            drivers_remaining_template;
        std::unordered_map<std::string, std::vector<const Assign*>>
            partial_assigns;
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
            emit_driver(assign, driver_info[index]);
            auto it = drivers_remaining.find(assign.lhs);
            if (it != drivers_remaining.end()) {
              if (it->second > 0) {
                it->second -= 1;
              }
              if (it->second == 0) {
                emit_resolve(assign.lhs, drivers_for_net[assign.lhs],
                             locals_ctx, regs_ctx, declared_ctx);
              }
            }
            continue;
          }
          if (assign.lhs_has_range) {
            continue;
          }
          std::string expr = EmitExpr(*assign.rhs, module, locals_ctx,
                                      regs_ctx);
          int lhs_width = SignalWidth(module, assign.lhs);
          bool lhs_real = SignalIsReal(module, assign.lhs);
          std::string sized = lhs_real
                                  ? EmitRealBitsExpr(*assign.rhs, module,
                                                     locals_ctx, regs_ctx)
                                  : EmitExprSized(*assign.rhs, lhs_width,
                                                  module, locals_ctx,
                                                  regs_ctx);
          if (IsOutputPort(module, assign.lhs)) {
            out << "  " << assign.lhs << "[gid] = " << sized << ";\n";
          } else if (regs_ctx.count(assign.lhs) > 0) {
            out << "  " << assign.lhs << "[gid] = " << sized << ";\n";
          } else if (locals_ctx.count(assign.lhs) > 0) {
            if (declared_ctx && declared_ctx->count(assign.lhs) == 0) {
              std::string type = TypeForWidth(SignalWidth(module, assign.lhs));
              out << "  " << type << " " << assign.lhs << " = " << sized
                  << ";\n";
              declared_ctx->insert(assign.lhs);
            } else {
              out << "  " << assign.lhs << " = " << sized << ";\n";
            }
          } else {
            out << "  // Unmapped assign: " << assign.lhs << " = " << expr
                << ";\n";
          }
          if (switch_nets.count(assign.lhs) > 0) {
            std::string drive = lhs_real
                                    ? MaskLiteralForWidth(lhs_width)
                                    : emit_drive_expr(*assign.rhs, lhs_width);
            std::string drive_var =
                ensure_drive_declared(assign.lhs, lhs_width,
                                      ZeroForWidth(lhs_width));
            out << "  " << drive_var << " = "
                << MaskForWidthExpr(drive, lhs_width) << ";\n";
          }
        }
        for (const auto& entry : drivers_remaining) {
          if (entry.second != 0) {
            continue;
          }
          const auto& indices = drivers_for_net[entry.first];
          if (!indices.empty()) {
            continue;
          }
          emit_resolve(entry.first, indices, locals_ctx, regs_ctx,
                       declared_ctx);
        }
        for (const auto& entry : partial_assigns) {
          const std::string& name = entry.first;
          if (SignalIsReal(module, name)) {
            out << "  // Unsupported real partial assign: " << name << "\n";
            continue;
          }
          int lhs_width = SignalWidth(module, name);
          std::string type = TypeForWidth(lhs_width);
          bool target_is_local =
              locals_ctx.count(name) > 0 && !IsOutputPort(module, name) &&
              regs_ctx.count(name) == 0;
          std::string temp = target_is_local ? name : ("__gpga_partial_" + name);
          bool track_drive = switch_nets.count(name) > 0;
          std::string temp_drive = "__gpga_partial_" + name + "_drive";
          std::string zero = ZeroForWidth(lhs_width);
          if (target_is_local) {
            if (declared_ctx && declared_ctx->count(name) == 0) {
              out << "  " << type << " " << temp << " = " << zero << ";\n";
              if (track_drive) {
                out << "  " << type << " " << temp_drive << " = " << zero
                    << ";\n";
              }
              declared_ctx->insert(name);
            } else {
              out << "  " << temp << " = " << zero << ";\n";
              if (track_drive) {
                out << "  " << temp_drive << " = " << zero << ";\n";
              }
            }
          } else {
            out << "  " << type << " " << temp << " = " << zero << ";\n";
            if (track_drive) {
              out << "  " << type << " " << temp_drive << " = " << zero
                  << ";\n";
            }
          }
          for (const auto* assign : entry.second) {
            int lo = std::min(assign->lhs_msb, assign->lhs_lsb);
            int hi = std::max(assign->lhs_msb, assign->lhs_lsb);
            int slice_width = hi - lo + 1;
            std::string rhs = EmitExprSized(*assign->rhs, slice_width, module,
                                            locals_ctx, regs_ctx);
            std::string drive = emit_drive_expr(*assign->rhs, slice_width);
            uint64_t mask = MaskForWidth64(slice_width);
            std::string mask_literal = literal_for_width(mask, lhs_width);
            std::string shifted_mask =
                "(" + mask_literal + " << " + std::to_string(lo) + "u)";
            std::string cast = CastForWidth(lhs_width);
            out << "  " << temp << " = (" << temp << " & ~" << shifted_mask
                << ") | ((" << cast << rhs << " & " << mask_literal << ") << "
                << std::to_string(lo) << "u);\n";
            if (track_drive) {
              out << "  " << temp_drive << " = (" << temp_drive << " & ~"
                  << shifted_mask << ") | ((" << cast << drive << " & "
                  << mask_literal << ") << " << std::to_string(lo) << "u);\n";
            }
          }
          if (!target_is_local) {
            if (IsOutputPort(module, name) || regs_ctx.count(name) > 0) {
              out << "  " << name << "[gid] = " << temp << ";\n";
            } else if (locals_ctx.count(name) > 0) {
              if (declared_ctx && declared_ctx->count(name) == 0) {
                out << "  " << type << " " << name << " = " << temp << ";\n";
                declared_ctx->insert(name);
              } else {
                out << "  " << name << " = " << temp << ";\n";
              }
            } else {
              out << "  // Unmapped assign: " << name << " = " << temp << ";\n";
            }
          }
          if (track_drive) {
            std::string drive_var =
                ensure_drive_declared(name, lhs_width, ZeroForWidth(lhs_width));
            out << "  " << drive_var << " = " << temp_drive << ";\n";
          }
        }
      };

  emit_continuous_assigns(locals, regs, &declared);

  for (const auto& name : switch_nets) {
    if (drive_declared.count(name) > 0) {
      continue;
    }
    int width = SignalWidth(module, name);
    ensure_drive_declared(name, width, drive_init_for(name, width));
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
      bool lhs_real = SignalIsReal(module, stmt.assign.lhs);
      std::string sized = lhs_real
                              ? EmitRealBitsExpr(*stmt.assign.rhs, module,
                                                 locals, regs)
                              : EmitExprSized(*stmt.assign.rhs, lvalue.width,
                                              module, locals, regs);
      if (lvalue.is_bit_select) {
        if (lhs_real) {
          out << pad << "// Unsupported real bit-select assign: "
              << stmt.assign.lhs << "\n";
          return;
        }
        std::string update = EmitBitSelectUpdate(
            lvalue.expr, lvalue.bit_index, lvalue.base_width, sized);
        if (!lvalue.guard.empty()) {
          out << pad << "if " << lvalue.guard << " {\n";
          out << pad << "  " << lvalue.expr << " = " << update << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lvalue.expr << " = " << update << ";\n";
        }
        return;
      }
      if (lvalue.is_range) {
        if (lhs_real) {
          out << pad << "// Unsupported real range assign: " << stmt.assign.lhs
              << "\n";
          return;
        }
        std::string update = EmitRangeSelectUpdate(
            lvalue.expr,
            lvalue.is_indexed_range ? lvalue.range_index
                                    : std::to_string(lvalue.range_lsb),
            lvalue.base_width, lvalue.width, sized);
        if (!lvalue.guard.empty()) {
          out << pad << "if " << lvalue.guard << " {\n";
          out << pad << "  " << lvalue.expr << " = " << update << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lvalue.expr << " = " << update << ";\n";
        }
        return;
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
                             ? EmitCondExpr(*stmt.condition, module, locals,
                                            regs)
                             : "false";
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
  int switch_temp_index = 0;
  auto signal_lvalue2 = [&](const std::string& name, std::string* expr,
                            int* width) -> bool {
    if (!expr || !width) {
      return false;
    }
    *width = SignalWidth(module, name);
    if (*width <= 0) {
      return false;
    }
    if (IsOutputPort(module, name) || regs.count(name) > 0) {
      *expr = name + "[gid]";
      return true;
    }
    if (locals.count(name) > 0) {
      *expr = name;
      return true;
    }
    return false;
  };
  for (const auto& sw : module.switches) {
    std::string a_val;
    std::string b_val;
    int a_width = 0;
    int b_width = 0;
    if (!signal_lvalue2(sw.a, &a_val, &a_width) ||
        !signal_lvalue2(sw.b, &b_val, &b_width)) {
      continue;
    }
    int width = std::min(a_width, b_width);
    std::string cond_false;
    if (sw.kind == SwitchKind::kTran) {
      cond_false = "false";
    } else if (sw.kind == SwitchKind::kTranif1 ||
               sw.kind == SwitchKind::kTranif0) {
      std::string cond =
          sw.control ? EmitCondExpr(*sw.control, module, locals, regs)
                     : "false";
      cond_false = (sw.kind == SwitchKind::kTranif1) ? ("!(" + cond + ")")
                                                     : cond;
    } else {
      std::string cond =
          sw.control ? EmitCondExpr(*sw.control, module, locals, regs)
                     : "false";
      std::string cond_n =
          sw.control_n ? EmitCondExpr(*sw.control_n, module, locals, regs)
                       : "false";
      std::string on = "(" + cond + " && !(" + cond_n + "))";
      cond_false = "!(" + on + ")";
    }

    out << "  if (" << cond_false << ") {\n";
    out << "  } else {\n";
    int temp_index = switch_temp_index++;
    std::string type = TypeForWidth(width);
    std::string zero = ZeroForWidth(width);
    std::string one = (width > 32) ? "1ul" : "1u";
    std::string strength0 = StrengthLiteral(sw.strength0);
    std::string strength1 = StrengthLiteral(sw.strength1);
    std::string a_tmp = "__gpga_sw_a" + std::to_string(temp_index);
    std::string b_tmp = "__gpga_sw_b" + std::to_string(temp_index);
    std::string m_val = "__gpga_sw_val" + std::to_string(temp_index);
    std::string m_drive = "__gpga_sw_drive" + std::to_string(temp_index);
    std::string a_drive = drive_var_name(sw.a);
    std::string b_drive = drive_var_name(sw.b);
    out << "    " << type << " " << a_tmp << " = " << a_val << ";\n";
    out << "    " << type << " " << b_tmp << " = " << b_val << ";\n";
    out << "    " << type << " " << m_val << " = " << zero << ";\n";
    out << "    " << type << " " << m_drive << " = " << zero << ";\n";
    out << "    for (uint bit = 0u; bit < " << width << "u; ++bit) {\n";
    out << "      " << type << " mask = (" << one << " << bit);\n";
    out << "      uint best0 = 0u;\n";
    out << "      uint best1 = 0u;\n";
    out << "      if ((" << a_drive << " & mask) != " << zero << ") {\n";
    out << "        if ((" << a_tmp << " & mask) != " << zero << ") {\n";
    out << "          best1 = (best1 > " << strength1 << ") ? best1 : "
        << strength1 << ";\n";
    out << "        } else {\n";
    out << "          best0 = (best0 > " << strength0 << ") ? best0 : "
        << strength0 << ";\n";
    out << "        }\n";
    out << "      }\n";
    out << "      if ((" << b_drive << " & mask) != " << zero << ") {\n";
    out << "        if ((" << b_tmp << " & mask) != " << zero << ") {\n";
    out << "          best1 = (best1 > " << strength1 << ") ? best1 : "
        << strength1 << ";\n";
    out << "        } else {\n";
    out << "          best0 = (best0 > " << strength0 << ") ? best0 : "
        << strength0 << ";\n";
    out << "        }\n";
    out << "      }\n";
    out << "      if (best0 == 0u && best1 == 0u) {\n";
    out << "        continue;\n";
    out << "      }\n";
    out << "      " << m_drive << " |= mask;\n";
    out << "      if (best1 > best0) {\n";
    out << "        " << m_val << " |= mask;\n";
    out << "      }\n";
    out << "    }\n";
    out << "    " << a_val << " = " << m_val << ";\n";
    out << "    " << b_val << " = " << m_val << ";\n";
    out << "    " << a_drive << " = " << m_drive << ";\n";
    out << "    " << b_drive << " = " << m_drive << ";\n";
    out << "  }\n";
  }
  out << "}\n";

  if (has_initial && !needs_scheduler) {
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
    for (const auto* reg : trireg_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg->name));
      out << "  device " << type << "* " << reg->name << " [[buffer("
          << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device ulong* " << decay_name(reg->name) << " [[buffer("
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
    for (const auto* reg : trireg_nets) {
      out << "  " << decay_name(reg->name) << "[gid] = 0ul;\n";
    }

    std::unordered_set<std::string> init_locals;
    std::unordered_set<std::string> init_regs;
    std::unordered_set<std::string> init_declared;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        continue;
      }
      if (net.type == NetType::kReg || IsTriregNet(net.type) ||
          export_wire_set.count(net.name) > 0) {
        if (port_names.count(net.name) == 0) {
          init_regs.insert(net.name);
        }
        continue;
      }
      if (port_names.count(net.name) == 0) {
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
        bool lhs_real = SignalIsReal(module, stmt.assign.lhs);
        std::string sized = lhs_real
                                ? EmitRealBitsExpr(*stmt.assign.rhs, module,
                                                   init_locals, init_regs)
                                : EmitExprSized(*stmt.assign.rhs, lvalue.width,
                                                module, init_locals, init_regs);
        if (lvalue.is_bit_select) {
          if (lhs_real) {
            out << pad << "// Unsupported real bit-select assign: "
                << stmt.assign.lhs << "\n";
            return;
          }
          std::string update = EmitBitSelectUpdate(
              lvalue.expr, lvalue.bit_index, lvalue.base_width, sized);
          if (!lvalue.guard.empty()) {
            out << pad << "if " << lvalue.guard << " {\n";
            out << pad << "  " << lvalue.expr << " = " << update << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << lvalue.expr << " = " << update << ";\n";
          }
          return;
        }
        if (lvalue.is_range) {
          if (lhs_real) {
            out << pad << "// Unsupported real range assign: "
                << stmt.assign.lhs << "\n";
            return;
          }
          std::string update = EmitRangeSelectUpdate(
              lvalue.expr,
              lvalue.is_indexed_range ? lvalue.range_index
                                      : std::to_string(lvalue.range_lsb),
              lvalue.base_width, lvalue.width, sized);
          if (!lvalue.guard.empty()) {
            out << pad << "if " << lvalue.guard << " {\n";
            out << pad << "  " << lvalue.expr << " = " << update << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << lvalue.expr << " = " << update << ";\n";
          }
          return;
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
                               ? EmitCondExpr(*stmt.condition, module,
                                              init_locals, init_regs)
                               : "false";
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
    for (const auto* reg : trireg_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg->name));
      out << "  device " << type << "* " << reg->name << " [[buffer("
          << buffer_index++ << ")]]";
      out << ",\n";
      out << "  device ulong* " << decay_name(reg->name) << " [[buffer("
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
        if (export_wire_set.count(net.name) > 0) {
          tick_regs.insert(net.name);
        } else {
          tick_locals.insert(net.name);
        }
      } else if (net.type == NetType::kReg || IsTriregNet(net.type)) {
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
        bool lhs_real = SignalIsReal(module, stmt.assign.lhs);
        std::string sized = lhs_real
                                ? EmitRealBitsExpr(*stmt.assign.rhs, module,
                                                   tick_locals, tick_regs)
                                : EmitExprSized(*stmt.assign.rhs, lvalue.width,
                                                module, tick_locals, tick_regs);
        if (lvalue.is_array) {
          if (lhs_real) {
            out << pad << "// Unsupported real array assign: "
                << stmt.assign.lhs << "\n";
            return;
          }
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
        if (lvalue.is_bit_select) {
          if (lhs_real) {
            out << pad << "// Unsupported real bit-select assign: "
                << stmt.assign.lhs << "\n";
            return;
          }
          std::string target = lvalue.expr;
          if (stmt.assign.nonblocking) {
            auto it = nb_map.find(stmt.assign.lhs);
            if (it != nb_map.end()) {
              target = it->second;
            }
          }
          std::string update = EmitBitSelectUpdate(
              target, lvalue.bit_index, lvalue.base_width, sized);
          if (!lvalue.guard.empty()) {
            out << pad << "if " << lvalue.guard << " {\n";
            out << pad << "  " << target << " = " << update << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << target << " = " << update << ";\n";
          }
          return;
        }
        if (lvalue.is_range) {
          if (lhs_real) {
            out << pad << "// Unsupported real range assign: "
                << stmt.assign.lhs << "\n";
            return;
          }
          std::string target = lvalue.expr;
          if (stmt.assign.nonblocking) {
            auto it = nb_map.find(stmt.assign.lhs);
            if (it != nb_map.end()) {
              target = it->second;
            }
          }
          std::string update = EmitRangeSelectUpdate(
              target,
              lvalue.is_indexed_range ? lvalue.range_index
                                      : std::to_string(lvalue.range_lsb),
              lvalue.base_width, lvalue.width, sized);
          if (!lvalue.guard.empty()) {
            out << pad << "if " << lvalue.guard << " {\n";
            out << pad << "  " << target << " = " << update << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << target << " = " << update << ";\n";
          }
          return;
        }
        if (stmt.assign.nonblocking && !stmt.assign.lhs_index &&
            !stmt.assign.lhs_has_range) {
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
                               ? EmitCondExpr(*stmt.condition, module,
                                              tick_locals, tick_regs)
                               : "false";
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
  if (needs_scheduler) {
    struct ProcDef {
      int pid = 0;
      const std::vector<Statement>* body = nullptr;
      const Statement* single = nullptr;
    };

    std::vector<const AlwaysBlock*> initial_blocks;
    for (const auto& block : module.always_blocks) {
      if (block.edge == EdgeKind::kInitial) {
        initial_blocks.push_back(&block);
      }
    }

    if (!initial_blocks.empty()) {
      std::unordered_map<std::string, int> event_ids;
      for (size_t i = 0; i < module.events.size(); ++i) {
        event_ids[module.events[i].name] = static_cast<int>(i);
      }

      struct ForkInfo {
        int tag = 0;
        std::vector<int> children;
      };

      std::unordered_map<const Statement*, ForkInfo> fork_info;
      std::unordered_map<int, std::unordered_map<std::string, int>>
          fork_child_labels;
      std::vector<ProcDef> procs;
      std::vector<int> proc_parent;
      std::vector<int> proc_join_tag;

      int next_pid = 0;
      for (const auto* block : initial_blocks) {
        procs.push_back(ProcDef{next_pid, &block->statements, nullptr});
        proc_parent.push_back(-1);
        proc_join_tag.push_back(-1);
        ++next_pid;
      }
      const int root_proc_count = next_pid;
      int next_fork_tag = 0;

      std::function<void(const Statement&, int)> collect_forks;
      std::function<void(const std::vector<Statement>&, int)>
          collect_forks_in_list;
      collect_forks = [&](const Statement& stmt, int parent_pid) {
        if (stmt.kind == StatementKind::kFork) {
          ForkInfo info;
          info.tag = next_fork_tag++;
        for (const auto& branch : stmt.fork_branches) {
          int child_pid = next_pid++;
          info.children.push_back(child_pid);
          procs.push_back(ProcDef{child_pid, nullptr, &branch});
          proc_parent.push_back(parent_pid);
          proc_join_tag.push_back(info.tag);
          if (branch.kind == StatementKind::kBlock &&
              !branch.block_label.empty()) {
            fork_child_labels[parent_pid][branch.block_label] = child_pid;
          }
          collect_forks(branch, child_pid);
        }
        fork_info[&stmt] = info;
        return;
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            collect_forks(inner, parent_pid);
          }
          for (const auto& inner : stmt.else_branch) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFor) {
          for (const auto& inner : stmt.for_body) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWhile) {
          for (const auto& inner : stmt.while_body) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kRepeat) {
          for (const auto& inner : stmt.repeat_body) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          for (const auto& inner : stmt.delay_body) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          for (const auto& inner : stmt.event_body) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          for (const auto& inner : stmt.wait_body) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          for (const auto& inner : stmt.forever_body) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              collect_forks(inner, parent_pid);
            }
          }
          for (const auto& inner : stmt.default_branch) {
            collect_forks(inner, parent_pid);
          }
          return;
        }
      };
      collect_forks_in_list = [&](const std::vector<Statement>& stmts,
                                  int parent_pid) {
        for (const auto& stmt : stmts) {
          collect_forks(stmt, parent_pid);
        }
      };
      for (int i = 0; i < root_proc_count; ++i) {
        collect_forks_in_list(*procs[i].body, procs[i].pid);
      }

      std::unordered_map<const Statement*, int> wait_ids;
      std::vector<const Expr*> wait_exprs;
      std::function<void(const Statement&)> collect_waits;
      collect_waits = [&](const Statement& stmt) -> void {
        if (stmt.kind == StatementKind::kWait && stmt.wait_condition) {
          if (wait_ids.find(&stmt) == wait_ids.end()) {
            wait_ids[&stmt] = static_cast<int>(wait_exprs.size());
            wait_exprs.push_back(stmt.wait_condition.get());
          }
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            collect_waits(inner);
          }
          for (const auto& inner : stmt.else_branch) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFor) {
          for (const auto& inner : stmt.for_body) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWhile) {
          for (const auto& inner : stmt.while_body) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kRepeat) {
          for (const auto& inner : stmt.repeat_body) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          for (const auto& inner : stmt.delay_body) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          for (const auto& inner : stmt.event_body) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          for (const auto& inner : stmt.wait_body) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          for (const auto& inner : stmt.forever_body) {
            collect_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              collect_waits(inner);
            }
          }
          for (const auto& inner : stmt.default_branch) {
            collect_waits(inner);
          }
          return;
        }
      };
      for (const auto& block : module.always_blocks) {
        for (const auto& stmt : block.statements) {
          collect_waits(stmt);
        }
      }

      struct EdgeWaitItem {
        const Expr* expr = nullptr;
        EventEdgeKind edge = EventEdgeKind::kAny;
      };
      struct EdgeWaitInfo {
        const Statement* stmt = nullptr;
        const Expr* expr = nullptr;
        std::vector<EdgeWaitItem> items;
        std::vector<std::string> star_signals;
        size_t star_offset = 0;
        size_t item_offset = 0;
      };
      std::unordered_map<const Statement*, int> edge_wait_ids;
      std::vector<EdgeWaitInfo> edge_waits;
      size_t edge_star_count = 0;
      size_t edge_item_count = 0;
      std::function<void(const Statement&)> collect_edge_waits;
      collect_edge_waits = [&](const Statement& stmt) -> void {
        if (stmt.kind == StatementKind::kEventControl) {
          bool named_event = false;
          const Expr* named_expr = nullptr;
          if (!stmt.event_items.empty()) {
            if (stmt.event_items.size() == 1 &&
                stmt.event_items[0].edge == EventEdgeKind::kAny &&
                stmt.event_items[0].expr) {
              named_expr = stmt.event_items[0].expr.get();
            }
          } else if (stmt.event_expr &&
                     stmt.event_edge == EventEdgeKind::kAny) {
            named_expr = stmt.event_expr.get();
          }
          if (named_expr && named_expr->kind == ExprKind::kIdentifier) {
            auto it = event_ids.find(named_expr->ident);
            if (it != event_ids.end()) {
              named_event = true;
            }
          }
          if (!named_event &&
              edge_wait_ids.find(&stmt) == edge_wait_ids.end()) {
            EdgeWaitInfo info;
            info.stmt = &stmt;
            if (!stmt.event_items.empty()) {
              for (const auto& item : stmt.event_items) {
                if (!item.expr) {
                  continue;
                }
                info.items.push_back(
                    EdgeWaitItem{item.expr.get(), item.edge});
              }
            } else {
              info.expr = stmt.event_expr.get();
            }
            if (!info.items.empty()) {
              info.item_offset = edge_item_count;
              edge_item_count += info.items.size();
            } else if (info.expr) {
              info.item_offset = edge_item_count;
              edge_item_count += 1;
            } else {
              std::unordered_set<std::string> signals;
              for (const auto& inner : stmt.event_body) {
                CollectReadSignals(inner, &signals);
              }
              info.star_signals.assign(signals.begin(), signals.end());
              std::sort(info.star_signals.begin(), info.star_signals.end());
              info.star_offset = edge_star_count;
              edge_star_count += info.star_signals.size();
            }
            edge_wait_ids[&stmt] = static_cast<int>(edge_waits.size());
            edge_waits.push_back(std::move(info));
          }
          for (const auto& inner : stmt.event_body) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            collect_edge_waits(inner);
          }
          for (const auto& inner : stmt.else_branch) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFor) {
          for (const auto& inner : stmt.for_body) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWhile) {
          for (const auto& inner : stmt.while_body) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kRepeat) {
          for (const auto& inner : stmt.repeat_body) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          for (const auto& inner : stmt.delay_body) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          for (const auto& inner : stmt.wait_body) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          for (const auto& inner : stmt.forever_body) {
            collect_edge_waits(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              collect_edge_waits(inner);
            }
          }
          for (const auto& inner : stmt.default_branch) {
            collect_edge_waits(inner);
          }
          return;
        }
      };
      for (const auto& block : module.always_blocks) {
        for (const auto& stmt : block.statements) {
          collect_edge_waits(stmt);
        }
      }

      struct DelayAssignInfo {
        const Statement* stmt = nullptr;
        std::string lhs;
        bool nonblocking = false;
        bool lhs_real = false;
        bool is_array = false;
        bool is_bit_select = false;
        bool is_range = false;
        bool is_indexed_range = false;
        int width = 0;
        int base_width = 0;
        int range_lsb = 0;
        int array_size = 0;
        int element_width = 0;
      };

      std::unordered_map<const Statement*, uint32_t> delay_assign_ids;
      std::vector<DelayAssignInfo> delay_assigns;
      size_t delayed_nba_count = 0;
      std::function<void(const Statement&)> collect_delay_assigns;
      collect_delay_assigns = [&](const Statement& stmt) -> void {
        if (stmt.kind == StatementKind::kAssign && stmt.assign.delay) {
          DelayAssignInfo info;
          info.stmt = &stmt;
          info.lhs = stmt.assign.lhs;
          info.nonblocking = stmt.assign.nonblocking;
          info.lhs_real = SignalIsReal(module, stmt.assign.lhs);
          info.base_width = SignalWidth(module, stmt.assign.lhs);
          int element_width = 0;
          int array_size = 0;
          bool is_array = stmt.assign.lhs_index &&
                          IsArrayNet(module, stmt.assign.lhs, &element_width,
                                     &array_size);
          info.is_array = is_array;
          info.element_width = element_width;
          info.array_size = array_size;
          if (is_array) {
            info.width = element_width;
          } else if (stmt.assign.lhs_index) {
            info.is_bit_select = true;
            info.width = 1;
          } else if (stmt.assign.lhs_has_range) {
            info.is_range = true;
            info.base_width = SignalWidth(module, stmt.assign.lhs);
            if (stmt.assign.lhs_indexed_range) {
              info.is_indexed_range = true;
              info.width = stmt.assign.lhs_indexed_width;
            } else {
              int lo = std::min(stmt.assign.lhs_msb, stmt.assign.lhs_lsb);
              int hi = std::max(stmt.assign.lhs_msb, stmt.assign.lhs_lsb);
              info.range_lsb = lo;
              info.width = hi - lo + 1;
            }
          } else {
            info.width = SignalWidth(module, stmt.assign.lhs);
          }
          if (info.width <= 0) {
            info.width = info.base_width > 0 ? info.base_width : 1;
          }
          delay_assign_ids[&stmt] =
              static_cast<uint32_t>(delay_assigns.size());
          delay_assigns.push_back(info);
          if (info.nonblocking) {
            delayed_nba_count += 1;
          }
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            collect_delay_assigns(inner);
          }
          for (const auto& inner : stmt.else_branch) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFor) {
          for (const auto& inner : stmt.for_body) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWhile) {
          for (const auto& inner : stmt.while_body) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kRepeat) {
          for (const auto& inner : stmt.repeat_body) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          for (const auto& inner : stmt.delay_body) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          for (const auto& inner : stmt.event_body) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          for (const auto& inner : stmt.wait_body) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          for (const auto& inner : stmt.forever_body) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              collect_delay_assigns(inner);
            }
          }
          for (const auto& inner : stmt.default_branch) {
            collect_delay_assigns(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFork) {
          for (const auto& inner : stmt.fork_branches) {
            collect_delay_assigns(inner);
          }
        }
      };
      for (const auto& block : module.always_blocks) {
        for (const auto& stmt : block.statements) {
          collect_delay_assigns(stmt);
        }
      }

      std::unordered_set<std::string> nb_targets;
      std::unordered_set<std::string> nb_array_targets;
      std::function<void(const Statement&)> collect_nb_targets;
      collect_nb_targets = [&](const Statement& stmt) -> void {
        if (stmt.kind == StatementKind::kAssign && stmt.assign.nonblocking) {
          if (stmt.assign.lhs_index) {
            nb_array_targets.insert(stmt.assign.lhs);
          } else {
            nb_targets.insert(stmt.assign.lhs);
          }
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            collect_nb_targets(inner);
          }
          for (const auto& inner : stmt.else_branch) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFor) {
          for (const auto& inner : stmt.for_body) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWhile) {
          for (const auto& inner : stmt.while_body) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kRepeat) {
          for (const auto& inner : stmt.repeat_body) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          for (const auto& inner : stmt.delay_body) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          for (const auto& inner : stmt.event_body) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          for (const auto& inner : stmt.wait_body) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          for (const auto& inner : stmt.forever_body) {
            collect_nb_targets(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              collect_nb_targets(inner);
            }
          }
          for (const auto& inner : stmt.default_branch) {
            collect_nb_targets(inner);
          }
          return;
        }
      };
      for (const auto& block : module.always_blocks) {
        for (const auto& stmt : block.statements) {
          collect_nb_targets(stmt);
        }
      }

      std::vector<std::string> nb_targets_sorted(nb_targets.begin(),
                                                 nb_targets.end());
      std::sort(nb_targets_sorted.begin(), nb_targets_sorted.end());
      std::vector<const Net*> nb_array_nets;
      for (const auto& net : module.nets) {
        if (net.array_size <= 0) {
          continue;
        }
        if (nb_array_targets.count(net.name) > 0) {
          nb_array_nets.push_back(&net);
        }
      }
      std::sort(nb_array_nets.begin(), nb_array_nets.end(),
                [](const Net* a, const Net* b) { return a->name < b->name; });

      const bool has_delayed_assigns = !delay_assigns.empty();
      const bool has_delayed_nba = delayed_nba_count > 0;
      size_t delayed_nba_capacity =
          has_delayed_nba ? std::max<size_t>(1, delayed_nba_count * 4) : 0;

      std::unordered_set<std::string> sched_locals;
      std::unordered_set<std::string> sched_regs;
      for (const auto& net : module.nets) {
        if (net.array_size > 0) {
          continue;
        }
        if (port_names.count(net.name) > 0) {
          continue;
        }
        if (net.type == NetType::kReg || IsTriregNet(net.type) ||
            scheduled_reads.count(net.name) > 0) {
          sched_regs.insert(net.name);
          continue;
        }
        if (!IsOutputPort(module, net.name)) {
          sched_locals.insert(net.name);
        }
      }

      std::unordered_set<std::string> sched_reg_set;
      for (const auto& net : module.nets) {
        if (net.array_size > 0 || port_names.count(net.name) > 0) {
          continue;
        }
        if (net.type == NetType::kReg || IsTriregNet(net.type) ||
            scheduled_reads.count(net.name) > 0) {
          sched_reg_set.insert(net.name);
        }
      }
      std::vector<std::string> sched_reg_names(sched_reg_set.begin(),
                                               sched_reg_set.end());
      std::sort(sched_reg_names.begin(), sched_reg_names.end());

      std::unordered_map<const Statement*, uint32_t> monitor_pid;
      std::unordered_map<const Statement*, uint32_t> strobe_pid;
      if (!system_task_info.monitor_stmts.empty() ||
          !system_task_info.strobe_stmts.empty()) {
        std::function<void(const Statement&, uint32_t)> collect_monitor_pids;
        collect_monitor_pids = [&](const Statement& stmt, uint32_t pid) {
          if (stmt.kind == StatementKind::kTaskCall &&
              stmt.task_name == "$monitor") {
            monitor_pid[&stmt] = pid;
            return;
          }
          if (stmt.kind == StatementKind::kTaskCall &&
              stmt.task_name == "$strobe") {
            strobe_pid[&stmt] = pid;
            return;
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_monitor_pids(inner, pid);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_monitor_pids(inner, pid);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            for (const auto& inner : stmt.event_body) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFork) {
            for (const auto& inner : stmt.fork_branches) {
              collect_monitor_pids(inner, pid);
            }
            return;
          }
        };

        for (const auto& proc : procs) {
          if (proc.body) {
            for (const auto& stmt : *proc.body) {
              collect_monitor_pids(stmt, proc.pid);
            }
          } else if (proc.single) {
            collect_monitor_pids(*proc.single, proc.pid);
          }
        }
      }

      out << "\n";
      out << "struct GpgaSchedParams { uint max_steps; uint max_proc_steps; uint service_capacity; };\n";
      out << "constexpr uint GPGA_SCHED_PROC_COUNT = " << procs.size() << "u;\n";
      out << "constexpr uint GPGA_SCHED_ROOT_COUNT = " << root_proc_count
          << "u;\n";
      out << "constexpr uint GPGA_SCHED_EVENT_COUNT = "
          << module.events.size() << "u;\n";
      out << "constexpr uint GPGA_SCHED_EDGE_COUNT = " << edge_item_count
          << "u;\n";
      out << "constexpr uint GPGA_SCHED_EDGE_STAR_COUNT = " << edge_star_count
          << "u;\n";
      out << "constexpr uint GPGA_SCHED_MAX_READY = " << procs.size() << "u;\n";
      out << "constexpr uint GPGA_SCHED_MAX_TIME = " << procs.size() << "u;\n";
      out << "constexpr uint GPGA_SCHED_MAX_NBA = " << nb_targets_sorted.size()
          << "u;\n";
      if (has_delayed_assigns) {
        out << "constexpr uint GPGA_SCHED_DELAY_COUNT = "
            << delay_assigns.size() << "u;\n";
      }
      if (has_delayed_nba) {
        out << "constexpr uint GPGA_SCHED_MAX_DNBA = " << delayed_nba_capacity
            << "u;\n";
      }
      out << "constexpr uint GPGA_SCHED_NO_PARENT = 0xFFFFFFFFu;\n";
      out << "constexpr uint GPGA_SCHED_WAIT_NONE = 0u;\n";
      out << "constexpr uint GPGA_SCHED_WAIT_TIME = 1u;\n";
      out << "constexpr uint GPGA_SCHED_WAIT_EVENT = 2u;\n";
      out << "constexpr uint GPGA_SCHED_WAIT_COND = 3u;\n";
      out << "constexpr uint GPGA_SCHED_WAIT_JOIN = 4u;\n";
      out << "constexpr uint GPGA_SCHED_WAIT_DELTA = 5u;\n";
      out << "constexpr uint GPGA_SCHED_WAIT_EDGE = 6u;\n";
      out << "constexpr uint GPGA_SCHED_EDGE_ANY = 0u;\n";
      out << "constexpr uint GPGA_SCHED_EDGE_POSEDGE = 1u;\n";
      out << "constexpr uint GPGA_SCHED_EDGE_NEGEDGE = 2u;\n";
      out << "constexpr uint GPGA_SCHED_EDGE_LIST = 3u;\n";
      out << "constexpr uint GPGA_SCHED_PROC_READY = 0u;\n";
      out << "constexpr uint GPGA_SCHED_PROC_BLOCKED = 1u;\n";
      out << "constexpr uint GPGA_SCHED_PROC_DONE = 2u;\n";
      out << "constexpr uint GPGA_SCHED_PHASE_ACTIVE = 0u;\n";
      out << "constexpr uint GPGA_SCHED_PHASE_NBA = 1u;\n";
      out << "constexpr uint GPGA_SCHED_STATUS_RUNNING = 0u;\n";
      out << "constexpr uint GPGA_SCHED_STATUS_IDLE = 1u;\n";
      out << "constexpr uint GPGA_SCHED_STATUS_FINISHED = 2u;\n";
      out << "constexpr uint GPGA_SCHED_STATUS_ERROR = 3u;\n";
      out << "constexpr uint GPGA_SCHED_STATUS_STOPPED = 4u;\n";
      if (!system_task_info.monitor_stmts.empty()) {
        size_t max_args =
            std::max<size_t>(1, system_task_info.monitor_max_args);
        out << "constexpr uint GPGA_SCHED_MONITOR_COUNT = "
            << system_task_info.monitor_stmts.size() << "u;\n";
        out << "constexpr uint GPGA_SCHED_MONITOR_MAX_ARGS = " << max_args
            << "u;\n";
      }
      if (!system_task_info.strobe_stmts.empty()) {
        out << "constexpr uint GPGA_SCHED_STROBE_COUNT = "
            << system_task_info.strobe_stmts.size() << "u;\n";
      }
      if (system_task_info.has_system_tasks) {
        size_t max_args = std::max<size_t>(1, system_task_info.max_args);
        out << "constexpr uint GPGA_SCHED_SERVICE_MAX_ARGS = " << max_args
            << "u;\n";
        out << "constexpr uint GPGA_SCHED_STRING_COUNT = "
            << system_task_info.string_table.size() << "u;\n";
        out << "constexpr uint GPGA_SERVICE_INVALID_ID = 0xFFFFFFFFu;\n";
        out << "constexpr uint GPGA_SERVICE_ARG_VALUE = 0u;\n";
        out << "constexpr uint GPGA_SERVICE_ARG_IDENT = 1u;\n";
        out << "constexpr uint GPGA_SERVICE_ARG_STRING = 2u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_DISPLAY = 0u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_MONITOR = 1u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_FINISH = 2u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_DUMPFILE = 3u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_DUMPVARS = 4u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_READMEMH = 5u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_READMEMB = 6u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_STOP = 7u;\n";
        out << "constexpr uint GPGA_SERVICE_KIND_STROBE = 8u;\n";
        out << "struct GpgaServiceRecord {\n";
        out << "  uint kind;\n";
        out << "  uint pid;\n";
        out << "  uint format_id;\n";
        out << "  uint arg_count;\n";
        out << "  uint arg_kind[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
        out << "  uint arg_width[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
        out << "  ulong arg_val[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
        out << "};\n";
      }
      out << "inline uint gpga_sched_index(uint gid, uint pid) {\n";
      out << "  return (gid * GPGA_SCHED_PROC_COUNT) + pid;\n";
      out << "}\n";
      out << "constant uint gpga_proc_parent[GPGA_SCHED_PROC_COUNT] = {";
      for (size_t i = 0; i < procs.size(); ++i) {
        uint32_t parent =
            proc_parent[i] < 0 ? 0xFFFFFFFFu
                               : static_cast<uint32_t>(proc_parent[i]);
        if (i > 0) {
          out << ", ";
        }
        out << parent << "u";
      }
      out << "};\n";
      out << "constant uint gpga_proc_join_tag[GPGA_SCHED_PROC_COUNT] = {";
      for (size_t i = 0; i < procs.size(); ++i) {
        uint32_t tag = proc_join_tag[i] < 0
                           ? 0xFFFFFFFFu
                           : static_cast<uint32_t>(proc_join_tag[i]);
        if (i > 0) {
          out << ", ";
        }
        out << tag << "u";
      }
      out << "};\n";

      out << "\n";
      out << "kernel void gpga_" << module.name << "_sched_step(";
      int buffer_index = 0;
      bool first = true;
      auto emit_param = [&](const std::string& text) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        out << text;
      };
      for (const auto& port : module.ports) {
        std::string qualifier =
            (port.dir == PortDir::kInput) ? "constant" : "device";
        std::string type = TypeForWidth(port.width);
        emit_param("  " + qualifier + " " + type + "* " + port.name +
                   " [[buffer(" + std::to_string(buffer_index++) + ")]]");
      }
      for (const auto& reg : sched_reg_names) {
        std::string type = TypeForWidth(SignalWidth(module, reg));
        emit_param("  device " + type + "* " + reg + " [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      for (const auto* reg : trireg_nets) {
        emit_param("  device ulong* " + decay_name(reg->name) + " [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      for (const auto* net : array_nets) {
        std::string type = TypeForWidth(net->width);
        emit_param("  device " + type + "* " + net->name + " [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      for (const auto& target : nb_targets_sorted) {
        std::string type = TypeForWidth(SignalWidth(module, target));
        emit_param("  device " + type + "* nb_" + target + " [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      for (const auto* net : nb_array_nets) {
        std::string type = TypeForWidth(net->width);
        emit_param("  device " + type + "* " + net->name + "_next [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      };
      emit_param("  device uint* sched_pc [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_state [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_wait_kind [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_wait_edge_kind [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_wait_id [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_wait_event [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device ulong* sched_edge_prev_val [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device ulong* sched_edge_star_prev_val [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device ulong* sched_wait_time [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_join_count [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_parent [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_join_tag [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device ulong* sched_time [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_phase [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_active_init [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_initialized [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_event_pending [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_error [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_status [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      if (has_delayed_assigns) {
        emit_param("  device ulong* sched_delay_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_delay_index_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (has_delayed_nba) {
        emit_param("  device uint* sched_dnba_count [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_dnba_time [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_dnba_id [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_dnba_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_dnba_index_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (!system_task_info.monitor_stmts.empty()) {
        emit_param("  device uint* sched_monitor_active [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_monitor_enable [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device ulong* sched_monitor_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (system_task_info.has_system_tasks) {
        emit_param("  device uint* sched_service_count [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device GpgaServiceRecord* sched_service [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      emit_param("  constant GpgaSchedParams& sched [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  constant GpgaParams& params [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
    emit_param("  uint gid [[thread_position_in_grid]]) {\n");
    out << "  if (gid >= params.count) {\n";
    out << "    return;\n";
    out << "  }\n";
    if (system_task_info.has_system_tasks) {
      out << "  sched_service_count[gid] = 0u;\n";
    }
    out << "  ulong __gpga_time = sched_time[gid];\n";
      out << "  if (sched_initialized[gid] == 0u) {\n";
      out << "    sched_time[gid] = 0ul;\n";
      out << "    __gpga_time = 0ul;\n";
      out << "    sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
      out << "    sched_active_init[gid] = 1u;\n";
      out << "    sched_error[gid] = 0u;\n";
      for (const auto* reg : trireg_nets) {
        out << "    " << decay_name(reg->name) << "[gid] = 0ul;\n";
      }
      if (has_delayed_nba) {
        out << "    sched_dnba_count[gid] = 0u;\n";
      }
    out << "    for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
    out << "      sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
    out << "    }\n";
    out << "    for (uint e = 0u; e < GPGA_SCHED_EDGE_COUNT; ++e) {\n";
    out << "      uint eidx = (gid * GPGA_SCHED_EDGE_COUNT) + e;\n";
    out << "      sched_edge_prev_val[eidx] = 0ul;\n";
    out << "    }\n";
    out << "    for (uint s = 0u; s < GPGA_SCHED_EDGE_STAR_COUNT; ++s) {\n";
    out << "      uint sidx = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + s;\n";
    out << "      sched_edge_star_prev_val[sidx] = 0ul;\n";
    out << "    }\n";
    if (!system_task_info.monitor_stmts.empty()) {
      out << "    sched_monitor_enable[gid] = 1u;\n";
      out << "    for (uint m = 0u; m < GPGA_SCHED_MONITOR_COUNT; ++m) {\n";
      out << "      sched_monitor_active[(gid * GPGA_SCHED_MONITOR_COUNT) + m] = 0u;\n";
      out << "      for (uint a = 0u; a < GPGA_SCHED_MONITOR_MAX_ARGS; ++a) {\n";
      out << "        uint offset = ((gid * GPGA_SCHED_MONITOR_COUNT) + m) * GPGA_SCHED_MONITOR_MAX_ARGS + a;\n";
      out << "        sched_monitor_val[offset] = 0ul;\n";
      out << "      }\n";
      out << "    }\n";
    }
    out << "    for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
      out << "      uint idx = gpga_sched_index(gid, pid);\n";
      out << "      sched_pc[idx] = 0u;\n";
      out << "      sched_state[idx] = (pid < GPGA_SCHED_ROOT_COUNT)\n";
      out << "          ? GPGA_SCHED_PROC_READY : GPGA_SCHED_PROC_BLOCKED;\n";
      out << "      sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "      sched_wait_edge_kind[idx] = GPGA_SCHED_EDGE_ANY;\n";
      out << "      sched_wait_id[idx] = 0u;\n";
      out << "      sched_wait_event[idx] = 0u;\n";
      out << "      sched_wait_time[idx] = 0ul;\n";
      out << "      sched_join_count[idx] = 0u;\n";
      out << "      sched_parent[idx] = gpga_proc_parent[pid];\n";
      out << "      sched_join_tag[idx] = gpga_proc_join_tag[pid];\n";
      out << "    }\n";
      out << "    sched_initialized[gid] = 1u;\n";
      out << "  }\n";
      out << "  if (sched_error[gid] != 0u) {\n";
      out << "    sched_status[gid] = GPGA_SCHED_STATUS_ERROR;\n";
      out << "    return;\n";
      out << "  }\n";

      auto delay_base_expr = [&](const std::string& name) -> LvalueInfo {
        LvalueInfo out_lhs;
        out_lhs.width = SignalWidth(module, name);
        out_lhs.base_width = out_lhs.width;
        if (IsOutputPort(module, name) || sched_regs.count(name) > 0) {
          out_lhs.expr = name + "[gid]";
          out_lhs.ok = true;
          return out_lhs;
        }
        if (sched_locals.count(name) > 0) {
          out_lhs.expr = name;
          out_lhs.ok = true;
          return out_lhs;
        }
        return out_lhs;
      };

      auto emit_delay_assign_apply =
          [&](const std::string& id_expr, const std::string& val_expr,
              const std::string& idx_val_expr, bool use_nb,
              int indent) -> void {
        std::string pad(indent, ' ');
        out << pad << "switch (" << id_expr << ") {\n";
        for (size_t i = 0; i < delay_assigns.size(); ++i) {
          const DelayAssignInfo& info = delay_assigns[i];
          std::string pad2(indent + 2, ' ');
          out << pad2 << "case " << i << "u: {\n";
          if (info.lhs_real && (info.is_bit_select || info.is_range)) {
            out << pad2 << "  sched_error[gid] = 1u;\n";
            out << pad2 << "  break;\n";
            out << pad2 << "}\n";
            continue;
          }
          std::string target;
          if (info.is_array) {
            std::string name = info.lhs;
            if (use_nb) {
              name += "_next";
            }
            std::string idx = "uint(" + idx_val_expr + ")";
            std::string base = "(gid * " + std::to_string(info.array_size) +
                               "u) + " + idx;
            std::string guard =
                "(" + idx + " < " + std::to_string(info.array_size) + "u)";
            target = name + "[" + base + "]";
            out << pad2 << "  if " << guard << " {\n";
            out << pad2 << "    " << target << " = "
                << MaskForWidthExpr(val_expr, info.width) << ";\n";
            out << pad2 << "  }\n";
            out << pad2 << "  break;\n";
            out << pad2 << "}\n";
            continue;
          }
          if (use_nb) {
            target = "nb_" + info.lhs + "[gid]";
          } else {
            LvalueInfo base = delay_base_expr(info.lhs);
            if (!base.ok) {
              out << pad2 << "  sched_error[gid] = 1u;\n";
              out << pad2 << "  break;\n";
              out << pad2 << "}\n";
              continue;
            }
            target = base.expr;
          }
          if (info.is_bit_select) {
            std::string update = EmitBitSelectUpdate(
                target, idx_val_expr, info.base_width,
                MaskForWidthExpr(val_expr, 1));
            std::string guard;
            if (info.base_width > 0) {
              guard = "(uint(" + idx_val_expr + ") < " +
                      std::to_string(info.base_width) + "u)";
            } else {
              guard = "false";
            }
            out << pad2 << "  if " << guard << " {\n";
            out << pad2 << "    " << target << " = " << update << ";\n";
            out << pad2 << "  }\n";
            out << pad2 << "  break;\n";
            out << pad2 << "}\n";
            continue;
          }
          if (info.is_range) {
            std::string index_expr = info.is_indexed_range
                                         ? idx_val_expr
                                         : std::to_string(info.range_lsb);
            std::string update = EmitRangeSelectUpdate(
                target, index_expr, info.base_width, info.width,
                MaskForWidthExpr(val_expr, info.width));
            std::string guard;
            if (info.is_indexed_range) {
              if (info.base_width >= info.width) {
                int limit = info.base_width - info.width;
                guard = "(uint(" + idx_val_expr + ") <= " +
                        std::to_string(limit) + "u)";
              } else {
                guard = "false";
              }
            }
            if (!guard.empty()) {
              out << pad2 << "  if " << guard << " {\n";
              out << pad2 << "    " << target << " = " << update << ";\n";
              out << pad2 << "  }\n";
            } else {
              out << pad2 << "  " << target << " = " << update << ";\n";
            }
            out << pad2 << "  break;\n";
            out << pad2 << "}\n";
            continue;
          }
          out << pad2 << "  " << target << " = "
              << MaskForWidthExpr(val_expr, info.width) << ";\n";
          out << pad2 << "  break;\n";
          out << pad2 << "}\n";
        }
        out << pad << "}\n";
      };

      auto emit_delay_value2 = [&](const Expr& expr) -> std::string {
        if (ExprIsRealValue(expr, module)) {
          return "ulong(" +
                 EmitRealValueExpr(expr, module, sched_locals, sched_regs) +
                 ")";
        }
        std::string delay =
            EmitExprSized(expr, 64, module, sched_locals, sched_regs);
        return "ulong(" + delay + ")";
      };

      out << "  sched_status[gid] = GPGA_SCHED_STATUS_RUNNING;\n";
      out << "  bool finished = false;\n";
      out << "  bool stopped = false;\n";
      out << "  uint steps = sched.max_steps;\n";
      out << "  while (steps > 0u) {\n";
      out << "    bool did_work = false;\n";
      out << "    if (sched_phase[gid] == GPGA_SCHED_PHASE_ACTIVE) {\n";
      out << "      if (sched_active_init[gid] != 0u) {\n";
      out << "        sched_active_init[gid] = 0u;\n";
      if (!nb_targets_sorted.empty()) {
        out << "        // Initialize NBA buffers for this delta.\n";
        for (const auto& target : nb_targets_sorted) {
          out << "        nb_" << target << "[gid] = " << target << "[gid];\n";
        }
      }
      if (!nb_array_nets.empty()) {
        out << "        // Initialize array NBA buffers.\n";
        for (const auto* net : nb_array_nets) {
          out << "        for (uint i = 0u; i < " << net->array_size
              << "u; ++i) {\n";
          out << "          " << net->name << "_next[(gid * " << net->array_size
              << "u) + i] = " << net->name << "[(gid * " << net->array_size
              << "u) + i];\n";
          out << "        }\n";
        }
      }
      if (has_delayed_nba) {
        out << "        if (sched_dnba_count[gid] != 0u) {\n";
        out << "          uint __gpga_dnba_base = gid * GPGA_SCHED_MAX_DNBA;\n";
        out << "          uint __gpga_dnba_count = sched_dnba_count[gid];\n";
        out << "          uint __gpga_dnba_write = 0u;\n";
        out << "          for (uint __gpga_dnba_i = 0u; __gpga_dnba_i < __gpga_dnba_count; ++__gpga_dnba_i) {\n";
        out << "            uint __gpga_dnba_idx = __gpga_dnba_base + __gpga_dnba_i;\n";
        out << "            ulong __gpga_dnba_time = sched_dnba_time[__gpga_dnba_idx];\n";
        out << "            if (__gpga_dnba_time <= __gpga_time) {\n";
        out << "              uint __gpga_dnba_id = sched_dnba_id[__gpga_dnba_idx];\n";
        out << "              ulong __gpga_dval = sched_dnba_val[__gpga_dnba_idx];\n";
        out << "              uint __gpga_didx_val = sched_dnba_index_val[__gpga_dnba_idx];\n";
        emit_delay_assign_apply("__gpga_dnba_id", "__gpga_dval",
                                "__gpga_didx_val", true, 14);
        out << "            } else {\n";
        out << "              uint __gpga_dnba_out = __gpga_dnba_base + __gpga_dnba_write;\n";
        out << "              if (__gpga_dnba_out != __gpga_dnba_idx) {\n";
        out << "                sched_dnba_time[__gpga_dnba_out] = __gpga_dnba_time;\n";
        out << "                sched_dnba_id[__gpga_dnba_out] = sched_dnba_id[__gpga_dnba_idx];\n";
        out << "                sched_dnba_val[__gpga_dnba_out] = sched_dnba_val[__gpga_dnba_idx];\n";
        out << "                sched_dnba_index_val[__gpga_dnba_out] = sched_dnba_index_val[__gpga_dnba_idx];\n";
        out << "              }\n";
        out << "              __gpga_dnba_write += 1u;\n";
        out << "            }\n";
        out << "          }\n";
        out << "          sched_dnba_count[gid] = __gpga_dnba_write;\n";
        out << "        }\n";
      }
      out << "      }\n";
      out << "      for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
      out << "        uint idx = gpga_sched_index(gid, pid);\n";
      out << "        while (steps > 0u && sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
      out << "          did_work = true;\n";
      out << "          steps--;\n";
      out << "          switch (pid) {\n";

    auto emit_inline_assign =
        [&](const SequentialAssign& assign, int indent,
            const std::unordered_set<std::string>& locals_override) -> void {
      if (!assign.rhs) {
        return;
      }
      std::string pad(indent, ' ');
      LvalueInfo lhs =
          BuildLvalue(assign, module, locals_override, sched_regs, false);
      if (!lhs.ok) {
        return;
      }
      std::string sized = EmitExprSized(*assign.rhs, lhs.width, module,
                                        locals_override, sched_regs);
      if (assign.nonblocking) {
        if (assign.lhs_index) {
          LvalueInfo next =
              BuildLvalue(assign, module, locals_override, sched_regs, true);
          if (next.ok) {
            if (!next.guard.empty()) {
              out << pad << "if " << next.guard << " {\n";
              out << pad << "  " << next.expr << " = " << sized << ";\n";
              out << pad << "}\n";
            } else {
              out << pad << next.expr << " = " << sized << ";\n";
            }
          }
          return;
        }
        if (lhs.is_bit_select) {
          std::string target = lhs.expr;
          if (IsOutputPort(module, assign.lhs) ||
              sched_regs.count(assign.lhs) > 0) {
            target = "nb_" + assign.lhs + "[gid]";
          }
          std::string update = EmitBitSelectUpdate(
              target, lhs.bit_index, lhs.base_width, sized);
          if (!lhs.guard.empty()) {
            out << pad << "if " << lhs.guard << " {\n";
            out << pad << "  " << target << " = " << update << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << target << " = " << update << ";\n";
          }
          return;
        }
        if (lhs.is_range) {
          std::string target = lhs.expr;
          if (IsOutputPort(module, assign.lhs) ||
              sched_regs.count(assign.lhs) > 0) {
            target = "nb_" + assign.lhs + "[gid]";
          }
          std::string update = EmitRangeSelectUpdate(
              target,
              lhs.is_indexed_range ? lhs.range_index
                                   : std::to_string(lhs.range_lsb),
              lhs.base_width, lhs.width, sized);
          if (!lhs.guard.empty()) {
            out << pad << "if " << lhs.guard << " {\n";
            out << pad << "  " << target << " = " << update << ";\n";
            out << pad << "}\n";
          } else {
            out << pad << target << " = " << update << ";\n";
          }
          return;
        }
        out << pad << "nb_" << assign.lhs << "[gid] = " << sized << ";\n";
        return;
      }
      if (lhs.is_bit_select) {
        std::string update = EmitBitSelectUpdate(
            lhs.expr, lhs.bit_index, lhs.base_width, sized);
        if (!lhs.guard.empty()) {
          out << pad << "if " << lhs.guard << " {\n";
          out << pad << "  " << lhs.expr << " = " << update << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lhs.expr << " = " << update << ";\n";
        }
        return;
      }
      if (lhs.is_range) {
        std::string update = EmitRangeSelectUpdate(
            lhs.expr,
            lhs.is_indexed_range ? lhs.range_index
                                 : std::to_string(lhs.range_lsb),
            lhs.base_width, lhs.width, sized);
        if (!lhs.guard.empty()) {
          out << pad << "if " << lhs.guard << " {\n";
          out << pad << "  " << lhs.expr << " = " << update << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lhs.expr << " = " << update << ";\n";
        }
        return;
      }
      if (!lhs.guard.empty()) {
        out << pad << "if " << lhs.guard << " {\n";
        out << pad << "  " << lhs.expr << " = " << sized << ";\n";
          out << pad << "}\n";
        } else {
          out << pad << lhs.expr << " = " << sized << ";\n";
        }
      };

      auto string_id_for = [&](const std::string& value,
                               uint32_t* out_id) -> bool {
        auto it = system_task_info.string_ids.find(value);
        if (it == system_task_info.string_ids.end()) {
          return false;
        }
        if (out_id) {
          *out_id = it->second;
        }
        return true;
      };

      auto to_ulong = [&](const std::string& expr, int width) -> std::string {
        return (width > 32) ? expr : "(ulong)(" + expr + ")";
      };

      struct ServiceArg {
        std::string kind;
        int width = 0;
        std::string val;
      };

      auto build_service_args =
          [&](const Statement& stmt, const std::string& name,
              std::string* format_id_expr,
              std::vector<ServiceArg>* args) -> bool {
        if (!format_id_expr || !args) {
          return false;
        }
        *format_id_expr = "GPGA_SERVICE_INVALID_ID";
        if (!stmt.task_args.empty() &&
            stmt.task_args[0]->kind == ExprKind::kString) {
          uint32_t id = 0;
          if (!string_id_for(stmt.task_args[0]->string_value, &id)) {
            return false;
          }
          *format_id_expr = std::to_string(id) + "u";
        }

        bool requires_string =
            name == "$dumpfile" || name == "$readmemh" || name == "$readmemb";
        if (requires_string &&
            *format_id_expr == "GPGA_SERVICE_INVALID_ID") {
          return false;
        }

        bool ident_as_string = TaskTreatsIdentifierAsString(name);
        args->clear();
        args->reserve(stmt.task_args.size());
        for (const auto& arg : stmt.task_args) {
          if (!arg) {
            continue;
          }
          if (arg->kind == ExprKind::kString) {
            uint32_t id = 0;
            if (!string_id_for(arg->string_value, &id)) {
              return false;
            }
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                       std::to_string(id) + "ul"});
            continue;
          }
          if (ident_as_string && arg->kind == ExprKind::kIdentifier) {
            uint32_t id = 0;
            if (!string_id_for(arg->ident, &id)) {
              return false;
            }
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", 0,
                                       std::to_string(id) + "ul"});
            continue;
          }
          if (arg->kind == ExprKind::kCall && arg->ident == "$time") {
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_VALUE", 64,
                                       "__gpga_time"});
            continue;
          }
          int width = ExprWidth(*arg, module);
          if (width <= 0) {
            width = 1;
          }
          std::string value =
              EmitExprSized(*arg, width, module, sched_locals, sched_regs);
          args->push_back(ServiceArg{"GPGA_SERVICE_ARG_VALUE", width,
                                     to_ulong(value, width)});
        }
        return true;
      };

      auto emit_service_record =
          [&](const char* kind_expr, const std::string& format_id_expr,
              const std::vector<ServiceArg>& args, int indent) -> void {
        std::string pad(indent, ' ');
        out << pad << "{\n";
        out << pad << "  uint __gpga_svc_index = sched_service_count[gid];\n";
        out << pad << "  if (__gpga_svc_index >= sched.service_capacity) {\n";
        out << pad << "    sched_error[gid] = 1u;\n";
        out << pad << "    sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
        out << pad << "  } else {\n";
        out << pad
            << "    uint __gpga_svc_offset = (gid * sched.service_capacity) + "
               "__gpga_svc_index;\n";
        out << pad
            << "    sched_service_count[gid] = __gpga_svc_index + 1u;\n";
        out << pad << "    sched_service[__gpga_svc_offset].kind = "
            << kind_expr << ";\n";
        out << pad << "    sched_service[__gpga_svc_offset].pid = pid;\n";
        out << pad << "    sched_service[__gpga_svc_offset].format_id = "
            << format_id_expr << ";\n";
        out << pad << "    sched_service[__gpga_svc_offset].arg_count = "
            << args.size() << "u;\n";
        for (size_t i = 0; i < args.size(); ++i) {
          out << pad << "    sched_service[__gpga_svc_offset].arg_kind[" << i
              << "] = " << args[i].kind << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_width[" << i
              << "] = " << args[i].width << "u;\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
              << "] = " << args[i].val << ";\n";
        }
        out << pad << "  }\n";
        out << pad << "}\n";
      };

      auto emit_monitor_record =
          [&](const std::string& pid_expr, const std::string& format_id_expr,
              const std::vector<ServiceArg>& args, int indent) -> void {
        std::string pad(indent, ' ');
        out << pad << "{\n";
        out << pad << "  uint __gpga_svc_index = sched_service_count[gid];\n";
        out << pad << "  if (__gpga_svc_index >= sched.service_capacity) {\n";
        out << pad << "    sched_error[gid] = 1u;\n";
        out << pad << "    steps = 0u;\n";
        out << pad << "  } else {\n";
        out << pad
            << "    uint __gpga_svc_offset = (gid * sched.service_capacity) + "
               "__gpga_svc_index;\n";
        out << pad
            << "    sched_service_count[gid] = __gpga_svc_index + 1u;\n";
        out << pad << "    sched_service[__gpga_svc_offset].kind = "
            << "GPGA_SERVICE_KIND_MONITOR" << ";\n";
        out << pad << "    sched_service[__gpga_svc_offset].pid = "
            << pid_expr << ";\n";
        out << pad << "    sched_service[__gpga_svc_offset].format_id = "
            << format_id_expr << ";\n";
        out << pad << "    sched_service[__gpga_svc_offset].arg_count = "
            << args.size() << "u;\n";
        for (size_t i = 0; i < args.size(); ++i) {
          out << pad << "    sched_service[__gpga_svc_offset].arg_kind[" << i
              << "] = " << args[i].kind << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_width[" << i
              << "] = " << args[i].width << "u;\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
              << "] = " << args[i].val << ";\n";
        }
        out << pad << "  }\n";
        out << pad << "}\n";
      };

      auto emit_service_record_with_pid =
          [&](const char* kind_expr, const std::string& pid_expr,
              const std::string& format_id_expr,
              const std::vector<ServiceArg>& args, int indent) -> void {
        std::string pad(indent, ' ');
        out << pad << "{\n";
        out << pad << "  uint __gpga_svc_index = sched_service_count[gid];\n";
        out << pad << "  if (__gpga_svc_index >= sched.service_capacity) {\n";
        out << pad << "    sched_error[gid] = 1u;\n";
        out << pad << "    steps = 0u;\n";
        out << pad << "  } else {\n";
        out << pad
            << "    uint __gpga_svc_offset = (gid * sched.service_capacity) + "
               "__gpga_svc_index;\n";
        out << pad
            << "    sched_service_count[gid] = __gpga_svc_index + 1u;\n";
        out << pad << "    sched_service[__gpga_svc_offset].kind = "
            << kind_expr << ";\n";
        out << pad << "    sched_service[__gpga_svc_offset].pid = "
            << pid_expr << ";\n";
        out << pad << "    sched_service[__gpga_svc_offset].format_id = "
            << format_id_expr << ";\n";
        out << pad << "    sched_service[__gpga_svc_offset].arg_count = "
            << args.size() << "u;\n";
        for (size_t i = 0; i < args.size(); ++i) {
          out << pad << "    sched_service[__gpga_svc_offset].arg_kind[" << i
              << "] = " << args[i].kind << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_width[" << i
              << "] = " << args[i].width << "u;\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
              << "] = " << args[i].val << ";\n";
        }
        out << pad << "  }\n";
        out << pad << "}\n";
      };

      auto emit_monitor_snapshot =
          [&](uint32_t monitor_id, const std::vector<ServiceArg>& args,
              int indent, bool force_emit) -> std::string {
        std::string pad(indent, ' ');
        std::string prefix = "__gpga_mon_" + std::to_string(monitor_id);
        std::string changed = prefix + "_changed";
        out << pad << "uint " << prefix << "_base = ((gid * "
            << "GPGA_SCHED_MONITOR_COUNT) + " << monitor_id
            << "u) * GPGA_SCHED_MONITOR_MAX_ARGS;\n";
        out << pad << "bool " << changed << " = "
            << (force_emit ? "true" : "false") << ";\n";
        for (size_t i = 0; i < args.size(); ++i) {
          if (args[i].kind != "GPGA_SERVICE_ARG_VALUE") {
            continue;
          }
          int width = args[i].width;
          if (width <= 0) {
            width = 1;
          }
          uint64_t mask = MaskForWidth64(width);
          std::string mask_literal = std::to_string(mask) + "ul";
          out << pad << "ulong " << prefix << "_val" << i << " = ("
              << args[i].val << ") & " << mask_literal << ";\n";
          out << pad << "uint " << prefix << "_slot" << i << " = "
              << prefix << "_base + " << i << "u;\n";
          out << pad << "if (((sched_monitor_val[" << prefix << "_slot" << i
              << "] ^ " << prefix << "_val" << i << ") & " << mask_literal
              << ") != 0ul) {\n";
          out << pad << "  " << changed << " = true;\n";
          out << pad << "}\n";
          out << pad << "sched_monitor_val[" << prefix << "_slot" << i
              << "] = " << prefix << "_val" << i << ";\n";
        }
        return changed;
      };

      auto emit_system_task = [&](const Statement& stmt, int indent) -> void {
        if (!system_task_info.has_system_tasks) {
          out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          const std::string& name = stmt.task_name;
        if (name == "$monitoron") {
          if (!system_task_info.monitor_stmts.empty()) {
            out << std::string(indent, ' ')
                << "sched_monitor_enable[gid] = 1u;\n";
          }
          return;
        }
        if (name == "$monitoroff") {
          if (!system_task_info.monitor_stmts.empty()) {
            out << std::string(indent, ' ')
                << "sched_monitor_enable[gid] = 0u;\n";
          }
          return;
        }
        if (name == "$strobe") {
          auto it = system_task_info.strobe_ids.find(&stmt);
          if (it == system_task_info.strobe_ids.end()) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          uint32_t strobe_id = it->second;
          std::string pad(indent, ' ');
          out << pad << "sched_strobe_pending[(gid * "
              << "GPGA_SCHED_STROBE_COUNT) + " << strobe_id << "u] += 1u;\n";
          return;
        }
        const char* kind_expr = nullptr;
        if (name == "$display") {
          kind_expr = "GPGA_SERVICE_KIND_DISPLAY";
        } else if (name == "$monitor") {
          kind_expr = "GPGA_SERVICE_KIND_MONITOR";
        } else if (name == "$finish") {
          kind_expr = "GPGA_SERVICE_KIND_FINISH";
        } else if (name == "$stop") {
          kind_expr = "GPGA_SERVICE_KIND_STOP";
        } else if (name == "$dumpfile") {
          kind_expr = "GPGA_SERVICE_KIND_DUMPFILE";
        } else if (name == "$dumpvars") {
          kind_expr = "GPGA_SERVICE_KIND_DUMPVARS";
        } else if (name == "$readmemh") {
          kind_expr = "GPGA_SERVICE_KIND_READMEMH";
        } else if (name == "$readmemb") {
          kind_expr = "GPGA_SERVICE_KIND_READMEMB";
        } else {
          out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
          out << std::string(indent, ' ')
              << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          return;
        }

        std::string format_id_expr;
        std::vector<ServiceArg> args;
        if (!build_service_args(stmt, name, &format_id_expr, &args)) {
          out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
          out << std::string(indent, ' ')
              << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          return;
        }

        if (name == "$monitor") {
          auto it = system_task_info.monitor_ids.find(&stmt);
          if (it == system_task_info.monitor_ids.end()) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          uint32_t monitor_id = it->second;
          std::string pad(indent, ' ');
          out << pad << "sched_monitor_active[(gid * "
              << "GPGA_SCHED_MONITOR_COUNT) + " << monitor_id << "u] = 1u;\n";
          std::string changed =
              emit_monitor_snapshot(monitor_id, args, indent, true);
          out << pad << "if (sched_monitor_enable[gid] != 0u && " << changed
              << ") {\n";
          emit_service_record(kind_expr, format_id_expr, args, indent + 2);
          out << pad << "}\n";
        } else {
          emit_service_record(kind_expr, format_id_expr, args, indent);
        }

        if (name == "$finish") {
          out << std::string(indent, ' ') << "finished = true;\n";
          out << std::string(indent, ' ') << "steps = 0u;\n";
          out << std::string(indent, ' ')
              << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
        } else if (name == "$stop") {
          out << std::string(indent, ' ') << "stopped = true;\n";
          out << std::string(indent, ' ') << "steps = 0u;\n";
          }
        };

        std::function<bool(const Statement&)> inline_needs_scheduler;
        inline_needs_scheduler = [&](const Statement& stmt) -> bool {
          if (stmt.kind == StatementKind::kAssign && stmt.assign.delay) {
            return true;
          }
          if (stmt.kind == StatementKind::kTaskCall) {
            return !IsSystemTaskName(stmt.task_name);
          }
          if (stmt.kind == StatementKind::kEventTrigger) {
            return false;
          }
          switch (stmt.kind) {
            case StatementKind::kDelay:
            case StatementKind::kEventControl:
            case StatementKind::kWait:
            case StatementKind::kForever:
            case StatementKind::kFork:
            case StatementKind::kDisable:
              return true;
            default:
              break;
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              if (inline_needs_scheduler(inner)) {
                return true;
              }
            }
            for (const auto& inner : stmt.else_branch) {
              if (inline_needs_scheduler(inner)) {
                return true;
              }
            }
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              if (inline_needs_scheduler(inner)) {
                return true;
              }
            }
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                if (inline_needs_scheduler(inner)) {
                  return true;
                }
              }
            }
            for (const auto& inner : stmt.default_branch) {
              if (inline_needs_scheduler(inner)) {
                return true;
              }
            }
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              if (inline_needs_scheduler(inner)) {
                return true;
              }
            }
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              if (inline_needs_scheduler(inner)) {
                return true;
              }
            }
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              if (inline_needs_scheduler(inner)) {
                return true;
              }
            }
          }
          return false;
        };

        auto emit_inline_stmt =
            [&](const Statement& stmt, int indent,
                const std::unordered_set<std::string>& locals_override,
                const auto& self) -> void {
        std::string pad(indent, ' ');
        if (stmt.kind == StatementKind::kTaskCall &&
            IsSystemTaskName(stmt.task_name)) {
          emit_system_task(stmt, indent);
          return;
        }
        if (stmt.kind == StatementKind::kEventTrigger) {
          auto it = event_ids.find(stmt.trigger_target);
          if (it == event_ids.end()) {
            out << pad << "sched_error[gid] = 1u;\n";
            out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          out << pad << "sched_event_pending[(gid * "
              << "GPGA_SCHED_EVENT_COUNT) + " << it->second << "u] = 1u;\n";
          return;
        }
        if (stmt.kind == StatementKind::kAssign) {
          if (stmt.assign.delay) {
            out << pad << "sched_error[gid] = 1u;\n";
            out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          emit_inline_assign(stmt.assign, indent, locals_override);
          return;
        }
        if (inline_needs_scheduler(stmt)) {
          out << pad << "sched_error[gid] = 1u;\n";
          out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          std::string cond = stmt.condition
                                 ? EmitCondExpr(*stmt.condition, module,
                                                locals_override, sched_regs)
                                 : "false";
          out << pad << "if (" << cond << ") {\n";
          for (const auto& inner : stmt.then_branch) {
            self(inner, indent + 2, locals_override, self);
          }
          if (!stmt.else_branch.empty()) {
            out << pad << "} else {\n";
            for (const auto& inner : stmt.else_branch) {
              self(inner, indent + 2, locals_override, self);
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
              EmitExpr(*stmt.case_expr, module, locals_override, sched_regs);
          int case_width = ExprWidth(*stmt.case_expr, module);
          bool first = true;
          for (const auto& item : stmt.case_items) {
            std::string cond;
            for (const auto& label : item.labels) {
              int label_width = ExprWidth(*label, module);
              int target = std::max(case_width, label_width);
              std::string lhs = ExtendExpr(case_value, case_width, target);
              std::string rhs = EmitExpr(*label, module, locals_override,
                                         sched_regs);
              std::string rhs_ext = ExtendExpr(rhs, label_width, target);
              std::string piece = "(" + lhs + " == " + rhs_ext + ")";
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
              self(inner, indent + 2, locals_override, self);
            }
          }
          if (!stmt.default_branch.empty()) {
            out << pad << "} else {\n";
            for (const auto& inner : stmt.default_branch) {
              self(inner, indent + 2, locals_override, self);
            }
            out << pad << "}\n";
          } else if (!first) {
            out << pad << "}\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kFor) {
          std::string init = stmt.for_init_rhs
                                 ? EmitExprSized(*stmt.for_init_rhs,
                                                 SignalWidth(module,
                                                             stmt.for_init_lhs),
                                                 module, locals_override,
                                                 sched_regs)
                                 : "0u";
          out << pad << stmt.for_init_lhs << " = " << init << ";\n";
          std::string cond = stmt.for_condition
                                 ? EmitCondExpr(*stmt.for_condition, module,
                                                locals_override, sched_regs)
                                 : "false";
          out << pad << "while (" << cond << ") {\n";
          for (const auto& inner : stmt.for_body) {
            self(inner, indent + 2, locals_override, self);
          }
          std::string step = stmt.for_step_rhs
                                 ? EmitExprSized(*stmt.for_step_rhs,
                                                 SignalWidth(module,
                                                             stmt.for_step_lhs),
                                                 module, locals_override,
                                                 sched_regs)
                                 : "0u";
          out << pad << "  " << stmt.for_step_lhs << " = " << step << ";\n";
          out << pad << "}\n";
          return;
        }
        if (stmt.kind == StatementKind::kWhile) {
          std::string cond = stmt.while_condition
                                 ? EmitCondExpr(*stmt.while_condition, module,
                                                locals_override, sched_regs)
                                 : "false";
          out << pad << "while (" << cond << ") {\n";
          for (const auto& inner : stmt.while_body) {
            self(inner, indent + 2, locals_override, self);
          }
          out << pad << "}\n";
          return;
        }
        if (stmt.kind == StatementKind::kRepeat) {
          std::string count =
              stmt.repeat_count
                  ? EmitExprSized(*stmt.repeat_count, 32, module,
                                  locals_override, sched_regs)
                  : "0u";
          out << pad << "for (uint __gpga_rep = 0u; __gpga_rep < " << count
              << "; ++__gpga_rep) {\n";
          for (const auto& inner : stmt.repeat_body) {
            self(inner, indent + 2, locals_override, self);
          }
          out << pad << "}\n";
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          out << pad << "{\n";
          for (const auto& inner : stmt.block) {
            self(inner, indent + 2, locals_override, self);
          }
          out << pad << "}\n";
          return;
        }
      };

    auto emit_task_call =
        [&](const Statement& stmt, int indent,
            const auto& emit_inline_stmt_fn) -> void {
      if (IsSystemTaskName(stmt.task_name)) {
        emit_system_task(stmt, indent);
        return;
      }
      const Task* task = FindTask(module, stmt.task_name);
      if (!task) {
        out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
          out << std::string(indent, ' ') << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          return;
        }
        std::unordered_set<std::string> task_locals = sched_locals;
        std::unordered_map<std::string, int> task_widths;
        std::unordered_map<std::string, bool> task_signed;
        std::unordered_map<std::string, bool> task_real;
        struct TaskOutArg {
          std::string name;
          std::string target;
          int target_width = 0;
          bool target_real = false;
        };
        std::vector<TaskOutArg> task_outs;
        task_widths.reserve(task->args.size());
        task_signed.reserve(task->args.size());
        task_real.reserve(task->args.size());
        task_outs.reserve(task->args.size());

        for (const auto& arg : task->args) {
          task_widths[arg.name] = arg.width;
          task_signed[arg.name] = arg.is_signed;
          task_real[arg.name] = arg.is_real;
        }

        for (size_t i = 0; i < task->args.size(); ++i) {
          const auto& arg = task->args[i];
          const Expr* call_arg = nullptr;
          if (i < stmt.task_args.size()) {
            call_arg = stmt.task_args[i].get();
          }
          std::string type = TypeForWidth(arg.width);
          if (arg.dir == TaskArgDir::kInput) {
            std::string expr = call_arg
                                   ? (arg.is_real
                                          ? EmitRealBitsExpr(*call_arg, module,
                                                             sched_locals,
                                                             sched_regs)
                                          : EmitExprSized(*call_arg, arg.width,
                                                          module, sched_locals,
                                                          sched_regs))
                                   : "0u";
            out << std::string(indent, ' ') << type << " " << arg.name
                << " = " << expr << ";\n";
            task_locals.insert(arg.name);
            continue;
          }
          if (!call_arg || call_arg->kind != ExprKind::kIdentifier) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          std::string init = arg.is_real
                                 ? EmitRealBitsExpr(*call_arg, module,
                                                    sched_locals, sched_regs)
                                 : EmitExprSized(*call_arg, arg.width, module,
                                                 sched_locals, sched_regs);
          out << std::string(indent, ' ') << type << " " << arg.name << " = "
              << init << ";\n";
          task_locals.insert(arg.name);
          std::string target =
              EmitExpr(*call_arg, module, sched_locals, sched_regs);
          int target_width = ExprWidth(*call_arg, module);
          bool target_real = SignalIsReal(module, call_arg->ident);
          task_outs.push_back(
              TaskOutArg{arg.name, target, target_width, target_real});
        }

        const auto* prev_widths = g_task_arg_widths;
        const auto* prev_signed = g_task_arg_signed;
        const auto* prev_real = g_task_arg_real;
        g_task_arg_widths = &task_widths;
        g_task_arg_signed = &task_signed;
        g_task_arg_real = &task_real;
        for (const auto& inner : task->body) {
          emit_inline_stmt_fn(inner, indent, task_locals, emit_inline_stmt_fn);
        }
        for (const auto& out_arg : task_outs) {
          Expr arg_expr;
          arg_expr.kind = ExprKind::kIdentifier;
          arg_expr.ident = out_arg.name;
          std::string value =
              out_arg.target_real
                  ? EmitRealBitsExpr(arg_expr, module, task_locals, sched_regs)
                  : EmitExprSized(arg_expr, out_arg.target_width, module,
                                  task_locals, sched_regs);
          out << std::string(indent, ' ') << out_arg.target << " = " << value
              << ";\n";
        }
        g_task_arg_widths = prev_widths;
        g_task_arg_signed = prev_signed;
        g_task_arg_real = prev_real;
      };

      for (const auto& proc : procs) {
        std::vector<const Statement*> stmts;
        if (proc.body) {
          for (const auto& stmt : *proc.body) {
            stmts.push_back(&stmt);
          }
        } else if (proc.single) {
          stmts.push_back(proc.single);
        }
        std::unordered_map<const Statement*, int> pc_for_stmt;
        int pc_counter = 0;
        for (const auto* stmt : stmts) {
          pc_for_stmt[stmt] = pc_counter++;
        }
        const int pc_done = pc_counter++;
        struct BodyCase {
          int pc = 0;
          const Statement* owner = nullptr;
          std::vector<const Statement*> body;
          int next_pc = 0;
          int loop_pc = -1;
          bool is_forever_body = false;
          bool is_assign_delay = false;
          int delay_id = -1;
        };
        std::vector<BodyCase> body_cases;

        std::unordered_map<std::string, int> block_end_pc;
        for (size_t i = 0; i < stmts.size(); ++i) {
          const auto* stmt = stmts[i];
          if (stmt->kind == StatementKind::kBlock &&
              !stmt->block_label.empty()) {
            int next_pc = (i + 1 < stmts.size()) ? pc_for_stmt[stmts[i + 1]]
                                                 : pc_done;
            block_end_pc[stmt->block_label] = next_pc;
          }
        }

        out << "            case " << proc.pid << ": {\n";
        out << "              uint pc = sched_pc[idx];\n";
        out << "              switch (pc) {\n";
        for (size_t i = 0; i < stmts.size(); ++i) {
          const Statement& stmt = *stmts[i];
          int pc = pc_for_stmt[&stmt];
          int next_pc =
              (i + 1 < stmts.size()) ? pc_for_stmt[stmts[i + 1]] : pc_done;
          out << "                case " << pc << ": {\n";
          if (stmt.kind == StatementKind::kAssign) {
            if (!stmt.assign.rhs) {
              out << "                  sched_pc[idx] = " << next_pc
                  << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.assign.delay) {
              auto it = delay_assign_ids.find(&stmt);
              if (it == delay_assign_ids.end()) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              uint32_t delay_id = it->second;
              const DelayAssignInfo& info = delay_assigns[delay_id];
              std::string rhs =
                  info.lhs_real
                      ? EmitRealBitsExpr(*stmt.assign.rhs, module, sched_locals,
                                         sched_regs)
                      : EmitExprSized(*stmt.assign.rhs, info.width, module,
                                      sched_locals, sched_regs);
              std::string mask =
                  std::to_string(MaskForWidth64(info.width)) + "ul";
              out << "                  ulong __gpga_dval = ((ulong)("
                  << rhs << ")) & " << mask << ";\n";
              std::string idx_val = "0u";
              if (info.is_array || info.is_bit_select ||
                  info.is_indexed_range) {
                const Expr* idx_expr = nullptr;
                if (info.is_indexed_range) {
                  idx_expr = stmt.assign.lhs_lsb_expr.get();
                } else {
                  idx_expr = stmt.assign.lhs_index.get();
                }
                if (!idx_expr) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                idx_val = EmitExpr(*idx_expr, module, sched_locals, sched_regs);
              }
              out << "                  uint __gpga_didx_val = uint("
                  << idx_val << ");\n";
              std::string delay = emit_delay_value2(*stmt.assign.delay);
              out << "                  ulong __gpga_delay = " << delay
                  << ";\n";
              if (stmt.assign.nonblocking) {
                out << "                  if (__gpga_delay == 0ul) {\n";
                emit_delay_assign_apply(std::to_string(delay_id) + "u",
                                        "__gpga_dval", "__gpga_didx_val", true,
                                        20);
                out << "                  } else {\n";
                out << "                    uint __gpga_dnba_count = sched_dnba_count[gid];\n";
                out << "                    if (__gpga_dnba_count >= GPGA_SCHED_MAX_DNBA) {\n";
                out << "                      sched_error[gid] = 1u;\n";
                out << "                    } else {\n";
                out << "                      uint __gpga_dnba_slot = (gid * GPGA_SCHED_MAX_DNBA) + __gpga_dnba_count;\n";
                out << "                      sched_dnba_count[gid] = __gpga_dnba_count + 1u;\n";
                out << "                      sched_dnba_time[__gpga_dnba_slot] = __gpga_time + __gpga_delay;\n";
                out << "                      sched_dnba_id[__gpga_dnba_slot] = "
                    << delay_id << "u;\n";
                out << "                      sched_dnba_val[__gpga_dnba_slot] = __gpga_dval;\n";
                out << "                      sched_dnba_index_val[__gpga_dnba_slot] = __gpga_didx_val;\n";
                out << "                    }\n";
                out << "                  }\n";
                out << "                  sched_pc[idx] = " << next_pc << "u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              int body_pc = pc_counter++;
              BodyCase body_case;
              body_case.pc = body_pc;
              body_case.owner = &stmt;
              body_case.next_pc = next_pc;
              body_case.is_assign_delay = true;
              body_case.delay_id = static_cast<int>(delay_id);
              body_cases.push_back(std::move(body_case));
              out << "                  uint __gpga_delay_slot = (gid * "
                  << "GPGA_SCHED_DELAY_COUNT) + " << delay_id << "u;\n";
              out << "                  sched_delay_val[__gpga_delay_slot] = __gpga_dval;\n";
              out << "                  sched_delay_index_val[__gpga_delay_slot] = __gpga_didx_val;\n";
              out << "                  sched_wait_kind[idx] = "
                     "(__gpga_delay == 0ul) ? GPGA_SCHED_WAIT_DELTA : "
                     "GPGA_SCHED_WAIT_TIME;\n";
              out << "                  sched_wait_time[idx] = __gpga_time + "
                     "__gpga_delay;\n";
              out << "                  sched_pc[idx] = " << body_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            emit_inline_stmt(stmt, 18, sched_locals, emit_inline_stmt);
            out << "                  sched_pc[idx] = " << next_pc << "u;\n";
            out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kDelay) {
            int body_pc = -1;
            if (!stmt.delay_body.empty()) {
              body_pc = pc_counter++;
              BodyCase body_case;
              body_case.pc = body_pc;
              body_case.owner = &stmt;
              body_case.next_pc = next_pc;
              for (const auto& inner : stmt.delay_body) {
                body_case.body.push_back(&inner);
              }
              body_cases.push_back(std::move(body_case));
            }
            std::string delay =
                stmt.delay ? emit_delay_value2(*stmt.delay) : "0ul";
            out << "                  ulong __gpga_delay = " << delay << ";\n";
            out << "                  sched_wait_kind[idx] = "
                   "(__gpga_delay == 0ul) ? GPGA_SCHED_WAIT_DELTA : "
                   "GPGA_SCHED_WAIT_TIME;\n";
            out << "                  sched_wait_time[idx] = __gpga_time + "
                   "__gpga_delay;\n";
            out << "                  sched_pc[idx] = "
                << (body_pc >= 0 ? std::to_string(body_pc) + "u"
                                 : std::to_string(next_pc) + "u")
                << ";\n";
            out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            int body_pc = -1;
            if (!stmt.event_body.empty()) {
              body_pc = pc_counter++;
              BodyCase body_case;
              body_case.pc = body_pc;
              body_case.owner = &stmt;
              body_case.next_pc = next_pc;
              for (const auto& inner : stmt.event_body) {
                body_case.body.push_back(&inner);
              }
              body_cases.push_back(std::move(body_case));
            }
            int event_id = -1;
            bool named_event = false;
            const Expr* named_expr = nullptr;
            if (!stmt.event_items.empty()) {
              if (stmt.event_items.size() == 1 &&
                  stmt.event_items[0].edge == EventEdgeKind::kAny &&
                  stmt.event_items[0].expr) {
                named_expr = stmt.event_items[0].expr.get();
              }
            } else if (stmt.event_expr &&
                       stmt.event_edge == EventEdgeKind::kAny) {
              named_expr = stmt.event_expr.get();
            }
            if (named_expr && named_expr->kind == ExprKind::kIdentifier) {
              auto it = event_ids.find(named_expr->ident);
              if (it != event_ids.end()) {
                event_id = it->second;
                named_event = true;
              }
            }
            if (named_event) {
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_EVENT;\n";
              out << "                  sched_wait_event[idx] = " << event_id
                  << "u;\n";
            } else {
              int edge_id = -1;
              auto it = edge_wait_ids.find(&stmt);
              if (it != edge_wait_ids.end()) {
                edge_id = it->second;
              }
              if (edge_id < 0) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              const EdgeWaitInfo& info = edge_waits[edge_id];
              const char* edge_kind = "GPGA_SCHED_EDGE_ANY";
              if (!info.items.empty()) {
                edge_kind = "GPGA_SCHED_EDGE_LIST";
              } else if (stmt.event_edge == EventEdgeKind::kPosedge) {
                edge_kind = "GPGA_SCHED_EDGE_POSEDGE";
              } else if (stmt.event_edge == EventEdgeKind::kNegedge) {
                edge_kind = "GPGA_SCHED_EDGE_NEGEDGE";
              }
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_EDGE;\n";
              out << "                  sched_wait_id[idx] = " << edge_id << "u;\n";
              out << "                  sched_wait_edge_kind[idx] = " << edge_kind
                  << ";\n";
              if (!info.items.empty()) {
                out << "                  uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_COUNT) + "
                    << info.item_offset << "u;\n";
                for (size_t i = 0; i < info.items.size(); ++i) {
                  int width = ExprWidth(*info.items[i].expr, module);
                  std::string curr =
                      EmitExprSized(*info.items[i].expr, width, module,
                                    sched_locals, sched_regs);
                  std::string mask =
                      std::to_string(MaskForWidth64(width)) + "ul";
                  out << "                  ulong __gpga_edge_val = ((ulong)("
                      << curr << ")) & " << mask << ";\n";
                  out << "                  sched_edge_prev_val[__gpga_edge_base + "
                      << i << "u] = __gpga_edge_val;\n";
                }
              } else if (info.expr) {
                int width = ExprWidth(*info.expr, module);
                std::string curr =
                    EmitExprSized(*info.expr, width, module, sched_locals,
                                  sched_regs);
                std::string mask =
                    std::to_string(MaskForWidth64(width)) + "ul";
                out << "                  uint __gpga_edge_idx = (gid * GPGA_SCHED_EDGE_COUNT) + "
                    << info.item_offset << "u;\n";
                out << "                  ulong __gpga_edge_val = ((ulong)(" << curr
                    << ")) & " << mask << ";\n";
                out << "                  sched_edge_prev_val[__gpga_edge_idx] = __gpga_edge_val;\n";
              } else {
                out << "                  uint __gpga_edge_star_base = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + "
                    << info.star_offset << "u;\n";
                for (size_t i = 0; i < info.star_signals.size(); ++i) {
                  Expr ident_expr;
                  ident_expr.kind = ExprKind::kIdentifier;
                  ident_expr.ident = info.star_signals[i];
                  int width = ExprWidth(ident_expr, module);
                  std::string curr =
                      EmitExprSized(ident_expr, width, module, sched_locals,
                                    sched_regs);
                  std::string mask =
                      std::to_string(MaskForWidth64(width)) + "ul";
                  out << "                  sched_edge_star_prev_val[__gpga_edge_star_base + "
                      << i << "u] = ((ulong)(" << curr << ")) & " << mask
                      << ";\n";
                }
              }
            }
            out << "                  sched_pc[idx] = "
                << (body_pc >= 0 ? std::to_string(body_pc) + "u"
                                 : std::to_string(next_pc) + "u")
                << ";\n";
            out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kWait) {
            int body_pc = -1;
            if (!stmt.wait_body.empty()) {
              body_pc = pc_counter++;
              BodyCase body_case;
              body_case.pc = body_pc;
              body_case.owner = &stmt;
              body_case.next_pc = next_pc;
              for (const auto& inner : stmt.wait_body) {
                body_case.body.push_back(&inner);
              }
              body_cases.push_back(std::move(body_case));
            }
            int wait_id = -1;
            auto it = wait_ids.find(&stmt);
            if (it != wait_ids.end()) {
              wait_id = it->second;
            }
            if (!stmt.wait_condition || wait_id < 0) {
              out << "                  sched_pc[idx] = " << next_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            std::string cond =
                EmitCondExpr(*stmt.wait_condition, module, sched_locals,
                             sched_regs);
            out << "                  if ((" << cond << ") != 0u) {\n";
            if (body_pc >= 0) {
              out << "                    sched_pc[idx] = " << body_pc << "u;\n";
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            } else {
              out << "                    sched_pc[idx] = " << next_pc << "u;\n";
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            }
            out << "                    break;\n";
            out << "                  }\n";
            out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_COND;\n";
            out << "                  sched_wait_id[idx] = " << wait_id << "u;\n";
            out << "                  sched_pc[idx] = "
                << (body_pc >= 0 ? std::to_string(body_pc) + "u"
                                 : std::to_string(next_pc) + "u")
                << ";\n";
            out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kForever) {
            if (stmt.forever_body.empty()) {
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            const Statement& body_stmt = stmt.forever_body.front();
            if (body_stmt.kind != StatementKind::kDelay) {
              out << "                  sched_error[gid] = 1u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            int body_pc = pc_counter++;
            BodyCase body_case;
            body_case.pc = body_pc;
            body_case.owner = &stmt;
            body_case.next_pc = pc;
            body_case.loop_pc = pc;
            body_case.is_forever_body = true;
            for (const auto& inner : body_stmt.delay_body) {
              body_case.body.push_back(&inner);
            }
            body_cases.push_back(std::move(body_case));
            std::string delay =
                body_stmt.delay ? emit_delay_value2(*body_stmt.delay) : "0ul";
            out << "                  ulong __gpga_delay = " << delay << ";\n";
            out << "                  sched_wait_kind[idx] = "
                   "(__gpga_delay == 0ul) ? GPGA_SCHED_WAIT_DELTA : "
                   "GPGA_SCHED_WAIT_TIME;\n";
            out << "                  sched_wait_time[idx] = __gpga_time + "
                   "__gpga_delay;\n";
            out << "                  sched_pc[idx] = " << body_pc << "u;\n";
            out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kFork) {
            auto it = fork_info.find(&stmt);
            if (it == fork_info.end()) {
              out << "                  sched_error[gid] = 1u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            const ForkInfo& info = it->second;
            for (int child : info.children) {
              out << "                  {\n";
              out << "                    uint cidx = gpga_sched_index(gid, "
                  << child << "u);\n";
              out << "                    sched_pc[cidx] = 0u;\n";
              out << "                    sched_state[cidx] = GPGA_SCHED_PROC_READY;\n";
              out << "                    sched_wait_kind[cidx] = GPGA_SCHED_WAIT_NONE;\n";
              out << "                    sched_wait_id[cidx] = 0u;\n";
              out << "                    sched_wait_event[cidx] = 0u;\n";
              out << "                    sched_wait_time[cidx] = 0ul;\n";
              out << "                    sched_join_count[cidx] = 0u;\n";
              out << "                  }\n";
            }
            out << "                  sched_join_count[idx] = "
                << info.children.size() << "u;\n";
            out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_JOIN;\n";
            out << "                  sched_wait_id[idx] = " << info.tag << "u;\n";
            out << "                  sched_pc[idx] = " << next_pc << "u;\n";
            out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kDisable) {
            auto it = block_end_pc.find(stmt.disable_target);
            if (it == block_end_pc.end()) {
              int disable_pid = -1;
              auto fork_it = fork_child_labels.find(proc.pid);
              if (fork_it != fork_child_labels.end()) {
                auto label_it = fork_it->second.find(stmt.disable_target);
                if (label_it != fork_it->second.end()) {
                  disable_pid = label_it->second;
                }
              }
              if (disable_pid < 0) {
                int parent_pid = proc_parent[proc.pid];
                if (parent_pid >= 0) {
                  auto parent_it = fork_child_labels.find(parent_pid);
                  if (parent_it != fork_child_labels.end()) {
                    auto label_it =
                        parent_it->second.find(stmt.disable_target);
                    if (label_it != parent_it->second.end()) {
                      disable_pid = label_it->second;
                    }
                  }
                }
              }
              if (disable_pid < 0) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              out << "                  {\n";
              out << "                    uint __gpga_didx = gpga_sched_index(gid, "
                  << disable_pid << "u);\n";
              out << "                    if (sched_state[__gpga_didx] != GPGA_SCHED_PROC_DONE) {\n";
              out << "                      sched_state[__gpga_didx] = GPGA_SCHED_PROC_DONE;\n";
              out << "                      uint parent = sched_parent[__gpga_didx];\n";
              out << "                      if (parent != GPGA_SCHED_NO_PARENT) {\n";
              out << "                        uint pidx = gpga_sched_index(gid, parent);\n";
              out << "                        if (sched_wait_kind[pidx] == GPGA_SCHED_WAIT_JOIN &&\n";
              out << "                            sched_wait_id[pidx] == sched_join_tag[__gpga_didx]) {\n";
              out << "                          if (sched_join_count[pidx] > 0u) {\n";
              out << "                            sched_join_count[pidx] -= 1u;\n";
              out << "                          }\n";
              out << "                          if (sched_join_count[pidx] == 0u) {\n";
              out << "                            sched_wait_kind[pidx] = GPGA_SCHED_WAIT_NONE;\n";
              out << "                            sched_state[pidx] = GPGA_SCHED_PROC_READY;\n";
              out << "                          }\n";
              out << "                        }\n";
              out << "                      }\n";
              out << "                    }\n";
              out << "                  }\n";
              out << "                  sched_pc[idx] = " << next_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            out << "                  sched_pc[idx] = " << it->second << "u;\n";
            out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kEventTrigger) {
            int event_id = -1;
            auto it = event_ids.find(stmt.trigger_target);
            if (it != event_ids.end()) {
              event_id = it->second;
            }
            if (event_id < 0) {
              out << "                  sched_error[gid] = 1u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            out << "                  sched_event_pending[(gid * "
                << "GPGA_SCHED_EVENT_COUNT) + " << event_id << "u] = 1u;\n";
            out << "                  sched_pc[idx] = " << next_pc << "u;\n";
            if (next_pc == pc_done) {
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            } else {
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            }
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kTaskCall) {
            emit_task_call(stmt, 18, emit_inline_stmt);
            out << "                  if (sched_state[idx] != GPGA_SCHED_PROC_DONE) {\n";
            out << "                    sched_pc[idx] = " << next_pc << "u;\n";
            out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            out << "                  }\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          emit_inline_stmt(stmt, 18, sched_locals, emit_inline_stmt);
          out << "                  if (sched_state[idx] != GPGA_SCHED_PROC_DONE) {\n";
          out << "                    sched_pc[idx] = " << next_pc << "u;\n";
          out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
          out << "                  }\n";
          out << "                  break;\n";
          out << "                }\n";
        }
        for (const auto& body_case : body_cases) {
          out << "                case " << body_case.pc << ": {\n";
          if (body_case.is_assign_delay) {
            out << "                  uint __gpga_delay_slot = (gid * "
                << "GPGA_SCHED_DELAY_COUNT) + " << body_case.delay_id << "u;\n";
            out << "                  ulong __gpga_dval = "
                << "sched_delay_val[__gpga_delay_slot];\n";
            out << "                  uint __gpga_didx_val = "
                << "sched_delay_index_val[__gpga_delay_slot];\n";
            emit_delay_assign_apply(
                std::to_string(body_case.delay_id) + "u", "__gpga_dval",
                "__gpga_didx_val", false, 18);
          } else {
            for (const auto* inner : body_case.body) {
              emit_inline_stmt(*inner, 18, sched_locals, emit_inline_stmt);
            }
          }
          int next_pc = body_case.is_forever_body ? body_case.loop_pc
                                                   : body_case.next_pc;
          out << "                  if (sched_state[idx] != GPGA_SCHED_PROC_DONE) {\n";
          out << "                    sched_pc[idx] = " << next_pc << "u;\n";
          if (next_pc == pc_done) {
            out << "                    sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          } else {
            out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
          }
          out << "                  }\n";
          out << "                  break;\n";
          out << "                }\n";
        }
        out << "                default: {\n";
        out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
        out << "                  break;\n";
        out << "                }\n";
        out << "              }\n";
        out << "              break;\n";
        out << "            }\n";
      }

      out << "            default: {\n";
      out << "              sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
      out << "              break;\n";
      out << "            }\n";
      out << "          }\n";
      out << "          if (sched_state[idx] == GPGA_SCHED_PROC_DONE) {\n";
      out << "            uint parent = sched_parent[idx];\n";
      out << "            if (parent != GPGA_SCHED_NO_PARENT) {\n";
      out << "              uint pidx = gpga_sched_index(gid, parent);\n";
      out << "              if (sched_wait_kind[pidx] == GPGA_SCHED_WAIT_JOIN &&\n";
      out << "                  sched_wait_id[pidx] == sched_join_tag[idx]) {\n";
      out << "                if (sched_join_count[pidx] > 0u) {\n";
      out << "                  sched_join_count[pidx] -= 1u;\n";
      out << "                }\n";
      out << "                if (sched_join_count[pidx] == 0u) {\n";
      out << "                  sched_wait_kind[pidx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "                  sched_state[pidx] = GPGA_SCHED_PROC_READY;\n";
      out << "                }\n";
      out << "              }\n";
      out << "            }\n";
      out << "          }\n";
      out << "        }\n";
      out << "      }\n";
      out << "      if (!did_work) {\n";
      out << "        bool any_ready = false;\n";
      out << "        for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
      out << "          uint idx = gpga_sched_index(gid, pid);\n";
      out << "          if (sched_state[idx] != GPGA_SCHED_PROC_BLOCKED) {\n";
      out << "            continue;\n";
      out << "          }\n";
      out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_DELTA) {\n";
      out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
      out << "            any_ready = true;\n";
      out << "            continue;\n";
      out << "          }\n";
      out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_EVENT) {\n";
      out << "            uint ev = sched_wait_event[idx];\n";
      out << "            uint eidx = (gid * GPGA_SCHED_EVENT_COUNT) + ev;\n";
      out << "            if (ev < GPGA_SCHED_EVENT_COUNT &&\n";
      out << "                sched_event_pending[eidx] != 0u) {\n";
      out << "              sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "              sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
      out << "              any_ready = true;\n";
      out << "            }\n";
      out << "            continue;\n";
      out << "          }\n";
      out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_EDGE) {\n";
      out << "            bool ready = false;\n";
      out << "            uint edge_kind = sched_wait_edge_kind[idx];\n";
      out << "            switch (sched_wait_id[idx]) {\n";
      for (size_t i = 0; i < edge_waits.size(); ++i) {
        const EdgeWaitInfo& info = edge_waits[i];
        out << "              case " << i << "u: {\n";
        if (!info.items.empty()) {
          out << "                uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_COUNT) + "
              << info.item_offset << "u;\n";
          out << "                bool __gpga_any = false;\n";
          for (size_t j = 0; j < info.items.size(); ++j) {
            int width = ExprWidth(*info.items[j].expr, module);
            std::string curr =
                EmitExprSized(*info.items[j].expr, width, module, sched_locals,
                              sched_regs);
            std::string mask = std::to_string(MaskForWidth64(width)) + "ul";
            out << "                ulong __gpga_prev_val = sched_edge_prev_val[__gpga_edge_base + "
                << j << "u];\n";
            out << "                ulong __gpga_curr_val = ((ulong)(" << curr
                << ")) & " << mask << ";\n";
            if (info.items[j].edge == EventEdgeKind::kAny) {
              out << "                if (__gpga_curr_val != __gpga_prev_val) { __gpga_any = true; }\n";
            } else {
              out << "                {\n";
              out << "                  ulong __gpga_prev_zero = (~__gpga_prev_val) & "
                  << mask << ";\n";
              out << "                  ulong __gpga_prev_one = __gpga_prev_val & " << mask
                  << ";\n";
              out << "                  ulong __gpga_curr_zero = (~__gpga_curr_val) & "
                  << mask << ";\n";
              out << "                  ulong __gpga_curr_one = __gpga_curr_val & " << mask
                  << ";\n";
              if (info.items[j].edge == EventEdgeKind::kPosedge) {
                out << "                  if ((__gpga_prev_zero & __gpga_curr_one) != 0ul) { __gpga_any = true; }\n";
              } else {
                out << "                  if ((__gpga_prev_one & __gpga_curr_zero) != 0ul) { __gpga_any = true; }\n";
              }
              out << "                }\n";
            }
            out << "                sched_edge_prev_val[__gpga_edge_base + " << j
                << "u] = __gpga_curr_val;\n";
          }
          out << "                ready = __gpga_any;\n";
        } else if (info.expr) {
          int width = ExprWidth(*info.expr, module);
          std::string curr =
              EmitExprSized(*info.expr, width, module, sched_locals, sched_regs);
          std::string mask = std::to_string(MaskForWidth64(width)) + "ul";
          out << "                uint __gpga_edge_idx = (gid * GPGA_SCHED_EDGE_COUNT) + "
              << info.item_offset << "u;\n";
          out << "                ulong __gpga_prev_val = sched_edge_prev_val[__gpga_edge_idx];\n";
          out << "                ulong __gpga_curr_val = ((ulong)(" << curr
              << ")) & " << mask << ";\n";
          out << "                if (edge_kind == GPGA_SCHED_EDGE_ANY) {\n";
          out << "                  ready = (__gpga_curr_val != __gpga_prev_val);\n";
          out << "                } else {\n";
          out << "                  ulong __gpga_prev_zero = (~__gpga_prev_val) & "
              << mask << ";\n";
          out << "                  ulong __gpga_prev_one = __gpga_prev_val & " << mask
              << ";\n";
          out << "                  ulong __gpga_curr_zero = (~__gpga_curr_val) & "
              << mask << ";\n";
          out << "                  ulong __gpga_curr_one = __gpga_curr_val & " << mask
              << ";\n";
          out << "                  if (edge_kind == GPGA_SCHED_EDGE_POSEDGE) {\n";
          out << "                    ready = ((__gpga_prev_zero & __gpga_curr_one) != 0ul);\n";
          out << "                  } else if (edge_kind == GPGA_SCHED_EDGE_NEGEDGE) {\n";
          out << "                    ready = ((__gpga_prev_one & __gpga_curr_zero) != 0ul);\n";
          out << "                  }\n";
          out << "                }\n";
          out << "                sched_edge_prev_val[__gpga_edge_idx] = __gpga_curr_val;\n";
        } else {
          out << "                uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + "
              << info.star_offset << "u;\n";
          out << "                bool __gpga_changed = false;\n";
          for (size_t s = 0; s < info.star_signals.size(); ++s) {
            Expr ident_expr;
            ident_expr.kind = ExprKind::kIdentifier;
            ident_expr.ident = info.star_signals[s];
            int width = ExprWidth(ident_expr, module);
            std::string curr =
                EmitExprSized(ident_expr, width, module, sched_locals,
                              sched_regs);
            std::string mask = std::to_string(MaskForWidth64(width)) + "ul";
            out << "                {\n";
            out << "                  ulong __gpga_curr_val = ((ulong)(" << curr
                << ")) & " << mask << ";\n";
            out << "                  ulong __gpga_prev_val = sched_edge_star_prev_val[__gpga_edge_base + "
                << s << "u];\n";
            out << "                  if (__gpga_curr_val != __gpga_prev_val) {\n";
            out << "                    __gpga_changed = true;\n";
            out << "                  }\n";
            out << "                  sched_edge_star_prev_val[__gpga_edge_base + "
                << s << "u] = __gpga_curr_val;\n";
            out << "                }\n";
          }
          out << "                ready = __gpga_changed;\n";
        }
        out << "                break;\n";
        out << "              }\n";
      }
      out << "              default:\n";
      out << "                ready = false;\n";
      out << "                break;\n";
      out << "            }\n";
      out << "            if (ready) {\n";
      out << "              sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "              sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
      out << "              any_ready = true;\n";
      out << "            }\n";
      out << "            continue;\n";
      out << "          }\n";
      out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_COND) {\n";
      out << "            bool ready = false;\n";
      out << "            switch (sched_wait_id[idx]) {\n";
      for (size_t i = 0; i < wait_exprs.size(); ++i) {
        std::string cond = EmitCondExpr(*wait_exprs[i], module, sched_locals,
                                        sched_regs);
        out << "              case " << i << "u:\n";
        out << "                ready = (" << cond << ");\n";
        out << "                break;\n";
      }
      out << "              default:\n";
      out << "                ready = false;\n";
      out << "                break;\n";
      out << "            }\n";
      out << "            if (ready) {\n";
      out << "              sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "              sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
      out << "              any_ready = true;\n";
      out << "            }\n";
      out << "            continue;\n";
      out << "          }\n";
      out << "        }\n";
      out << "        for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
      out << "          sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
      out << "        }\n";
      out << "        if (any_ready) {\n";
      out << "          sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
      out << "          continue;\n";
      out << "        }\n";
      out << "        sched_phase[gid] = GPGA_SCHED_PHASE_NBA;\n";
      out << "      }\n";
      out << "      continue;\n";
      out << "    }\n";
      out << "    if (sched_phase[gid] == GPGA_SCHED_PHASE_NBA) {\n";
      if (!nb_targets_sorted.empty()) {
        out << "      // Commit scalar NBAs.\n";
        for (const auto& target : nb_targets_sorted) {
          out << "      " << target << "[gid] = nb_" << target << "[gid];\n";
        }
      }
      if (!nb_array_nets.empty()) {
        out << "      // Commit array NBAs.\n";
        for (const auto* net : nb_array_nets) {
          out << "      for (uint i = 0u; i < " << net->array_size << "u; ++i) {\n";
          out << "        " << net->name << "[(gid * " << net->array_size
              << "u) + i] = " << net->name << "_next[(gid * "
              << net->array_size << "u) + i];\n";
          out << "      }\n";
        }
      }
      if (!system_task_info.monitor_stmts.empty()) {
        out << "      // Monitor change detection.\n";
        for (size_t i = 0; i < system_task_info.monitor_stmts.size(); ++i) {
          const Statement* monitor_stmt = system_task_info.monitor_stmts[i];
          std::string format_id_expr;
          std::vector<ServiceArg> args;
          build_service_args(*monitor_stmt, monitor_stmt->task_name,
                             &format_id_expr, &args);
          uint32_t monitor_pid_value = 0u;
          auto pid_it = monitor_pid.find(monitor_stmt);
          if (pid_it != monitor_pid.end()) {
            monitor_pid_value = pid_it->second;
          }
          std::string pid_expr = std::to_string(monitor_pid_value) + "u";
          out << "      if (sched_monitor_active[(gid * "
              << "GPGA_SCHED_MONITOR_COUNT) + " << i << "u] != 0u) {\n";
          std::string changed =
              emit_monitor_snapshot(static_cast<uint32_t>(i), args, 8, false);
          out << "        if (sched_monitor_enable[gid] != 0u && " << changed
              << ") {\n";
          emit_monitor_record(pid_expr, format_id_expr, args, 10);
          out << "        }\n";
          out << "      }\n";
        }
      }
      if (!system_task_info.strobe_stmts.empty()) {
        out << "      // Strobe emissions.\n";
        for (size_t i = 0; i < system_task_info.strobe_stmts.size(); ++i) {
          const Statement* strobe_stmt = system_task_info.strobe_stmts[i];
          std::string format_id_expr;
          std::vector<ServiceArg> args;
          build_service_args(*strobe_stmt, strobe_stmt->task_name,
                             &format_id_expr, &args);
          uint32_t strobe_pid_value = 0u;
          auto pid_it = strobe_pid.find(strobe_stmt);
          if (pid_it != strobe_pid.end()) {
            strobe_pid_value = pid_it->second;
          }
          std::string pid_expr = std::to_string(strobe_pid_value) + "u";
          out << "      uint __gpga_strobe_count = sched_strobe_pending[(gid * "
              << "GPGA_SCHED_STROBE_COUNT) + " << i << "u];\n";
          out << "      while (__gpga_strobe_count > 0u) {\n";
          emit_service_record_with_pid("GPGA_SERVICE_KIND_STROBE", pid_expr,
                                       format_id_expr, args, 8);
          out << "        __gpga_strobe_count -= 1u;\n";
          out << "      }\n";
          out << "      sched_strobe_pending[(gid * GPGA_SCHED_STROBE_COUNT) + "
              << i << "u] = 0u;\n";
        }
      }
      out << "      bool any_ready = false;\n";
      out << "      for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
      out << "        uint idx = gpga_sched_index(gid, pid);\n";
      out << "        if (sched_state[idx] != GPGA_SCHED_PROC_BLOCKED) {\n";
      out << "          continue;\n";
      out << "        }\n";
      out << "        if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_EVENT) {\n";
      out << "          uint ev = sched_wait_event[idx];\n";
      out << "          uint eidx = (gid * GPGA_SCHED_EVENT_COUNT) + ev;\n";
      out << "          if (ev < GPGA_SCHED_EVENT_COUNT &&\n";
      out << "              sched_event_pending[eidx] != 0u) {\n";
      out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
      out << "            any_ready = true;\n";
      out << "          }\n";
      out << "          continue;\n";
      out << "        }\n";
      out << "        if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_EDGE) {\n";
      out << "          bool ready = false;\n";
      out << "          uint edge_kind = sched_wait_edge_kind[idx];\n";
      out << "          switch (sched_wait_id[idx]) {\n";
      for (size_t i = 0; i < edge_waits.size(); ++i) {
        const EdgeWaitInfo& info = edge_waits[i];
        out << "            case " << i << "u: {\n";
        if (!info.items.empty()) {
          out << "              uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_COUNT) + "
              << info.item_offset << "u;\n";
          out << "              bool __gpga_any = false;\n";
          for (size_t j = 0; j < info.items.size(); ++j) {
            int width = ExprWidth(*info.items[j].expr, module);
            std::string curr =
                EmitExprSized(*info.items[j].expr, width, module, sched_locals,
                              sched_regs);
            std::string mask = std::to_string(MaskForWidth64(width)) + "ul";
            out << "              ulong __gpga_prev_val = sched_edge_prev_val[__gpga_edge_base + "
                << j << "u];\n";
            out << "              ulong __gpga_curr_val = ((ulong)(" << curr
                << ")) & " << mask << ";\n";
            if (info.items[j].edge == EventEdgeKind::kAny) {
              out << "              if (__gpga_curr_val != __gpga_prev_val) { __gpga_any = true; }\n";
            } else {
              out << "              {\n";
              out << "                ulong __gpga_prev_zero = (~__gpga_prev_val) & "
                  << mask << ";\n";
              out << "                ulong __gpga_prev_one = __gpga_prev_val & " << mask
                  << ";\n";
              out << "                ulong __gpga_curr_zero = (~__gpga_curr_val) & "
                  << mask << ";\n";
              out << "                ulong __gpga_curr_one = __gpga_curr_val & " << mask
                  << ";\n";
              if (info.items[j].edge == EventEdgeKind::kPosedge) {
                out << "                if ((__gpga_prev_zero & __gpga_curr_one) != 0ul) { __gpga_any = true; }\n";
              } else {
                out << "                if ((__gpga_prev_one & __gpga_curr_zero) != 0ul) { __gpga_any = true; }\n";
              }
              out << "              }\n";
            }
            out << "              sched_edge_prev_val[__gpga_edge_base + " << j
                << "u] = __gpga_curr_val;\n";
          }
          out << "              ready = __gpga_any;\n";
        } else if (info.expr) {
          int width = ExprWidth(*info.expr, module);
          std::string curr =
              EmitExprSized(*info.expr, width, module, sched_locals, sched_regs);
          std::string mask = std::to_string(MaskForWidth64(width)) + "ul";
          out << "              uint __gpga_edge_idx = (gid * GPGA_SCHED_EDGE_COUNT) + "
              << info.item_offset << "u;\n";
          out << "              ulong __gpga_prev_val = sched_edge_prev_val[__gpga_edge_idx];\n";
          out << "              ulong __gpga_curr_val = ((ulong)(" << curr
              << ")) & " << mask << ";\n";
          out << "              if (edge_kind == GPGA_SCHED_EDGE_ANY) {\n";
          out << "                ready = (__gpga_curr_val != __gpga_prev_val);\n";
          out << "              } else {\n";
          out << "                ulong __gpga_prev_zero = (~__gpga_prev_val) & "
              << mask << ";\n";
          out << "                ulong __gpga_prev_one = __gpga_prev_val & " << mask
              << ";\n";
          out << "                ulong __gpga_curr_zero = (~__gpga_curr_val) & "
              << mask << ";\n";
          out << "                ulong __gpga_curr_one = __gpga_curr_val & " << mask
              << ";\n";
          out << "                if (edge_kind == GPGA_SCHED_EDGE_POSEDGE) {\n";
          out << "                  ready = ((__gpga_prev_zero & __gpga_curr_one) != 0ul);\n";
          out << "                } else if (edge_kind == GPGA_SCHED_EDGE_NEGEDGE) {\n";
          out << "                  ready = ((__gpga_prev_one & __gpga_curr_zero) != 0ul);\n";
          out << "                }\n";
          out << "              }\n";
          out << "              sched_edge_prev_val[__gpga_edge_idx] = __gpga_curr_val;\n";
        } else {
          out << "              uint __gpga_edge_base = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + "
              << info.star_offset << "u;\n";
          out << "              bool __gpga_changed = false;\n";
          for (size_t s = 0; s < info.star_signals.size(); ++s) {
            Expr ident_expr;
            ident_expr.kind = ExprKind::kIdentifier;
            ident_expr.ident = info.star_signals[s];
            int width = ExprWidth(ident_expr, module);
            std::string curr =
                EmitExprSized(ident_expr, width, module, sched_locals,
                              sched_regs);
            std::string mask = std::to_string(MaskForWidth64(width)) + "ul";
            out << "              {\n";
            out << "                ulong __gpga_curr_val = ((ulong)(" << curr
                << ")) & " << mask << ";\n";
            out << "                ulong __gpga_prev_val = sched_edge_star_prev_val[__gpga_edge_base + "
                << s << "u];\n";
            out << "                if (__gpga_curr_val != __gpga_prev_val) {\n";
            out << "                  __gpga_changed = true;\n";
            out << "                }\n";
            out << "                sched_edge_star_prev_val[__gpga_edge_base + "
                << s << "u] = __gpga_curr_val;\n";
            out << "              }\n";
          }
          out << "              ready = __gpga_changed;\n";
        }
        out << "              break;\n";
        out << "            }\n";
      }
      out << "            default:\n";
      out << "              ready = false;\n";
      out << "              break;\n";
      out << "          }\n";
      out << "          if (ready) {\n";
      out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
      out << "            any_ready = true;\n";
      out << "          }\n";
      out << "          continue;\n";
      out << "        }\n";
      out << "        if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_COND) {\n";
      out << "          bool ready = false;\n";
      out << "          switch (sched_wait_id[idx]) {\n";
      for (size_t i = 0; i < wait_exprs.size(); ++i) {
        std::string cond =
            EmitExpr(*wait_exprs[i], module, sched_locals, sched_regs);
        out << "            case " << i << "u:\n";
        out << "              ready = ((" << cond << ") != 0u);\n";
        out << "              break;\n";
      }
      out << "            default:\n";
      out << "              ready = false;\n";
      out << "              break;\n";
      out << "          }\n";
      out << "          if (ready) {\n";
      out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
      out << "            any_ready = true;\n";
      out << "          }\n";
      out << "          continue;\n";
      out << "        }\n";
      out << "      }\n";
      out << "      for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
      out << "        sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
      out << "      }\n";
      out << "      if (any_ready) {\n";
      out << "        sched_active_init[gid] = 1u;\n";
      out << "        sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
      out << "        continue;\n";
      out << "      }\n";
      out << "      // Advance time to next wakeup.\n";
      out << "      bool have_time = false;\n";
      out << "      ulong next_time = ~0ul;\n";
      out << "      for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
      out << "        uint idx = gpga_sched_index(gid, pid);\n";
      out << "        if (sched_wait_kind[idx] != GPGA_SCHED_WAIT_TIME) {\n";
      out << "          continue;\n";
      out << "        }\n";
      out << "        ulong t = sched_wait_time[idx];\n";
      out << "        if (!have_time || t < next_time) {\n";
      out << "          have_time = true;\n";
      out << "          next_time = t;\n";
      out << "        }\n";
      out << "      }\n";
      out << "      if (have_time) {\n";
      out << "        sched_time[gid] = next_time;\n";
      out << "        __gpga_time = next_time;\n";
      out << "        for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
      out << "          uint idx = gpga_sched_index(gid, pid);\n";
      out << "          if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_TIME &&\n";
      out << "              sched_wait_time[idx] == next_time) {\n";
      out << "            sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
      out << "            sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
      out << "          }\n";
      out << "        }\n";
      out << "        sched_active_init[gid] = 1u;\n";
      out << "        sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
      out << "        continue;\n";
      out << "      }\n";
      out << "      finished = true;\n";
      out << "      break;\n";
      out << "    }\n";
      out << "  }\n";
    out << "  if (sched_error[gid] != 0u) {\n";
    out << "    sched_status[gid] = GPGA_SCHED_STATUS_ERROR;\n";
    out << "  } else if (finished) {\n";
    out << "    sched_status[gid] = GPGA_SCHED_STATUS_FINISHED;\n";
    out << "  } else if (stopped) {\n";
    out << "    sched_status[gid] = GPGA_SCHED_STATUS_STOPPED;\n";
    out << "  } else {\n";
    out << "    sched_status[gid] = GPGA_SCHED_STATUS_IDLE;\n";
    out << "  }\n";
      out << "}\n";
    }
  }
  return out.str();
}

}  // namespace gpga
