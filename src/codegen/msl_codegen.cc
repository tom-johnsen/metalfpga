#include "codegen/msl_codegen.hh"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "utils/msl_naming.hh"

namespace gpga {

namespace {

std::string MslName(const std::string& name) {
  return MslMangleIdentifier(name);
}

std::string MslNameWithSuffix(const std::string& name, const char* suffix) {
  std::string out = MslName(name);
  out += suffix;
  return out;
}

std::string MslNameNext(const std::string& name) {
  return MslNameWithSuffix(name, "_next");
}

std::string MslValName(const std::string& name) {
  return MslNameWithSuffix(name, "_val");
}

std::string MslXzName(const std::string& name) {
  return MslNameWithSuffix(name, "_xz");
}

std::string MslValNextName(const std::string& name) {
  return MslNameWithSuffix(name, "_next_val");
}

std::string MslXzNextName(const std::string& name) {
  return MslNameWithSuffix(name, "_next_xz");
}

std::string MslDecayName(const std::string& name) {
  return MslNameWithSuffix(name, "_decay_time");
}

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

SequentialAssign CloneSequentialAssign(const SequentialAssign& assign) {
  SequentialAssign out;
  out.lhs = assign.lhs;
  out.lhs_has_range = assign.lhs_has_range;
  out.lhs_indexed_range = assign.lhs_indexed_range;
  out.lhs_indexed_desc = assign.lhs_indexed_desc;
  out.lhs_indexed_width = assign.lhs_indexed_width;
  out.lhs_msb = assign.lhs_msb;
  out.lhs_lsb = assign.lhs_lsb;
  out.nonblocking = assign.nonblocking;
  if (assign.lhs_index) {
    out.lhs_index = CloneExpr(*assign.lhs_index);
  }
  for (const auto& idx : assign.lhs_indices) {
    out.lhs_indices.push_back(CloneExpr(*idx));
  }
  if (assign.lhs_msb_expr) {
    out.lhs_msb_expr = CloneExpr(*assign.lhs_msb_expr);
  }
  if (assign.lhs_lsb_expr) {
    out.lhs_lsb_expr = CloneExpr(*assign.lhs_lsb_expr);
  }
  if (assign.rhs) {
    out.rhs = CloneExpr(*assign.rhs);
  }
  if (assign.delay) {
    out.delay = CloneExpr(*assign.delay);
  }
  return out;
}

EventItem CloneEventItem(const EventItem& item) {
  EventItem out;
  out.edge = item.edge;
  if (item.expr) {
    out.expr = CloneExpr(*item.expr);
  }
  return out;
}

Statement CloneStatement(const Statement& stmt) {
  Statement out;
  out.kind = stmt.kind;
  out.case_kind = stmt.case_kind;
  out.assign = CloneSequentialAssign(stmt.assign);
  out.for_init_lhs = stmt.for_init_lhs;
  if (stmt.for_init_rhs) {
    out.for_init_rhs = CloneExpr(*stmt.for_init_rhs);
  }
  if (stmt.for_condition) {
    out.for_condition = CloneExpr(*stmt.for_condition);
  }
  out.for_step_lhs = stmt.for_step_lhs;
  if (stmt.for_step_rhs) {
    out.for_step_rhs = CloneExpr(*stmt.for_step_rhs);
  }
  for (const auto& inner : stmt.for_body) {
    out.for_body.push_back(CloneStatement(inner));
  }
  if (stmt.while_condition) {
    out.while_condition = CloneExpr(*stmt.while_condition);
  }
  for (const auto& inner : stmt.while_body) {
    out.while_body.push_back(CloneStatement(inner));
  }
  if (stmt.repeat_count) {
    out.repeat_count = CloneExpr(*stmt.repeat_count);
  }
  for (const auto& inner : stmt.repeat_body) {
    out.repeat_body.push_back(CloneStatement(inner));
  }
  if (stmt.delay) {
    out.delay = CloneExpr(*stmt.delay);
  }
  for (const auto& inner : stmt.delay_body) {
    out.delay_body.push_back(CloneStatement(inner));
  }
  out.event_edge = stmt.event_edge;
  if (stmt.event_expr) {
    out.event_expr = CloneExpr(*stmt.event_expr);
  }
  for (const auto& item : stmt.event_items) {
    out.event_items.push_back(CloneEventItem(item));
  }
  for (const auto& inner : stmt.event_body) {
    out.event_body.push_back(CloneStatement(inner));
  }
  if (stmt.wait_condition) {
    out.wait_condition = CloneExpr(*stmt.wait_condition);
  }
  for (const auto& inner : stmt.wait_body) {
    out.wait_body.push_back(CloneStatement(inner));
  }
  for (const auto& inner : stmt.forever_body) {
    out.forever_body.push_back(CloneStatement(inner));
  }
  for (const auto& inner : stmt.fork_branches) {
    out.fork_branches.push_back(CloneStatement(inner));
  }
  out.disable_target = stmt.disable_target;
  out.task_name = stmt.task_name;
  for (const auto& arg : stmt.task_args) {
    out.task_args.push_back(arg ? CloneExpr(*arg) : nullptr);
  }
  out.trigger_target = stmt.trigger_target;
  out.force_target = stmt.force_target;
  out.release_target = stmt.release_target;
  if (stmt.condition) {
    out.condition = CloneExpr(*stmt.condition);
  }
  for (const auto& inner : stmt.then_branch) {
    out.then_branch.push_back(CloneStatement(inner));
  }
  for (const auto& inner : stmt.else_branch) {
    out.else_branch.push_back(CloneStatement(inner));
  }
  for (const auto& inner : stmt.block) {
    out.block.push_back(CloneStatement(inner));
  }
  out.block_label = stmt.block_label;
  if (stmt.case_expr) {
    out.case_expr = CloneExpr(*stmt.case_expr);
  }
  for (const auto& item : stmt.case_items) {
    CaseItem cloned;
    for (const auto& label : item.labels) {
      cloned.labels.push_back(CloneExpr(*label));
    }
    for (const auto& inner : item.body) {
      cloned.body.push_back(CloneStatement(inner));
    }
    out.case_items.push_back(std::move(cloned));
  }
  for (const auto& inner : stmt.default_branch) {
    out.default_branch.push_back(CloneStatement(inner));
  }
  return out;
}

const std::unordered_map<std::string, int>* g_task_arg_widths = nullptr;
const std::unordered_map<std::string, bool>* g_task_arg_signed = nullptr;
const std::unordered_map<std::string, bool>* g_task_arg_real = nullptr;

int ExprWidth(const Expr& expr, const Module& module);

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
  for (const auto& param : module.parameters) {
    if (param.name == name) {
      return param.is_real;
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
      {
        std::string name = expr.ident;
        if (!name.empty() && name.front() == '$') {
          name = name.substr(1);
        }
        return name == "realtime" || name == "itor" ||
               name == "bitstoreal" || name == "log10" || name == "ln" ||
               name == "exp" || name == "sqrt" || name == "pow" ||
               name == "floor" || name == "ceil" || name == "sin" ||
               name == "cos" || name == "tan" || name == "asin" ||
               name == "acos" || name == "atan" || name == "atan2" ||
               name == "hypot" || name == "sinh" || name == "cosh" ||
               name == "tanh" || name == "asinh" || name == "acosh" ||
               name == "atanh";
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

bool GetArrayDims(const Module& module, const std::string& name,
                  std::vector<int>* dims, int* element_width,
                  int* array_size) {
  for (const auto& net : module.nets) {
    if (net.name != name ||
        (net.array_size <= 0 && net.array_dims.empty())) {
      continue;
    }
    if (element_width) {
      *element_width = net.width;
    }
    int size = net.array_size;
    if (dims) {
      dims->clear();
      dims->reserve(net.array_dims.size());
      for (const auto& dim : net.array_dims) {
        if (dim.size <= 0) {
          dims->clear();
          break;
        }
        dims->push_back(dim.size);
      }
    }
    if (size <= 0 && dims && !dims->empty()) {
      int64_t product = 1;
      for (int dim : *dims) {
        if (dim <= 0 || product > (0x7FFFFFFF / dim)) {
          product = 0;
          break;
        }
        product *= dim;
      }
      size = static_cast<int>(product);
    }
    if (dims && dims->empty() && size > 0) {
      dims->push_back(size);
    }
    if (array_size) {
      *array_size = size;
    }
    return size > 0;
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

std::string MaskForWidthExpr(const std::string& expr, int width);
uint64_t StringLiteralBitsForWidth(const std::string& value, int width);

uint8_t StringPadByte() {
  static int cached = -1;
  if (cached >= 0) {
    return static_cast<uint8_t>(cached);
  }
  uint8_t pad = 0u;
  if (const char* env = std::getenv("METALFPGA_STRING_PAD")) {
    std::string lowered;
    lowered.reserve(std::strlen(env));
    for (const char* p = env; *p != '\0'; ++p) {
      lowered.push_back(
          static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }
    if (lowered == "space") {
      pad = 0x20u;
    }
  }
  cached = static_cast<int>(pad);
  return pad;
}

uint64_t StringLiteralBits(const std::string& value) {
  int width = static_cast<int>(value.size() * 8);
  if (width <= 0) {
    width = 1;
  }
  if (width > 64) {
    width = 64;
  }
  return StringLiteralBitsForWidth(value, width);
}

std::vector<uint64_t> StringLiteralWords(const std::string& value, int width) {
  if (width <= 0) {
    width = 1;
  }
  size_t max_bytes = static_cast<size_t>((width + 7) / 8);
  size_t word_count = std::max<size_t>(1u, (max_bytes + 7u) / 8u);
  size_t byte_count = word_count * 8u;
  uint8_t pad = StringPadByte();
  std::vector<uint8_t> bytes(byte_count, pad);
  size_t usable_start = byte_count > max_bytes ? byte_count - max_bytes : 0u;
  size_t count = std::min(value.size(), max_bytes);
  size_t src_start = value.size() > count ? (value.size() - count) : 0u;
  size_t dest_start = usable_start + (max_bytes - count);
  for (size_t i = 0; i < count; ++i) {
    bytes[dest_start + i] = static_cast<uint8_t>(value[src_start + i]);
  }
  std::vector<uint64_t> words(word_count, 0ull);
  for (size_t word_index = 0; word_index < word_count; ++word_index) {
    size_t byte_base = byte_count - (word_index + 1u) * 8u;
    uint64_t word = 0;
    for (size_t b = 0; b < 8u; ++b) {
      word |= static_cast<uint64_t>(bytes[byte_base + b]) << (8u * (7u - b));
    }
    words[word_index] = word;
  }
  return words;
}

uint64_t StringLiteralBitsForWidth(const std::string& value, int width) {
  if (width <= 0) {
    return 0ull;
  }
  if (width > 64) {
    width = 64;
  }
  std::vector<uint64_t> words = StringLiteralWords(value, width);
  return words.empty() ? 0ull : words[0];
}

std::string WideLiteralExpr(const std::string& value, int width) {
  std::vector<uint64_t> words = StringLiteralWords(value, width);
  if (words.empty()) {
    return "gpga_wide_zero_" + std::to_string(width) + "()";
  }
  std::string expr = "gpga_wide_from_u64_" + std::to_string(width) + "(" +
                     std::to_string(words[0]) + "ul)";
  for (size_t i = 1; i < words.size(); ++i) {
    expr = "gpga_wide_set_word_" + std::to_string(width) + "(" + expr + ", " +
           std::to_string(i) + "u, " + std::to_string(words[i]) + "ul)";
  }
  return expr;
}

std::string StringLiteralExpr(const std::string& value, int width) {
  if (width <= 0) {
    width = 1;
  }
  if (width > 64) {
    return WideLiteralExpr(value, width);
  }
  uint64_t bits = StringLiteralBitsForWidth(value, width);
  std::string literal = (bits > 0xFFFFFFFFull)
                            ? std::to_string(bits) + "ul"
                            : std::to_string(bits) + "u";
  return MaskForWidthExpr(literal, width);
}

std::string TypeForWidth(int width) {
  if (width > 64) {
    return "GpgaWide" + std::to_string(width);
  }
  return (width > 32) ? "ulong" : "uint";
}

std::string SignedTypeForWidth(int width) {
  return (width > 32) ? "long" : "int";
}

std::string ZeroForWidth(int width) {
  if (width > 64) {
    return "gpga_wide_zero_" + std::to_string(width) + "()";
  }
  return (width > 32) ? "0ul" : "0u";
}

std::string CastForWidth(int width) {
  if (width > 64) {
    return "";
  }
  return (width > 32) ? "(ulong)" : "";
}

std::string SignedCastForWidth(int width) {
  if (width > 64) {
    return "";
  }
  return (width > 32) ? "(long)" : "(int)";
}

std::string UnsignedCastForWidth(int width) {
  if (width > 64) {
    return "";
  }
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
  if (width > 64) {
    return "gpga_wide_mask_" + std::to_string(width) + "(" + expr + ")";
  }
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
  if (width > 64) {
    return "gpga_wide_mask_const_" + std::to_string(width) + "()";
  }
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
  if (target_width > 64) {
    if (expr_width > 64) {
      if (expr_width == target_width) {
        return masked;
      }
      return "gpga_wide_resize_" + std::to_string(target_width) + "_from_" +
             std::to_string(expr_width) + "(" + masked + ")";
    }
    return "gpga_wide_from_u64_" + std::to_string(target_width) + "(" + masked +
           ")";
  }
  if (expr_width > 64) {
    std::string low =
        "gpga_wide_to_u64_" + std::to_string(expr_width) + "(" + masked + ")";
    if (target_width <= 32) {
      return "(uint)" + MaskForWidthExpr(low, target_width);
    }
    return MaskForWidthExpr(low, target_width);
  }
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
  if (target_width > 64) {
    if (expr_width > 64) {
      if (expr_width == target_width) {
        return expr;
      }
      if (expr_width > target_width) {
        return "gpga_wide_resize_" + std::to_string(target_width) + "_from_" +
               std::to_string(expr_width) + "(" + expr + ")";
      }
      return "gpga_wide_sext_" + std::to_string(target_width) + "_from_" +
             std::to_string(expr_width) + "(" + expr + ")";
    }
    return "gpga_wide_sext_from_u64_" + std::to_string(target_width) + "(" +
           expr + ", " + std::to_string(expr_width) + "u)";
  }
  if (expr_width > 64) {
    std::string low =
        "gpga_wide_to_u64_" + std::to_string(expr_width) + "(" + expr + ")";
    std::string masked = MaskForWidthExpr(low, std::min(expr_width, 64));
    int width = std::max(std::min(expr_width, 64), target_width);
    int shift = width - std::min(expr_width, 64);
    std::string cast = SignedCastForWidth(width);
    if (shift == 0) {
      return cast + masked;
    }
    std::string widened = cast + masked;
    return "(" + cast + "(" + widened + " << " + std::to_string(shift) +
           "u) >> " + std::to_string(shift) + "u)";
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

bool IsFileSystemFunctionName(const std::string& name) {
  return name == "$fopen" || name == "$fclose" || name == "$fgetc" ||
         name == "$fgets" || name == "$feof" || name == "$fscanf" ||
         name == "$sscanf" || name == "$ftell" || name == "$fseek" ||
         name == "$ferror" || name == "$ungetc" || name == "$fread" ||
         name == "$rewind" || name == "$test$plusargs" ||
         name == "$value$plusargs";
}

bool ExprHasFileSystemCall(const Expr& expr) {
  if (expr.kind == ExprKind::kCall) {
    if (IsFileSystemFunctionName(expr.ident)) {
      return true;
    }
    for (const auto& arg : expr.call_args) {
      if (arg && ExprHasFileSystemCall(*arg)) {
        return true;
      }
    }
    return false;
  }
  switch (expr.kind) {
    case ExprKind::kUnary:
      return expr.operand && ExprHasFileSystemCall(*expr.operand);
    case ExprKind::kBinary:
      return (expr.lhs && ExprHasFileSystemCall(*expr.lhs)) ||
             (expr.rhs && ExprHasFileSystemCall(*expr.rhs));
    case ExprKind::kTernary:
      return (expr.condition && ExprHasFileSystemCall(*expr.condition)) ||
             (expr.then_expr && ExprHasFileSystemCall(*expr.then_expr)) ||
             (expr.else_expr && ExprHasFileSystemCall(*expr.else_expr));
    case ExprKind::kSelect:
      return (expr.base && ExprHasFileSystemCall(*expr.base)) ||
             (expr.msb_expr && ExprHasFileSystemCall(*expr.msb_expr)) ||
             (expr.lsb_expr && ExprHasFileSystemCall(*expr.lsb_expr));
    case ExprKind::kIndex:
      return (expr.base && ExprHasFileSystemCall(*expr.base)) ||
             (expr.index && ExprHasFileSystemCall(*expr.index));
    case ExprKind::kConcat:
      if (expr.repeat_expr && ExprHasFileSystemCall(*expr.repeat_expr)) {
        return true;
      }
      for (const auto& element : expr.elements) {
        if (element && ExprHasFileSystemCall(*element)) {
          return true;
        }
      }
      return false;
    case ExprKind::kIdentifier:
    case ExprKind::kNumber:
    case ExprKind::kString:
      return false;
    case ExprKind::kCall:
      return false;
  }
  return false;
}

bool StatementHasFileSystemCall(const Statement& statement) {
  switch (statement.kind) {
    case StatementKind::kAssign: {
      const auto& assign = statement.assign;
      if (assign.lhs_index && ExprHasFileSystemCall(*assign.lhs_index)) {
        return true;
      }
      for (const auto& idx : assign.lhs_indices) {
        if (idx && ExprHasFileSystemCall(*idx)) {
          return true;
        }
      }
      if (assign.lhs_msb_expr && ExprHasFileSystemCall(*assign.lhs_msb_expr)) {
        return true;
      }
      if (assign.lhs_lsb_expr && ExprHasFileSystemCall(*assign.lhs_lsb_expr)) {
        return true;
      }
      if (assign.rhs && ExprHasFileSystemCall(*assign.rhs)) {
        return true;
      }
      if (assign.delay && ExprHasFileSystemCall(*assign.delay)) {
        return true;
      }
      return false;
    }
    case StatementKind::kIf:
      if (statement.condition &&
          ExprHasFileSystemCall(*statement.condition)) {
        return true;
      }
      for (const auto& stmt : statement.then_branch) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      for (const auto& stmt : statement.else_branch) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kBlock:
      for (const auto& stmt : statement.block) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kCase:
      if (statement.case_expr &&
          ExprHasFileSystemCall(*statement.case_expr)) {
        return true;
      }
      for (const auto& item : statement.case_items) {
        for (const auto& label : item.labels) {
          if (label && ExprHasFileSystemCall(*label)) {
            return true;
          }
        }
        for (const auto& stmt : item.body) {
          if (StatementHasFileSystemCall(stmt)) {
            return true;
          }
        }
      }
      for (const auto& stmt : statement.default_branch) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kFor:
      if (statement.for_init_rhs &&
          ExprHasFileSystemCall(*statement.for_init_rhs)) {
        return true;
      }
      if (statement.for_condition &&
          ExprHasFileSystemCall(*statement.for_condition)) {
        return true;
      }
      if (statement.for_step_rhs &&
          ExprHasFileSystemCall(*statement.for_step_rhs)) {
        return true;
      }
      for (const auto& stmt : statement.for_body) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kWhile:
      if (statement.while_condition &&
          ExprHasFileSystemCall(*statement.while_condition)) {
        return true;
      }
      for (const auto& stmt : statement.while_body) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kRepeat:
      if (statement.repeat_count &&
          ExprHasFileSystemCall(*statement.repeat_count)) {
        return true;
      }
      for (const auto& stmt : statement.repeat_body) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kDelay:
      if (statement.delay && ExprHasFileSystemCall(*statement.delay)) {
        return true;
      }
      for (const auto& stmt : statement.delay_body) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kEventControl:
      if (statement.event_expr &&
          ExprHasFileSystemCall(*statement.event_expr)) {
        return true;
      }
      for (const auto& stmt : statement.event_body) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kWait:
      if (statement.wait_condition &&
          ExprHasFileSystemCall(*statement.wait_condition)) {
        return true;
      }
      for (const auto& stmt : statement.wait_body) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kForever:
      for (const auto& stmt : statement.forever_body) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kFork:
      for (const auto& stmt : statement.fork_branches) {
        if (StatementHasFileSystemCall(stmt)) {
          return true;
        }
      }
      return false;
    case StatementKind::kTaskCall:
      for (const auto& arg : statement.task_args) {
        if (arg && ExprHasFileSystemCall(*arg)) {
          return true;
        }
      }
      return false;
    case StatementKind::kEventTrigger:
    case StatementKind::kDisable:
    case StatementKind::kForce:
    case StatementKind::kRelease:
      return false;
  }
  return false;
}

bool ExtractFeofCondition(const Expr& expr, const Expr** fd_expr,
                          bool* invert) {
  if (expr.kind == ExprKind::kCall && expr.ident == "$feof") {
    if (fd_expr) {
      *fd_expr = (expr.call_args.empty() ? nullptr : expr.call_args[0].get());
    }
    if (invert) {
      *invert = false;
    }
    return true;
  }
  if (expr.kind == ExprKind::kUnary && expr.unary_op == '!' && expr.operand &&
      expr.operand->kind == ExprKind::kCall &&
      expr.operand->ident == "$feof") {
    if (fd_expr) {
      *fd_expr = (expr.operand->call_args.empty()
                      ? nullptr
                      : expr.operand->call_args[0].get());
    }
    if (invert) {
      *invert = true;
    }
    return true;
  }
  return false;
}

bool ExtractPlusargsCondition(const Expr& expr, const Expr** call_expr,
                              bool* invert) {
  if (expr.kind == ExprKind::kCall &&
      (expr.ident == "$test$plusargs" || expr.ident == "$value$plusargs")) {
    if (call_expr) {
      *call_expr = &expr;
    }
    if (invert) {
      *invert = false;
    }
    return true;
  }
  if (expr.kind == ExprKind::kUnary && expr.unary_op == '!' && expr.operand &&
      expr.operand->kind == ExprKind::kCall &&
      (expr.operand->ident == "$test$plusargs" ||
       expr.operand->ident == "$value$plusargs")) {
    if (call_expr) {
      *call_expr = expr.operand.get();
    }
    if (invert) {
      *invert = true;
    }
    return true;
  }
  return false;
}

bool TaskTreatsIdentifierAsString(const std::string& name) {
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

void CollectSystemFunctionExpr(const Expr& expr, SystemTaskInfo* info) {
  if (!info) {
    return;
  }
  if (expr.kind == ExprKind::kCall) {
    if (IsFileSystemFunctionName(expr.ident)) {
      info->has_system_tasks = true;
      info->max_args = std::max(info->max_args, expr.call_args.size());
      for (size_t i = 0; i < expr.call_args.size(); ++i) {
        const Expr* arg = expr.call_args[i].get();
        if (!arg) {
          continue;
        }
        if (arg->kind == ExprKind::kString) {
          AddSystemTaskString(info, arg->string_value);
          continue;
        }
        if (arg->kind == ExprKind::kIdentifier) {
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
            AddSystemTaskString(info, arg->ident);
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
  if (expr.kind == ExprKind::kUnary && expr.operand) {
    CollectSystemFunctionExpr(*expr.operand, info);
    return;
  }
  if (expr.kind == ExprKind::kBinary) {
    if (expr.lhs) {
      CollectSystemFunctionExpr(*expr.lhs, info);
    }
    if (expr.rhs) {
      CollectSystemFunctionExpr(*expr.rhs, info);
    }
    return;
  }
  if (expr.kind == ExprKind::kTernary) {
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
  if (expr.kind == ExprKind::kSelect) {
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
  if (expr.kind == ExprKind::kIndex) {
    if (expr.base) {
      CollectSystemFunctionExpr(*expr.base, info);
    }
    if (expr.index) {
      CollectSystemFunctionExpr(*expr.index, info);
    }
    return;
  }
  if (expr.kind == ExprKind::kConcat) {
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

void CollectSystemFunctionInfo(const Statement& stmt, SystemTaskInfo* info) {
  if (!info) {
    return;
  }
  if (stmt.kind == StatementKind::kAssign) {
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
  if (stmt.kind == StatementKind::kIf) {
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
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kCase) {
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
  if (stmt.kind == StatementKind::kFor) {
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
  if (stmt.kind == StatementKind::kWhile) {
    if (stmt.while_condition) {
      CollectSystemFunctionExpr(*stmt.while_condition, info);
    }
    for (const auto& inner : stmt.while_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kRepeat) {
    if (stmt.repeat_count) {
      CollectSystemFunctionExpr(*stmt.repeat_count, info);
    }
    for (const auto& inner : stmt.repeat_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kDelay) {
    if (stmt.delay) {
      CollectSystemFunctionExpr(*stmt.delay, info);
    }
    for (const auto& inner : stmt.delay_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kEventControl) {
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
  if (stmt.kind == StatementKind::kWait) {
    if (stmt.wait_condition) {
      CollectSystemFunctionExpr(*stmt.wait_condition, info);
    }
    for (const auto& inner : stmt.wait_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectSystemFunctionInfo(inner, info);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      CollectSystemFunctionInfo(inner, info);
    }
  }
}

void CollectWideWidthsExpr(const Expr& expr, const Module& module,
                            std::unordered_set<int>* widths) {
  if (!widths) {
    return;
  }
  int width = ExprWidth(expr, module);
  if (width > 64) {
    widths->insert(width);
  }
  switch (expr.kind) {
    case ExprKind::kIdentifier:
    case ExprKind::kNumber:
    case ExprKind::kString:
      return;
    case ExprKind::kUnary:
      if (expr.operand) {
        CollectWideWidthsExpr(*expr.operand, module, widths);
      }
      return;
    case ExprKind::kBinary:
      if (expr.lhs) {
        CollectWideWidthsExpr(*expr.lhs, module, widths);
      }
      if (expr.rhs) {
        CollectWideWidthsExpr(*expr.rhs, module, widths);
      }
      return;
    case ExprKind::kTernary:
      if (expr.condition) {
        CollectWideWidthsExpr(*expr.condition, module, widths);
      }
      if (expr.then_expr) {
        CollectWideWidthsExpr(*expr.then_expr, module, widths);
      }
      if (expr.else_expr) {
        CollectWideWidthsExpr(*expr.else_expr, module, widths);
      }
      return;
    case ExprKind::kSelect:
      if (expr.base) {
        CollectWideWidthsExpr(*expr.base, module, widths);
      }
      if (expr.msb_expr) {
        CollectWideWidthsExpr(*expr.msb_expr, module, widths);
      }
      if (expr.lsb_expr) {
        CollectWideWidthsExpr(*expr.lsb_expr, module, widths);
      }
      return;
    case ExprKind::kIndex:
      if (expr.base) {
        CollectWideWidthsExpr(*expr.base, module, widths);
      }
      if (expr.index) {
        CollectWideWidthsExpr(*expr.index, module, widths);
      }
      return;
    case ExprKind::kCall:
      for (const auto& arg : expr.call_args) {
        if (arg) {
          CollectWideWidthsExpr(*arg, module, widths);
        }
      }
      return;
    case ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        if (element) {
          CollectWideWidthsExpr(*element, module, widths);
        }
      }
      if (expr.repeat_expr) {
        CollectWideWidthsExpr(*expr.repeat_expr, module, widths);
      }
      return;
  }
}

void CollectWideWidthsInfo(const Statement& stmt, const Module& module,
                           std::unordered_set<int>* widths) {
  if (!widths) {
    return;
  }
  if (stmt.kind == StatementKind::kAssign ||
      stmt.kind == StatementKind::kForce ||
      stmt.kind == StatementKind::kRelease) {
    if (stmt.assign.rhs) {
      CollectWideWidthsExpr(*stmt.assign.rhs, module, widths);
    }
    if (stmt.assign.lhs_index) {
      CollectWideWidthsExpr(*stmt.assign.lhs_index, module, widths);
    }
    for (const auto& index : stmt.assign.lhs_indices) {
      if (index) {
        CollectWideWidthsExpr(*index, module, widths);
      }
    }
    if (stmt.assign.lhs_msb_expr) {
      CollectWideWidthsExpr(*stmt.assign.lhs_msb_expr, module, widths);
    }
    if (stmt.assign.lhs_lsb_expr) {
      CollectWideWidthsExpr(*stmt.assign.lhs_lsb_expr, module, widths);
    }
    if (stmt.assign.delay) {
      CollectWideWidthsExpr(*stmt.assign.delay, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kIf) {
    if (stmt.condition) {
      CollectWideWidthsExpr(*stmt.condition, module, widths);
    }
    for (const auto& inner : stmt.then_branch) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    for (const auto& inner : stmt.else_branch) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kCase) {
    if (stmt.case_expr) {
      CollectWideWidthsExpr(*stmt.case_expr, module, widths);
    }
    for (const auto& item : stmt.case_items) {
      for (const auto& label : item.labels) {
        if (label) {
          CollectWideWidthsExpr(*label, module, widths);
        }
      }
      for (const auto& inner : item.body) {
        CollectWideWidthsInfo(inner, module, widths);
      }
    }
    for (const auto& inner : stmt.default_branch) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFor) {
    if (stmt.for_init_rhs) {
      CollectWideWidthsExpr(*stmt.for_init_rhs, module, widths);
    }
    if (stmt.for_condition) {
      CollectWideWidthsExpr(*stmt.for_condition, module, widths);
    }
    if (stmt.for_step_rhs) {
      CollectWideWidthsExpr(*stmt.for_step_rhs, module, widths);
    }
    for (const auto& inner : stmt.for_body) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kWhile) {
    if (stmt.while_condition) {
      CollectWideWidthsExpr(*stmt.while_condition, module, widths);
    }
    for (const auto& inner : stmt.while_body) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kRepeat) {
    if (stmt.repeat_count) {
      CollectWideWidthsExpr(*stmt.repeat_count, module, widths);
    }
    for (const auto& inner : stmt.repeat_body) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kDelay) {
    if (stmt.delay) {
      CollectWideWidthsExpr(*stmt.delay, module, widths);
    }
    for (const auto& inner : stmt.delay_body) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kEventControl) {
    if (stmt.event_expr) {
      CollectWideWidthsExpr(*stmt.event_expr, module, widths);
    }
    for (const auto& item : stmt.event_items) {
      if (item.expr) {
        CollectWideWidthsExpr(*item.expr, module, widths);
      }
    }
    for (const auto& inner : stmt.event_body) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kWait) {
    if (stmt.wait_condition) {
      CollectWideWidthsExpr(*stmt.wait_condition, module, widths);
    }
    for (const auto& inner : stmt.wait_body) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectWideWidthsInfo(inner, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFork) {
    for (const auto& branch : stmt.fork_branches) {
      CollectWideWidthsInfo(branch, module, widths);
    }
    return;
  }
  if (stmt.kind == StatementKind::kTaskCall) {
    for (const auto& arg : stmt.task_args) {
      if (arg) {
        CollectWideWidthsExpr(*arg, module, widths);
      }
    }
    return;
  }
  if (stmt.kind == StatementKind::kForce ||
      stmt.kind == StatementKind::kRelease) {
    if (stmt.assign.rhs) {
      CollectWideWidthsExpr(*stmt.assign.rhs, module, widths);
    }
    return;
  }
}

void CollectServiceArgWidthsInfo(const Statement& stmt, const Module& module,
                                 int* max_width) {
  if (!max_width) {
    return;
  }
  if (stmt.kind == StatementKind::kTaskCall &&
      IsSystemTaskName(stmt.task_name)) {
    for (const auto& arg : stmt.task_args) {
      if (!arg || arg->kind == ExprKind::kString) {
        continue;
      }
      bool is_real = ExprIsRealValue(*arg, module);
      int width = is_real ? 64 : ExprWidth(*arg, module);
      if (width > *max_width) {
        *max_width = width;
      }
    }
  }
  if (stmt.kind == StatementKind::kIf) {
    for (const auto& inner : stmt.then_branch) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    for (const auto& inner : stmt.else_branch) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kCase) {
    for (const auto& item : stmt.case_items) {
      for (const auto& inner : item.body) {
        CollectServiceArgWidthsInfo(inner, module, max_width);
      }
    }
    for (const auto& inner : stmt.default_branch) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFor) {
    for (const auto& inner : stmt.for_body) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kWhile) {
    for (const auto& inner : stmt.while_body) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kRepeat) {
    for (const auto& inner : stmt.repeat_body) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kDelay) {
    for (const auto& inner : stmt.delay_body) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kEventControl) {
    for (const auto& inner : stmt.event_body) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kWait) {
    for (const auto& inner : stmt.wait_body) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectServiceArgWidthsInfo(inner, module, max_width);
    }
    return;
  }
  if (stmt.kind == StatementKind::kFork) {
    for (const auto& branch : stmt.fork_branches) {
      CollectServiceArgWidthsInfo(branch, module, max_width);
    }
    return;
  }
}

uint32_t CollectServiceWideWordCount(const Module& module) {
  int max_width = 0;
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectServiceArgWidthsInfo(stmt, module, &max_width);
    }
  }
  for (const auto& func : module.functions) {
    for (const auto& stmt : func.body) {
      CollectServiceArgWidthsInfo(stmt, module, &max_width);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      CollectServiceArgWidthsInfo(stmt, module, &max_width);
    }
  }
  if (max_width <= 64) {
    return 0u;
  }
  return static_cast<uint32_t>((max_width + 63) / 64);
}

std::vector<int> CollectWideWidths(const Module& module) {
  std::unordered_set<int> widths;
  for (const auto& port : module.ports) {
    if (port.width > 64) {
      widths.insert(port.width);
    }
  }
  for (const auto& net : module.nets) {
    if (net.width > 64) {
      widths.insert(net.width);
    }
  }
  for (const auto& param : module.parameters) {
    if (param.value) {
      CollectWideWidthsExpr(*param.value, module, &widths);
    }
  }
  for (const auto& assign : module.assigns) {
    if (assign.rhs) {
      CollectWideWidthsExpr(*assign.rhs, module, &widths);
    }
  }
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectWideWidthsInfo(stmt, module, &widths);
    }
  }
  for (const auto& func : module.functions) {
    if (func.body_expr) {
      CollectWideWidthsExpr(*func.body_expr, module, &widths);
    }
    for (const auto& stmt : func.body) {
      CollectWideWidthsInfo(stmt, module, &widths);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      CollectWideWidthsInfo(stmt, module, &widths);
    }
  }
  std::vector<int> result(widths.begin(), widths.end());
  std::sort(result.begin(), result.end());
  return result;
}

void CollectSystemTaskInfo(const Statement& stmt, SystemTaskInfo* info) {
  if (!info) {
    return;
  }
  if (stmt.kind == StatementKind::kTaskCall &&
      IsSystemTaskName(stmt.task_name)) {
    info->has_system_tasks = true;
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
        stmt.task_args[format_arg_start]->kind == ExprKind::kString;
    if (has_format_specs) {
      format_specs =
          ExtractFormatSpecs(stmt.task_args[format_arg_start]->string_value);
    }
    size_t format_arg_index = 0;
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
    if (stmt.task_name == "$sformat" && !stmt.task_args.empty() &&
        stmt.task_args[0] &&
        stmt.task_args[0]->kind == ExprKind::kIdentifier) {
      AddSystemTaskString(info, stmt.task_args[0]->ident);
    }
    bool ident_as_string = TaskTreatsIdentifierAsString(stmt.task_name);
    for (size_t i = 0; i < stmt.task_args.size(); ++i) {
      const auto& arg = stmt.task_args[i];
      if (!arg) {
        continue;
      }
      bool is_format_literal = has_format_specs && i == format_arg_start &&
                               arg->kind == ExprKind::kString;
      if (arg->kind == ExprKind::kString) {
        AddSystemTaskString(info, arg->string_value);
      } else if (ident_as_string && arg->kind == ExprKind::kIdentifier) {
        AddSystemTaskString(info, arg->ident);
      } else if (has_format_specs && !is_format_literal &&
                 format_arg_index < format_specs.size() &&
                 format_specs[format_arg_index] == 's' &&
                 arg->kind == ExprKind::kIdentifier) {
        AddSystemTaskString(info, arg->ident);
      }
      if (has_format_specs && !is_format_literal) {
        ++format_arg_index;
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
      CollectSystemFunctionInfo(stmt, &info);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      CollectSystemTaskInfo(stmt, &info);
      CollectSystemFunctionInfo(stmt, &info);
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
      return std::max<int>(
          1, static_cast<int>(expr.string_value.size() * 8));
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
      if (expr.ident == "$stime") {
        return 32;
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
      int repeats = std::max(0, expr.repeat);
      total = base * repeats;
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
  if (expr.kind == ExprKind::kString && expr_width < target_width) {
    return StringLiteralExpr(expr.string_value, target_width);
  }
  if (expr_width > 64 && target_width <= 64) {
    std::string low =
        "gpga_wide_to_u64_" + std::to_string(expr_width) + "(" + raw + ")";
    return MaskForWidthExpr(low, target_width);
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
    if (width > 32) {
      if (signed_expr) {
        return "gpga_double_from_s64((long)(" + raw + "))";
      }
      return "gpga_double_from_u64((ulong)(" + raw + "))";
    }
    if (signed_expr) {
      return "gpga_double_from_s32((int)(" + raw + "))";
    }
    return "gpga_double_from_u32((uint)(" + raw + "))";
  };

  switch (expr.kind) {
    case ExprKind::kIdentifier: {
      const Port* port = FindPort(module, expr.ident);
      if (SignalIsReal(module, expr.ident)) {
        if (port) {
          return "gpga_bits_to_real(" + MslName(port->name) + "[gid])";
        }
        if (regs.count(expr.ident) > 0) {
          return "gpga_bits_to_real(" + MslName(expr.ident) + "[gid])";
        }
        if (locals.count(expr.ident) > 0) {
          return "gpga_bits_to_real(" + MslName(expr.ident) + ")";
        }
        return "gpga_bits_to_real(" + MslName(expr.ident) + ")";
      }
      return emit_int_as_real(expr);
    }
    case ExprKind::kNumber:
      if (IsRealLiteralExpr(expr)) {
        return "gpga_bits_to_real(" + std::to_string(expr.value_bits) + "ul)";
      }
      return emit_int_as_real(expr);
    case ExprKind::kString:
      return "gpga_bits_to_real(0ul)";
    case ExprKind::kUnary: {
      std::string operand = expr.operand
                                ? EmitRealValueExpr(*expr.operand, module,
                                                    locals, regs)
                                : "gpga_bits_to_real(0ul)";
      if (expr.unary_op == '+') {
        return operand;
      }
      if (expr.unary_op == '-') {
        return "gpga_double_neg(" + operand + ")";
      }
      return "gpga_bits_to_real(0ul)";
    }
    case ExprKind::kBinary: {
      std::string lhs = expr.lhs ? EmitRealValueExpr(*expr.lhs, module, locals,
                                                     regs)
                                 : "gpga_bits_to_real(0ul)";
      std::string rhs = expr.rhs ? EmitRealValueExpr(*expr.rhs, module, locals,
                                                     regs)
                                 : "gpga_bits_to_real(0ul)";
      if (expr.op == '+' || expr.op == '-' || expr.op == '*' ||
          expr.op == '/') {
        if (expr.op == '+') {
          return "gpga_double_add(" + lhs + ", " + rhs + ")";
        }
        if (expr.op == '-') {
          return "gpga_double_sub(" + lhs + ", " + rhs + ")";
        }
        if (expr.op == '*') {
          return "gpga_double_mul(" + lhs + ", " + rhs + ")";
        }
        return "gpga_double_div(" + lhs + ", " + rhs + ")";
      }
      if (expr.op == 'p') {
        return "gpga_double_pow(" + lhs + ", " + rhs + ")";
      }
      return "gpga_bits_to_real(0ul)";
    }
    case ExprKind::kTernary: {
      std::string cond = expr.condition
                             ? EmitCondExpr(*expr.condition, module, locals,
                                            regs)
                             : "false";
      std::string then_expr =
          expr.then_expr
              ? EmitRealValueExpr(*expr.then_expr, module, locals, regs)
              : "gpga_bits_to_real(0ul)";
      std::string else_expr =
          expr.else_expr
              ? EmitRealValueExpr(*expr.else_expr, module, locals, regs)
              : "gpga_bits_to_real(0ul)";
      return "((" + cond + ") ? (" + then_expr + ") : (" + else_expr + "))";
    }
    case ExprKind::kIndex: {
      if (!expr.base || !expr.index) {
        return "gpga_bits_to_real(0ul)";
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
                   MslName(expr.base->ident) + "[" + base +
                   "]) : gpga_bits_to_real(0ul))";
          }
        }
      }
      return emit_int_as_real(expr);
    }
    case ExprKind::kCall: {
      std::string name = expr.ident;
      if (!name.empty() && name.front() == '$') {
        name = name.substr(1);
      }
      if (name == "realtime") {
        return "gpga_double_from_u64(__gpga_time)";
      }
      if (name == "itor") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          return emit_int_as_real(*expr.call_args.front());
        }
        return "gpga_bits_to_real(0ul)";
      }
      if (name == "bitstoreal") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          std::string bits =
              EmitExprSized(*expr.call_args.front(), 64, module, locals, regs);
          return "gpga_bits_to_real(" + bits + ")";
        }
        return "gpga_bits_to_real(0ul)";
      }
      if (name == "log10" || name == "ln" || name == "exp" ||
          name == "sqrt" || name == "floor" || name == "ceil" ||
          name == "sin" || name == "cos" || name == "tan" ||
          name == "asin" || name == "acos" || name == "atan" ||
          name == "sinh" || name == "cosh" || name == "tanh" ||
          name == "asinh" || name == "acosh" || name == "atanh") {
        std::string arg =
            (!expr.call_args.empty() && expr.call_args.front())
                ? EmitRealValueExpr(*expr.call_args.front(), module, locals,
                                    regs)
                : "gpga_bits_to_real(0ul)";
        if (name == "log10") {
          return "gpga_double_log10(" + arg + ")";
        }
        if (name == "ln") {
          return "gpga_double_ln(" + arg + ")";
        }
        if (name == "exp") {
          return "gpga_double_exp_real(" + arg + ")";
        }
        if (name == "sqrt") {
          return "gpga_double_sqrt(" + arg + ")";
        }
        if (name == "floor") {
          return "gpga_double_floor(" + arg + ")";
        }
        if (name == "ceil") {
          return "gpga_double_ceil(" + arg + ")";
        }
        if (name == "sin") {
          return "gpga_double_sin(" + arg + ")";
        }
        if (name == "cos") {
          return "gpga_double_cos(" + arg + ")";
        }
        if (name == "tan") {
          return "gpga_double_tan(" + arg + ")";
        }
        if (name == "asin") {
          return "gpga_double_asin(" + arg + ")";
        }
        if (name == "acos") {
          return "gpga_double_acos(" + arg + ")";
        }
        if (name == "atan") {
          return "gpga_double_atan(" + arg + ")";
        }
        if (name == "sinh") {
          return "gpga_double_sinh(" + arg + ")";
        }
        if (name == "cosh") {
          return "gpga_double_cosh(" + arg + ")";
        }
        if (name == "tanh") {
          return "gpga_double_tanh(" + arg + ")";
        }
        if (name == "asinh") {
          return "gpga_double_asinh(" + arg + ")";
        }
        if (name == "acosh") {
          return "gpga_double_acosh(" + arg + ")";
        }
        if (name == "atanh") {
          return "gpga_double_atanh(" + arg + ")";
        }
      }
      if (name == "pow") {
        std::string lhs =
            (expr.call_args.size() > 0 && expr.call_args[0])
                ? EmitRealValueExpr(*expr.call_args[0], module, locals, regs)
                : "gpga_bits_to_real(0ul)";
        std::string rhs =
            (expr.call_args.size() > 1 && expr.call_args[1])
                ? EmitRealValueExpr(*expr.call_args[1], module, locals, regs)
                : "gpga_bits_to_real(0ul)";
        return "gpga_double_pow(" + lhs + ", " + rhs + ")";
      }
      if (name == "atan2") {
        std::string lhs =
            (expr.call_args.size() > 0 && expr.call_args[0])
                ? EmitRealValueExpr(*expr.call_args[0], module, locals, regs)
                : "gpga_bits_to_real(0ul)";
        std::string rhs =
            (expr.call_args.size() > 1 && expr.call_args[1])
                ? EmitRealValueExpr(*expr.call_args[1], module, locals, regs)
                : "gpga_bits_to_real(0ul)";
        return "gpga_double_atan2(" + lhs + ", " + rhs + ")";
      }
      if (name == "hypot") {
        std::string lhs =
            (expr.call_args.size() > 0 && expr.call_args[0])
                ? EmitRealValueExpr(*expr.call_args[0], module, locals, regs)
                : "gpga_bits_to_real(0ul)";
        std::string rhs =
            (expr.call_args.size() > 1 && expr.call_args[1])
                ? EmitRealValueExpr(*expr.call_args[1], module, locals, regs)
                : "gpga_bits_to_real(0ul)";
        return "gpga_double_hypot(" + lhs + ", " + rhs + ")";
      }
      return "gpga_bits_to_real(0ul)";
    }
    case ExprKind::kSelect:
    case ExprKind::kConcat:
      return "gpga_bits_to_real(0ul)";
  }
  return "gpga_bits_to_real(0ul)";
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
  std::string raw = "gpga_double_to_s64(" + real_value + ")";
  if (!signed_target) {
    raw = "(ulong)(" + raw + ")";
  }
  return MaskForWidthExpr(raw, target_width);
}

std::string EmitCondExpr(const Expr& expr, const Module& module,
                         const std::unordered_set<std::string>& locals,
                         const std::unordered_set<std::string>& regs) {
  if (ExprIsRealValue(expr, module)) {
    std::string real_val = EmitRealValueExpr(expr, module, locals, regs);
    return "(!gpga_double_is_zero(" + real_val + "))";
  }
  std::string raw = EmitExpr(expr, module, locals, regs);
  int width = ExprWidth(expr, module);
  if (width > 64) {
    return "gpga_wide_any_" + std::to_string(width) + "(" + raw + ")";
  }
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
  int repeats = std::max(0, expr.repeat);
  int total_width = element_width * repeats;
  if (total_width <= 0) {
    return "0u";
  }
  bool wide = total_width > 32;
  if (total_width > 64) {
    int shift = total_width;
    std::string acc = ZeroForWidth(total_width);
    for (int r = 0; r < repeats; ++r) {
      for (const auto& element : expr.elements) {
        int width = ExprWidth(*element, module);
        if (width <= 0) {
          continue;
        }
        shift -= width;
        if (shift < 0) {
          shift = 0;
        }
        std::string part = EmitExpr(*element, module, locals, regs);
        std::string part_ext;
        if (width > 64) {
          part_ext = ExtendExpr(part, width, total_width);
        } else {
          part_ext = "gpga_wide_from_u64_" + std::to_string(total_width) + "(" +
                     MaskForWidthExpr(part, width) + ")";
        }
        std::string shifted =
            "gpga_wide_shl_" + std::to_string(total_width) + "(" + part_ext +
            ", " + std::to_string(shift) + "u)";
        acc = "gpga_wide_or_" + std::to_string(total_width) + "(" + acc + ", " +
              shifted + ")";
      }
    }
    return acc;
  }
  int shift = total_width;
  std::string acc = wide ? "0ul" : "0u";
  for (int r = 0; r < repeats; ++r) {
    for (const auto& element : expr.elements) {
      int width = ExprWidth(*element, module);
      if (width <= 0) {
        continue;
      }
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
      {
        std::string name = expr.ident;
        if (!name.empty() && name.front() == '$') {
          name = name.substr(1);
        }
        if (name == "realtime" || name == "itor" || name == "bitstoreal" ||
            name == "rtoi" || name == "realtobits") {
          return true;
        }
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
    case StatementKind::kForce:
    case StatementKind::kRelease:
      return true;
    default:
      return false;
  }
}

bool StatementNeedsScheduler(const Statement& stmt) {
  if (stmt.kind == StatementKind::kAssign && stmt.assign.delay) {
    return true;
  }
  if (stmt.kind == StatementKind::kForce ||
      stmt.kind == StatementKind::kRelease) {
    return true;
  }
  if (StatementHasFileSystemCall(stmt)) {
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
        return MslName(port->name) + "[gid]";
      }
      if (regs.count(expr.ident) > 0) {
        return MslName(expr.ident) + "[gid]";
      }
      if (locals.count(expr.ident) > 0) {
        return MslName(expr.ident);
      }
      return MslName(expr.ident);
    }
    case ExprKind::kNumber:
      if (expr.has_width && expr.number_width > 64) {
        std::string literal = std::to_string(expr.number) + "ul";
        return "gpga_wide_from_u64_" + std::to_string(expr.number_width) +
               "(" + literal + ")";
      }
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
    case ExprKind::kString: {
      int width = static_cast<int>(expr.string_value.size() * 8);
      if (width <= 0) {
        width = 1;
      }
      return StringLiteralExpr(expr.string_value, width);
    }
    case ExprKind::kUnary: {
      int width = expr.operand ? ExprWidth(*expr.operand, module) : 32;
      std::string operand =
          expr.operand ? EmitExpr(*expr.operand, module, locals, regs)
                       : ZeroForWidth(width);
      if (width > 64) {
        std::string masked = MaskForWidthExpr(operand, width);
        if (expr.unary_op == 'S' || expr.unary_op == 'U') {
          return masked;
        }
        if (expr.unary_op == '+') {
          return masked;
        }
        if (expr.unary_op == '-') {
          return "gpga_wide_sub_" + std::to_string(width) + "(" +
                 ZeroForWidth(width) + ", " + masked + ")";
        }
        if (expr.unary_op == '~') {
          return "gpga_wide_not_" + std::to_string(width) + "(" + masked + ")";
        }
        if (expr.unary_op == '&') {
          return "gpga_wide_red_and_" + std::to_string(width) + "(" + masked +
                 ")";
        }
        if (expr.unary_op == '|') {
          return "gpga_wide_red_or_" + std::to_string(width) + "(" + masked +
                 ")";
        }
        if (expr.unary_op == '^') {
          return "gpga_wide_red_xor_" + std::to_string(width) + "(" + masked +
                 ")";
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
        return masked;
      }
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
        if (width > 64) {
          std::string rhs_shift =
              EmitExprSized(*expr.rhs, 32, module, locals, regs);
          std::string lhs_ext =
              ExprSigned(*expr.lhs, module)
                  ? SignExtendExpr(lhs, lhs_width, width)
                  : ExtendExpr(lhs, lhs_width, width);
          std::string func = "gpga_wide_shr_" + std::to_string(width);
          if (expr.op == 'l') {
            func = "gpga_wide_shl_" + std::to_string(width);
          } else if (expr.op == 'R' && lhs_signed) {
            func = "gpga_wide_sar_" + std::to_string(width);
          }
          return func + "(" + lhs_ext + ", uint(" + rhs_shift + "))";
        }
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
        if (target_width > 64) {
          std::string lhs_ext = signed_op
                                    ? SignExtendExpr(lhs, lhs_width, target_width)
                                    : ExtendExpr(lhs, lhs_width, target_width);
          std::string rhs_ext = signed_op
                                    ? SignExtendExpr(rhs, rhs_width, target_width)
                                    : ExtendExpr(rhs, rhs_width, target_width);
          std::string func = "gpga_wide_pow_u_" + std::to_string(target_width);
          if (signed_op) {
            func = "gpga_wide_pow_s_" + std::to_string(target_width);
          }
          return func + "(" + lhs_ext + ", " + rhs_ext + ")";
        }
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
                       : "gpga_bits_to_real(0ul)";
          std::string rhs_real =
              expr.rhs ? EmitRealValueExpr(*expr.rhs, module, locals, regs)
                       : "gpga_bits_to_real(0ul)";
          std::string pred;
          if (expr.op == 'E' || expr.op == 'C' || expr.op == 'W') {
            pred = "gpga_double_eq(" + lhs_real + ", " + rhs_real + ")";
          } else if (expr.op == 'N' || expr.op == 'c' || expr.op == 'w') {
            pred = "!gpga_double_eq(" + lhs_real + ", " + rhs_real + ")";
          } else if (expr.op == '<') {
            pred = "gpga_double_lt(" + lhs_real + ", " + rhs_real + ")";
          } else if (expr.op == '>') {
            pred = "gpga_double_gt(" + lhs_real + ", " + rhs_real + ")";
          } else if (expr.op == 'L') {
            pred = "gpga_double_le(" + lhs_real + ", " + rhs_real + ")";
          } else if (expr.op == 'G') {
            pred = "gpga_double_ge(" + lhs_real + ", " + rhs_real + ")";
          } else {
            pred = "false";
          }
          return "((" + pred + ") ? 1u : 0u)";
        }
        std::string lhs_ext = signed_op
                                  ? SignExtendExpr(lhs, lhs_width, target_width)
                                  : ExtendExpr(lhs, lhs_width, target_width);
        std::string rhs_ext = signed_op
                                  ? SignExtendExpr(rhs, rhs_width, target_width)
                                  : ExtendExpr(rhs, rhs_width, target_width);
        if (target_width > 64) {
          std::string func = "gpga_wide_eq_" + std::to_string(target_width);
          if (expr.op == 'N' || expr.op == 'c' || expr.op == 'w') {
            func = "gpga_wide_ne_" + std::to_string(target_width);
          } else if (expr.op == '<') {
            func = (signed_op ? "gpga_wide_lt_s_" : "gpga_wide_lt_u_") +
                   std::to_string(target_width);
          } else if (expr.op == '>') {
            func = (signed_op ? "gpga_wide_gt_s_" : "gpga_wide_gt_u_") +
                   std::to_string(target_width);
          } else if (expr.op == 'L') {
            func = (signed_op ? "gpga_wide_le_s_" : "gpga_wide_le_u_") +
                   std::to_string(target_width);
          } else if (expr.op == 'G') {
            func = (signed_op ? "gpga_wide_ge_s_" : "gpga_wide_ge_u_") +
                   std::to_string(target_width);
          }
          return "((" + func + "(" + lhs_ext + ", " + rhs_ext +
                 ")) ? 1u : 0u)";
        }
        return "((" + lhs_ext + " " + BinaryOpString(expr.op) + " " + rhs_ext +
               ") ? 1u : 0u)";
      }
      std::string lhs_ext = signed_op
                                ? SignExtendExpr(lhs, lhs_width, target_width)
                                : ExtendExpr(lhs, lhs_width, target_width);
      std::string rhs_ext = signed_op
                                ? SignExtendExpr(rhs, rhs_width, target_width)
                                : ExtendExpr(rhs, rhs_width, target_width);
      if (target_width > 64) {
        std::string func = "gpga_wide_add_" + std::to_string(target_width);
        switch (expr.op) {
          case '+':
            func = "gpga_wide_add_" + std::to_string(target_width);
            break;
          case '-':
            func = "gpga_wide_sub_" + std::to_string(target_width);
            break;
          case '*':
            func = "gpga_wide_mul_" + std::to_string(target_width);
            break;
          case '/':
            func = "gpga_wide_div_" + std::to_string(target_width);
            break;
          case '%':
            func = "gpga_wide_mod_" + std::to_string(target_width);
            break;
          case '&':
            func = "gpga_wide_and_" + std::to_string(target_width);
            break;
          case '|':
            func = "gpga_wide_or_" + std::to_string(target_width);
            break;
          case '^':
            func = "gpga_wide_xor_" + std::to_string(target_width);
            break;
          default:
            func = "gpga_wide_add_" + std::to_string(target_width);
            break;
        }
        return func + "(" + lhs_ext + ", " + rhs_ext + ")";
      }
      std::string raw =
          "(" + lhs_ext + " " + BinaryOpString(expr.op) + " " + rhs_ext + ")";
      return MaskForWidthExpr(raw, target_width);
    }
    case ExprKind::kTernary: {
      std::string cond = expr.condition
                             ? EmitCondExpr(*expr.condition, module, locals,
                                            regs)
                             : "false";
      int then_width =
          expr.then_expr ? ExprWidth(*expr.then_expr, module) : 32;
      int else_width =
          expr.else_expr ? ExprWidth(*expr.else_expr, module) : 32;
      int target_width = std::max(then_width, else_width);
      std::string then_expr =
          expr.then_expr ? EmitExpr(*expr.then_expr, module, locals, regs) : "0u";
      std::string else_expr =
          expr.else_expr ? EmitExpr(*expr.else_expr, module, locals, regs) : "0u";
      if (target_width > 64) {
        std::string then_ext = ExtendExpr(then_expr, then_width, target_width);
        std::string else_ext = ExtendExpr(else_expr, else_width, target_width);
        return "((" + cond + ") ? (" + then_ext + ") : (" + else_ext + "))";
      }
      return "((" + cond + ") ? (" + then_expr + ") : (" + else_expr + "))";
    }
    case ExprKind::kSelect: {
      std::string base = EmitExpr(*expr.base, module, locals, regs);
      int base_width = ExprWidth(*expr.base, module);
      if (base_width > 64) {
        if (expr.indexed_range && expr.indexed_width > 0 && expr.lsb_expr) {
          int width = expr.indexed_width;
          std::string shift =
              EmitExprSized(*expr.lsb_expr, 32, module, locals, regs);
          std::string shift_val = "uint(" + shift + ")";
          std::string shifted =
              "gpga_wide_shr_" + std::to_string(base_width) + "(" + base +
              ", " + shift_val + ")";
          std::string zero = ZeroForWidth(width);
          if (width > 64) {
            std::string resized =
                "gpga_wide_resize_" + std::to_string(width) + "_from_" +
                std::to_string(base_width) + "(" + shifted + ")";
            return "((" + shift_val + ") >= " + std::to_string(base_width) +
                   "u ? " + zero + " : " + resized + ")";
          }
          std::string low =
              "gpga_wide_to_u64_" + std::to_string(base_width) + "(" +
              shifted + ")";
          std::string masked = MaskForWidthExpr(low, width);
          return "((" + shift_val + ") >= " + std::to_string(base_width) +
                 "u ? " + zero + " : " + masked + ")";
        }
        int lo = std::min(expr.msb, expr.lsb);
        int hi = std::max(expr.msb, expr.lsb);
        int width = hi - lo + 1;
        std::string shifted =
            "gpga_wide_shr_" + std::to_string(base_width) + "(" + base +
            ", " + std::to_string(lo) + "u)";
        if (width > 64) {
          return "gpga_wide_resize_" + std::to_string(width) + "_from_" +
                 std::to_string(base_width) + "(" + shifted + ")";
        }
        std::string low =
            "gpga_wide_to_u64_" + std::to_string(base_width) + "(" + shifted +
            ")";
        return MaskForWidthExpr(low, width);
      }
      if (expr.indexed_range && expr.indexed_width > 0 && expr.lsb_expr) {
        int width = expr.indexed_width;
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
          return "((" + bounds + ") ? " + MslName(expr.base->ident) + "[" +
                 base + "] : " + ZeroForWidth(element_width) + ")";
        }
      }
      std::string base = EmitExpr(*expr.base, module, locals, regs);
      int base_width = ExprWidth(*expr.base, module);
      if (base_width > 64) {
        std::string index =
            EmitExprSized(*expr.index, 32, module, locals, regs);
        return "gpga_wide_get_bit_" + std::to_string(base_width) + "(" + base +
               ", uint(" + index + "))";
      }
      std::string index = EmitExpr(*expr.index, module, locals, regs);
      std::string one = (base_width > 32) ? "1ul" : "1u";
      std::string cast = CastForWidth(base_width);
      std::string masked = MaskForWidthExpr(base, base_width);
      return "((" + cast + masked + " >> " + index + ") & " + one + ")";
    }
    case ExprKind::kCall:
      if (expr.ident == "$time") {
        return "__gpga_time";
      }
      if (expr.ident == "$stime") {
        return "uint(__gpga_time)";
      }
      if (expr.ident == "$fopen") {
        return "0u";
      }
      if (expr.ident == "$fclose") {
        return "0u";
      }
      if (expr.ident == "$fgetc") {
        return "4294967295u";
      }
      if (expr.ident == "$fgets") {
        return "0u";
      }
      if (expr.ident == "$feof") {
        return "1u";
      }
      if (expr.ident == "$ftell") {
        return "0u";
      }
      if (expr.ident == "$fseek") {
        return "0u";
      }
      if (expr.ident == "$ferror") {
        return "0u";
      }
      if (expr.ident == "$ungetc") {
        return "4294967295u";
      }
      if (expr.ident == "$fread") {
        return "0u";
      }
      if (expr.ident == "$fscanf" || expr.ident == "$sscanf") {
        return "0u";
      }
      if (expr.ident == "$test$plusargs" || expr.ident == "$value$plusargs") {
        return "0u";
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
  if (!assign.lhs_indices.empty()) {
    std::vector<int> dims;
    int element_width = 0;
    int array_size = 0;
    if (!GetArrayDims(module, assign.lhs, &dims, &element_width, &array_size)) {
      return out;
    }
    if (dims.empty() || element_width <= 0 || array_size <= 0) {
      return out;
    }
    size_t dim_count = dims.size();
    size_t index_count = assign.lhs_indices.size();
    bool has_bit_select = false;
    const Expr* bit_expr = nullptr;
    if (assign.lhs_has_range) {
      if (assign.lhs_lsb_expr) {
        return out;
      }
      if (index_count != dim_count) {
        return out;
      }
      has_bit_select = true;
      bit_expr = assign.lhs_msb_expr.get();
      if (!bit_expr) {
        return out;
      }
    } else if (index_count == dim_count + 1) {
      has_bit_select = true;
      bit_expr = assign.lhs_indices.back().get();
      index_count = dim_count;
    } else if (index_count != dim_count) {
      return out;
    }
    std::string linear;
    std::string guard;
    for (size_t i = 0; i < dim_count; ++i) {
      const Expr* idx_expr = assign.lhs_indices[i].get();
      if (!idx_expr) {
        return out;
      }
      std::string idx = EmitExpr(*idx_expr, module, locals, regs);
      std::string idx_u = "uint(" + idx + ")";
      if (linear.empty()) {
        linear = idx_u;
      } else {
        linear = "(" + linear + " * " + std::to_string(dims[i]) + "u + " +
                 idx_u + ")";
      }
      std::string cond =
          "(" + idx_u + " < " + std::to_string(dims[i]) + "u)";
      guard = guard.empty() ? cond : "(" + guard + " && " + cond + ")";
    }
    std::string target =
        use_next ? MslNameNext(assign.lhs) : MslName(assign.lhs);
    std::string base =
        "(gid * " + std::to_string(array_size) + "u) + " + linear;
    out.expr = target + "[" + base + "]";
    out.base_width = element_width;
    out.ok = true;
    if (has_bit_select) {
      if (SignalIsReal(module, assign.lhs)) {
        return LvalueInfo{};
      }
      std::string bit_index = EmitExpr(*bit_expr, module, locals, regs);
      std::string bit_guard = "(uint(" + bit_index + ") < " +
                              std::to_string(element_width) + "u)";
      guard = guard.empty() ? bit_guard : "(" + guard + " && " + bit_guard + ")";
      out.guard = guard;
      out.is_bit_select = true;
      out.width = 1;
      out.bit_index = bit_index;
      return out;
    }
    out.guard = guard;
    out.is_array = true;
    out.width = element_width;
    return out;
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
      std::string target =
          use_next ? MslNameNext(assign.lhs) : MslName(assign.lhs);
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
      base = MslName(assign.lhs) + "[gid]";
    } else if (locals.count(assign.lhs) > 0) {
      base = MslName(assign.lhs);
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
        base = MslName(assign.lhs) + "[gid]";
      } else if (locals.count(assign.lhs) > 0) {
        base = MslName(assign.lhs);
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
    std::string target =
        use_next ? MslNameNext(assign.lhs) : MslName(assign.lhs);
    out.expr = target + "[" + base + "]";
    out.guard = "(" + idx + " < " + std::to_string(array_size) + "u)";
    out.width = element_width;
    out.ok = true;
    out.is_array = true;
    return out;
  }
  if (IsOutputPort(module, assign.lhs) || regs.count(assign.lhs) > 0) {
    out.expr = MslName(assign.lhs) + "[gid]";
  } else if (locals.count(assign.lhs) > 0) {
    out.expr = MslName(assign.lhs);
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
  if (base_width > 64) {
    std::string idx = "uint(" + index_expr + ")";
    std::string rhs_masked = MaskForWidthExpr(rhs_expr, 1);
    return "gpga_wide_set_bit_" + std::to_string(base_width) + "(" + base_expr +
           ", " + idx + ", uint(" + rhs_masked + "))";
  }
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
  if (base_width > 64) {
    std::string idx = "uint(" + index_expr + ")";
    std::string rhs_ext;
    if (slice_width > 64) {
      rhs_ext = "gpga_wide_resize_" + std::to_string(base_width) + "_from_" +
                std::to_string(slice_width) + "(" + rhs_expr + ")";
    } else {
      rhs_ext = "gpga_wide_from_u64_" + std::to_string(base_width) + "(" +
                MaskForWidthExpr(rhs_expr, slice_width) + ")";
    }
    std::string mask;
    if (slice_width > 64) {
      mask = "gpga_wide_resize_" + std::to_string(base_width) + "_from_" +
             std::to_string(slice_width) + "(gpga_wide_mask_const_" +
             std::to_string(slice_width) + "())";
    } else {
      uint64_t slice_mask = MaskForWidth64(slice_width);
      mask = "gpga_wide_from_u64_" + std::to_string(base_width) + "(" +
             std::to_string(slice_mask) + "ul)";
    }
    std::string shifted_mask =
        "gpga_wide_shl_" + std::to_string(base_width) + "(" + mask + ", " +
        idx + ")";
    std::string clear =
        "gpga_wide_not_" + std::to_string(base_width) + "(" + shifted_mask +
        ")";
    std::string set =
        "gpga_wide_shl_" + std::to_string(base_width) + "(" + rhs_ext + ", " +
        idx + ")";
    std::string cleared =
        "gpga_wide_and_" + std::to_string(base_width) + "(" + base_expr + ", " +
        clear + ")";
    return "gpga_wide_or_" + std::to_string(base_width) + "(" + cleared + ", " +
           set + ")";
  }
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
  const bool needs_scheduler = ModuleNeedsScheduler(module);
  std::ostringstream out;
  out << "#include <metal_stdlib>\n";
  out << "using namespace metal;\n\n";
  if (four_state) {
    out << "#include \"gpga_4state.h\"\n";
  }
  std::vector<int> wide_widths = CollectWideWidths(module);
  if (!wide_widths.empty()) {
    out << "#include \"gpga_wide.h\"\n";
  }
  if (needs_scheduler) {
    out << "#include \"gpga_sched.h\"\n";
  }
  out << "\n";
  if (!wide_widths.empty()) {
    out << "// Wide (>64-bit) helpers.\n";
    for (int width : wide_widths) {
      int words = (width + 63) / 64;
      uint64_t last_mask =
          (width % 64 == 0) ? 0xFFFFFFFFFFFFFFFFull
                            : ((1ull << (width % 64)) - 1ull);
      out << "GPGA_WIDE_DEFINE(" << width << ", " << words << ", " << last_mask
          << "ul)\n";
    }
    for (int dst : wide_widths) {
      int dst_words = (dst + 63) / 64;
      uint64_t dst_last_mask =
          (dst % 64 == 0) ? 0xFFFFFFFFFFFFFFFFull
                          : ((1ull << (dst % 64)) - 1ull);
      for (int src : wide_widths) {
        int src_words = (src + 63) / 64;
        int src_mod = src % 64;
        out << "GPGA_WIDE_DEFINE_RESIZE(" << dst << ", " << src << ", "
            << dst_words << ", " << src_words << ", " << dst_last_mask
            << "ul, " << src_mod << ")\n";
      }
    }
    if (four_state) {
      for (int width : wide_widths) {
        out << "GPGA_WIDE_DEFINE_FS(" << width << ")\n";
      }
    }
    out << "\n";
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
    out << "#include \"gpga_real_decl.h\"\n\n";
  }
  out << "struct GpgaParams { uint count; };\n\n";
  out << "constant constexpr ulong __gpga_time = 0ul;\n\n";
  const SystemTaskInfo system_task_info = BuildSystemTaskInfo(module);
  const uint32_t service_wide_words = CollectServiceWideWordCount(module);
  if (four_state) {
    auto suffix_for_width = [](int width) -> std::string {
      return (width > 32) ? "ul" : "u";
    };
    auto literal_for_width = [&](uint64_t value, int width) -> std::string {
      if (width > 64) {
        return "gpga_wide_from_u64_" + std::to_string(width) + "(" +
               std::to_string(value) + "ul)";
      }
      return std::to_string(value) + suffix_for_width(width);
    };
    auto mask_literal = [&](int width) -> std::string {
      if (width > 64) {
        return "gpga_wide_mask_const_" + std::to_string(width) + "()";
      }
      uint64_t mask = MaskForWidth64(width);
      return std::to_string(mask) + suffix_for_width(width);
    };
    auto drive_full = [&](int width) -> std::string {
      return mask_literal(width);
    };
    auto drive_zero = [&](int width) -> std::string {
      return literal_for_width(0, width);
    };
    auto wide_not = [&](const std::string& expr, int width) -> std::string {
      return "gpga_wide_not_" + std::to_string(width) + "(" + expr + ")";
    };
    auto wide_and = [&](const std::string& lhs, const std::string& rhs,
                        int width) -> std::string {
      return "gpga_wide_and_" + std::to_string(width) + "(" + lhs + ", " + rhs +
             ")";
    };
    auto wide_or = [&](const std::string& lhs, const std::string& rhs,
                       int width) -> std::string {
      return "gpga_wide_or_" + std::to_string(width) + "(" + lhs + ", " + rhs +
             ")";
    };
    auto wide_xor = [&](const std::string& lhs, const std::string& rhs,
                        int width) -> std::string {
      return "gpga_wide_xor_" + std::to_string(width) + "(" + lhs + ", " + rhs +
             ")";
    };
    auto wide_shl = [&](const std::string& lhs, const std::string& rhs,
                        int width) -> std::string {
      return "gpga_wide_shl_" + std::to_string(width) + "(" + lhs + ", " + rhs +
             ")";
    };
    auto wide_shr = [&](const std::string& lhs, const std::string& rhs,
                        int width) -> std::string {
      return "gpga_wide_shr_" + std::to_string(width) + "(" + lhs + ", " + rhs +
             ")";
    };
    auto wide_sar = [&](const std::string& lhs, const std::string& rhs,
                        int width) -> std::string {
      return "gpga_wide_sar_" + std::to_string(width) + "(" + lhs + ", " + rhs +
             ")";
    };
    auto wide_any = [&](const std::string& expr, int width) -> std::string {
      return "gpga_wide_any_" + std::to_string(width) + "(" + expr + ")";
    };
    auto wide_eq = [&](const std::string& lhs, const std::string& rhs,
                       int width) -> std::string {
      return "gpga_wide_eq_" + std::to_string(width) + "(" + lhs + ", " + rhs +
             ")";
    };
    auto to_u64 = [&](const std::string& expr, int width) -> std::string {
      if (width > 64) {
        return "gpga_wide_to_u64_" + std::to_string(width) + "(" + expr + ")";
      }
      return expr;
    };
    auto to_uint = [&](const std::string& expr, int width) -> std::string {
      return "uint(" + to_u64(expr, width) + ")";
    };
    auto xz_is_zero = [&](const std::string& expr, int width) -> std::string {
      if (width > 64) {
        return "(!" + wide_any(expr, width) + ")";
      }
      return "(" + expr + " == " + literal_for_width(0, width) + ")";
    };
    auto val_is_zero = [&](const std::string& expr, int width) -> std::string {
      if (width > 64) {
        return "(!" + wide_any(expr, width) + ")";
      }
      return "(" + expr + " == " + literal_for_width(0, width) + ")";
    };
    auto val_is_nonzero = [&](const std::string& expr,
                              int width) -> std::string {
      if (width > 64) {
        return wide_any(expr, width);
      }
      return "(" + expr + " != " + literal_for_width(0, width) + ")";
    };
    auto val_name = [](const std::string& name) { return MslValName(name); };
    auto xz_name = [](const std::string& name) { return MslXzName(name); };
    auto shadow_val_name = [](const std::string& name) {
      return "__gpga_force_shadow_" + MslValName(name);
    };
    auto shadow_xz_name = [](const std::string& name) {
      return "__gpga_force_shadow_" + MslXzName(name);
    };
    auto shadow_any_name = [](const std::string& name) {
      return "__gpga_force_shadow_" + name;
    };
    auto decay_name = [](const std::string& name) {
      return MslDecayName(name);
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
    std::unordered_set<std::string> initial_reads;
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
        CollectReadSignals(stmt, &initial_reads);
      }
    }
    std::unordered_set<std::string> scheduled_reads;
    for (const auto& block : module.always_blocks) {
      if (block.edge == EdgeKind::kCombinational) {
        continue;
      }
      if (block.edge == EdgeKind::kPosedge || block.edge == EdgeKind::kNegedge) {
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
      bool is_string = false;
      std::string string_value;
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
      out.is_const = width <= 64;
      out.val = literal_for_width(out.const_val, width);
      out.xz = literal_for_width(out.const_xz, width);
      if (width > 64) {
        std::string drive = literal_for_width(out.const_drive, width);
        std::string upper_mask = wide_and(
            mask_literal(width),
            wide_not(literal_for_width(0xFFFFFFFFFFFFFFFFull, width), width),
            width);
        out.drive = wide_or(drive, upper_mask, width);
      } else {
        out.drive = literal_for_width(out.const_drive, width);
      }
      return out;
    };

    auto fs_string_literal = [&](const std::string& value, int width) -> FsExpr {
      if (width <= 0) {
        width = 1;
      }
      if (width > 64) {
        FsExpr out;
        out.width = width;
        out.val = WideLiteralExpr(value, width);
        out.xz = literal_for_width(0, width);
        out.drive = drive_full(width);
        out.is_string = true;
        out.string_value = value;
        return out;
      }
      uint64_t bits = StringLiteralBitsForWidth(value, width);
      uint64_t drive_bits = MaskForWidth64(width);
      FsExpr out = fs_const_expr(bits, 0u, drive_bits, width);
      out.is_string = true;
      out.string_value = value;
      return out;
    };

    auto fs_make_expr = [&](const FsExpr& expr, int width) -> std::string {
      if (width > 64) {
        if (!expr.full.empty() && expr.width == width) {
          return expr.full;
        }
        return "GpgaWideFs" + std::to_string(width) + "{" +
               MaskForWidthExpr(expr.val, width) + ", " +
               MaskForWidthExpr(expr.xz, width) + "}";
      }
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
        if (width > 64 || expr.width > 64) {
          return ExtendExpr(expr.drive, expr.width, width);
        }
        return MaskForWidthExpr(expr.drive, width);
      }
      std::string widened = ExtendExpr(expr.drive, expr.width, width);
      if (width > 64) {
        std::string lower_mask = ExtendExpr(mask_literal(expr.width),
                                            expr.width, width);
        std::string upper_mask = wide_and(
            mask_literal(width), wide_not(lower_mask, width), width);
        if (!sign_extend || expr.width <= 0) {
          return wide_or(widened, upper_mask, width);
        }
        std::string sign_bit =
            "gpga_wide_get_bit_" + std::to_string(width) + "(" + widened +
            ", " + std::to_string(expr.width - 1) + "u)";
        std::string upper_drive =
            "(" + sign_bit + " != 0u ? " + upper_mask + " : " +
            drive_zero(width) + ")";
        return wide_or(widened, upper_drive, width);
      }
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
      if (!expr.is_const || expr.width > 64 || width > 64) {
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
      if (expr.is_string && width > expr.width) {
        return fs_string_literal(expr.string_value, width);
      }
      if (expr.width == width) {
        return expr;
      }
      if (expr.is_const && expr.width <= 64 && width <= 64) {
        return fs_const_extend(expr, width, false);
      }
      if (width > 64 || expr.width > 64) {
        FsExpr out;
        out.width = width;
        out.val = ExtendExpr(expr.val, expr.width, width);
        out.xz = ExtendExpr(expr.xz, expr.width, width);
        out.drive = fs_resize_drive(expr, width, false);
        return out;
      }
      std::string func = (width > 32) ? "fs_resize64" : "fs_resize32";
      std::string base = func + "(" + fs_make_expr(expr, expr.width) + ", " +
                         std::to_string(width) + "u)";
      std::string drive = fs_resize_drive(expr, width, false);
      return fs_expr_from_base(base, drive, width);
    };

    auto fs_sext_expr = [&](const FsExpr& expr, int width) -> FsExpr {
      if (expr.is_string && width > expr.width) {
        return fs_string_literal(expr.string_value, width);
      }
      if (expr.width >= width) {
        return fs_resize_expr(expr, width);
      }
      if (expr.is_const && expr.width <= 64 && width <= 64) {
        return fs_const_extend(expr, width, true);
      }
      if (width > 64 || expr.width > 64) {
        FsExpr out;
        out.width = width;
        out.val = SignExtendExpr(expr.val, expr.width, width);
        out.xz = SignExtendExpr(expr.xz, expr.width, width);
        out.drive = fs_resize_drive(expr, width, true);
        return out;
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
      if (width > 64) {
        return FsExpr{drive_zero(width), mask_literal(width),
                      mask_literal(width), width};
      }
      uint64_t mask = MaskForWidth64(width);
      return fs_const_expr(0u, mask, mask, width);
    };

    auto fs_unary = [&](const char* op, const FsExpr& arg, int width) -> FsExpr {
      if (width > 64) {
        std::string aval = MaskForWidthExpr(arg.val, width);
        std::string ax = MaskForWidthExpr(arg.xz, width);
        if (std::strcmp(op, "not") == 0) {
          return FsExpr{wide_not(aval, width), ax, drive_full(width), width};
        }
        return fs_allx_expr(width);
      }
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
      if (width > 64) {
        std::string mask = mask_literal(width);
        std::string lhs_val = MaskForWidthExpr(lhs.val, width);
        std::string rhs_val = MaskForWidthExpr(rhs.val, width);
        std::string lhs_xz = MaskForWidthExpr(lhs.xz, width);
        std::string rhs_xz = MaskForWidthExpr(rhs.xz, width);
        std::string any_xz =
            wide_any(wide_or(lhs_xz, rhs_xz, width), width);
        if (std::strcmp(op, "eq") == 0 || std::strcmp(op, "ne") == 0 ||
            std::strcmp(op, "lt") == 0 || std::strcmp(op, "gt") == 0 ||
            std::strcmp(op, "le") == 0 || std::strcmp(op, "ge") == 0 ||
            std::strcmp(op, "slt") == 0 || std::strcmp(op, "sgt") == 0 ||
            std::strcmp(op, "sle") == 0 || std::strcmp(op, "sge") == 0) {
          std::string pred;
          if (std::strcmp(op, "eq") == 0) {
            pred = wide_eq(lhs_val, rhs_val, width);
          } else if (std::strcmp(op, "ne") == 0) {
            pred = "!" + wide_eq(lhs_val, rhs_val, width);
          } else if (std::strcmp(op, "lt") == 0) {
            pred = "gpga_wide_lt_u_" + std::to_string(width) + "(" + lhs_val +
                   ", " + rhs_val + ")";
          } else if (std::strcmp(op, "gt") == 0) {
            pred = "gpga_wide_gt_u_" + std::to_string(width) + "(" + lhs_val +
                   ", " + rhs_val + ")";
          } else if (std::strcmp(op, "le") == 0) {
            pred = "gpga_wide_le_u_" + std::to_string(width) + "(" + lhs_val +
                   ", " + rhs_val + ")";
          } else if (std::strcmp(op, "ge") == 0) {
            pred = "gpga_wide_ge_u_" + std::to_string(width) + "(" + lhs_val +
                   ", " + rhs_val + ")";
          } else if (std::strcmp(op, "slt") == 0) {
            pred = "gpga_wide_lt_s_" + std::to_string(width) + "(" + lhs_val +
                   ", " + rhs_val + ")";
          } else if (std::strcmp(op, "sgt") == 0) {
            pred = "gpga_wide_gt_s_" + std::to_string(width) + "(" + lhs_val +
                   ", " + rhs_val + ")";
          } else if (std::strcmp(op, "sle") == 0) {
            pred = "gpga_wide_le_s_" + std::to_string(width) + "(" + lhs_val +
                   ", " + rhs_val + ")";
          } else {
            pred = "gpga_wide_ge_s_" + std::to_string(width) + "(" + lhs_val +
                   ", " + rhs_val + ")";
          }
          std::string val =
              "((" + any_xz + ") ? 0u : ((" + pred + ") ? 1u : 0u))";
          std::string xz = "((" + any_xz + ") ? 1u : 0u)";
          return FsExpr{val, xz, drive_full(1), 1};
        }
        if (std::strcmp(op, "and") == 0 || std::strcmp(op, "or") == 0 ||
            std::strcmp(op, "xor") == 0) {
          std::string ax = lhs_xz;
          std::string bx = rhs_xz;
          std::string a0 = wide_and(
              wide_and(wide_not(lhs_val, width), wide_not(ax, width), width),
              mask, width);
          std::string b0 = wide_and(
              wide_and(wide_not(rhs_val, width), wide_not(bx, width), width),
              mask, width);
          std::string a1 =
              wide_and(wide_and(lhs_val, wide_not(ax, width), width), mask,
                       width);
          std::string b1 =
              wide_and(wide_and(rhs_val, wide_not(bx, width), width), mask,
                       width);
          if (std::strcmp(op, "and") == 0) {
            std::string known0 = wide_or(a0, b0, width);
            std::string known1 = wide_and(a1, b1, width);
            std::string unknown =
                wide_and(mask, wide_not(wide_or(known0, known1, width), width),
                         width);
            return FsExpr{known1, unknown, drive_full(width), width};
          }
          if (std::strcmp(op, "or") == 0) {
            std::string known1 = wide_or(a1, b1, width);
            std::string known0 = wide_and(a0, b0, width);
            std::string unknown =
                wide_and(mask, wide_not(wide_or(known0, known1, width), width),
                         width);
            return FsExpr{known1, unknown, drive_full(width), width};
          }
          std::string unknown = wide_and(wide_or(ax, bx, width), mask, width);
          std::string val =
              wide_and(wide_xor(lhs_val, rhs_val, width),
                       wide_not(unknown, width), width);
          return FsExpr{val, unknown, drive_full(width), width};
        }
        std::string func = "gpga_wide_add_" + std::to_string(width);
        if (std::strcmp(op, "sub") == 0) {
          func = "gpga_wide_sub_" + std::to_string(width);
        } else if (std::strcmp(op, "mul") == 0) {
          func = "gpga_wide_mul_" + std::to_string(width);
        } else if (std::strcmp(op, "div") == 0) {
          func = "gpga_wide_div_" + std::to_string(width);
        } else if (std::strcmp(op, "mod") == 0) {
          func = "gpga_wide_mod_" + std::to_string(width);
        } else if (std::strcmp(op, "pow") == 0) {
          func = "gpga_wide_pow_u_" + std::to_string(width);
        } else if (std::strcmp(op, "spow") == 0) {
          func = "gpga_wide_pow_s_" + std::to_string(width);
        }
        std::string val = func + "(" + lhs_val + ", " + rhs_val + ")";
        std::string xz = "((" + any_xz + ") ? " + mask + " : " +
                         drive_zero(width) + ")";
        if (std::strcmp(op, "div") == 0 || std::strcmp(op, "mod") == 0) {
          std::string rhs_zero = "!" + wide_any(rhs_val, width);
          std::string bad = "(" + any_xz + " || " + rhs_zero + ")";
          xz = "((" + bad + ") ? " + mask + " : " + drive_zero(width) + ")";
        }
        return FsExpr{val, xz, drive_full(width), width};
      }
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
      if (width > 64) {
        std::string rhs_xz = (rhs.width > 64)
                                 ? wide_any(rhs.xz, rhs.width)
                                 : "(" + rhs.xz + " != " +
                                       literal_for_width(0, rhs.width) + ")";
        std::string shift_val =
            (rhs.width > 64)
                ? "uint(gpga_wide_to_u64_" + std::to_string(rhs.width) + "(" +
                      rhs.val + "))"
                : "uint(" + rhs.val + ")";
        std::string mask = mask_literal(width);
        std::string xz_any =
            rhs_xz + " || " + wide_any(lhs.xz, width);
        std::string val;
        std::string xz;
        if (std::strcmp(op, "shl") == 0) {
          val = wide_shl(lhs.val, shift_val, width);
          xz = wide_shl(lhs.xz, shift_val, width);
        } else if (std::strcmp(op, "shr") == 0) {
          val = wide_shr(lhs.val, shift_val, width);
          xz = wide_shr(lhs.xz, shift_val, width);
        } else {
          std::string sign_xz =
              "gpga_wide_get_bit_" + std::to_string(width) + "(" + lhs.xz +
              ", " + std::to_string(width - 1) + "u)";
          xz_any = "(" + xz_any + " || " + sign_xz + " != 0u)";
          val = wide_sar(lhs.val, shift_val, width);
          xz = wide_shr(lhs.xz, shift_val, width);
        }
        std::string xz_out =
            "((" + xz_any + ") ? " + mask + " : " + xz + ")";
        return FsExpr{val, xz_out, drive_full(width), width};
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
      std::string type = (expr.width > 64)
                             ? ("GpgaWideFs" + std::to_string(expr.width))
                             : ((expr.width > 32) ? "FourState64"
                                                  : "FourState32");
      std::string dtype = (expr.width > 64)
                              ? ("GpgaWide" + std::to_string(expr.width))
                              : ((expr.width > 32) ? "ulong" : "uint");
      std::string pad(state->indent, ' ');
      out << pad << type << " " << name << " = "
          << fs_make_expr(expr, expr.width) << ";\n";
      out << pad << dtype << " " << name << "_drive = " << expr.drive << ";\n";
      return FsExpr{name + ".val", name + ".xz", name + "_drive", expr.width,
                    name};
    };

    std::function<FsExpr(const Expr&)> emit_expr4;
    std::function<FsExpr(const Expr&)> emit_expr4_impl;
    std::function<FsExpr(FsExpr, FsExpr, int)> fs_merge_expr;
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
      if (total_width > 64) {
        std::string acc_val = drive_zero(total_width);
        std::string acc_xz = drive_zero(total_width);
        std::string acc_drive = drive_zero(total_width);
        int repeats = std::max(0, expr.repeat);
        int shift = total_width;
        for (int rep = 0; rep < repeats; ++rep) {
          for (const auto& element : expr.elements) {
            int width = ExprWidth(*element, module);
            if (width <= 0) {
              continue;
            }
            FsExpr part = emit_expr4(*element);
            shift -= width;
            std::string masked_val = MaskForWidthExpr(part.val, width);
            std::string masked_xz = MaskForWidthExpr(part.xz, width);
            std::string masked_drive = MaskForWidthExpr(part.drive, width);
            std::string part_val = ExtendExpr(masked_val, width, total_width);
            std::string part_xz = ExtendExpr(masked_xz, width, total_width);
            std::string part_drive =
                ExtendExpr(masked_drive, width, total_width);
            std::string shift_amount = std::to_string(shift) + "u";
            acc_val = wide_or(acc_val,
                              wide_shl(part_val, shift_amount, total_width),
                              total_width);
            acc_xz = wide_or(acc_xz,
                             wide_shl(part_xz, shift_amount, total_width),
                             total_width);
            acc_drive =
                wide_or(acc_drive,
                        wide_shl(part_drive, shift_amount, total_width),
                        total_width);
          }
        }
        return FsExpr{acc_val, acc_xz, acc_drive, total_width};
      }
      std::string acc_val = (total_width > 32) ? "0ul" : "0u";
      std::string acc_xz = (total_width > 32) ? "0ul" : "0u";
      std::string acc_drive = (total_width > 32) ? "0ul" : "0u";
      int repeats = std::max(0, expr.repeat);
      int shift = total_width;
      for (int rep = 0; rep < repeats; ++rep) {
        for (const auto& element : expr.elements) {
          int width = ExprWidth(*element, module);
          if (width <= 0) {
            continue;
          }
          FsExpr part = emit_expr4(*element);
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

    auto wide_extract = [&](const std::string& expr, int expr_width,
                            int out_width,
                            const std::string& shift) -> std::string {
      std::string shifted = wide_shr(expr, shift, expr_width);
      if (out_width > 64) {
        return "gpga_wide_resize_" + std::to_string(out_width) + "_from_" +
               std::to_string(expr_width) + "(" + shifted + ")";
      }
      std::string low = "gpga_wide_to_u64_" + std::to_string(expr_width) +
                        "(" + shifted + ")";
      return MaskForWidthExpr(low, out_width);
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
        if (int_expr.width > 32) {
          if (signed_expr) {
            return "gpga_double_from_s64((long)(" + known_val + "))";
          }
          return "gpga_double_from_u64((ulong)(" + known_val + "))";
        }
        if (signed_expr) {
          return "gpga_double_from_s32((int)(" + known_val + "))";
        }
        return "gpga_double_from_u32((uint)(" + known_val + "))";
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
          return "gpga_bits_to_real(0ul)";
        case ExprKind::kString:
          return "gpga_bits_to_real(0ul)";
        case ExprKind::kUnary: {
          std::string operand =
              expr.operand ? emit_real_value4(*expr.operand)
                           : "gpga_bits_to_real(0ul)";
          if (expr.unary_op == '+') {
            return operand;
          }
          if (expr.unary_op == '-') {
            return "gpga_double_neg(" + operand + ")";
          }
          return "gpga_bits_to_real(0ul)";
        }
        case ExprKind::kBinary: {
          std::string lhs =
              expr.lhs ? emit_real_value4(*expr.lhs)
                       : "gpga_bits_to_real(0ul)";
          std::string rhs =
              expr.rhs ? emit_real_value4(*expr.rhs)
                       : "gpga_bits_to_real(0ul)";
          if (expr.op == '+' || expr.op == '-' || expr.op == '*' ||
              expr.op == '/') {
            if (expr.op == '+') {
              return "gpga_double_add(" + lhs + ", " + rhs + ")";
            }
            if (expr.op == '-') {
              return "gpga_double_sub(" + lhs + ", " + rhs + ")";
            }
            if (expr.op == '*') {
              return "gpga_double_mul(" + lhs + ", " + rhs + ")";
            }
            return "gpga_double_div(" + lhs + ", " + rhs + ")";
          }
          if (expr.op == 'p') {
            return "gpga_double_pow(" + lhs + ", " + rhs + ")";
          }
          return "gpga_bits_to_real(0ul)";
        }
        case ExprKind::kTernary: {
          std::string cond = expr.condition
                                 ? "(!gpga_double_is_zero(" +
                                       emit_real_value4(*expr.condition) + "))"
                                 : "false";
          std::string then_expr =
              expr.then_expr ? emit_real_value4(*expr.then_expr)
                             : "gpga_bits_to_real(0ul)";
          std::string else_expr =
              expr.else_expr ? emit_real_value4(*expr.else_expr)
                             : "gpga_bits_to_real(0ul)";
          return "((" + cond + ") ? (" + then_expr + ") : (" + else_expr + "))";
        }
        case ExprKind::kCall: {
          std::string name = expr.ident;
          if (!name.empty() && name.front() == '$') {
            name = name.substr(1);
          }
          if (name == "realtime") {
            return "gpga_double_from_u64(__gpga_time)";
          }
          if (name == "itor") {
            if (!expr.call_args.empty() && expr.call_args.front()) {
              return emit_real_value4(*expr.call_args.front());
            }
            return "gpga_bits_to_real(0ul)";
          }
          if (name == "bitstoreal") {
            if (!expr.call_args.empty() && expr.call_args.front()) {
              FsExpr bits_expr = emit_expr4(*expr.call_args.front());
              std::string mask =
                  literal_for_width(MaskForWidth64(bits_expr.width), 64);
              std::string bits = "((" + bits_expr.val + ") & " + mask + ")";
              return "gpga_bits_to_real(" + bits + ")";
            }
            return "gpga_bits_to_real(0ul)";
          }
          if (name == "log10" || name == "ln" || name == "exp" ||
              name == "sqrt" || name == "floor" || name == "ceil" ||
              name == "sin" || name == "cos" || name == "tan" ||
              name == "asin" || name == "acos" || name == "atan") {
            std::string arg =
                (!expr.call_args.empty() && expr.call_args.front())
                    ? emit_real_value4(*expr.call_args.front())
                    : "gpga_bits_to_real(0ul)";
            if (name == "log10") {
              return "gpga_double_log10(" + arg + ")";
            }
            if (name == "ln") {
              return "gpga_double_ln(" + arg + ")";
            }
            if (name == "exp") {
              return "gpga_double_exp_real(" + arg + ")";
            }
            if (name == "sqrt") {
              return "gpga_double_sqrt(" + arg + ")";
            }
            if (name == "floor") {
              return "gpga_double_floor(" + arg + ")";
            }
            if (name == "ceil") {
              return "gpga_double_ceil(" + arg + ")";
            }
            if (name == "sin") {
              return "gpga_double_sin(" + arg + ")";
            }
            if (name == "cos") {
              return "gpga_double_cos(" + arg + ")";
            }
            if (name == "tan") {
              return "gpga_double_tan(" + arg + ")";
            }
            if (name == "asin") {
              return "gpga_double_asin(" + arg + ")";
            }
            if (name == "acos") {
              return "gpga_double_acos(" + arg + ")";
            }
            if (name == "atan") {
              return "gpga_double_atan(" + arg + ")";
            }
          }
          if (name == "pow") {
            std::string lhs =
                (expr.call_args.size() > 0 && expr.call_args[0])
                    ? emit_real_value4(*expr.call_args[0])
                    : "gpga_bits_to_real(0ul)";
            std::string rhs =
                (expr.call_args.size() > 1 && expr.call_args[1])
                    ? emit_real_value4(*expr.call_args[1])
                    : "gpga_bits_to_real(0ul)";
            return "gpga_double_pow(" + lhs + ", " + rhs + ")";
          }
          return "gpga_bits_to_real(0ul)";
        }
        case ExprKind::kSelect:
          return "gpga_bits_to_real(0ul)";
        case ExprKind::kIndex: {
          if (!expr.base || !expr.index) {
            return "gpga_bits_to_real(0ul)";
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
              std::string idx_u = to_uint(idx.val, idx.width);
              std::string idx_xz = idx.xz;
              std::string bounds = "(" + idx_u + " < " +
                                   std::to_string(array_size) + "u)";
              std::string xguard = xz_is_zero(idx_xz, idx.width);
              std::string base =
                  "(gid * " + std::to_string(array_size) + "u) + " + idx_u;
              return "((" + xguard + ") ? ((" + bounds + ") ? " +
                     "gpga_bits_to_real(" + val_name(expr.base->ident) + "[" +
                     base + "]) : gpga_bits_to_real(0ul)) : gpga_bits_to_real(0ul))";
            }
          }
          return "gpga_bits_to_real(0ul)";
        }
        case ExprKind::kConcat:
          return "gpga_bits_to_real(0ul)";
      }
      return "gpga_bits_to_real(0ul)";
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
      int width = std::max(ExprWidth(expr, module), value.width);
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
        case ExprKind::kString: {
          int width = static_cast<int>(expr.string_value.size() * 8);
          if (width <= 0) {
            width = 1;
          }
          return fs_string_literal(expr.string_value, width);
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
            if (expr.operand && ExprIsRealValue(*expr.operand, module)) {
              std::string real_val = emit_real_value4(*expr.operand);
              std::string pred = "gpga_double_is_zero(" + real_val + ")";
              std::string val = "(" + pred + " ? 1u : 0u)";
              return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
            }
            if (width > 64) {
              std::string ax = MaskForWidthExpr(operand.xz, width);
              std::string aval = MaskForWidthExpr(operand.val, width);
              std::string known1 = wide_and(aval, wide_not(ax, width), width);
              std::string a_true = wide_any(known1, width);
              std::string a_false =
                  "(!" + wide_any(ax, width) + " && !" +
                  wide_any(aval, width) + ")";
              std::string val =
                  "((" + a_true + ") ? 0u : ((" + a_false + ") ? 1u : 0u))";
              std::string xz =
                  "((" + a_true + " || " + a_false + ") ? 0u : 1u)";
              return FsExpr{val, xz, drive_full(1), 1};
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
              std::string pred = "!gpga_double_is_zero(" + real_val + ")";
              std::string val = "(" + pred + " ? 1u : 0u)";
              return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
            }
            std::string val;
            if (width > 64) {
              std::string known = "(!" + wide_any(operand.xz, width) + ")";
              std::string non_zero = wide_any(operand.val, width);
              val = "((" + known + " && " + non_zero + ") ? 1u : 0u)";
            } else {
              std::string zero = literal_for_width(0, width);
              val = "((" + operand.xz + " == " + zero + " && " + operand.val +
                    " != " + zero + ") ? 1u : 0u)";
            }
            return FsExpr{val, literal_for_width(0, 1), drive_full(1), 1};
          }
          if (expr.unary_op == '&' || expr.unary_op == '|' ||
              expr.unary_op == '^') {
            if (width > 64) {
              std::string mask = mask_literal(width);
              std::string ax = MaskForWidthExpr(operand.xz, width);
              std::string aval = MaskForWidthExpr(operand.val, width);
              std::string a0 = wide_and(
                  wide_and(wide_not(aval, width), wide_not(ax, width), width),
                  mask, width);
              std::string a1 =
                  wide_and(wide_and(aval, wide_not(ax, width), width), mask,
                           width);
              if (expr.unary_op == '^') {
                std::string any_xz = wide_any(ax, width);
                std::string parity =
                    "gpga_wide_red_xor_" + std::to_string(width) + "(" +
                    MaskForWidthExpr(operand.val, width) + ")";
                std::string val =
                    "((" + any_xz + ") ? 0u : " + parity + ")";
                std::string xz = "((" + any_xz + ") ? 1u : 0u)";
                return FsExpr{val, xz, drive_full(1), 1};
              }
              if (expr.unary_op == '&') {
                std::string a0_any = wide_any(a0, width);
                std::string a1_all = wide_eq(a1, mask, width);
                std::string val =
                    "((" + a0_any + ") ? 0u : ((" + a1_all + ") ? 1u : 0u))";
                std::string xz =
                    "((" + a0_any + " || " + a1_all + ") ? 0u : 1u)";
                return FsExpr{val, xz, drive_full(1), 1};
              }
              std::string a1_any = wide_any(a1, width);
              std::string a0_all = wide_eq(a0, mask, width);
              std::string val =
                  "((" + a1_any + ") ? 1u : ((" + a0_all + ") ? 0u : 0u))";
              std::string xz =
                  "((" + a1_any + " || " + a0_all + ") ? 0u : 1u)";
              return FsExpr{val, xz, drive_full(1), 1};
            }
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
                return "(!gpga_double_is_zero(gpga_bits_to_real(" + value.val +
                       ")))";
              }
              if (value.width > 64) {
                return "(!" + wide_any(value.xz, value.width) + " && " +
                       wide_any(value.val, value.width) + ")";
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
            if (width > 64) {
              std::string ax = MaskForWidthExpr(lhs.xz, width);
              std::string bx = MaskForWidthExpr(rhs.xz, width);
              std::string aval = MaskForWidthExpr(lhs.val, width);
              std::string bval = MaskForWidthExpr(rhs.val, width);
              std::string a_known1 =
                  wide_and(aval, wide_not(ax, width), width);
              std::string b_known1 =
                  wide_and(bval, wide_not(bx, width), width);
              std::string a_true = wide_any(a_known1, width);
              std::string b_true = wide_any(b_known1, width);
              std::string a_false =
                  "(!" + wide_any(ax, width) + " && !" +
                  wide_any(aval, width) + ")";
              std::string b_false =
                  "(!" + wide_any(bx, width) + " && !" +
                  wide_any(bval, width) + ")";
              std::string val;
              std::string xz;
              if (expr.op == 'A') {
                val =
                    "((" + a_false + " || " + b_false +
                    ") ? 0u : ((" + a_true + " && " + b_true +
                    ") ? 1u : 0u))";
                xz =
                    "((" + a_false + " || " + b_false + " || (" + a_true +
                    " && " + b_true + ")) ? 0u : 1u)";
              } else {
                val =
                    "((" + a_true + " || " + b_true +
                    ") ? 1u : ((" + a_false + " && " + b_false +
                    ") ? 0u : 0u))";
                xz =
                    "((" + a_true + " || " + b_true + " || (" + a_false +
                    " && " + b_false + ")) ? 0u : 1u)";
              }
              return FsExpr{val, xz, drive_full(1), 1};
            }
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
                  expr.lhs ? emit_real_value4(*expr.lhs)
                           : "gpga_bits_to_real(0ul)";
              std::string rhs_real =
                  expr.rhs ? emit_real_value4(*expr.rhs)
                           : "gpga_bits_to_real(0ul)";
              std::string pred = "gpga_double_eq(" + lhs_real + ", " +
                                 rhs_real + ")";
              if (expr.op == 'c' || expr.op == 'w') {
                pred = "!" + pred;
              }
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
            if (width > 64) {
              std::string mask = mask_literal(width);
              if (expr.op == 'C' || expr.op == 'c') {
                std::string ax = MaskForWidthExpr(lhs.xz, width);
                std::string bx = MaskForWidthExpr(rhs.xz, width);
                std::string diff = wide_xor(ax, bx, width);
                std::string known = wide_and(
                    wide_not(wide_or(ax, bx, width), width), mask, width);
                std::string val_diff = wide_and(
                    wide_xor(lhs.val, rhs.val, width), known, width);
                pred = "(!" + wide_any(diff, width) + " && !" +
                       wide_any(val_diff, width) + ")";
              } else if (expr.op == 'W' || expr.op == 'w') {
                std::string ignore = MaskForWidthExpr(rhs.xz, width);
                std::string cared =
                    wide_and(wide_not(ignore, width), mask, width);
                std::string ax = MaskForWidthExpr(lhs.xz, width);
                std::string bad = wide_and(ax, cared, width);
                std::string val_diff = wide_and(
                    wide_xor(lhs.val, rhs.val, width), cared, width);
                pred = "(!" + wide_any(bad, width) + " && !" +
                       wide_any(val_diff, width) + ")";
              } else {
                std::string cared = wide_and(
                    wide_not(wide_or(lhs.xz, rhs.xz, width), width), mask,
                    width);
                std::string val_diff = wide_and(
                    wide_xor(lhs.val, rhs.val, width), cared, width);
                pred = "(!" + wide_any(val_diff, width) + ")";
              }
            } else if (expr.op == 'C' || expr.op == 'c') {
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
                  expr.lhs ? emit_real_value4(*expr.lhs)
                           : "gpga_bits_to_real(0ul)";
              std::string rhs_real =
                  expr.rhs ? emit_real_value4(*expr.rhs)
                           : "gpga_bits_to_real(0ul)";
              std::string pred;
              if (expr.op == 'E') {
                pred = "gpga_double_eq(" + lhs_real + ", " + rhs_real + ")";
              } else if (expr.op == 'N') {
                pred = "!gpga_double_eq(" + lhs_real + ", " + rhs_real + ")";
              } else if (expr.op == '<') {
                pred = "gpga_double_lt(" + lhs_real + ", " + rhs_real + ")";
              } else if (expr.op == '>') {
                pred = "gpga_double_gt(" + lhs_real + ", " + rhs_real + ")";
              } else if (expr.op == 'L') {
                pred = "gpga_double_le(" + lhs_real + ", " + rhs_real + ")";
              } else if (expr.op == 'G') {
                pred = "gpga_double_ge(" + lhs_real + ", " + rhs_real + ")";
              } else {
                pred = "false";
              }
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
          if (width > 64) {
            FsExpr merged = fs_merge_expr(then_resized, else_resized, width);
            std::string cond_known =
                (cond.width > 64)
                    ? ("!" + wide_any(cond.xz, cond.width))
                    : ("(" + cond.xz + " == " +
                       literal_for_width(0, cond.width) + ")");
            std::string cond_true =
                (cond.width > 64)
                    ? ("(" + cond_known + " && " +
                       wide_any(cond.val, cond.width) + ")")
                    : ("(" + cond_known + " && " + cond.val + " != " +
                       literal_for_width(0, cond.width) + ")");
            std::string cond_false =
                (cond.width > 64)
                    ? ("(" + cond_known + " && !" +
                       wide_any(cond.val, cond.width) + ")")
                    : ("(" + cond_known + " && " + cond.val + " == " +
                       literal_for_width(0, cond.width) + ")");
            std::string select_fn =
                "gpga_wide_select_" + std::to_string(width);
            std::string val = select_fn + "(" + cond_true + ", " +
                              then_resized.val + ", " + select_fn + "(" +
                              cond_false + ", " + else_resized.val + ", " +
                              merged.val + "))";
            std::string xz = select_fn + "(" + cond_true + ", " +
                             then_resized.xz + ", " + select_fn + "(" +
                             cond_false + ", " + else_resized.xz + ", " +
                             merged.xz + "))";
            std::string merge_drive =
                wide_or(then_resized.drive, else_resized.drive, width);
            std::string drive = select_fn + "(" + cond_true + ", " +
                                then_resized.drive + ", " + select_fn + "(" +
                                cond_false + ", " + else_resized.drive + ", " +
                                merge_drive + "))";
            return FsExpr{val, xz, drive, width};
          }
          std::string func = (width > 32) ? "fs_mux64" : "fs_mux32";
          std::string base =
              func + "(" + fs_make_expr(cond, cond.width) + ", " +
              fs_make_expr(then_resized, width) + ", " +
              fs_make_expr(else_resized, width) + ", " +
              std::to_string(width) + "u)";
          std::string cond_known = xz_is_zero(cond.xz, cond.width);
          std::string cond_true = "(" + cond_known + " && " +
                                  val_is_nonzero(cond.val, cond.width) + ")";
          std::string cond_false =
              "(" + cond_known + " && " + val_is_zero(cond.val, cond.width) +
              ")";
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
            if (base.width > 64) {
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
                std::string val =
                    wide_extract(base.val, base.width, width, idx);
                std::string xz =
                    wide_extract(base.xz, base.width, width, idx);
                std::string drive =
                    wide_extract(base.drive, base.width, width, idx);
                return FsExpr{val, xz, drive, width};
              }
              std::string idx = to_uint(shift.val, shift.width);
              std::string xguard = xz_is_zero(shift.xz, shift.width);
              std::string bounds =
                  "(" + idx + " < " + std::to_string(base.width) + "u)";
              std::string zero = drive_zero(width);
              std::string val =
                  wide_extract(base.val, base.width, width, idx);
              std::string xz =
                  wide_extract(base.xz, base.width, width, idx);
              std::string drive =
                  wide_extract(base.drive, base.width, width, idx);
              if (width > 64) {
                std::string select_fn =
                    "gpga_wide_select_" + std::to_string(width);
                std::string val_sel =
                    select_fn + "(" + xguard + " && " + bounds + ", " + val +
                    ", " + zero + ")";
                std::string xz_sel =
                    select_fn + "(" + xguard + ", " +
                    select_fn + "(" + bounds + ", " + xz + ", " + zero + "), " +
                    mask + ")";
                std::string drive_sel =
                    select_fn + "(" + xguard + ", " +
                    select_fn + "(" + bounds + ", " + drive + ", " + mask +
                    "), " + mask + ")";
                return FsExpr{val_sel, xz_sel, drive_sel, width};
              }
              std::string val_sel =
                  "((" + xguard + ") ? ((" + bounds + ") ? " + val + " : " +
                  zero + ") : " + zero + ")";
              std::string xz_sel =
                  "((" + xguard + ") ? ((" + bounds + ") ? " + xz + " : " +
                  zero + ") : " + mask + ")";
              std::string drive_sel =
                  "((" + xguard + ") ? ((" + bounds + ") ? " + drive + " : " +
                  mask + ") : " + mask + ")";
              return FsExpr{val_sel, xz_sel, drive_sel, width};
            }
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
            std::string idx = to_uint(shift.val, shift.width);
            std::string zero = literal_for_width(0, width);
            std::string xguard = xz_is_zero(shift.xz, shift.width);
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
          if (base.width > 64) {
            std::string idx = std::to_string(lo) + "u";
            std::string val = wide_extract(base.val, base.width, width, idx);
            std::string xz = wide_extract(base.xz, base.width, width, idx);
            std::string drive =
                wide_extract(base.drive, base.width, width, idx);
            return FsExpr{val, xz, drive, width};
          }
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
              std::string idx_xz = idx.xz;
              if (idx.is_const) {
                if (idx.const_xz != 0) {
                  return fs_allx_expr(element_width);
                }
                if (idx.const_val >= static_cast<uint64_t>(array_size)) {
                  return fs_const_expr(0u, 0u, MaskForWidth64(element_width),
                                       element_width);
                }
                std::string base =
                    "(gid * " + std::to_string(array_size) + "u) + " +
                    to_uint(idx.val, idx.width);
                return FsExpr{val_name(expr.base->ident) + "[" + base + "]",
                              xz_name(expr.base->ident) + "[" + base + "]",
                              drive_full(element_width), element_width};
              }
              std::string idx_u = to_uint(idx.val, idx.width);
              std::string guard = "(" + idx_u + " < " +
                                  std::to_string(array_size) + "u)";
              std::string xguard = xz_is_zero(idx_xz, idx.width);
              std::string base =
                  "(gid * " + std::to_string(array_size) + "u) + " + idx_u;
              if (element_width > 64) {
                std::string zero = drive_zero(element_width);
                std::string mask = mask_literal(element_width);
                std::string select_fn =
                    "gpga_wide_select_" + std::to_string(element_width);
                std::string val =
                    select_fn + "(" + xguard + " && " + guard + ", " +
                    val_name(expr.base->ident) + "[" + base + "], " + zero +
                    ")";
                std::string xz =
                    select_fn + "(" + xguard + ", " +
                    select_fn + "(" + guard + ", " +
                    xz_name(expr.base->ident) + "[" + base + "], " + zero +
                    "), " + mask + ")";
                return FsExpr{val, xz, drive_full(element_width),
                              element_width};
              }
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
            if (base.width > 64) {
              std::string idx = std::to_string(index.const_val) + "u";
            std::string val =
                "gpga_wide_get_bit_" + std::to_string(base.width) + "(" +
                base.val + ", " + idx + ")";
              std::string xz =
                  "gpga_wide_get_bit_" + std::to_string(base.width) + "(" +
                  base.xz + ", " + idx + ")";
            std::string drive =
                "gpga_wide_get_bit_" + std::to_string(base.width) + "(" +
                base.drive + ", " + idx + ")";
            return FsExpr{val, xz, drive, width};
          }
            std::string idx = std::to_string(index.const_val) + "u";
            std::string val = "(((" + base.val + " >> " + idx + ") & " +
                              literal_for_width(1, 1) + "))";
            std::string xz = "(((" + base.xz + " >> " + idx + ") & " +
                             literal_for_width(1, 1) + "))";
            std::string drive = "(((" + base.drive + " >> " + idx + ") & " +
                                 literal_for_width(1, 1) + "))";
            return FsExpr{val, xz, drive, width};
          }
          std::string cond = xz_is_zero(index.xz, index.width);
          if (base.width > 64) {
            std::string idx = to_uint(index.val, index.width);
            std::string val =
                "((" + cond + ") ? gpga_wide_get_bit_" +
                std::to_string(base.width) + "(" + base.val + ", " + idx +
                ") : 0u)";
            std::string xz =
                "((" + cond + ") ? gpga_wide_get_bit_" +
                std::to_string(base.width) + "(" + base.xz + ", " + idx +
                ") : 1u)";
            std::string drive =
                "((" + cond + ") ? gpga_wide_get_bit_" +
                std::to_string(base.width) + "(" + base.drive + ", " + idx +
                ") : 1u)";
            return FsExpr{val, xz, drive, width};
          }
          std::string idx = to_uint(index.val, index.width);
          std::string val = "((" + cond + ") ? (((" + base.val + " >> " + idx +
                            ") & " + literal_for_width(1, 1) + ")) : 0u)";
          std::string xz = "((" + cond + ") ? (((" + base.xz + " >> " + idx +
                           ") & " + literal_for_width(1, 1) + ")) : 1u)";
          std::string drive = "((" + cond + ") ? (((" + base.drive + " >> " +
                              idx + ") & " + literal_for_width(1, 1) +
                              ")) : 1u)";
          return FsExpr{val, xz, drive, width};
        }
        case ExprKind::kCall:
          if (expr.ident == "$time") {
            int width = 64;
            return FsExpr{"__gpga_time", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$fopen") {
            int width = 32;
            return FsExpr{"0u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$fclose") {
            int width = 32;
            return FsExpr{"0u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$fgetc") {
            int width = 32;
            return FsExpr{"4294967295u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$fgets") {
            int width = 32;
            return FsExpr{"0u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$feof") {
            int width = 32;
            return FsExpr{"1u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$ftell") {
            int width = 32;
            return FsExpr{"0u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$fseek") {
            int width = 32;
            return FsExpr{"0u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$ferror") {
            int width = 32;
            return FsExpr{"0u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$ungetc") {
            int width = 32;
            return FsExpr{"4294967295u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$fread") {
            int width = 32;
            return FsExpr{"0u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$fscanf" || expr.ident == "$sscanf") {
            int width = 32;
            return FsExpr{"0u", literal_for_width(0, width),
                          drive_full(width), width};
          }
          if (expr.ident == "$rtoi") {
            int width = ExprWidth(expr, module);
            std::string real_val =
                (!expr.call_args.empty() && expr.call_args.front())
                    ? emit_real_value4(*expr.call_args.front())
                    : "gpga_bits_to_real(0ul)";
            std::string raw = "gpga_double_to_s64(" + real_val + ")";
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
      if (width > 64) {
        std::string mask = mask_literal(width);
        std::string ax = MaskForWidthExpr(case_w.xz, width);
        std::string bx = MaskForWidthExpr(label_w.xz, width);
        auto case_eq_pred = [&]() -> std::string {
          std::string diff = wide_xor(ax, bx, width);
          std::string known = wide_and(
              wide_not(wide_or(ax, bx, width), width), mask, width);
          std::string val_diff = wide_and(
              wide_xor(case_w.val, label_w.val, width), known, width);
          return "(!" + wide_any(diff, width) + " && !" +
                 wide_any(val_diff, width) + ")";
        };
        if (case_kind == CaseKind::kCaseZ) {
          if (label_expr.kind != ExprKind::kNumber) {
            return case_eq_pred();
          }
          if (label_expr.x_bits != 0) {
            return "false";
          }
          uint64_t ignore_bits = label_expr.z_bits;
          if (case_expr_src && case_expr_src->kind == ExprKind::kNumber) {
            ignore_bits |= case_expr_src->z_bits;
          }
          std::string ignore = literal_for_width(ignore_bits, width);
          std::string cared =
              wide_and(wide_not(ignore, width), mask, width);
          std::string bad = wide_and(ax, cared, width);
          std::string val_diff = wide_and(
              wide_xor(case_w.val, label_w.val, width), cared, width);
          return "(!" + wide_any(bad, width) + " && !" +
                 wide_any(val_diff, width) + ")";
        }
        if (case_kind == CaseKind::kCaseX) {
          std::string cared = wide_and(
              wide_not(wide_or(ax, bx, width), width), mask, width);
          std::string val_diff = wide_and(
              wide_xor(case_w.val, label_w.val, width), cared, width);
          return "(!" + wide_any(val_diff, width) + ")";
        }
        return case_eq_pred();
      }
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
      std::string type = (expr.width > 64)
                             ? ("GpgaWideFs" + std::to_string(expr.width))
                             : ((expr.width > 32) ? "FourState64"
                                                  : "FourState32");
      std::string dtype = (expr.width > 64)
                              ? ("GpgaWide" + std::to_string(expr.width))
                              : ((expr.width > 32) ? "ulong" : "uint");
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
      if (!assign.lhs_indices.empty()) {
        std::vector<int> dims;
        int element_width = 0;
        int array_size = 0;
        if (!GetArrayDims(module, assign.lhs, &dims, &element_width,
                          &array_size)) {
          return out;
        }
        if (dims.empty() || element_width <= 0 || array_size <= 0) {
          return out;
        }
        size_t dim_count = dims.size();
        size_t index_count = assign.lhs_indices.size();
        bool has_bit_select = false;
        const Expr* bit_expr = nullptr;
        if (assign.lhs_has_range) {
          if (assign.lhs_lsb_expr) {
            return out;
          }
          if (index_count != dim_count) {
            return out;
          }
          has_bit_select = true;
          bit_expr = assign.lhs_msb_expr.get();
          if (!bit_expr) {
            return out;
          }
        } else if (index_count == dim_count + 1) {
          has_bit_select = true;
          bit_expr = assign.lhs_indices.back().get();
          index_count = dim_count;
        } else if (index_count != dim_count) {
          return out;
        }
        std::string linear;
        std::string guard;
        bool all_const = true;
        uint64_t linear_const = 0;
        for (size_t i = 0; i < dim_count; ++i) {
          const Expr* idx_expr = assign.lhs_indices[i].get();
          if (!idx_expr) {
            return out;
          }
          FsExpr idx = emit_expr4(*idx_expr);
          if (active_cse) {
            idx = maybe_hoist_full(idx, indent, false, false);
          }
          if (idx.is_const) {
            if (idx.const_xz != 0) {
              return out;
            }
            if (idx.const_val >= static_cast<uint64_t>(dims[i])) {
              return out;
            }
            linear_const = (i == 0)
                               ? idx.const_val
                               : (linear_const * dims[i] + idx.const_val);
          } else {
            all_const = false;
            std::string idx_u = to_uint(idx.val, idx.width);
            std::string cond =
                "(" + xz_is_zero(idx.xz, idx.width) + " && " + idx_u +
                " < " + std::to_string(dims[i]) + "u)";
            guard =
                guard.empty() ? cond : "(" + guard + " && " + cond + ")";
          }
          std::string idx_u = to_uint(idx.val, idx.width);
          if (linear.empty()) {
            linear = idx_u;
          } else {
            linear = "(" + linear + " * " + std::to_string(dims[i]) + "u + " +
                     idx_u + ")";
          }
        }
        if (all_const) {
          linear = std::to_string(linear_const) + "u";
        }
        std::string base = "(gid * " + std::to_string(array_size) + "u) + " +
                           linear;
        if (use_next) {
          out.val = MslValNextName(assign.lhs) + "[" + base + "]";
          out.xz = MslXzNextName(assign.lhs) + "[" + base + "]";
        } else {
          out.val = val_name(assign.lhs) + "[" + base + "]";
          out.xz = xz_name(assign.lhs) + "[" + base + "]";
        }
        out.width = element_width;
        out.ok = true;
        if (has_bit_select) {
          if (SignalIsReal(module, assign.lhs)) {
            return Lvalue4{};
          }
          FsExpr bit_idx = emit_expr4(*bit_expr);
          if (active_cse) {
            bit_idx = maybe_hoist_full(bit_idx, indent, false, false);
          }
          if (bit_idx.is_const) {
            if (bit_idx.const_xz != 0) {
              return out;
            }
            if (bit_idx.const_val >= static_cast<uint64_t>(element_width)) {
              return out;
            }
          } else {
            std::string bit_idx_u = to_uint(bit_idx.val, bit_idx.width);
            std::string bit_guard =
                "(" + xz_is_zero(bit_idx.xz, bit_idx.width) + " && " +
                bit_idx_u + " < " + std::to_string(element_width) + "u)";
            guard = guard.empty() ? bit_guard
                                  : "(" + guard + " && " + bit_guard + ")";
          }
          out.guard = guard;
          out.base_width = element_width;
          out.bit_index_val = to_u64(bit_idx.val, bit_idx.width);
          out.bit_index_xz = bit_idx.xz;
          out.width = 1;
          out.is_bit_select = true;
          return out;
        }
        out.guard = guard;
        out.is_array = true;
        return out;
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
          out.range_index_val = to_u64(idx.val, idx.width);
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
              std::string idx_u = to_uint(idx.val, idx.width);
              out.guard =
                  "(" + xz_is_zero(idx.xz, idx.width) + " && " + idx_u +
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
          std::string idx_val = to_u64(idx.val, idx.width);
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
            std::string idx_u = to_uint(idx.val, idx.width);
            out.guard =
                "(" + xz_is_zero(idx_xz, idx.width) + " && " + idx_u + " < " +
                std::to_string(base_width) + "u)";
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
        std::string idx_val = to_u64(idx.val, idx.width);
        std::string idx_xz = idx.xz;
        if (idx.is_const) {
          if (idx.const_xz != 0) {
            return out;
          }
          if (idx.const_val >= static_cast<uint64_t>(array_size)) {
            return out;
          }
        } else {
          std::string idx_u = to_uint(idx.val, idx.width);
          out.guard =
              "(" + xz_is_zero(idx_xz, idx.width) + " && " + idx_u + " < " +
              std::to_string(array_size) + "u)";
        }
        std::string base = "(gid * " + std::to_string(array_size) + "u) + " +
                           to_uint(idx.val, idx.width);
        if (use_next) {
          out.val = MslValNextName(assign.lhs) + "[" + base + "]";
          out.xz = MslXzNextName(assign.lhs) + "[" + base + "]";
        } else {
          out.val = val_name(assign.lhs) + "[" + base + "]";
          out.xz = xz_name(assign.lhs) + "[" + base + "]";
        }
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
          (initial_regs.count(net.name) > 0 ||
           initial_reads.count(net.name) > 0)) {
        init_reg_names.push_back(net.name);
      }
    }

    std::vector<const Net*> array_nets;
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        array_nets.push_back(&net);
      }
    }

    const bool pack_signals = needs_scheduler;
    const bool pack_nb = pack_signals;
    struct PackedSignal {
      std::string name;
      std::string type;
      int array_size = 1;
    };
    std::unordered_map<std::string, int> signal_array_sizes;
    signal_array_sizes.reserve(module.nets.size());
    for (const auto& net : module.nets) {
      if (net.array_size > 0) {
        signal_array_sizes[net.name] = net.array_size;
      }
    }
    auto array_size_for = [&](const std::string& name) -> int {
      auto it = signal_array_sizes.find(name);
      return (it != signal_array_sizes.end()) ? it->second : 1;
    };
    std::vector<PackedSignal> packed_signals;
    std::vector<PackedSignal> packed_nb_signals;
    bool needs_force_shadow = false;
    packed_signals.reserve(module.ports.size() * 2 +
                           reg_names.size() * 2 +
                           trireg_nets.size() * 3 +
                           array_nets.size() * 2);
    for (const auto& port : module.ports) {
      std::string type = TypeForWidth(port.width);
      PackedSignal val;
      val.name = val_name(port.name);
      val.type = type;
      val.array_size = 1;
      packed_signals.push_back(std::move(val));
      PackedSignal xz;
      xz.name = xz_name(port.name);
      xz.type = type;
      xz.array_size = 1;
      packed_signals.push_back(std::move(xz));
    }
    for (const auto& reg : reg_names) {
      std::string type = TypeForWidth(SignalWidth(module, reg));
      int arr = array_size_for(reg);
      PackedSignal val;
      val.name = val_name(reg);
      val.type = type;
      val.array_size = arr;
      packed_signals.push_back(std::move(val));
      PackedSignal xz;
      xz.name = xz_name(reg);
      xz.type = type;
      xz.array_size = arr;
      packed_signals.push_back(std::move(xz));
    }
    for (const auto* reg : trireg_nets) {
      std::string type = TypeForWidth(SignalWidth(module, reg->name));
      int arr = array_size_for(reg->name);
      PackedSignal val;
      val.name = val_name(reg->name);
      val.type = type;
      val.array_size = arr;
      packed_signals.push_back(std::move(val));
      PackedSignal xz;
      xz.name = xz_name(reg->name);
      xz.type = type;
      xz.array_size = arr;
      packed_signals.push_back(std::move(xz));
      PackedSignal decay;
      decay.name = decay_name(reg->name);
      decay.type = "ulong";
      decay.array_size = 1;
      packed_signals.push_back(std::move(decay));
    }
    for (const auto* net : array_nets) {
      std::string type = TypeForWidth(net->width);
      int arr = std::max(1, net->array_size);
      PackedSignal val;
      val.name = val_name(net->name);
      val.type = type;
      val.array_size = arr;
      packed_signals.push_back(std::move(val));
      PackedSignal xz;
      xz.name = xz_name(net->name);
      xz.type = type;
      xz.array_size = arr;
      packed_signals.push_back(std::move(xz));
    }
    auto emit_packed_signal_setup = [&](const std::string& count_expr) {
      if (!pack_signals) {
        return;
      }
      out << "  uint __gpga_count = " << count_expr << ";\n";
      out << "  ulong __gpga_offset = 0ul;\n";
      for (const auto& sig : packed_signals) {
        int array_size = std::max(1, sig.array_size);
        out << "  __gpga_offset = (__gpga_offset + 7ul) & ~7ul;\n";
        out << "  device " << sig.type << "* " << sig.name
            << " = (device " << sig.type << "*)(gpga_state + __gpga_offset);\n";
        out << "  __gpga_offset += (ulong)__gpga_count * " << array_size
            << "u * (ulong)sizeof(" << sig.type << ");\n";
      }
    };
    auto emit_packed_nb_setup = [&](const std::string& count_expr) {
      if (!pack_nb || packed_nb_signals.empty()) {
        return;
      }
      out << "  uint __gpga_nb_count = " << count_expr << ";\n";
      out << "  ulong __gpga_nb_offset = 0ul;\n";
      for (const auto& sig : packed_nb_signals) {
        int array_size = std::max(1, sig.array_size);
        out << "  __gpga_nb_offset = (__gpga_nb_offset + 7ul) & ~7ul;\n";
        out << "  device " << sig.type << "* " << sig.name
            << " = (device " << sig.type << "*)(nb_state + __gpga_nb_offset);\n";
        out << "  __gpga_nb_offset += (ulong)__gpga_nb_count * " << array_size
            << "u * (ulong)sizeof(" << sig.type << ");\n";
      }
    };
    std::vector<PackedSignal> packed_force_signals;
    auto emit_packed_force_setup = [&](const std::string& count_expr) {
      if (!needs_force_shadow) {
        return;
      }
      if (packed_force_signals.empty()) {
        packed_force_signals.reserve(packed_signals.size());
        for (const auto& sig : packed_signals) {
          PackedSignal shadow = sig;
          shadow.name = shadow_any_name(sig.name);
          packed_force_signals.push_back(std::move(shadow));
        }
      }
      out << "  uint __gpga_force_count = " << count_expr << ";\n";
      out << "  ulong __gpga_force_offset = 0ul;\n";
      for (const auto& sig : packed_force_signals) {
        int array_size = std::max(1, sig.array_size);
        out << "  __gpga_force_offset = (__gpga_force_offset + 7ul) & ~7ul;\n";
        out << "  device " << sig.type << "* " << sig.name
            << " = (device " << sig.type << "*)(sched_force_state + __gpga_force_offset);\n";
        out << "  __gpga_force_offset += (ulong)__gpga_force_count * " << array_size
            << "u * (ulong)sizeof(" << sig.type << ");\n";
      }
    };

    std::unordered_set<std::string> switch_nets;
    for (const auto& sw : module.switches) {
      switch_nets.insert(sw.a);
      switch_nets.insert(sw.b);
    }
    std::unordered_set<std::string> drive_declared;
    std::unordered_map<std::string, std::string> drive_vars;
    auto drive_var_name = [&](const std::string& name) -> std::string {
      return "__gpga_drive_" + MslName(name);
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

    out << "kernel void gpga_" << MslName(module.name) << "(";
    int buffer_index = 0;
    bool first = true;
    if (pack_signals) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      out << "  device uchar* gpga_state [[buffer(" << buffer_index++
          << ")]]";
    }
    if (!pack_signals) {
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
    if (pack_signals) {
      emit_packed_signal_setup("params.count");
    }

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
            "__gpga_drv_" + MslName(entry.first) + "_" +
            std::to_string(idx) + "_val";
        info.xz =
            "__gpga_drv_" + MslName(entry.first) + "_" +
            std::to_string(idx) + "_xz";
        info.drive =
            "__gpga_drv_" + MslName(entry.first) + "_" +
            std::to_string(idx) + "_drive";
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
        if (lhs_width > 64) {
          std::string idx = std::to_string(lo) + "u";
          std::string rhs_val_ext;
          std::string rhs_xz_ext;
          std::string rhs_drive_ext;
          if (slice_width > 64) {
            rhs_val_ext =
                "gpga_wide_resize_" + std::to_string(lhs_width) + "_from_" +
                std::to_string(slice_width) + "(" + rhs.val + ")";
            rhs_xz_ext =
                "gpga_wide_resize_" + std::to_string(lhs_width) + "_from_" +
                std::to_string(slice_width) + "(" + rhs.xz + ")";
            rhs_drive_ext =
                "gpga_wide_resize_" + std::to_string(lhs_width) + "_from_" +
                std::to_string(slice_width) + "(" + rhs.drive + ")";
          } else {
            rhs_val_ext = "gpga_wide_from_u64_" + std::to_string(lhs_width) +
                          "(" + rhs.val + ")";
            rhs_xz_ext = "gpga_wide_from_u64_" + std::to_string(lhs_width) +
                         "(" + rhs.xz + ")";
            rhs_drive_ext =
                "gpga_wide_from_u64_" + std::to_string(lhs_width) + "(" +
                rhs.drive + ")";
          }
          out << "  " << type << " " << info.val << " = "
              << wide_shl(rhs_val_ext, idx, lhs_width) << ";\n";
          out << "  " << type << " " << info.xz << " = "
              << wide_shl(rhs_xz_ext, idx, lhs_width) << ";\n";
          out << "  " << type << " " << info.drive << " = "
              << wide_shl(rhs_drive_ext, idx, lhs_width) << ";\n";
        } else {
          std::string mask = mask_literal(slice_width);
          std::string cast = CastForWidth(lhs_width);
          out << "  " << type << " " << info.val << " = ((" << cast << rhs.val
              << " & " << mask << ") << " << std::to_string(lo) << "u);\n";
          out << "  " << type << " " << info.xz << " = ((" << cast << rhs.xz
              << " & " << mask << ") << " << std::to_string(lo) << "u);\n";
          out << "  " << type << " " << info.drive << " = ((" << cast
              << rhs.drive << " & " << mask << ") << " << std::to_string(lo)
              << "u);\n";
        }
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
      std::string msl_name = MslName(name);
      std::string zero = drive_zero(lhs_width);
      std::string resolved_val = "__gpga_res_" + MslName(name) + "_val";
      std::string resolved_xz = "__gpga_res_" + MslName(name) + "_xz";
      std::string resolved_drive = "__gpga_res_" + MslName(name) + "_drive";
      out << "  " << type << " " << resolved_val << " = " << zero << ";\n";
      out << "  " << type << " " << resolved_xz << " = " << zero << ";\n";
      out << "  " << type << " " << resolved_drive << " = " << zero << ";\n";
      if (lhs_width > 64) {
        out << "  for (uint bit = 0u; bit < " << lhs_width << "u; ++bit) {\n";
        if (wired_and || wired_or) {
          out << "    bool has0 = false;\n";
          out << "    bool has1 = false;\n";
          out << "    bool hasx = false;\n";
          for (size_t idx : indices) {
            const auto& info = driver_info[idx];
            out << "    if (gpga_wide_get_bit_" << lhs_width << "("
                << info.drive << ", bit) != 0u) {\n";
            out << "      if (gpga_wide_get_bit_" << lhs_width << "("
                << info.xz << ", bit) != 0u) {\n";
            out << "        hasx = true;\n";
            out << "      } else if (gpga_wide_get_bit_" << lhs_width << "("
                << info.val << ", bit) != 0u) {\n";
            out << "        has1 = true;\n";
            out << "      } else {\n";
            out << "        has0 = true;\n";
            out << "      }\n";
            out << "    }\n";
          }
          out << "    if (!has0 && !has1 && !hasx) {\n";
          out << "      " << resolved_xz << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_xz << ", bit, 1u);\n";
          out << "      continue;\n";
          out << "    }\n";
          out << "    " << resolved_drive << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_drive << ", bit, 1u);\n";
          if (wired_and) {
            out << "    if (has0) {\n";
            out << "      // 0 dominates wired-AND\n";
            out << "    } else if (hasx) {\n";
            out << "      " << resolved_xz << " = gpga_wide_set_bit_"
                << lhs_width << "(" << resolved_xz << ", bit, 1u);\n";
            out << "    } else {\n";
            out << "      " << resolved_val << " = gpga_wide_set_bit_"
                << lhs_width << "(" << resolved_val << ", bit, 1u);\n";
            out << "    }\n";
          } else {
            out << "    if (has1) {\n";
            out << "      " << resolved_val << " = gpga_wide_set_bit_"
                << lhs_width << "(" << resolved_val << ", bit, 1u);\n";
            out << "    } else if (hasx) {\n";
            out << "      " << resolved_xz << " = gpga_wide_set_bit_"
                << lhs_width << "(" << resolved_xz << ", bit, 1u);\n";
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
            out << "    if (gpga_wide_get_bit_" << lhs_width << "("
                << info.drive << ", bit) != 0u) {\n";
            out << "      if (gpga_wide_get_bit_" << lhs_width << "("
                << info.xz << ", bit) != 0u) {\n";
            if (info.strength0 == info.strength1) {
              out << "        uint x_strength = " << info.strength0 << ";\n";
            } else {
              out << "        uint x_strength = (" << info.strength0 << " > "
                  << info.strength1 << ") ? " << info.strength0 << " : "
                  << info.strength1 << ";\n";
            }
            out << "        bestx = (bestx > x_strength) ? bestx : x_strength;\n";
            out << "      } else if (gpga_wide_get_bit_" << lhs_width << "("
                << info.val << ", bit) != 0u) {\n";
            out << "        best1 = (best1 > " << info.strength1 << ") ? best1 : "
                << info.strength1 << ";\n";
            out << "      } else {\n";
            out << "        best0 = (best0 > " << info.strength0 << ") ? best0 : "
                << info.strength0 << ";\n";
            out << "      }\n";
            out << "    }\n";
          }
          out << "    if (best0 == 0u && best1 == 0u && bestx == 0u) {\n";
          out << "      " << resolved_xz << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_xz << ", bit, 1u);\n";
          out << "      continue;\n";
          out << "    }\n";
          out << "    " << resolved_drive << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_drive << ", bit, 1u);\n";
          out << "    uint max01 = (best0 > best1) ? best0 : best1;\n";
          out << "    if ((bestx >= max01) && max01 != 0u) {\n";
          out << "      " << resolved_xz << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_xz << ", bit, 1u);\n";
          out << "    } else if (best0 > best1) {\n";
          out << "      // 0 wins\n";
          out << "    } else if (best1 > best0) {\n";
          out << "      " << resolved_val << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_val << ", bit, 1u);\n";
          out << "    } else {\n";
          out << "      " << resolved_xz << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_xz << ", bit, 1u);\n";
          out << "    }\n";
        }
        out << "  }\n";

        if (switch_nets.count(name) > 0) {
          ensure_drive_declared(name, lhs_width, drive_zero(lhs_width));
          out << "  " << drive_var_name(name) << " = " << resolved_drive
              << ";\n";
        }

        bool is_output =
            IsOutputPort(module, name) || regs_ctx.count(name) > 0;
        bool is_local =
            locals_ctx.count(name) > 0 && !is_output && regs_ctx.count(name) == 0;
        if (is_output) {
          if (is_trireg) {
            std::string decay_ref = decay_name(name) + "[gid]";
            std::string decay_delay = trireg_decay_delay(name);
            std::string drive_flag = "__gpga_trireg_drive_" + MslName(name);
            std::string decay_flag = "__gpga_trireg_decay_" + MslName(name);
            out << "  bool " << drive_flag << " = "
                << wide_any(resolved_drive, lhs_width) << ";\n";
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
            out << "  " << val_name(name) << "[gid] = "
                << wide_or(wide_and(val_name(name) + "[gid]",
                                    wide_not(resolved_drive, lhs_width),
                                    lhs_width),
                           wide_and(resolved_val, resolved_drive, lhs_width),
                           lhs_width)
                << ";\n";
            out << "  " << xz_name(name) << "[gid] = "
                << wide_or(wide_and(xz_name(name) + "[gid]",
                                    wide_not(resolved_drive, lhs_width),
                                    lhs_width),
                           wide_and(resolved_xz, resolved_drive, lhs_width),
                           lhs_width)
                << ";\n";
            out << "  if (" << decay_flag << ") {\n";
            out << "    " << xz_name(name) << "[gid] = "
                << wide_or(xz_name(name) + "[gid]",
                           drive_full(lhs_width), lhs_width)
                << ";\n";
            out << "  }\n";
          } else {
            out << "  " << val_name(name) << "[gid] = " << resolved_val
                << ";\n";
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
        return;
      }
      std::string one = (lhs_width > 32) ? "1ul" : "1u";
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
          std::string drive_flag = "__gpga_trireg_drive_" + msl_name;
          std::string decay_flag = "__gpga_trireg_decay_" + msl_name;
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
                                : ("__gpga_partial_" + MslName(name) + "_val");
            std::string temp_xz =
                target_is_local ? xz_name(name)
                                : ("__gpga_partial_" + MslName(name) + "_xz");
            bool track_drive = switch_nets.count(name) > 0;
            std::string temp_drive =
                "__gpga_partial_" + MslName(name) + "_drive";
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
              if (lhs_width > 64) {
                std::string mask;
                if (slice_width > 64) {
                  mask = "gpga_wide_resize_" + std::to_string(lhs_width) +
                         "_from_" + std::to_string(slice_width) +
                         "(gpga_wide_mask_const_" +
                         std::to_string(slice_width) + "())";
                } else {
                  uint64_t slice_mask = MaskForWidth64(slice_width);
                  mask =
                      "gpga_wide_from_u64_" + std::to_string(lhs_width) + "(" +
                      std::to_string(slice_mask) + "ul)";
                }
                std::string idx = std::to_string(lo) + "u";
                std::string shifted_mask =
                    wide_shl(mask, idx, lhs_width);
                std::string clear_mask =
                    wide_not(shifted_mask, lhs_width);
                std::string rhs_val_ext;
                std::string rhs_xz_ext;
                std::string rhs_drive_ext;
                if (slice_width > 64) {
                  rhs_val_ext =
                      "gpga_wide_resize_" + std::to_string(lhs_width) +
                      "_from_" + std::to_string(slice_width) + "(" + rhs.val +
                      ")";
                  rhs_xz_ext =
                      "gpga_wide_resize_" + std::to_string(lhs_width) +
                      "_from_" + std::to_string(slice_width) + "(" + rhs.xz +
                      ")";
                  rhs_drive_ext =
                      "gpga_wide_resize_" + std::to_string(lhs_width) +
                      "_from_" + std::to_string(slice_width) + "(" +
                      rhs.drive + ")";
                } else {
                  rhs_val_ext =
                      "gpga_wide_from_u64_" + std::to_string(lhs_width) + "(" +
                      rhs.val + ")";
                  rhs_xz_ext =
                      "gpga_wide_from_u64_" + std::to_string(lhs_width) + "(" +
                      rhs.xz + ")";
                  rhs_drive_ext =
                      "gpga_wide_from_u64_" + std::to_string(lhs_width) + "(" +
                      rhs.drive + ")";
                }
                std::string shifted_val =
                    wide_shl(rhs_val_ext, idx, lhs_width);
                std::string shifted_xz =
                    wide_shl(rhs_xz_ext, idx, lhs_width);
                out << "  " << temp_val << " = "
                    << wide_or(wide_and(temp_val, clear_mask, lhs_width),
                               shifted_val, lhs_width)
                    << ";\n";
                out << "  " << temp_xz << " = "
                    << wide_or(wide_and(temp_xz, clear_mask, lhs_width),
                               shifted_xz, lhs_width)
                    << ";\n";
                if (track_drive) {
                  std::string shifted_drive =
                      wide_shl(rhs_drive_ext, idx, lhs_width);
                  out << "  " << temp_drive << " = "
                      << wide_or(wide_and(temp_drive, clear_mask, lhs_width),
                                 shifted_drive, lhs_width)
                      << ";\n";
                }
              } else {
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
        return "(!gpga_double_is_zero(gpga_bits_to_real(" + expr.val + ")))";
      }
      if (expr.width > 64) {
        return "(!" + wide_any(expr.xz, expr.width) + " && " +
               wide_any(expr.val, expr.width) + ")";
      }
      return "(" + expr.xz + " == " + literal_for_width(0, expr.width) +
             " && " + expr.val + " != " + literal_for_width(0, expr.width) +
             ")";
    };

    auto hoist_full_for_use = [&](FsExpr expr, int indent) -> FsExpr {
      return maybe_hoist_full(expr, indent, false, true);
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

    fs_merge_expr = [&](FsExpr lhs, FsExpr rhs, int width) -> FsExpr {
      lhs = fs_resize_expr(lhs, width);
      rhs = fs_resize_expr(rhs, width);
      if (width > 64) {
        std::string mask = mask_literal(width);
        std::string ax = MaskForWidthExpr(lhs.xz, width);
        std::string bx = MaskForWidthExpr(rhs.xz, width);
        std::string ak = wide_and(wide_not(ax, width), mask, width);
        std::string bk = wide_and(wide_not(bx, width), mask, width);
        std::string same = wide_and(
            wide_and(wide_not(wide_xor(lhs.val, rhs.val, width), width), ak,
                     width),
            bk, width);
        std::string val = wide_and(lhs.val, same, width);
        std::string xz = wide_and(mask, wide_not(same, width), width);
        return FsExpr{val, xz, drive_full(width), width};
      }
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
      std::string rhs_val_masked = MaskForWidthExpr(rhs.val, 1);
      std::string rhs_xz_masked = MaskForWidthExpr(rhs.xz, 1);
      std::string update_val;
      std::string update_xz;
      if (lhs.base_width > 64) {
        update_val = "gpga_wide_set_bit_" + std::to_string(lhs.base_width) +
                     "(" + target_val + ", " + idx + ", " + rhs_val_masked +
                     ")";
        update_xz = "gpga_wide_set_bit_" + std::to_string(lhs.base_width) +
                    "(" + target_xz + ", " + idx + ", " + rhs_xz_masked + ")";
      } else {
        std::string one = (lhs.base_width > 32) ? "1ul" : "1u";
        std::string cast = CastForWidth(lhs.base_width);
        std::string mask = "(" + one + " << " + idx + ")";
        update_val =
            "(" + target_val + " & ~" + mask + ") | ((" + cast +
            rhs_val_masked + ") << " + idx + ")";
        update_xz =
            "(" + target_xz + " & ~" + mask + ") | ((" + cast +
            rhs_xz_masked + ") << " + idx + ")";
      }
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
      std::string rhs_val_masked = MaskForWidthExpr(rhs.val, lhs.width);
      std::string rhs_xz_masked = MaskForWidthExpr(rhs.xz, lhs.width);
      std::string update_val;
      std::string update_xz;
      if (lhs.base_width > 64) {
        std::string mask;
        if (lhs.width > 64) {
          mask = "gpga_wide_resize_" + std::to_string(lhs.base_width) +
                 "_from_" + std::to_string(lhs.width) +
                 "(gpga_wide_mask_const_" + std::to_string(lhs.width) + "())";
        } else {
          uint64_t slice_mask = MaskForWidth64(lhs.width);
          mask = "gpga_wide_from_u64_" + std::to_string(lhs.base_width) + "(" +
                 std::to_string(slice_mask) + "ul)";
        }
        std::string shifted_mask =
            wide_shl(mask, idx, lhs.base_width);
        std::string clear_mask =
            wide_not(shifted_mask, lhs.base_width);
        std::string rhs_val_ext;
        std::string rhs_xz_ext;
        if (lhs.width > 64) {
          rhs_val_ext = "gpga_wide_resize_" + std::to_string(lhs.base_width) +
                        "_from_" + std::to_string(lhs.width) + "(" +
                        rhs_val_masked + ")";
          rhs_xz_ext = "gpga_wide_resize_" + std::to_string(lhs.base_width) +
                       "_from_" + std::to_string(lhs.width) + "(" +
                       rhs_xz_masked + ")";
        } else {
          rhs_val_ext = "gpga_wide_from_u64_" + std::to_string(lhs.base_width) +
                        "(" + rhs_val_masked + ")";
          rhs_xz_ext = "gpga_wide_from_u64_" + std::to_string(lhs.base_width) +
                       "(" + rhs_xz_masked + ")";
        }
        std::string shifted_val =
            wide_shl(rhs_val_ext, idx, lhs.base_width);
        std::string shifted_xz =
            wide_shl(rhs_xz_ext, idx, lhs.base_width);
        update_val = wide_or(
            wide_and(target_val, clear_mask, lhs.base_width),
            shifted_val, lhs.base_width);
        update_xz = wide_or(
            wide_and(target_xz, clear_mask, lhs.base_width),
            shifted_xz, lhs.base_width);
      } else {
        uint64_t slice_mask = MaskForWidth64(lhs.width);
        uint64_t base_mask = MaskForWidth64(lhs.base_width);
        std::string suffix = (lhs.base_width > 32) ? "ul" : "u";
        std::string slice_literal = std::to_string(slice_mask) + suffix;
        std::string base_literal = std::to_string(base_mask) + suffix;
        std::string cast = CastForWidth(lhs.base_width);
        std::string mask =
            "((" + slice_literal + " << " + idx + ") & " + base_literal + ")";
        update_val =
            "(" + target_val + " & ~" + mask + ") | ((" + cast +
            rhs_val_masked + " & " + slice_literal + ") << " + idx + ")";
        update_xz =
            "(" + target_xz + " & ~" + mask + ") | ((" + cast +
            rhs_xz_masked + " & " + slice_literal + ") << " + idx + ")";
      }
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
      std::string cond_unknown = "false";
      if (sw.kind == SwitchKind::kTran) {
        cond_false = "false";
      } else if (sw.kind == SwitchKind::kTranif1 ||
                 sw.kind == SwitchKind::kTranif0) {
        FsExpr cond = sw.control ? emit_expr4(*sw.control)
                                 : FsExpr{literal_for_width(0, 1),
                                          literal_for_width(0, 1),
                                          drive_full(1), 1};
        cond = hoist_full_for_use(cond, 2);
        std::string known = xz_is_zero(cond.xz, cond.width);
        std::string is_zero = val_is_zero(cond.val, cond.width);
        std::string is_one = val_is_nonzero(cond.val, cond.width);
        cond_unknown = "!(" + known + ")";
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
        cond = hoist_full_for_use(cond, 2);
        cond_n = hoist_full_for_use(cond_n, 2);
        std::string known =
            "(" + xz_is_zero(cond.xz, cond.width) + " && " +
            xz_is_zero(cond_n.xz, cond_n.width) + ")";
        std::string on =
            "(" + val_is_nonzero(cond.val, cond.width) + " && " +
            val_is_zero(cond_n.val, cond_n.width) + ")";
        cond_unknown = "!(" + known + ")";
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
      out << "    if (" << cond_unknown << ") {\n";
      out << "      " << type << " __gpga_sw_diff_a = (" << a_tmp
          << ".val ^ " << m_val << ") | (" << a_tmp << ".xz ^ " << m_xz
          << ");\n";
      out << "      " << type << " __gpga_sw_diff_b = (" << b_tmp
          << ".val ^ " << m_val << ") | (" << b_tmp << ".xz ^ " << m_xz
          << ");\n";
      out << "      " << a_val << " = " << a_tmp << ".val;\n";
      out << "      " << a_xz << " = " << a_tmp
          << ".xz | __gpga_sw_diff_a;\n";
      out << "      " << b_val << " = " << b_tmp << ".val;\n";
      out << "      " << b_xz << " = " << b_tmp
          << ".xz | __gpga_sw_diff_b;\n";
      out << "    } else {\n";
      out << "      " << a_val << " = " << m_val << ";\n";
      out << "      " << a_xz << " = " << m_xz << ";\n";
      out << "      " << b_val << " = " << m_val << ";\n";
      out << "      " << b_xz << " = " << m_xz << ";\n";
      out << "    }\n";
      out << "    " << a_drive << " = " << m_drive << ";\n";
      out << "    " << b_drive << " = " << m_drive << ";\n";
      out << "  }\n";
    }
    out << "}\n";

    std::function<void(int)> emit_force_overrides;
    std::vector<std::string> override_target_list;
    auto emit_sched_comb_update = [&](int indent) {
      bool has_comb = !module.assigns.empty() || !module.switches.empty();
      if (!has_comb) {
        for (const auto& block : module.always_blocks) {
          if (block.edge == EdgeKind::kCombinational) {
            has_comb = true;
            break;
          }
        }
      }
      if (!has_comb) {
        return;
      }
      std::string pad(indent, ' ');
      out << pad << "{\n";
      std::unordered_set<std::string> comb_declared;
      emit_continuous_assigns(locals, regs, &comb_declared);

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
        if (locals.count(target) == 0 || comb_declared.count(target) > 0) {
          continue;
        }
        std::string type = TypeForWidth(SignalWidth(module, target));
        out << "  " << type << " " << val_name(target) << ";\n";
        out << "  " << type << " " << xz_name(target) << ";\n";
        comb_declared.insert(target);
      }
      for (const auto& target : override_target_list) {
        if (locals.count(target) == 0 || comb_declared.count(target) > 0) {
          continue;
        }
        std::string type = TypeForWidth(SignalWidth(module, target));
        out << "  " << type << " " << val_name(target) << ";\n";
        out << "  " << type << " " << xz_name(target) << ";\n";
        comb_declared.insert(target);
      }
      for (const auto& block : module.always_blocks) {
        if (block.edge != EdgeKind::kCombinational) {
          continue;
        }
        ExprCache block_cache;
        for (const auto& stmt : block.statements) {
          emit_comb_stmt(stmt, indent + 2, &block_cache);
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
        std::string cond_unknown = "false";
        if (sw.kind == SwitchKind::kTran) {
          cond_false = "false";
        } else if (sw.kind == SwitchKind::kTranif1 ||
                   sw.kind == SwitchKind::kTranif0) {
          FsExpr cond = sw.control ? emit_expr4(*sw.control)
                                   : FsExpr{literal_for_width(0, 1),
                                            literal_for_width(0, 1),
                                            drive_full(1), 1};
          cond = hoist_full_for_use(cond, 2);
          std::string known = xz_is_zero(cond.xz, cond.width);
          std::string is_zero = val_is_zero(cond.val, cond.width);
          std::string is_one = val_is_nonzero(cond.val, cond.width);
          cond_unknown = "!(" + known + ")";
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
          cond = hoist_full_for_use(cond, 2);
          cond_n = hoist_full_for_use(cond_n, 2);
          std::string known =
              "(" + xz_is_zero(cond.xz, cond.width) + " && " +
              xz_is_zero(cond_n.xz, cond_n.width) + ")";
          std::string on =
              "(" + val_is_nonzero(cond.val, cond.width) + " && " +
              val_is_zero(cond_n.val, cond_n.width) + ")";
          cond_unknown = "!(" + known + ")";
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
        out << "    if (" << cond_unknown << ") {\n";
        out << "      " << type << " __gpga_sw_diff_a = (" << a_tmp
            << ".val ^ " << m_val << ") | (" << a_tmp << ".xz ^ " << m_xz
            << ");\n";
        out << "      " << type << " __gpga_sw_diff_b = (" << b_tmp
            << ".val ^ " << m_val << ") | (" << b_tmp << ".xz ^ " << m_xz
            << ");\n";
        out << "      " << a_val << " = " << a_tmp << ".val;\n";
        out << "      " << a_xz << " = " << a_tmp
            << ".xz | __gpga_sw_diff_a;\n";
        out << "      " << b_val << " = " << b_tmp << ".val;\n";
        out << "      " << b_xz << " = " << b_tmp
            << ".xz | __gpga_sw_diff_b;\n";
        out << "    } else {\n";
        out << "      " << a_val << " = " << m_val << ";\n";
        out << "      " << a_xz << " = " << m_xz << ";\n";
        out << "      " << b_val << " = " << m_val << ";\n";
        out << "      " << b_xz << " = " << m_xz << ";\n";
        out << "    }\n";
        out << "    " << a_drive << " = " << m_drive << ";\n";
        out << "    " << b_drive << " = " << m_drive << ";\n";
        out << "  }\n";
      }
      if (emit_force_overrides) {
        emit_force_overrides(indent + 2);
      }
      out << pad << "}\n";
    };

    if (has_initial && !needs_scheduler) {
      out << "\n";
      out << "kernel void gpga_" << MslName(module.name) << "_init(";
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
      out << "kernel void gpga_" << MslName(module.name) << "_tick(";
      buffer_index = 0;
      first = true;
      if (pack_signals) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        out << "  device uchar* gpga_state [[buffer(" << buffer_index++
            << ")]]";
      }
      if (!pack_signals) {
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
      }
      for (const auto* net : array_nets) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(net->width);
        out << "  device " << type << "* " << MslValNextName(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
        out << ",\n";
        out << "  device " << type << "* " << MslXzNextName(net->name)
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
      if (pack_signals) {
        emit_packed_signal_setup("params.count");
      }
      out << "  // Tick kernel: sequential logic (posedge/negedge in v0).\n";
      for (const auto* net : array_nets) {
        out << "  for (uint i = 0u; i < " << net->array_size << "u; ++i) {\n";
        out << "    " << MslValNextName(net->name) << "[(gid * "
            << net->array_size << "u) + i] = " << val_name(net->name)
            << "[(gid * " << net->array_size << "u) + i];\n";
        out << "    " << MslXzNextName(net->name) << "[(gid * "
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
      std::vector<const AlwaysBlock*> edge_blocks;
      for (const auto& block : module.always_blocks) {
        if (block.edge == EdgeKind::kInitial) {
          initial_blocks.push_back(&block);
        } else if (block.edge == EdgeKind::kPosedge ||
                   block.edge == EdgeKind::kNegedge) {
          edge_blocks.push_back(&block);
        }
      }

      if (!initial_blocks.empty() || !edge_blocks.empty()) {
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
        std::vector<std::unique_ptr<Statement>> always_wrappers;

        int next_pid = 0;
        for (const auto* block : initial_blocks) {
          procs.push_back(ProcDef{next_pid, &block->statements, nullptr});
          proc_parent.push_back(-1);
          proc_join_tag.push_back(-1);
          ++next_pid;
        }
        always_wrappers.reserve(edge_blocks.size());
        auto make_edge_wrapper = [&](const AlwaysBlock& block) {
          auto forever_stmt = std::make_unique<Statement>();
          forever_stmt->kind = StatementKind::kForever;
          Statement event_stmt;
          event_stmt.kind = StatementKind::kEventControl;
          event_stmt.event_edge = (block.edge == EdgeKind::kPosedge)
                                      ? EventEdgeKind::kPosedge
                                      : EventEdgeKind::kNegedge;
          auto clock_expr = std::make_unique<Expr>();
          clock_expr->kind = ExprKind::kIdentifier;
          clock_expr->ident = block.clock;
          event_stmt.event_expr = std::move(clock_expr);
          event_stmt.event_body.reserve(block.statements.size());
          for (const auto& stmt : block.statements) {
            event_stmt.event_body.push_back(CloneStatement(stmt));
          }
          forever_stmt->forever_body.push_back(std::move(event_stmt));
          return forever_stmt;
        };
        for (const auto* block : edge_blocks) {
          auto wrapper = make_edge_wrapper(*block);
          procs.push_back(ProcDef{next_pid, nullptr, wrapper.get()});
          proc_parent.push_back(-1);
          proc_join_tag.push_back(-1);
          always_wrappers.push_back(std::move(wrapper));
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
          if (procs[i].body) {
            collect_forks_in_list(*procs[i].body, procs[i].pid);
          } else if (procs[i].single) {
            collect_forks(*procs[i].single, procs[i].pid);
          }
        }

        auto for_each_proc_stmt = [&](const auto& fn) {
          for (const auto& proc : procs) {
            if (proc.body) {
              for (const auto& stmt : *proc.body) {
                fn(stmt);
              }
            } else if (proc.single) {
              fn(*proc.single);
            }
          }
        };

        std::unordered_set<std::string> force_targets;
        std::unordered_set<std::string> passign_targets;
        std::vector<const Statement*> force_stmts;
        std::vector<const Statement*> passign_stmts;
        std::function<void(const Statement&)> collect_force_stmts;
        collect_force_stmts = [&](const Statement& stmt) -> void {
          if (stmt.kind == StatementKind::kForce) {
            const std::string& target = stmt.force_target;
            if (stmt.is_procedural) {
              passign_targets.insert(target);
              passign_stmts.push_back(&stmt);
            } else {
              force_targets.insert(target);
              force_stmts.push_back(&stmt);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRelease) {
            const std::string& target = stmt.release_target;
            if (stmt.is_procedural) {
              passign_targets.insert(target);
            } else {
              force_targets.insert(target);
            }
            return;
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_force_stmts(inner);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_force_stmts(inner);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            for (const auto& inner : stmt.repeat_body) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            for (const auto& inner : stmt.event_body) {
              collect_force_stmts(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFork) {
            for (const auto& inner : stmt.fork_branches) {
              collect_force_stmts(inner);
            }
            return;
          }
        };
        for_each_proc_stmt(
            [&](const Statement& stmt) { collect_force_stmts(stmt); });

        std::vector<std::string> force_target_list(force_targets.begin(),
                                                   force_targets.end());
        std::vector<std::string> passign_target_list(passign_targets.begin(),
                                                     passign_targets.end());
        std::sort(force_target_list.begin(), force_target_list.end());
        std::sort(passign_target_list.begin(), passign_target_list.end());

        std::unordered_set<std::string> override_targets(force_targets);
        override_targets.insert(passign_targets.begin(), passign_targets.end());
        override_target_list.assign(override_targets.begin(),
                                    override_targets.end());
        std::sort(override_target_list.begin(), override_target_list.end());

        std::unordered_map<std::string, uint32_t> force_target_index;
        std::unordered_map<std::string, uint32_t> passign_target_index;
        for (size_t i = 0; i < force_target_list.size(); ++i) {
          force_target_index[force_target_list[i]] =
              static_cast<uint32_t>(i);
        }
        for (size_t i = 0; i < passign_target_list.size(); ++i) {
          passign_target_index[passign_target_list[i]] =
              static_cast<uint32_t>(i);
        }

        std::unordered_map<const Statement*, uint32_t> force_stmt_ids;
        std::unordered_map<const Statement*, uint32_t> passign_stmt_ids;
        for (size_t i = 0; i < force_stmts.size(); ++i) {
          force_stmt_ids[force_stmts[i]] = static_cast<uint32_t>(i);
        }
        for (size_t i = 0; i < passign_stmts.size(); ++i) {
          passign_stmt_ids[passign_stmts[i]] = static_cast<uint32_t>(i);
        }
        std::unordered_map<std::string, std::vector<const Statement*>>
            force_stmts_by_target;
        std::unordered_map<std::string, std::vector<const Statement*>>
            passign_stmts_by_target;
        for (const auto* stmt : force_stmts) {
          force_stmts_by_target[stmt->force_target].push_back(stmt);
        }
        for (const auto* stmt : passign_stmts) {
          passign_stmts_by_target[stmt->force_target].push_back(stmt);
        }

        needs_force_shadow = !force_target_list.empty() ||
                             !passign_target_list.empty();
        std::unordered_map<std::string, bool> override_is_reg;
        for (const auto& name : override_target_list) {
          NetType net_type = SignalNetType(module, name);
          bool is_reg = (net_type == NetType::kReg || IsTriregNet(net_type));
          override_is_reg[name] = is_reg;
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
        for_each_proc_stmt([&](const Statement& stmt) { collect_waits(stmt); });

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
        for_each_proc_stmt(
            [&](const Statement& stmt) { collect_edge_waits(stmt); });

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
        for_each_proc_stmt(
            [&](const Statement& stmt) { collect_delay_assigns(stmt); });

        const uint64_t kRepeatUnrollLimit = 4096u;
        const std::unordered_map<std::string, int64_t> kRepeatEmptyParams;
        std::unordered_map<const Statement*, uint32_t> repeat_ids;
        uint32_t repeat_state_count = 0;
        auto repeat_const_count = [&](const Statement& stmt,
                                      uint64_t* out_count) -> bool {
          if (!stmt.repeat_count || !out_count) {
            return false;
          }
          FourStateValue count_value;
          if (!EvalConstExpr4State(*stmt.repeat_count, kRepeatEmptyParams,
                                   &count_value, nullptr) ||
              count_value.HasXorZ()) {
            return false;
          }
          *out_count = count_value.value_bits;
          return true;
        };
        std::function<void(const Statement&)> collect_repeat_states;
        collect_repeat_states = [&](const Statement& stmt) -> void {
          if (stmt.kind == StatementKind::kRepeat && stmt.repeat_count) {
            uint64_t count = 0;
            bool is_const = repeat_const_count(stmt, &count);
            if (!is_const || count > kRepeatUnrollLimit) {
              auto inserted = repeat_ids.emplace(&stmt, repeat_state_count);
              if (inserted.second) {
                repeat_state_count++;
              }
            } else if (count == 0u) {
              return;
            }
            for (const auto& inner : stmt.repeat_body) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kIf) {
            for (const auto& inner : stmt.then_branch) {
              collect_repeat_states(inner);
            }
            for (const auto& inner : stmt.else_branch) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            for (const auto& inner : stmt.block) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFor) {
            for (const auto& inner : stmt.for_body) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWhile) {
            for (const auto& inner : stmt.while_body) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kDelay) {
            for (const auto& inner : stmt.delay_body) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kEventControl) {
            for (const auto& inner : stmt.event_body) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kWait) {
            for (const auto& inner : stmt.wait_body) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kForever) {
            for (const auto& inner : stmt.forever_body) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kCase) {
            for (const auto& item : stmt.case_items) {
              for (const auto& inner : item.body) {
                collect_repeat_states(inner);
              }
            }
            for (const auto& inner : stmt.default_branch) {
              collect_repeat_states(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kFork) {
            for (const auto& inner : stmt.fork_branches) {
              collect_repeat_states(inner);
            }
          }
        };
        for (const auto& proc : procs) {
          if (proc.body) {
            for (const auto& stmt : *proc.body) {
              collect_repeat_states(stmt);
            }
          } else if (proc.single) {
            collect_repeat_states(*proc.single);
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
        for_each_proc_stmt(
            [&](const Statement& stmt) { collect_nb_targets(stmt); });

        std::vector<std::string> nb_targets_sorted(nb_targets.begin(),
                                                   nb_targets.end());
        std::sort(nb_targets_sorted.begin(), nb_targets_sorted.end());
        packed_nb_signals.clear();
        if (pack_nb && !nb_targets_sorted.empty()) {
          packed_nb_signals.reserve(nb_targets_sorted.size() * 2);
          for (const auto& target : nb_targets_sorted) {
            std::string type = TypeForWidth(SignalWidth(module, target));
            PackedSignal val;
            val.name = "nb_" + val_name(target);
            val.type = type;
            val.array_size = 1;
            packed_nb_signals.push_back(std::move(val));
            PackedSignal xz;
            xz.name = "nb_" + xz_name(target);
            xz.type = type;
            xz.array_size = 1;
            packed_nb_signals.push_back(std::move(xz));
          }
        }
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
        const bool has_events = !module.events.empty();
        const bool has_edges = edge_item_count > 0;
        const bool has_edge_star = edge_star_count > 0;

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
        const uint32_t repeat_count =
            static_cast<uint32_t>(repeat_state_count);
        const uint32_t delay_count = has_delayed_assigns
                                         ? static_cast<uint32_t>(delay_assigns.size())
                                         : 0u;
        const uint32_t max_dnba =
            has_delayed_nba ? static_cast<uint32_t>(delayed_nba_capacity) : 0u;
        const uint32_t monitor_count =
            static_cast<uint32_t>(system_task_info.monitor_stmts.size());
        const uint32_t monitor_max_args =
            monitor_count > 0u
                ? static_cast<uint32_t>(
                      std::max<size_t>(1, system_task_info.monitor_max_args))
                : 0u;
        const uint32_t strobe_count =
            static_cast<uint32_t>(system_task_info.strobe_stmts.size());
        const uint32_t service_max_args =
            system_task_info.has_system_tasks
                ? static_cast<uint32_t>(
                      std::max<size_t>(1, system_task_info.max_args))
                : 0u;
        const uint32_t service_wide_words_local =
            system_task_info.has_system_tasks ? service_wide_words : 0u;
        const uint32_t string_count =
            system_task_info.has_system_tasks
                ? static_cast<uint32_t>(system_task_info.string_table.size())
                : 0u;
        out << "GPGA_SCHED_DEFINE_CONSTANTS(" << procs.size() << "u, "
            << root_proc_count << "u, " << module.events.size() << "u, "
            << edge_item_count << "u, " << edge_star_count << "u, "
            << procs.size() << "u, " << procs.size() << "u, "
            << nb_targets_sorted.size() << "u, " << repeat_count << "u, "
            << delay_count << "u, " << max_dnba << "u, " << monitor_count
            << "u, " << monitor_max_args << "u, " << strobe_count << "u, "
            << service_max_args << "u, " << service_wide_words_local << "u, "
            << string_count << "u, " << force_target_list.size() << "u, "
            << passign_target_list.size() << "u)\n";
        if (system_task_info.has_system_tasks) {
          if (service_wide_words_local > 0u) {
            out << "GPGA_SCHED_DEFINE_SERVICE_RECORD_WIDE()\n";
          } else {
            out << "GPGA_SCHED_DEFINE_SERVICE_RECORD_SIMPLE()\n";
          }
        }
        out << "GPGA_SCHED_DEFINE_INDEX()\n";
        out << "GPGA_SCHED_DEFINE_PROC_PARENT(";
        for (size_t i = 0; i < procs.size(); ++i) {
          uint32_t parent =
              proc_parent[i] < 0 ? 0xFFFFFFFFu
                                 : static_cast<uint32_t>(proc_parent[i]);
          if (i > 0) {
            out << ", ";
          }
          out << parent << "u";
        }
        out << ")\n";
        out << "GPGA_SCHED_DEFINE_PROC_JOIN_TAG(";
        for (size_t i = 0; i < procs.size(); ++i) {
          uint32_t tag = proc_join_tag[i] < 0
                             ? 0xFFFFFFFFu
                             : static_cast<uint32_t>(proc_join_tag[i]);
          if (i > 0) {
            out << ", ";
          }
          out << tag << "u";
        }
        out << ")\n";

        drive_declared.clear();

        out << "\n";
        out << "kernel void gpga_" << MslName(module.name) << "_sched_step(";
        int buffer_index = 0;
        bool first = true;
        auto emit_param = [&](const std::string& text) {
          if (!first) {
            out << ",\n";
          }
          first = false;
          out << text;
        };
        if (pack_signals) {
          emit_param("  device uchar* gpga_state [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        if (!pack_signals) {
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
        }
        if (pack_nb && !packed_nb_signals.empty()) {
          emit_param("  device uchar* nb_state [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        if (!pack_nb) {
          for (const auto& target : nb_targets_sorted) {
            std::string type = TypeForWidth(SignalWidth(module, target));
            emit_param("  device " + type + "* nb_" + val_name(target) +
                       " [[buffer(" + std::to_string(buffer_index++) + ")]]");
            emit_param("  device " + type + "* nb_" + xz_name(target) +
                       " [[buffer(" + std::to_string(buffer_index++) + ")]]");
          }
        }
        for (const auto* net : nb_array_nets) {
          std::string type = TypeForWidth(net->width);
          emit_param("  device " + type + "* " +
                     MslValNextName(net->name) + " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device " + type + "* " +
                     MslXzNextName(net->name) + " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        if (needs_force_shadow) {
          emit_param("  device uchar* sched_force_state [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        if (!force_target_list.empty()) {
          emit_param("  device uint* sched_force_id [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        if (!passign_target_list.empty()) {
          emit_param("  device uint* sched_passign_id [[buffer(" +
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
        if (has_edges) {
          emit_param("  device ulong* sched_edge_prev_val [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device ulong* sched_edge_prev_xz [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        if (has_edge_star) {
          emit_param("  device ulong* sched_edge_star_prev_val [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device ulong* sched_edge_star_prev_xz [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        emit_param("  device ulong* sched_wait_time [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_join_count [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_parent [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_join_tag [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        if (repeat_state_count > 0) {
          emit_param("  device uint* sched_repeat_left [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device uint* sched_repeat_active [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        emit_param("  device ulong* sched_time [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_phase [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_flags [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        if (has_events) {
          emit_param("  device uint* sched_event_pending [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
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
        if (service_wide_words > 0u) {
          emit_param("  device ulong* sched_monitor_wide_val [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
          emit_param("  device ulong* sched_monitor_wide_xz [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
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
        emit_param("  uint gid [[thread_position_in_grid]]) {\n");
        out << "  if (gid >= sched.count) {\n";
        out << "    return;\n";
        out << "  }\n";
        if (pack_signals) {
          emit_packed_signal_setup("sched.count");
        }
        emit_packed_nb_setup("sched.count");
        emit_packed_force_setup("sched.count");
        if (system_task_info.has_system_tasks) {
          out << "  sched_service_count[gid] = 0u;\n";
        }
        out << "  ulong __gpga_time = sched_time[gid];\n";
        out << "  if ((sched_flags[gid] & GPGA_SCHED_FLAG_INITIALIZED) == 0u) {\n";
        out << "    sched_time[gid] = 0ul;\n";
        out << "    __gpga_time = 0ul;\n";
        out << "    sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
        out << "    sched_flags[gid] = GPGA_SCHED_FLAG_INITIALIZED | GPGA_SCHED_FLAG_ACTIVE_INIT;\n";
        out << "    sched_error[gid] = 0u;\n";
        for (const auto* reg : trireg_nets) {
          out << "    " << decay_name(reg->name) << "[gid] = 0ul;\n";
        }
        if (has_delayed_nba) {
          out << "    sched_dnba_count[gid] = 0u;\n";
        }
        if (has_events) {
          out << "    for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
          out << "      sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
          out << "    }\n";
        }
        if (has_edges) {
          out << "    for (uint e = 0u; e < GPGA_SCHED_EDGE_COUNT; ++e) {\n";
          out << "      uint eidx = (gid * GPGA_SCHED_EDGE_COUNT) + e;\n";
          out << "      sched_edge_prev_val[eidx] = 0ul;\n";
          out << "      sched_edge_prev_xz[eidx] = 0ul;\n";
          out << "    }\n";
        }
        if (has_edge_star) {
          out << "    for (uint s = 0u; s < GPGA_SCHED_EDGE_STAR_COUNT; ++s) {\n";
          out << "      uint sidx = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + s;\n";
          out << "      sched_edge_star_prev_val[sidx] = 0ul;\n";
          out << "      sched_edge_star_prev_xz[sidx] = 0ul;\n";
          out << "    }\n";
        }
      if (!system_task_info.monitor_stmts.empty()) {
        out << "    sched_monitor_enable[gid] = 1u;\n";
        out << "    for (uint m = 0u; m < GPGA_SCHED_MONITOR_COUNT; ++m) {\n";
        out << "      sched_monitor_active[(gid * GPGA_SCHED_MONITOR_COUNT) + m] = 0u;\n";
        out << "      for (uint a = 0u; a < GPGA_SCHED_MONITOR_MAX_ARGS; ++a) {\n";
        out << "        uint offset = ((gid * GPGA_SCHED_MONITOR_COUNT) + m) * GPGA_SCHED_MONITOR_MAX_ARGS + a;\n";
        out << "        sched_monitor_val[offset] = 0ul;\n";
        out << "        sched_monitor_xz[offset] = 0ul;\n";
        if (service_wide_words > 0u) {
          out << "        uint wide_offset = offset * GPGA_SCHED_SERVICE_WIDE_WORDS;\n";
          out << "        for (uint w = 0u; w < GPGA_SCHED_SERVICE_WIDE_WORDS; ++w) {\n";
          out << "          sched_monitor_wide_val[wide_offset + w] = 0ul;\n";
          out << "          sched_monitor_wide_xz[wide_offset + w] = 0ul;\n";
          out << "        }\n";
        }
        out << "      }\n";
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
        if (repeat_state_count > 0) {
          out << "    for (uint r = 0u; r < GPGA_SCHED_REPEAT_COUNT; ++r) {\n";
          out << "      uint ridx = (gid * GPGA_SCHED_REPEAT_COUNT) + r;\n";
          out << "      sched_repeat_left[ridx] = 0u;\n";
          out << "      sched_repeat_active[ridx] = 0u;\n";
          out << "    }\n";
        }
        if (!force_target_list.empty()) {
          out << "    for (uint f = 0u; f < GPGA_SCHED_FORCE_COUNT; ++f) {\n";
          out << "      sched_force_id[(gid * GPGA_SCHED_FORCE_COUNT) + f] = 0xFFFFFFFFu;\n";
          out << "    }\n";
        }
        if (!passign_target_list.empty()) {
          out << "    for (uint f = 0u; f < GPGA_SCHED_PCONT_COUNT; ++f) {\n";
          out << "      sched_passign_id[(gid * GPGA_SCHED_PCONT_COUNT) + f] = 0xFFFFFFFFu;\n";
          out << "    }\n";
        }
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
              std::string base = "(gid * " + std::to_string(info.array_size) +
                                 "u) + uint(" + idx_val_expr + ")";
              std::string guard =
                  "(" + idx_xz_expr + " == 0u && " + idx_val_expr + " < " +
                  std::to_string(info.array_size) + "u)";
              if (use_nb) {
                target_val = MslValNextName(info.lhs) + "[" + base + "]";
                target_xz = MslXzNextName(info.lhs) + "[" + base + "]";
              } else {
                target_val = val_name(info.lhs) + "[" + base + "]";
                target_xz = xz_name(info.lhs) + "[" + base + "]";
              }
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
            return "ulong(gpga_double_to_s64(" + real + "))";
          }
          FsExpr delay_expr = emit_expr4_sized(expr, 64);
          std::string zero = literal_for_width(0, delay_expr.width);
          return "(" + delay_expr.xz + " == " + zero + " ? " + delay_expr.val +
                 " : 0ul)";
        };
        auto force_slot_expr = [&](const std::string& target) -> std::string {
          auto it = force_target_index.find(target);
          if (it == force_target_index.end()) {
            return "";
          }
          return "(gid * GPGA_SCHED_FORCE_COUNT) + " +
                 std::to_string(it->second) + "u";
        };
        auto passign_slot_expr = [&](const std::string& target) -> std::string {
          auto it = passign_target_index.find(target);
          if (it == passign_target_index.end()) {
            return "";
          }
          return "(gid * GPGA_SCHED_PCONT_COUNT) + " +
                 std::to_string(it->second) + "u";
        };
        auto force_active_expr = [&](const std::string& target) -> std::string {
          std::string slot = force_slot_expr(target);
          if (slot.empty()) {
            return "false";
          }
          return "(sched_force_id[" + slot + "] != 0xFFFFFFFFu)";
        };
        auto passign_active_expr = [&](const std::string& target) -> std::string {
          std::string slot = passign_slot_expr(target);
          if (slot.empty()) {
            return "false";
          }
          return "(sched_passign_id[" + slot + "] != 0xFFFFFFFFu)";
        };
        auto override_active_expr = [&](const std::string& target) -> std::string {
          std::string force_active = force_active_expr(target);
          std::string passign_active = passign_active_expr(target);
          if (force_active == "false") {
            return passign_active;
          }
          if (passign_active == "false") {
            return force_active;
          }
          return "(" + force_active + " || " + passign_active + ")";
        };
        auto replace_prefix = [&](const std::string& ref,
                                  const std::string& base,
                                  const std::string& repl) -> std::string {
          if (ref.rfind(base, 0) == 0) {
            return repl + ref.substr(base.size());
          }
          return repl;
        };
        auto emit_force_value_assign =
            [&](const Statement& stmt, const std::string& target_val,
                const std::string& target_xz, int indent) -> void {
              if (!stmt.assign.rhs) {
                return;
              }
              int width = SignalWidth(module, stmt.assign.lhs);
              if (width <= 0) {
                return;
              }
              bool lhs_real = SignalIsReal(module, stmt.assign.lhs);
              FsExpr rhs = lhs_real
                               ? emit_real_expr4(*stmt.assign.rhs)
                               : emit_expr4_sized_with_cse(*stmt.assign.rhs,
                                                           width, indent);
              rhs = maybe_hoist_full(rhs, indent, false, true);
              std::string pad(indent, ' ');
              out << pad << target_val << " = " << rhs.val << ";\n";
              out << pad << target_xz << " = " << rhs.xz << ";\n";
            };

        emit_force_overrides = [&](int indent) -> void {
          if (override_target_list.empty()) {
            return;
          }
          std::string pad(indent, ' ');
          out << pad << "{\n";
          for (const auto& target : override_target_list) {
            auto force_it = force_target_index.find(target);
            auto passign_it = passign_target_index.find(target);
            if (force_it == force_target_index.end() &&
                passign_it == passign_target_index.end()) {
              continue;
            }
            SequentialAssign temp;
            temp.lhs = target;
            temp.nonblocking = false;
            Lvalue4 lhs =
                build_lvalue4(temp, sched_locals, sched_regs, false, indent + 2);
            if (!lhs.ok) {
              continue;
            }
            std::string suffix = MslName(target);
            if (force_it != force_target_index.end()) {
              std::string force_slot = force_slot_expr(target);
              out << pad << "  uint __gpga_force_id_" << suffix
                  << " = sched_force_id[" << force_slot << "];\n";
              out << pad << "  if (__gpga_force_id_" << suffix
                  << " != 0xFFFFFFFFu) {\n";
              out << pad << "    switch (__gpga_force_id_" << suffix << ") {\n";
              auto list_it = force_stmts_by_target.find(target);
              if (list_it != force_stmts_by_target.end()) {
                for (const auto* stmt : list_it->second) {
                  auto id_it = force_stmt_ids.find(stmt);
                  if (id_it == force_stmt_ids.end()) {
                    continue;
                  }
                  out << pad << "      case " << id_it->second << "u: {\n";
                  emit_force_value_assign(*stmt, lhs.val, lhs.xz, indent + 8);
                  out << pad << "        break;\n";
                  out << pad << "      }\n";
                }
              }
              out << pad << "      default:\n";
              out << pad << "        break;\n";
              out << pad << "    }\n";
              out << pad << "  }";
              if (passign_it != passign_target_index.end()) {
                out << " else {\n";
                std::string passign_slot = passign_slot_expr(target);
                out << pad << "    uint __gpga_passign_id_" << suffix
                    << " = sched_passign_id[" << passign_slot << "];\n";
                out << pad << "    if (__gpga_passign_id_" << suffix
                    << " != 0xFFFFFFFFu) {\n";
                out << pad << "      switch (__gpga_passign_id_" << suffix
                    << ") {\n";
                auto plist_it = passign_stmts_by_target.find(target);
                if (plist_it != passign_stmts_by_target.end()) {
                  for (const auto* stmt : plist_it->second) {
                    auto id_it = passign_stmt_ids.find(stmt);
                    if (id_it == passign_stmt_ids.end()) {
                      continue;
                    }
                    out << pad << "        case " << id_it->second << "u: {\n";
                    emit_force_value_assign(*stmt, lhs.val, lhs.xz,
                                            indent + 10);
                    out << pad << "          break;\n";
                    out << pad << "        }\n";
                  }
                }
                out << pad << "        default:\n";
                out << pad << "          break;\n";
                out << pad << "      }\n";
                out << pad << "    }\n";
                out << pad << "  }\n";
              } else {
                out << "\n";
              }
              continue;
            }
            if (passign_it != passign_target_index.end()) {
              std::string passign_slot = passign_slot_expr(target);
              out << pad << "  uint __gpga_passign_id_" << suffix
                  << " = sched_passign_id[" << passign_slot << "];\n";
              out << pad << "  if (__gpga_passign_id_" << suffix
                  << " != 0xFFFFFFFFu) {\n";
              out << pad << "    switch (__gpga_passign_id_" << suffix
                  << ") {\n";
              auto plist_it = passign_stmts_by_target.find(target);
              if (plist_it != passign_stmts_by_target.end()) {
                for (const auto* stmt : plist_it->second) {
                  auto id_it = passign_stmt_ids.find(stmt);
                  if (id_it == passign_stmt_ids.end()) {
                    continue;
                  }
                  out << pad << "      case " << id_it->second << "u: {\n";
                  emit_force_value_assign(*stmt, lhs.val, lhs.xz, indent + 8);
                  out << pad << "        break;\n";
                  out << pad << "      }\n";
                }
              }
              out << pad << "      default:\n";
              out << pad << "        break;\n";
              out << pad << "    }\n";
              out << pad << "  }\n";
            }
          }
          out << pad << "}\n";
        };

        out << "  sched_status[gid] = GPGA_SCHED_STATUS_RUNNING;\n";
        out << "  bool finished = false;\n";
        out << "  bool stopped = false;\n";
        out << "  uint steps = sched.max_steps;\n";
        out << "  while (steps > 0u) {\n";
        out << "    bool did_work = false;\n";
        out << "    if (sched_phase[gid] == GPGA_SCHED_PHASE_ACTIVE) {\n";
        out << "      if ((sched_flags[gid] & GPGA_SCHED_FLAG_ACTIVE_INIT) != 0u) {\n";
        out << "        sched_flags[gid] &= ~GPGA_SCHED_FLAG_ACTIVE_INIT;\n";
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
            out << "          " << MslValNextName(net->name) << "[(gid * "
                << net->array_size << "u) + i] = " << val_name(net->name)
                << "[(gid * " << net->array_size << "u) + i];\n";
            out << "          " << MslXzNextName(net->name) << "[(gid * "
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
        emit_sched_comb_update(6);
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
          auto emit_store = [&](const std::string& target_val,
                                const std::string& target_xz,
                                int store_indent) -> void {
            std::string store_pad(store_indent, ' ');
            if (lhs.is_bit_select) {
              emit_bit_select4(lhs, rhs, target_val, target_xz, store_indent);
              return;
            }
            if (lhs.is_range) {
              emit_range_select4(lhs, rhs, target_val, target_xz, store_indent);
              return;
            }
            if (!lhs.guard.empty()) {
              out << store_pad << "if " << lhs.guard << " {\n";
              out << store_pad << "  " << target_val << " = " << rhs.val
                  << ";\n";
              out << store_pad << "  " << target_xz << " = " << rhs.xz
                  << ";\n";
              out << store_pad << "}\n";
            } else {
              out << store_pad << target_val << " = " << rhs.val << ";\n";
              out << store_pad << target_xz << " = " << rhs.xz << ";\n";
            }
          };

          bool is_local = locals_override.count(assign.lhs) > 0;
          bool has_override =
              !is_local &&
              (force_target_index.count(assign.lhs) > 0 ||
               passign_target_index.count(assign.lhs) > 0);
          if (has_override) {
            std::string override_cond = override_active_expr(assign.lhs);
            std::string shadow_val =
                replace_prefix(lhs.val, val_name(assign.lhs),
                               shadow_val_name(assign.lhs));
            std::string shadow_xz =
                replace_prefix(lhs.xz, xz_name(assign.lhs),
                               shadow_xz_name(assign.lhs));
            out << pad << "if (" << override_cond << ") {\n";
            emit_store(shadow_val, shadow_xz, indent + 2);
            out << pad << "} else {\n";
            emit_store(lhs.val, lhs.xz, indent + 2);
            out << pad << "}\n";
            return;
          }
          emit_store(lhs.val, lhs.xz, indent);
          return;
        };

        auto emit_lvalue_assign =
            [&](const SequentialAssign& assign, const FsExpr& rhs, int indent,
                const std::unordered_set<std::string>& locals_override) -> void {
          std::string pad(indent, ' ');
          Lvalue4 lhs =
              build_lvalue4(assign, locals_override, sched_regs, false, indent);
          if (!lhs.ok) {
            return;
          }
          auto emit_store = [&](const std::string& target_val,
                                const std::string& target_xz,
                                int store_indent) -> void {
            std::string store_pad(store_indent, ' ');
            if (lhs.is_bit_select) {
              emit_bit_select4(lhs, rhs, target_val, target_xz, store_indent);
              return;
            }
            if (lhs.is_range) {
              emit_range_select4(lhs, rhs, target_val, target_xz, store_indent);
              return;
            }
            if (!lhs.guard.empty()) {
              out << store_pad << "if " << lhs.guard << " {\n";
              out << store_pad << "  " << target_val << " = " << rhs.val
                  << ";\n";
              out << store_pad << "  " << target_xz << " = " << rhs.xz
                  << ";\n";
              out << store_pad << "}\n";
            } else {
              out << store_pad << target_val << " = " << rhs.val << ";\n";
              out << store_pad << target_xz << " = " << rhs.xz << ";\n";
            }
          };

          bool is_local = locals_override.count(assign.lhs) > 0;
          bool has_override =
              !is_local &&
              (force_target_index.count(assign.lhs) > 0 ||
               passign_target_index.count(assign.lhs) > 0);
          if (has_override) {
            std::string override_cond = override_active_expr(assign.lhs);
            std::string shadow_val =
                replace_prefix(lhs.val, val_name(assign.lhs),
                               shadow_val_name(assign.lhs));
            std::string shadow_xz =
                replace_prefix(lhs.xz, xz_name(assign.lhs),
                               shadow_xz_name(assign.lhs));
            out << pad << "if (" << override_cond << ") {\n";
            emit_store(shadow_val, shadow_xz, indent + 2);
            out << pad << "} else {\n";
            emit_store(lhs.val, lhs.xz, indent + 2);
            out << pad << "}\n";
            return;
          }
          emit_store(lhs.val, lhs.xz, indent);
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
          auto emit_store = [&](const std::string& target_val,
                                const std::string& target_xz,
                                int store_indent) -> void {
            std::string store_pad(store_indent, ' ');
            if (lhs.is_bit_select) {
              emit_bit_select4(lhs, rhs, target_val, target_xz, store_indent);
              return;
            }
            if (lhs.is_range) {
              emit_range_select4(lhs, rhs, target_val, target_xz, store_indent);
              return;
            }
            if (!lhs.guard.empty()) {
              out << store_pad << "if " << lhs.guard << " {\n";
              out << store_pad << "  " << target_val << " = " << rhs.val
                  << ";\n";
              out << store_pad << "  " << target_xz << " = " << rhs.xz
                  << ";\n";
              out << store_pad << "}\n";
            } else {
              out << store_pad << target_val << " = " << rhs.val << ";\n";
              out << store_pad << target_xz << " = " << rhs.xz << ";\n";
            }
          };

          bool is_local = locals_override.count(name) > 0;
          bool has_override =
              !is_local &&
              (force_target_index.count(name) > 0 ||
               passign_target_index.count(name) > 0);
          if (has_override) {
            std::string override_cond = override_active_expr(name);
            std::string shadow_val =
                replace_prefix(lhs.val, val_name(name), shadow_val_name(name));
            std::string shadow_xz =
                replace_prefix(lhs.xz, xz_name(name), shadow_xz_name(name));
            out << std::string(indent, ' ') << "if (" << override_cond
                << ") {\n";
            emit_store(shadow_val, shadow_xz, indent + 2);
            out << std::string(indent, ' ') << "} else {\n";
            emit_store(lhs.val, lhs.xz, indent + 2);
            out << std::string(indent, ' ') << "}\n";
            return;
          }
          emit_store(lhs.val, lhs.xz, indent);
        };

        auto emit_passign_apply_target =
            [&](const std::string& target, const Lvalue4& lhs,
                int indent) -> void {
              auto list_it = passign_stmts_by_target.find(target);
              if (list_it == passign_stmts_by_target.end()) {
                return;
              }
              std::string pad(indent, ' ');
              std::string slot = passign_slot_expr(target);
              std::string suffix = MslName(target);
              out << pad << "uint __gpga_passign_id_" << suffix
                  << " = sched_passign_id[" << slot << "];\n";
              out << pad << "if (__gpga_passign_id_" << suffix
                  << " != 0xFFFFFFFFu) {\n";
              out << pad << "  switch (__gpga_passign_id_" << suffix << ") {\n";
              for (const auto* stmt : list_it->second) {
                auto id_it = passign_stmt_ids.find(stmt);
                if (id_it == passign_stmt_ids.end()) {
                  continue;
                }
                out << pad << "    case " << id_it->second << "u: {\n";
                emit_force_value_assign(*stmt, lhs.val, lhs.xz, indent + 6);
                out << pad << "      break;\n";
                out << pad << "    }\n";
              }
              out << pad << "    default:\n";
              out << pad << "      break;\n";
              out << pad << "  }\n";
              out << pad << "}\n";
            };

        struct ServiceArg {
          std::string kind;
          int width = 0;
          std::string val;
          std::string xz;
          bool wide = false;
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
                size_t arg_start, std::string* format_id_expr,
                std::vector<ServiceArg>* args) -> bool {
          if (!format_id_expr || !args) {
            return false;
          }
          *format_id_expr = "GPGA_SERVICE_INVALID_ID";
          if (stmt.task_args.size() > arg_start &&
              stmt.task_args[arg_start]->kind == ExprKind::kString) {
            uint32_t id = 0;
            if (!string_id_for(stmt.task_args[arg_start]->string_value, &id)) {
              return false;
            }
            *format_id_expr = std::to_string(id) + "u";
          }

          std::vector<char> format_specs;
          bool has_format_specs =
              stmt.task_args.size() > arg_start &&
              stmt.task_args[arg_start] &&
              stmt.task_args[arg_start]->kind == ExprKind::kString;
          if (has_format_specs) {
            format_specs =
                ExtractFormatSpecs(stmt.task_args[arg_start]->string_value);
          }
          size_t format_arg_index = 0;

          bool requires_string =
              name == "$dumpfile" || name == "$readmemh" ||
              name == "$readmemb" || name == "$writememh" ||
              name == "$writememb";
          if (requires_string &&
              *format_id_expr == "GPGA_SERVICE_INVALID_ID") {
            return false;
          }

          bool ident_as_string = TaskTreatsIdentifierAsString(name);
          args->clear();
          if (stmt.task_args.size() > arg_start) {
            args->reserve(stmt.task_args.size() - arg_start);
          }
          for (size_t i = arg_start; i < stmt.task_args.size(); ++i) {
            const auto& arg = stmt.task_args[i];
            if (!arg) {
              continue;
            }
            bool is_format_literal = has_format_specs && i == arg_start &&
                                     arg->kind == ExprKind::kString;
            char spec = '\0';
            if (has_format_specs && !is_format_literal) {
              if (format_arg_index < format_specs.size()) {
                spec = format_specs[format_arg_index];
              }
              ++format_arg_index;
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
            if (spec == 's' && arg->kind == ExprKind::kIdentifier) {
              uint32_t id = 0;
              if (!string_id_for(arg->ident, &id)) {
                return false;
              }
              int width = SignalWidth(module, arg->ident);
              if (width <= 0) {
                width = 1;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                          std::to_string(id) + "ul", "0ul"});
              continue;
            }
            if (arg->kind == ExprKind::kCall && arg->ident == "$time") {
              args->push_back(
                  ServiceArg{"GPGA_SERVICE_ARG_VALUE", 64, "__gpga_time",
                             "0ul"});
              continue;
            }
            if (arg->kind == ExprKind::kCall && arg->ident == "$stime") {
              args->push_back(
                  ServiceArg{"GPGA_SERVICE_ARG_VALUE", 32, "uint(__gpga_time)",
                             "0u"});
              continue;
            }
            bool is_real = ExprIsRealValue(*arg, module);
            int width = is_real ? 64 : ExprWidth(*arg, module);
            if (width <= 0) {
              width = 1;
            }
            FsExpr value = emit_expr4_sized(*arg, width);
            bool wide = !is_real && width > 64;
            std::string kind = is_real
                                   ? "GPGA_SERVICE_ARG_REAL"
                                   : (wide ? "GPGA_SERVICE_ARG_WIDE"
                                           : "GPGA_SERVICE_ARG_VALUE");
            std::string val = wide ? value.val : to_ulong(value.val, width);
            std::string xz = wide ? value.xz : to_ulong(value.xz, width);
            args->push_back(ServiceArg{kind, width, val, xz, wide});
          }
          return true;
        };

        auto build_syscall_args =
            [&](const Expr& call, const std::string& name,
                std::string* format_id_expr,
                std::vector<ServiceArg>* args) -> bool {
          if (!format_id_expr || !args) {
            return false;
          }
          *format_id_expr = "GPGA_SERVICE_INVALID_ID";
          args->clear();
          args->reserve(call.call_args.size());
          for (size_t i = 0; i < call.call_args.size(); ++i) {
            const Expr* arg = call.call_args[i].get();
            if (!arg) {
              continue;
            }
          if (name == "$fgets" && i == 0) {
            if (arg->kind != ExprKind::kIdentifier) {
              return false;
            }
            uint32_t id = 0;
            if (!string_id_for(arg->ident, &id)) {
              return false;
            }
            int width = SignalWidth(module, arg->ident);
            if (width <= 0) {
              width = 1;
            }
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                        std::to_string(id) + "ul", "0ul"});
            continue;
          }
          if (name == "$fread" && i == 0) {
            if (arg->kind != ExprKind::kIdentifier) {
              return false;
            }
            uint32_t id = 0;
            if (!string_id_for(arg->ident, &id)) {
              return false;
            }
            int width = SignalWidth(module, arg->ident);
            if (width <= 0) {
              width = 1;
            }
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                        std::to_string(id) + "ul", "0ul"});
            continue;
          }
            if ((name == "$fscanf" || name == "$sscanf") && i >= 2) {
              if (arg->kind != ExprKind::kIdentifier) {
                return false;
              }
              uint32_t id = 0;
              if (!string_id_for(arg->ident, &id)) {
                return false;
              }
              int width = SignalWidth(module, arg->ident);
              if (width <= 0) {
                width = 1;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                          std::to_string(id) + "ul", "0ul"});
              continue;
            }
            if (name == "$value$plusargs" && i >= 1) {
              if (arg->kind != ExprKind::kIdentifier) {
                return false;
              }
              uint32_t id = 0;
              if (!string_id_for(arg->ident, &id)) {
                return false;
              }
              int width = SignalWidth(module, arg->ident);
              if (width <= 0) {
                width = 1;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                          std::to_string(id) + "ul", "0ul"});
              continue;
            }
            if (name == "$sscanf" && i == 0) {
              if (arg->kind == ExprKind::kString) {
                uint32_t id = 0;
                if (!string_id_for(arg->string_value, &id)) {
                  return false;
                }
                args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                            std::to_string(id) + "ul", "0ul"});
                continue;
              }
              if (arg->kind == ExprKind::kIdentifier) {
                uint32_t id = 0;
                if (!string_id_for(arg->ident, &id)) {
                  return false;
                }
                int width = SignalWidth(module, arg->ident);
                if (width <= 0) {
                  width = 1;
                }
                args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                            std::to_string(id) + "ul", "0ul"});
                continue;
              }
              return false;
            }
            if ((name == "$test$plusargs" || name == "$value$plusargs") &&
                i == 0) {
              if (arg->kind == ExprKind::kString) {
                uint32_t id = 0;
                if (!string_id_for(arg->string_value, &id)) {
                  return false;
                }
                *format_id_expr = std::to_string(id) + "u";
                args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                            std::to_string(id) + "ul", "0ul"});
                continue;
              }
              if (arg->kind == ExprKind::kIdentifier) {
                uint32_t id = 0;
                if (!string_id_for(arg->ident, &id)) {
                  return false;
                }
                *format_id_expr = std::to_string(id) + "u";
                args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", 0,
                                            std::to_string(id) + "ul", "0ul"});
                continue;
              }
              return false;
            }
            if (name == "$fopen" && i < 2) {
              if (arg->kind == ExprKind::kString) {
                uint32_t id = 0;
                if (!string_id_for(arg->string_value, &id)) {
                  return false;
                }
                args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                            std::to_string(id) + "ul", "0ul"});
                continue;
              }
              if (arg->kind == ExprKind::kIdentifier) {
                uint32_t id = 0;
                if (!string_id_for(arg->ident, &id)) {
                  return false;
                }
                args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", 0,
                                            std::to_string(id) + "ul", "0ul"});
                continue;
              }
              return false;
            }
            if ((name == "$fscanf" || name == "$sscanf") && i == 1 &&
                arg->kind == ExprKind::kString) {
              uint32_t id = 0;
              if (!string_id_for(arg->string_value, &id)) {
                return false;
              }
              *format_id_expr = std::to_string(id) + "u";
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                          std::to_string(id) + "ul", "0ul"});
              continue;
            }
            bool is_real = ExprIsRealValue(*arg, module);
            int width = is_real ? 64 : ExprWidth(*arg, module);
            if (width <= 0) {
              width = 1;
            }
            FsExpr value = emit_expr4_sized(*arg, width);
            bool wide = !is_real && width > 64;
            std::string kind = is_real
                                   ? "GPGA_SERVICE_ARG_REAL"
                                   : (wide ? "GPGA_SERVICE_ARG_WIDE"
                                           : "GPGA_SERVICE_ARG_VALUE");
            std::string val = wide ? value.val : to_ulong(value.val, width);
            std::string xz = wide ? value.xz : to_ulong(value.xz, width);
            args->push_back(ServiceArg{kind, width, val, xz, wide});
          }
          return true;
        };

        auto emit_service_args =
            [&](const std::vector<ServiceArg>& args, int indent) -> void {
          std::string pad(indent, ' ');
          for (size_t i = 0; i < args.size(); ++i) {
            out << pad << "    sched_service[__gpga_svc_offset].arg_kind[" << i
                << "] = " << args[i].kind << ";\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_width[" << i
                << "] = " << args[i].width << "u;\n";
            if (args[i].wide) {
              std::string type = TypeForWidth(args[i].width);
              out << pad << "    " << type << " __gpga_wide_val" << i << " = "
                  << args[i].val << ";\n";
              out << pad << "    " << type << " __gpga_wide_xz" << i << " = "
                  << args[i].xz << ";\n";
              out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
                  << "] = gpga_wide_to_u64_" << args[i].width
                  << "(__gpga_wide_val" << i << ");\n";
              out << pad << "    sched_service[__gpga_svc_offset].arg_xz[" << i
                  << "] = gpga_wide_to_u64_" << args[i].width
                  << "(__gpga_wide_xz" << i << ");\n";
              int word_count = (args[i].width + 63) / 64;
              out << pad << "    uint __gpga_wide_base" << i << " = " << i
                  << "u * GPGA_SCHED_SERVICE_WIDE_WORDS;\n";
              out << pad << "    for (uint __gpga_wide_word = 0u; "
                         "__gpga_wide_word < "
                  << word_count << "u; ++__gpga_wide_word) {\n";
              out << pad << "      sched_service[__gpga_svc_offset].arg_wide_val"
                         "[__gpga_wide_base"
                  << i << " + __gpga_wide_word] = __gpga_wide_val" << i
                  << ".w[__gpga_wide_word];\n";
              out << pad << "      sched_service[__gpga_svc_offset].arg_wide_xz"
                         "[__gpga_wide_base"
                  << i << " + __gpga_wide_word] = __gpga_wide_xz" << i
                  << ".w[__gpga_wide_word];\n";
              out << pad << "    }\n";
            } else {
              out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
                  << "] = " << args[i].val << ";\n";
              out << pad << "    sched_service[__gpga_svc_offset].arg_xz[" << i
                  << "] = " << args[i].xz << ";\n";
            }
          }
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
          emit_service_args(args, indent);
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
          emit_service_args(args, indent);
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
          emit_service_args(args, indent);
          out << pad << "  }\n";
          out << pad << "}\n";
        };

        struct BodyCase {
          int pc = 0;
          const Statement* owner = nullptr;
          std::vector<const Statement*> body;
          int next_pc = 0;
          int loop_pc = -1;
          bool is_forever_body = false;
          bool is_assign_delay = false;
          int delay_id = -1;
          bool is_service_resume = false;
          bool is_service_cond = false;
          int service_width = 0;
          bool service_invert = false;
          int service_true_pc = 0;
          int service_false_pc = 0;
        };
        int pc_counter = 0;
        std::vector<BodyCase> body_cases;

        auto emit_syscall_assign =
            [&](const Statement& stmt, const Expr& call, int resume_pc,
                int indent) -> bool {
          if (call.kind != ExprKind::kCall ||
              !IsFileSystemFunctionName(call.ident)) {
            return false;
          }
          const char* kind_expr = nullptr;
          if (call.ident == "$fopen") {
            kind_expr = "GPGA_SERVICE_KIND_FOPEN";
          } else if (call.ident == "$fclose") {
            kind_expr = "GPGA_SERVICE_KIND_FCLOSE";
          } else if (call.ident == "$fgetc") {
            kind_expr = "GPGA_SERVICE_KIND_FGETC";
          } else if (call.ident == "$fgets") {
            kind_expr = "GPGA_SERVICE_KIND_FGETS";
          } else if (call.ident == "$feof") {
            kind_expr = "GPGA_SERVICE_KIND_FEOF";
          } else if (call.ident == "$ftell") {
            kind_expr = "GPGA_SERVICE_KIND_FTELL";
          } else if (call.ident == "$fseek") {
            kind_expr = "GPGA_SERVICE_KIND_FSEEK";
          } else if (call.ident == "$ferror") {
            kind_expr = "GPGA_SERVICE_KIND_FERROR";
          } else if (call.ident == "$ungetc") {
            kind_expr = "GPGA_SERVICE_KIND_FUNGETC";
          } else if (call.ident == "$fread") {
            kind_expr = "GPGA_SERVICE_KIND_FREAD";
          } else if (call.ident == "$fscanf") {
            kind_expr = "GPGA_SERVICE_KIND_FSCANF";
          } else if (call.ident == "$sscanf") {
            kind_expr = "GPGA_SERVICE_KIND_SSCANF";
          } else if (call.ident == "$test$plusargs") {
            kind_expr = "GPGA_SERVICE_KIND_TESTPLUSARGS";
          } else if (call.ident == "$value$plusargs") {
            kind_expr = "GPGA_SERVICE_KIND_VALUEPLUSARGS";
          }
          if (!kind_expr) {
            return false;
          }
          std::string format_id_expr;
          std::vector<ServiceArg> args;
          if (!build_syscall_args(call, call.ident, &format_id_expr, &args)) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return true;
          }
          int width = SignalWidth(module, stmt.assign.lhs);
          if (width <= 0) {
            width = ExprWidth(call, module);
          }
          int body_pc = pc_counter++;
          BodyCase body_case;
          body_case.pc = body_pc;
          body_case.owner = &stmt;
          body_case.next_pc = resume_pc;
          body_case.is_service_resume = true;
          body_case.service_width = width;
          body_cases.push_back(std::move(body_case));

          emit_service_record(kind_expr, format_id_expr, args, indent);
          out << std::string(indent, ' ')
              << "sched_wait_kind[idx] = GPGA_SCHED_WAIT_SERVICE;\n";
          out << std::string(indent, ' ') << "sched_wait_time[idx] = 0ul;\n";
          out << std::string(indent, ' ') << "sched_pc[idx] = " << body_pc
              << "u;\n";
          out << std::string(indent, ' ')
              << "sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
          return true;
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
            if (args[i].kind != "GPGA_SERVICE_ARG_VALUE" &&
                args[i].kind != "GPGA_SERVICE_ARG_REAL" &&
                args[i].kind != "GPGA_SERVICE_ARG_WIDE") {
              continue;
            }
            int width = args[i].width;
            if (width <= 0) {
              width = 1;
            }
            uint64_t mask = MaskForWidth64(width);
            std::string mask_literal = std::to_string(mask) + "ul";
            std::string val_expr = args[i].val;
            std::string xz_expr = args[i].xz;
            if (args[i].wide) {
              val_expr = "gpga_wide_to_u64_" + std::to_string(width) + "(" +
                         args[i].val + ")";
              xz_expr = "gpga_wide_to_u64_" + std::to_string(width) + "(" +
                        args[i].xz + ")";
            }
            out << pad << "ulong " << prefix << "_val" << i << " = ("
                << val_expr << ") & " << mask_literal << ";\n";
            out << pad << "ulong " << prefix << "_xz" << i << " = ("
                << xz_expr << ") & " << mask_literal << ";\n";
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
            if (args[i].wide && service_wide_words > 0u) {
              int word_count = (width + 63) / 64;
              int last_bits = width - (word_count - 1) * 64;
              uint64_t last_mask = MaskForWidth64(last_bits);
              std::string type = TypeForWidth(width);
              out << pad << type << " " << prefix << "_wide_val" << i << " = "
                  << args[i].val << ";\n";
              out << pad << type << " " << prefix << "_wide_xz" << i << " = "
                  << args[i].xz << ";\n";
              out << pad << "uint " << prefix << "_wbase" << i << " = "
                  << prefix << "_slot" << i
                  << " * GPGA_SCHED_SERVICE_WIDE_WORDS;\n";
              out << pad << "for (uint __gpga_wide_word" << i
                  << " = 0u; __gpga_wide_word" << i << " < " << word_count
                  << "u; ++__gpga_wide_word" << i << ") {\n";
              out << pad << "  ulong __gpga_wide_mask" << i
                  << " = 0xFFFFFFFFFFFFFFFFul;\n";
              if (last_bits < 64) {
                out << pad << "  if (__gpga_wide_word" << i << " == "
                    << (word_count - 1) << "u) {\n";
                out << pad << "    __gpga_wide_mask" << i << " = "
                    << std::to_string(last_mask) << "ul;\n";
                out << pad << "  }\n";
              }
              out << pad << "  ulong __gpga_wide_val" << i << "_w = "
                  << prefix << "_wide_val" << i << ".w[__gpga_wide_word" << i
                  << "] & __gpga_wide_mask" << i << ";\n";
              out << pad << "  ulong __gpga_wide_xz" << i << "_w = "
                  << prefix << "_wide_xz" << i << ".w[__gpga_wide_word" << i
                  << "] & __gpga_wide_mask" << i << ";\n";
              out << pad << "  uint __gpga_wide_slot" << i << " = "
                  << prefix << "_wbase" << i
                  << " + __gpga_wide_word" << i << ";\n";
              out << pad << "  if ((((sched_monitor_wide_val[__gpga_wide_slot"
                  << i << "] ^ __gpga_wide_val" << i
                  << "_w) | (sched_monitor_wide_xz[__gpga_wide_slot" << i
                  << "] ^ __gpga_wide_xz" << i << "_w)) & __gpga_wide_mask"
                  << i << ") != 0ul) {\n";
              out << pad << "    " << changed << " = true;\n";
              out << pad << "  }\n";
              out << pad << "  sched_monitor_wide_val[__gpga_wide_slot" << i
                  << "] = __gpga_wide_val" << i << "_w;\n";
              out << pad << "  sched_monitor_wide_xz[__gpga_wide_slot" << i
                  << "] = __gpga_wide_xz" << i << "_w;\n";
              out << pad << "}\n";
            }
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
          if (name == "$sformat") {
            if (stmt.task_args.size() < 2 || !stmt.task_args[0]) {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            const Expr* target = stmt.task_args[0].get();
            if (!target || target->kind != ExprKind::kIdentifier) {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            std::string format_id_expr;
            std::vector<ServiceArg> args;
            if (!build_service_args(stmt, name, 1, &format_id_expr, &args) ||
                format_id_expr == "GPGA_SERVICE_INVALID_ID") {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            uint32_t target_id = 0;
            if (!string_id_for(target->ident, &target_id)) {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            int width = SignalWidth(module, target->ident);
            if (width <= 0) {
              width = 1;
            }
            ServiceArg target_arg{"GPGA_SERVICE_ARG_IDENT", width,
                                  std::to_string(target_id) + "ul", "0ul"};
            args.insert(args.begin(), target_arg);
            emit_service_record("GPGA_SERVICE_KIND_SFORMAT", format_id_expr,
                                args, indent);
            return;
          }
          const char* kind_expr = nullptr;
          size_t arg_start = 0;
          bool guard_file_fd = false;
          FsExpr fd_expr;
          bool has_fd_expr = false;
          if (name == "$display") {
            kind_expr = "GPGA_SERVICE_KIND_DISPLAY";
          } else if (name == "$write") {
            kind_expr = "GPGA_SERVICE_KIND_WRITE";
          } else if (name == "$fdisplay") {
            kind_expr = "GPGA_SERVICE_KIND_FDISPLAY";
            arg_start = 1;
            guard_file_fd = true;
          } else if (name == "$monitor") {
            kind_expr = "GPGA_SERVICE_KIND_MONITOR";
          } else if (name == "$finish") {
            kind_expr = "GPGA_SERVICE_KIND_FINISH";
          } else if (name == "$stop") {
            kind_expr = "GPGA_SERVICE_KIND_STOP";
          } else if (name == "$fwrite") {
            kind_expr = "GPGA_SERVICE_KIND_FWRITE";
            arg_start = 1;
            guard_file_fd = true;
        } else if (name == "$fclose") {
          kind_expr = "GPGA_SERVICE_KIND_FCLOSE";
          guard_file_fd = true;
        } else if (name == "$fflush") {
          kind_expr = "GPGA_SERVICE_KIND_FFLUSH";
          guard_file_fd = !stmt.task_args.empty();
        } else if (name == "$ftell") {
          kind_expr = "GPGA_SERVICE_KIND_FTELL";
          guard_file_fd = true;
          } else if (name == "$rewind") {
            kind_expr = "GPGA_SERVICE_KIND_REWIND";
            guard_file_fd = true;
          } else if (name == "$dumpfile") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPFILE";
          } else if (name == "$dumpvars") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPVARS";
          } else if (name == "$readmemh") {
            kind_expr = "GPGA_SERVICE_KIND_READMEMH";
          } else if (name == "$readmemb") {
            kind_expr = "GPGA_SERVICE_KIND_READMEMB";
          } else if (name == "$writememh") {
            kind_expr = "GPGA_SERVICE_KIND_WRITEMEMH";
          } else if (name == "$writememb") {
            kind_expr = "GPGA_SERVICE_KIND_WRITEMEMB";
          } else if (name == "$dumpoff") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPOFF";
          } else if (name == "$dumpon") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPON";
          } else if (name == "$dumpflush") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPFLUSH";
          } else if (name == "$dumpall") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPALL";
          } else if (name == "$dumplimit") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPLIMIT";
          } else if (name == "$timeformat") {
            kind_expr = "GPGA_SERVICE_KIND_TIMEFORMAT";
          } else if (name == "$printtimescale") {
            kind_expr = "GPGA_SERVICE_KIND_PRINTTIMESCALE";
          } else {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }

          std::string format_id_expr;
          std::vector<ServiceArg> args;
          std::string fd_guard;
          if (guard_file_fd) {
            if (stmt.task_args.empty() || !stmt.task_args[0]) {
              out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
              out << std::string(indent, ' ')
                  << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            fd_expr = emit_expr4_sized(*stmt.task_args[0], 32);
            has_fd_expr = true;
            fd_expr = maybe_hoist_full(fd_expr, indent, false, false);
            std::string zero = literal_for_width(0, fd_expr.width);
            fd_guard =
                "(" + fd_expr.xz + " == " + zero + " && " + fd_expr.val +
                " != " + zero + ")";
          }
          if (!build_service_args(stmt, name, arg_start, &format_id_expr,
                                  &args)) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          if (guard_file_fd && has_fd_expr && arg_start > 0) {
            args.insert(
                args.begin(),
                ServiceArg{"GPGA_SERVICE_ARG_VALUE", 32,
                           to_ulong(fd_expr.val, 32),
                           to_ulong(fd_expr.xz, 32)});
          }

          bool dump_control =
              name == "$dumpfile" || name == "$dumpvars" ||
              name == "$dumpoff" || name == "$dumpon" ||
              name == "$dumpflush" || name == "$dumpall" ||
              name == "$dumplimit" || name == "$writememh" ||
              name == "$writememb";

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
          } else if (dump_control) {
            std::string pad(indent, ' ');
            out << pad << "if (gid == 0u) {\n";
            emit_service_record(kind_expr, format_id_expr, args, indent + 2);
            out << pad << "}\n";
          } else if (guard_file_fd) {
            std::string pad(indent, ' ');
            out << pad << "if (" << fd_guard << ") {\n";
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
              int resume_pc, const auto& self) -> void {
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
          if (stmt.kind == StatementKind::kForce ||
              stmt.kind == StatementKind::kRelease) {
            bool is_proc = stmt.is_procedural;
            const std::string& target =
                (stmt.kind == StatementKind::kForce) ? stmt.force_target
                                                     : stmt.release_target;
            auto target_it = is_proc ? passign_target_index.find(target)
                                     : force_target_index.find(target);
            if (target_it == (is_proc ? passign_target_index.end()
                                      : force_target_index.end())) {
              out << pad << "sched_error[gid] = 1u;\n";
              out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            if (stmt.kind == StatementKind::kForce) {
              if (stmt.assign.delay) {
                out << pad << "sched_error[gid] = 1u;\n";
                out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                return;
              }
              auto id_it = is_proc ? passign_stmt_ids.find(&stmt)
                                   : force_stmt_ids.find(&stmt);
              if (id_it == (is_proc ? passign_stmt_ids.end()
                                    : force_stmt_ids.end())) {
                out << pad << "sched_error[gid] = 1u;\n";
                out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                return;
              }
              Lvalue4 lhs =
                  build_lvalue4(stmt.assign, locals_override, sched_regs,
                                false, indent);
              if (!lhs.ok) {
                out << pad << "sched_error[gid] = 1u;\n";
                out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                return;
              }
              if (override_is_reg[target]) {
                out << pad << "if (!(" << override_active_expr(target)
                    << ")) {\n";
                out << pad << "  " << shadow_val_name(target) << "[gid] = "
                    << lhs.val << ";\n";
                out << pad << "  " << shadow_xz_name(target) << "[gid] = "
                    << lhs.xz << ";\n";
                out << pad << "}\n";
              }
              std::string slot =
                  is_proc ? passign_slot_expr(target)
                          : force_slot_expr(target);
              if (is_proc) {
                out << pad << "sched_passign_id[" << slot << "] = "
                    << id_it->second << "u;\n";
                std::string force_active = force_active_expr(target);
                if (force_active != "false") {
                  out << pad << "if (!" << force_active << ") {\n";
                  emit_force_value_assign(stmt, lhs.val, lhs.xz, indent + 2);
                  out << pad << "}\n";
                } else {
                  emit_force_value_assign(stmt, lhs.val, lhs.xz, indent);
                }
              } else {
                out << pad << "sched_force_id[" << slot << "] = "
                    << id_it->second << "u;\n";
                emit_force_value_assign(stmt, lhs.val, lhs.xz, indent);
              }
              return;
            }
            std::string slot =
                is_proc ? passign_slot_expr(target) : force_slot_expr(target);
            if (is_proc) {
              out << pad << "sched_passign_id[" << slot
                  << "] = 0xFFFFFFFFu;\n";
              if (override_is_reg[target]) {
                std::string force_active = force_active_expr(target);
                if (force_active != "false") {
                  out << pad << "if (!" << force_active << ") {\n";
                  out << pad << "  " << val_name(target) << "[gid] = "
                      << shadow_val_name(target) << "[gid];\n";
                  out << pad << "  " << xz_name(target) << "[gid] = "
                      << shadow_xz_name(target) << "[gid];\n";
                  out << pad << "}\n";
                } else {
                  out << pad << val_name(target) << "[gid] = "
                      << shadow_val_name(target) << "[gid];\n";
                  out << pad << xz_name(target) << "[gid] = "
                      << shadow_xz_name(target) << "[gid];\n";
                }
              }
              return;
            }
            out << pad << "sched_force_id[" << slot << "] = 0xFFFFFFFFu;\n";
            Lvalue4 lhs =
                build_lvalue4(stmt.assign, locals_override, sched_regs,
                              false, indent);
            if (!lhs.ok) {
              out << pad << "sched_error[gid] = 1u;\n";
              out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            if (passign_target_index.count(target) > 0) {
              std::string passign_active = passign_active_expr(target);
              out << pad << "if (" << passign_active << ") {\n";
              emit_passign_apply_target(target, lhs, indent + 2);
              out << pad << "} else {\n";
              if (override_is_reg[target]) {
                out << pad << "  " << val_name(target) << "[gid] = "
                    << shadow_val_name(target) << "[gid];\n";
                out << pad << "  " << xz_name(target) << "[gid] = "
                    << shadow_xz_name(target) << "[gid];\n";
              }
              out << pad << "}\n";
            } else if (override_is_reg[target]) {
              out << pad << val_name(target) << "[gid] = "
                  << shadow_val_name(target) << "[gid];\n";
              out << pad << xz_name(target) << "[gid] = "
                  << shadow_xz_name(target) << "[gid];\n";
            }
            return;
          }
          if (stmt.kind == StatementKind::kAssign) {
            if (stmt.assign.delay) {
              out << pad << "sched_error[gid] = 1u;\n";
              out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            if (stmt.assign.rhs &&
                stmt.assign.rhs->kind == ExprKind::kCall &&
                IsFileSystemFunctionName(stmt.assign.rhs->ident)) {
              emit_syscall_assign(stmt, *stmt.assign.rhs, resume_pc, indent);
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
            cond = hoist_full_for_use(cond, indent);
            out << pad << "if (" << cond_bool(cond) << ") {\n";
            for (const auto& inner : stmt.then_branch) {
              out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
              self(inner, indent + 4, locals_override, resume_pc, self);
              out << pad << "  }\n";
            }
            if (!stmt.else_branch.empty()) {
              out << pad << "} else {\n";
              for (const auto& inner : stmt.else_branch) {
                out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
                self(inner, indent + 4, locals_override, resume_pc, self);
                out << pad << "  }\n";
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
            case_expr = hoist_full_for_use(case_expr, indent);
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
                out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
                self(inner, indent + 4, locals_override, resume_pc, self);
                out << pad << "  }\n";
              }
            }
            if (!stmt.default_branch.empty()) {
              out << pad << "} else {\n";
              for (const auto& inner : stmt.default_branch) {
                out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
                self(inner, indent + 4, locals_override, resume_pc, self);
                out << pad << "  }\n";
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
            cond = hoist_full_for_use(cond, indent);
            out << pad << "while (" << cond_bool(cond) << ") {\n";
            for (const auto& inner : stmt.for_body) {
              self(inner, indent + 2, locals_override, resume_pc, self);
              out << pad << "  if (sched_state[idx] != GPGA_SCHED_PROC_READY) { break; }\n";
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
            cond = hoist_full_for_use(cond, indent);
            out << pad << "while (" << cond_bool(cond) << ") {\n";
            for (const auto& inner : stmt.while_body) {
              self(inner, indent + 2, locals_override, resume_pc, self);
              out << pad << "  if (sched_state[idx] != GPGA_SCHED_PROC_READY) { break; }\n";
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
              self(inner, indent + 2, locals_override, resume_pc, self);
              out << pad << "  if (sched_state[idx] != GPGA_SCHED_PROC_READY) { break; }\n";
            }
            out << pad << "}\n";
            return;
          }
          if (stmt.kind == StatementKind::kBlock) {
            out << pad << "{\n";
            for (const auto& inner : stmt.block) {
              out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
              self(inner, indent + 4, locals_override, resume_pc, self);
              out << pad << "  }\n";
            }
            out << pad << "}\n";
            return;
          }
        };

        auto emit_task_call =
            [&](const Statement& stmt, int indent, int resume_pc,
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
            emit_inline_stmt_fn(inner, indent, task_locals, resume_pc,
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
          std::unordered_map<const Statement*,
                             std::pair<const Statement*, const Statement*>>
              repeat_spans;
          std::function<void(const Statement&)> append_stmt;
          append_stmt = [&](const Statement& stmt) -> void {
            if (stmt.kind == StatementKind::kBlock &&
                stmt.block_label.empty()) {
              for (const auto& inner : stmt.block) {
                append_stmt(inner);
              }
              return;
            }
            if (stmt.kind == StatementKind::kRepeat && stmt.repeat_count) {
              uint64_t count = 0;
              if (repeat_const_count(stmt, &count) &&
                  count <= kRepeatUnrollLimit) {
                if (count == 0u) {
                  return;
                }
                for (uint64_t rep = 0u; rep < count; ++rep) {
                  for (const auto& inner : stmt.repeat_body) {
                    append_stmt(inner);
                  }
                }
                return;
              }
              stmts.push_back(&stmt);
              size_t body_start = stmts.size();
              for (const auto& inner : stmt.repeat_body) {
                append_stmt(inner);
              }
              size_t body_end = stmts.size();
              const Statement* first =
                  (body_end > body_start) ? stmts[body_start] : nullptr;
              const Statement* last =
                  (body_end > body_start) ? stmts[body_end - 1] : nullptr;
              repeat_spans[&stmt] = std::make_pair(first, last);
              return;
            }
            stmts.push_back(&stmt);
          };
          if (proc.body) {
            for (const auto& stmt : *proc.body) {
              append_stmt(stmt);
            }
          } else if (proc.single) {
            append_stmt(*proc.single);
          }
        std::unordered_map<const Statement*, int> pc_for_stmt;
        pc_counter = 0;
        for (const auto* stmt : stmts) {
          pc_for_stmt[stmt] = pc_counter++;
        }
        const int pc_done = pc_counter++;
          std::unordered_map<const Statement*, size_t> stmt_index;
          for (size_t i = 0; i < stmts.size(); ++i) {
            stmt_index[stmts[i]] = i;
          }
          struct RepeatRuntime {
            uint32_t id = 0u;
            int body_pc = -1;
            int after_pc = -1;
          };
          std::unordered_map<const Statement*, RepeatRuntime> repeat_runtime;
          std::unordered_map<const Statement*, int> next_pc_override;
          for (const auto& entry : repeat_spans) {
            const Statement* stmt_ptr = entry.first;
            auto id_it = repeat_ids.find(stmt_ptr);
            if (id_it == repeat_ids.end()) {
              continue;
            }
            const Statement* first = entry.second.first;
            const Statement* last = entry.second.second;
            size_t after_index = 0;
            auto stmt_it = stmt_index.find(stmt_ptr);
            if (stmt_it == stmt_index.end()) {
              continue;
            }
            if (last) {
              auto last_it = stmt_index.find(last);
              if (last_it == stmt_index.end()) {
                continue;
              }
              after_index = last_it->second + 1;
              next_pc_override[last] = pc_for_stmt[stmt_ptr];
            } else {
              after_index = stmt_it->second + 1;
            }
            int after_pc =
                (after_index < stmts.size()) ? pc_for_stmt[stmts[after_index]]
                                             : pc_done;
            int body_pc =
                first ? pc_for_stmt[first] : after_pc;
            repeat_runtime[stmt_ptr] =
                RepeatRuntime{id_it->second, body_pc, after_pc};
          }
        body_cases.clear();

          std::unordered_map<std::string, int> block_end_pc;
          for (size_t i = 0; i < stmts.size(); ++i) {
            const auto* stmt = stmts[i];
          if (stmt->kind == StatementKind::kBlock &&
              !stmt->block_label.empty()) {
            int next_pc = (i + 1 < stmts.size()) ? pc_for_stmt[stmts[i + 1]]
                                                 : pc_done;
            auto next_override_it = next_pc_override.find(stmt);
            if (next_override_it != next_pc_override.end()) {
              next_pc = next_override_it->second;
            }
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
            auto next_override_it = next_pc_override.find(&stmt);
            if (next_override_it != next_pc_override.end()) {
              next_pc = next_override_it->second;
            }
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
                  if (idx.width > 64) {
                    idx_val = to_u64(idx.val, idx.width);
                    idx_xz =
                        "(" + wide_any(idx.xz, idx.width) + " ? 1u : 0u)";
                  } else {
                    idx_val = idx.val;
                    idx_xz = idx.xz;
                  }
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
              if (stmt.assign.rhs &&
                  stmt.assign.rhs->kind == ExprKind::kCall &&
                  IsFileSystemFunctionName(stmt.assign.rhs->ident)) {
                emit_syscall_assign(stmt, *stmt.assign.rhs, next_pc, 18);
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              emit_inline_stmt(stmt, 18, sched_locals, next_pc,
                               emit_inline_stmt);
              out << "                  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
                out << "                    sched_pc[idx] = " << next_pc << "u;\n";
                out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
                out << "                  }\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kRepeat) {
              auto rep_it = repeat_runtime.find(&stmt);
              if (rep_it == repeat_runtime.end()) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              const RepeatRuntime& rep = rep_it->second;
              out << "                  uint __gpga_rep_slot = (gid * "
                  << "GPGA_SCHED_REPEAT_COUNT) + " << rep.id << "u;\n";
              out << "                  uint __gpga_rep_left = sched_repeat_left[__gpga_rep_slot];\n";
              out << "                  uint __gpga_rep_active = sched_repeat_active[__gpga_rep_slot];\n";
              if (stmt.repeat_count) {
                FsExpr rep_count = emit_expr4_sized(*stmt.repeat_count, 32);
                rep_count = maybe_hoist_full(rep_count, 18, false, false);
                out << "                  if (__gpga_rep_active == 0u) {\n";
                out << "                    uint __gpga_rep_count = uint("
                    << rep_count.val << ");\n";
                out << "                    sched_repeat_left[__gpga_rep_slot] = __gpga_rep_count;\n";
                out << "                    sched_repeat_active[__gpga_rep_slot] = 1u;\n";
                out << "                    __gpga_rep_left = __gpga_rep_count;\n";
                out << "                  }\n";
              } else {
                out << "                  if (__gpga_rep_active == 0u) {\n";
                out << "                    sched_repeat_left[__gpga_rep_slot] = 0u;\n";
                out << "                    sched_repeat_active[__gpga_rep_slot] = 1u;\n";
                out << "                    __gpga_rep_left = 0u;\n";
                out << "                  }\n";
              }
              out << "                  if (__gpga_rep_left == 0u) {\n";
              out << "                    sched_repeat_active[__gpga_rep_slot] = 0u;\n";
              out << "                    sched_pc[idx] = " << rep.after_pc << "u;\n";
              if (rep.after_pc == pc_done) {
                out << "                    sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              } else {
                out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              }
              out << "                    break;\n";
              out << "                  }\n";
              if (rep.body_pc == rep.after_pc) {
                out << "                  sched_repeat_left[__gpga_rep_slot] = 0u;\n";
                out << "                  sched_repeat_active[__gpga_rep_slot] = 0u;\n";
                out << "                  sched_pc[idx] = " << rep.after_pc << "u;\n";
                if (rep.after_pc == pc_done) {
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                } else {
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
                }
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              out << "                  sched_repeat_left[__gpga_rep_slot] = __gpga_rep_left - 1u;\n";
              out << "                  sched_pc[idx] = " << rep.body_pc << "u;\n";
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
                    edge_expr = hoist_full_for_use(edge_expr, 18);
                    std::string mask =
                        literal_for_width(MaskForWidth64(edge_expr.width), 64);
                    out << "                  {\n";
                    out << "                    ulong __gpga_edge_val = ((ulong)("
                        << edge_expr.val << ")) & " << mask << ";\n";
                    out << "                    ulong __gpga_edge_xz = ((ulong)("
                        << edge_expr.xz << ")) & " << mask << ";\n";
                    out << "                    sched_edge_prev_val[__gpga_edge_base + "
                        << i << "u] = __gpga_edge_val;\n";
                    out << "                    sched_edge_prev_xz[__gpga_edge_base + "
                        << i << "u] = __gpga_edge_xz;\n";
                    out << "                  }\n";
                  }
                } else if (info.expr) {
                  FsExpr edge_expr = emit_expr4(*info.expr);
                  edge_expr = hoist_full_for_use(edge_expr, 18);
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
              cond = hoist_full_for_use(cond, 18);
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
              if (body_stmt.kind != StatementKind::kDelay &&
                  body_stmt.kind != StatementKind::kEventControl) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              if (body_stmt.kind == StatementKind::kDelay) {
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
              int body_pc = -1;
              if (!body_stmt.event_body.empty()) {
                body_pc = pc_counter++;
                BodyCase body_case;
                body_case.pc = body_pc;
                body_case.owner = &stmt;
                body_case.next_pc = pc;
                body_case.loop_pc = pc;
                body_case.is_forever_body = true;
                for (const auto& inner : body_stmt.event_body) {
                  body_case.body.push_back(&inner);
                }
                body_cases.push_back(std::move(body_case));
              }
              int event_id = -1;
              bool named_event = false;
              const Expr* named_expr = nullptr;
              if (!body_stmt.event_items.empty()) {
                if (body_stmt.event_items.size() == 1 &&
                    body_stmt.event_items[0].edge == EventEdgeKind::kAny &&
                    body_stmt.event_items[0].expr) {
                  named_expr = body_stmt.event_items[0].expr.get();
                }
              } else if (body_stmt.event_expr &&
                         body_stmt.event_edge == EventEdgeKind::kAny) {
                named_expr = body_stmt.event_expr.get();
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
                auto it = edge_wait_ids.find(&body_stmt);
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
                } else if (body_stmt.event_edge == EventEdgeKind::kPosedge) {
                  edge_kind = "GPGA_SCHED_EDGE_POSEDGE";
                } else if (body_stmt.event_edge == EventEdgeKind::kNegedge) {
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
                    edge_expr = hoist_full_for_use(edge_expr, 18);
                    std::string mask =
                        literal_for_width(MaskForWidth64(edge_expr.width), 64);
                    out << "                  {\n";
                    out << "                    ulong __gpga_edge_val = ((ulong)("
                        << edge_expr.val << ")) & " << mask << ";\n";
                    out << "                    ulong __gpga_edge_xz = ((ulong)("
                        << edge_expr.xz << ")) & " << mask << ";\n";
                    out << "                    sched_edge_prev_val[__gpga_edge_base + "
                        << i << "u] = __gpga_edge_val;\n";
                    out << "                    sched_edge_prev_xz[__gpga_edge_base + "
                        << i << "u] = __gpga_edge_xz;\n";
                    out << "                  }\n";
                  }
                } else if (info.expr) {
                  FsExpr edge_expr = emit_expr4(*info.expr);
                  edge_expr = hoist_full_for_use(edge_expr, 18);
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
                                   : std::to_string(pc) + "u")
                  << ";\n";
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
              emit_task_call(stmt, 18, next_pc, emit_inline_stmt);
              out << "                  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
              out << "                    sched_pc[idx] = " << next_pc << "u;\n";
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              out << "                  }\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (stmt.kind == StatementKind::kWhile && stmt.while_condition) {
              const Expr* fd_expr = nullptr;
              bool invert = false;
              if (ExtractFeofCondition(*stmt.while_condition, &fd_expr,
                                       &invert)) {
                const Expr* call_expr =
                    invert && stmt.while_condition->operand
                        ? stmt.while_condition->operand.get()
                        : stmt.while_condition.get();
                if (!call_expr || call_expr->kind != ExprKind::kCall) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                int body_pc = -1;
                if (!stmt.while_body.empty()) {
                  body_pc = pc_counter++;
                  BodyCase body_case;
                  body_case.pc = body_pc;
                  body_case.owner = &stmt;
                  for (const auto& inner : stmt.while_body) {
                    body_case.body.push_back(&inner);
                  }
                  body_case.next_pc = pc;
                  body_cases.push_back(std::move(body_case));
                }
                int cond_pc = pc_counter++;
                BodyCase cond_case;
                cond_case.pc = cond_pc;
                cond_case.is_service_cond = true;
                cond_case.service_invert = invert;
                cond_case.service_true_pc = (body_pc >= 0) ? body_pc : pc;
                cond_case.service_false_pc = next_pc;
                body_cases.push_back(std::move(cond_case));

                std::string format_id_expr;
                std::vector<ServiceArg> args;
                if (!build_syscall_args(*call_expr, call_expr->ident,
                                        &format_id_expr, &args)) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                emit_service_record("GPGA_SERVICE_KIND_FEOF", format_id_expr,
                                    args, 18);
                out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_SERVICE;\n";
                out << "                  sched_wait_time[idx] = 0ul;\n";
                out << "                  sched_pc[idx] = " << cond_pc << "u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
            }
            if (stmt.kind == StatementKind::kIf && stmt.condition) {
              const Expr* fd_expr = nullptr;
              bool invert = false;
              if (ExtractFeofCondition(*stmt.condition, &fd_expr, &invert)) {
                const Expr* call_expr =
                    invert && stmt.condition->operand
                        ? stmt.condition->operand.get()
                        : stmt.condition.get();
                if (!call_expr || call_expr->kind != ExprKind::kCall) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                int then_pc = -1;
                int else_pc = -1;
                if (!stmt.then_branch.empty()) {
                  then_pc = pc_counter++;
                  BodyCase then_case;
                  then_case.pc = then_pc;
                  then_case.owner = &stmt;
                  then_case.next_pc = next_pc;
                  for (const auto& inner : stmt.then_branch) {
                    then_case.body.push_back(&inner);
                  }
                  body_cases.push_back(std::move(then_case));
                }
                if (!stmt.else_branch.empty()) {
                  else_pc = pc_counter++;
                  BodyCase else_case;
                  else_case.pc = else_pc;
                  else_case.owner = &stmt;
                  else_case.next_pc = next_pc;
                  for (const auto& inner : stmt.else_branch) {
                    else_case.body.push_back(&inner);
                  }
                  body_cases.push_back(std::move(else_case));
                }
                int cond_pc = pc_counter++;
                BodyCase cond_case;
                cond_case.pc = cond_pc;
                cond_case.is_service_cond = true;
                cond_case.service_invert = invert;
                cond_case.service_true_pc =
                    (then_pc >= 0) ? then_pc : next_pc;
                cond_case.service_false_pc =
                    (else_pc >= 0) ? else_pc : next_pc;
                body_cases.push_back(std::move(cond_case));

                std::string format_id_expr;
                std::vector<ServiceArg> args;
                if (!build_syscall_args(*call_expr, call_expr->ident,
                                        &format_id_expr, &args)) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                emit_service_record("GPGA_SERVICE_KIND_FEOF", format_id_expr,
                                    args, 18);
                out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_SERVICE;\n";
                out << "                  sched_wait_time[idx] = 0ul;\n";
                out << "                  sched_pc[idx] = " << cond_pc << "u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
            }
            if (stmt.kind == StatementKind::kIf && stmt.condition) {
              const Expr* call_expr = nullptr;
              bool invert = false;
              if (ExtractPlusargsCondition(*stmt.condition, &call_expr,
                                           &invert)) {
                int then_pc = -1;
                int else_pc = -1;
                if (!stmt.then_branch.empty()) {
                  then_pc = pc_counter++;
                  BodyCase then_case;
                  then_case.pc = then_pc;
                  then_case.owner = &stmt;
                  then_case.next_pc = next_pc;
                  for (const auto& inner : stmt.then_branch) {
                    then_case.body.push_back(&inner);
                  }
                  body_cases.push_back(std::move(then_case));
                }
                if (!stmt.else_branch.empty()) {
                  else_pc = pc_counter++;
                  BodyCase else_case;
                  else_case.pc = else_pc;
                  else_case.owner = &stmt;
                  else_case.next_pc = next_pc;
                  for (const auto& inner : stmt.else_branch) {
                    else_case.body.push_back(&inner);
                  }
                  body_cases.push_back(std::move(else_case));
                }
                int cond_pc = pc_counter++;
                BodyCase cond_case;
                cond_case.pc = cond_pc;
                cond_case.is_service_cond = true;
                cond_case.service_invert = invert;
                cond_case.service_true_pc =
                    (then_pc >= 0) ? then_pc : next_pc;
                cond_case.service_false_pc =
                    (else_pc >= 0) ? else_pc : next_pc;
                body_cases.push_back(std::move(cond_case));

                if (!call_expr || call_expr->kind != ExprKind::kCall) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                const char* kind_expr = nullptr;
                if (call_expr->ident == "$test$plusargs") {
                  kind_expr = "GPGA_SERVICE_KIND_TESTPLUSARGS";
                } else if (call_expr->ident == "$value$plusargs") {
                  kind_expr = "GPGA_SERVICE_KIND_VALUEPLUSARGS";
                }
                if (!kind_expr) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                std::string format_id_expr;
                std::vector<ServiceArg> args;
                if (!build_syscall_args(*call_expr, call_expr->ident,
                                        &format_id_expr, &args)) {
                  out << "                  sched_error[gid] = 1u;\n";
                  out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                  out << "                  break;\n";
                  out << "                }\n";
                  continue;
                }
                emit_service_record(kind_expr, format_id_expr, args, 18);
                out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_SERVICE;\n";
                out << "                  sched_wait_time[idx] = 0ul;\n";
                out << "                  sched_pc[idx] = " << cond_pc << "u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
            }
            int inline_resume_pc = next_pc;
            if (stmt.kind == StatementKind::kWhile ||
                stmt.kind == StatementKind::kFor ||
                stmt.kind == StatementKind::kRepeat) {
              inline_resume_pc = pc;
            }
            emit_inline_stmt(stmt, 18, sched_locals, inline_resume_pc,
                             emit_inline_stmt);
            out << "                  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
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
            if (body_case.is_service_cond) {
              out << "                  ulong __gpga_ret = sched_wait_time[idx];\n";
              out << "                  bool __gpga_cond = ((__gpga_ret & 1ul) != 0ul);\n";
              if (body_case.service_invert) {
                out << "                  __gpga_cond = !__gpga_cond;\n";
              }
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
              out << "                  if (__gpga_cond) {\n";
              out << "                    sched_pc[idx] = " << body_case.service_true_pc
                  << "u;\n";
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              out << "                  } else {\n";
              out << "                    sched_pc[idx] = " << body_case.service_false_pc
                  << "u;\n";
              if (body_case.service_false_pc == pc_done) {
                out << "                    sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              } else {
                out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              }
              out << "                  }\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (body_case.is_service_resume) {
              int width = body_case.service_width;
              if (width <= 0) {
                width = 1;
              }
              std::string ret_val = "__gpga_ret";
              std::string masked =
                  (width > 32) ? MaskForWidthExpr(ret_val, width)
                               : MaskForWidthExpr("uint(" + ret_val + ")", width);
              FsExpr result{masked, literal_for_width(0, width),
                            drive_full(width), width};
              out << "                  ulong __gpga_ret = sched_wait_time[idx];\n";
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
              if (!body_case.owner || body_case.owner->kind != StatementKind::kAssign) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              } else {
                emit_lvalue_assign(body_case.owner->assign, result, 18,
                                   sched_locals);
              }
            } else {
              int inline_resume_pc = body_case.next_pc;
              for (const auto* inner : body_case.body) {
                emit_inline_stmt(*inner, 18, sched_locals, inline_resume_pc,
                                 emit_inline_stmt);
              }
            }
            out << "                  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
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
        if (has_events) {
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
        }
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
              curr = hoist_full_for_use(curr, 16);
              std::string mask =
                  literal_for_width(MaskForWidth64(curr.width), 64);
              out << "                {\n";
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
              out << "                }\n";
            }
            out << "                ready = __gpga_any;\n";
          } else if (info.expr) {
            FsExpr curr = emit_expr4(*info.expr);
            curr = hoist_full_for_use(curr, 16);
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
              sig = hoist_full_for_use(sig, 16);
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
          cond = hoist_full_for_use(cond, 16);
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
        if (has_events) {
          out << "        for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
          out << "          sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
          out << "        }\n";
        }
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
            bool has_override =
                force_target_index.count(target) > 0 ||
                passign_target_index.count(target) > 0;
            if (has_override) {
              std::string override_cond = override_active_expr(target);
              out << "      if (" << override_cond << ") {\n";
              out << "        " << shadow_val_name(target) << "[gid] = nb_"
                  << val_name(target) << "[gid];\n";
              out << "        " << shadow_xz_name(target) << "[gid] = nb_"
                  << xz_name(target) << "[gid];\n";
              out << "      } else {\n";
              out << "        " << val_name(target) << "[gid] = nb_"
                  << val_name(target) << "[gid];\n";
              out << "        " << xz_name(target) << "[gid] = nb_"
                  << xz_name(target) << "[gid];\n";
              out << "      }\n";
            } else {
              out << "      " << val_name(target) << "[gid] = nb_"
                  << val_name(target) << "[gid];\n";
              out << "      " << xz_name(target) << "[gid] = nb_"
                  << xz_name(target) << "[gid];\n";
            }
          }
        }
        if (!nb_array_nets.empty()) {
          out << "      // Commit array NBAs.\n";
          for (const auto* net : nb_array_nets) {
            out << "      for (uint i = 0u; i < " << net->array_size << "u; ++i) {\n";
            out << "        " << val_name(net->name) << "[(gid * "
                << net->array_size << "u) + i] = "
                << MslValNextName(net->name) << "[(gid * "
                << net->array_size << "u) + i];\n";
            out << "        " << xz_name(net->name) << "[(gid * "
                << net->array_size << "u) + i] = "
                << MslXzNextName(net->name) << "[(gid * "
                << net->array_size << "u) + i];\n";
            out << "      }\n";
          }
        }
        emit_sched_comb_update(6);
        if (!system_task_info.monitor_stmts.empty()) {
          out << "      // Monitor change detection.\n";
          for (size_t i = 0; i < system_task_info.monitor_stmts.size(); ++i) {
            const Statement* monitor_stmt = system_task_info.monitor_stmts[i];
            std::string format_id_expr;
            std::vector<ServiceArg> args;
            build_service_args(*monitor_stmt, monitor_stmt->task_name, 0,
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
            build_service_args(*strobe_stmt, strobe_stmt->task_name, 0,
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
        out << "        if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
        out << "          any_ready = true;\n";
        out << "          continue;\n";
        out << "        }\n";
        out << "        if (sched_state[idx] != GPGA_SCHED_PROC_BLOCKED) {\n";
        out << "          continue;\n";
        out << "        }\n";
        if (has_events) {
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
        }
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
              curr = hoist_full_for_use(curr, 16);
              std::string mask =
                  literal_for_width(MaskForWidth64(curr.width), 64);
              out << "              {\n";
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
              out << "              }\n";
            }
            out << "              ready = __gpga_any;\n";
          } else if (info.expr) {
            FsExpr curr = emit_expr4(*info.expr);
            curr = hoist_full_for_use(curr, 16);
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
              sig = hoist_full_for_use(sig, 16);
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
          cond = hoist_full_for_use(cond, 16);
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
        if (has_events) {
          out << "      for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
          out << "        sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
          out << "      }\n";
        }
        out << "      if (any_ready) {\n";
        out << "        sched_flags[gid] |= GPGA_SCHED_FLAG_ACTIVE_INIT;\n";
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
        if (has_delayed_nba) {
          out << "      if (sched_dnba_count[gid] != 0u) {\n";
          out << "        uint __gpga_dnba_base = gid * GPGA_SCHED_MAX_DNBA;\n";
          out << "        uint __gpga_dnba_count = sched_dnba_count[gid];\n";
          out << "        for (uint __gpga_dnba_i = 0u; __gpga_dnba_i < __gpga_dnba_count; ++__gpga_dnba_i) {\n";
          out << "          ulong __gpga_dnba_time = sched_dnba_time[__gpga_dnba_base + __gpga_dnba_i];\n";
          out << "          if (!have_time || __gpga_dnba_time < next_time) {\n";
          out << "            have_time = true;\n";
          out << "            next_time = __gpga_dnba_time;\n";
          out << "          }\n";
          out << "        }\n";
          out << "      }\n";
        }
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
        out << "        sched_flags[gid] |= GPGA_SCHED_FLAG_ACTIVE_INIT;\n";
        out << "        sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
        out << "        continue;\n";
        out << "      }\n";
        out << "      bool have_service = false;\n";
        out << "      for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
        out << "        uint idx = gpga_sched_index(gid, pid);\n";
        out << "        if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_SERVICE) {\n";
        out << "          have_service = true;\n";
        out << "          break;\n";
        out << "        }\n";
        out << "      }\n";
        out << "      if (have_service) {\n";
        out << "        break;\n";
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
  std::unordered_set<std::string> initial_reads;
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
      CollectReadSignals(stmt, &initial_reads);
    }
  }
  std::unordered_set<std::string> scheduled_reads;
  for (const auto& block : module.always_blocks) {
    if (block.edge == EdgeKind::kCombinational) {
      continue;
    }
    if (block.edge == EdgeKind::kPosedge || block.edge == EdgeKind::kNegedge) {
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

  auto literal_for_width = [](uint64_t value, int width) -> std::string {
    if (width > 64) {
      return "gpga_wide_from_u64_" + std::to_string(width) + "(" +
             std::to_string(value) + "ul)";
    }
    std::string suffix = (width > 32) ? "ul" : "u";
    return std::to_string(value) + suffix;
  };
  auto decay_name = [](const std::string& name) {
    return MslDecayName(name);
  };
  auto shadow_name = [](const std::string& name) {
    return "__gpga_force_shadow_" + MslName(name);
  };
  auto shadow_any_name = [](const std::string& name) {
    return "__gpga_force_shadow_" + name;
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
        (initial_regs.count(net.name) > 0 ||
         initial_reads.count(net.name) > 0)) {
      init_reg_names.push_back(net.name);
    }
  }
  std::vector<const Net*> array_nets;
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      array_nets.push_back(&net);
    }
  }

  const bool pack_signals = needs_scheduler;
  const bool pack_nb = pack_signals;
  struct PackedSignal {
    std::string name;
    std::string type;
    int array_size = 1;
  };
  std::unordered_map<std::string, int> signal_array_sizes;
  signal_array_sizes.reserve(module.nets.size());
  for (const auto& net : module.nets) {
    if (net.array_size > 0) {
      signal_array_sizes[net.name] = net.array_size;
    }
  }
  auto array_size_for = [&](const std::string& name) -> int {
    auto it = signal_array_sizes.find(name);
    return (it != signal_array_sizes.end()) ? it->second : 1;
  };
  std::vector<PackedSignal> packed_signals;
  std::vector<PackedSignal> packed_nb_signals;
  bool needs_force_shadow = false;
  if (pack_signals) {
    packed_signals.reserve(module.ports.size() + reg_names.size() +
                           trireg_nets.size() * 2 + array_nets.size());
    for (const auto& port : module.ports) {
      PackedSignal sig;
      sig.name = MslName(port.name);
      sig.type = TypeForWidth(port.width);
      sig.array_size = 1;
      packed_signals.push_back(std::move(sig));
    }
    for (const auto& reg : reg_names) {
      PackedSignal sig;
      sig.name = MslName(reg);
      sig.type = TypeForWidth(SignalWidth(module, reg));
      sig.array_size = array_size_for(reg);
      packed_signals.push_back(std::move(sig));
    }
    for (const auto* reg : trireg_nets) {
      PackedSignal sig;
      sig.name = MslName(reg->name);
      sig.type = TypeForWidth(SignalWidth(module, reg->name));
      sig.array_size = array_size_for(reg->name);
      packed_signals.push_back(std::move(sig));
      PackedSignal decay;
      decay.name = decay_name(reg->name);
      decay.type = "ulong";
      decay.array_size = 1;
      packed_signals.push_back(std::move(decay));
    }
    for (const auto* net : array_nets) {
      PackedSignal sig;
      sig.name = MslName(net->name);
      sig.type = TypeForWidth(net->width);
      sig.array_size = std::max(1, net->array_size);
      packed_signals.push_back(std::move(sig));
    }
  }
  std::vector<PackedSignal> packed_force_signals;
  if (pack_signals) {
    packed_force_signals.reserve(packed_signals.size());
    for (const auto& sig : packed_signals) {
      PackedSignal shadow = sig;
      shadow.name = shadow_any_name(sig.name);
      packed_force_signals.push_back(std::move(shadow));
    }
  }
  auto emit_packed_signal_setup = [&](const std::string& count_expr) {
    if (!pack_signals) {
      return;
    }
    out << "  uint __gpga_count = " << count_expr << ";\n";
    out << "  ulong __gpga_offset = 0ul;\n";
    for (const auto& sig : packed_signals) {
      int array_size = std::max(1, sig.array_size);
      out << "  __gpga_offset = (__gpga_offset + 7ul) & ~7ul;\n";
      out << "  device " << sig.type << "* " << sig.name
          << " = (device " << sig.type << "*)(gpga_state + __gpga_offset);\n";
      out << "  __gpga_offset += (ulong)__gpga_count * " << array_size
          << "u * (ulong)sizeof(" << sig.type << ");\n";
    }
  };
  auto emit_packed_nb_setup = [&](const std::string& count_expr) {
    if (!pack_nb || packed_nb_signals.empty()) {
      return;
    }
    out << "  uint __gpga_nb_count = " << count_expr << ";\n";
    out << "  ulong __gpga_nb_offset = 0ul;\n";
    for (const auto& sig : packed_nb_signals) {
      int array_size = std::max(1, sig.array_size);
      out << "  __gpga_nb_offset = (__gpga_nb_offset + 7ul) & ~7ul;\n";
      out << "  device " << sig.type << "* " << sig.name
          << " = (device " << sig.type << "*)(nb_state + __gpga_nb_offset);\n";
      out << "  __gpga_nb_offset += (ulong)__gpga_nb_count * " << array_size
          << "u * (ulong)sizeof(" << sig.type << ");\n";
    }
  };
  auto emit_packed_force_setup = [&](const std::string& count_expr) {
    if (!pack_signals || !needs_force_shadow) {
      return;
    }
    out << "  uint __gpga_force_count = " << count_expr << ";\n";
    out << "  ulong __gpga_force_offset = 0ul;\n";
    for (const auto& sig : packed_force_signals) {
      int array_size = std::max(1, sig.array_size);
      out << "  __gpga_force_offset = (__gpga_force_offset + 7ul) & ~7ul;\n";
      out << "  device " << sig.type << "* " << sig.name
          << " = (device " << sig.type
          << "*)(sched_force_state + __gpga_force_offset);\n";
      out << "  __gpga_force_offset += (ulong)__gpga_force_count * "
          << array_size << "u * (ulong)sizeof(" << sig.type << ");\n";
    }
  };

  std::unordered_set<std::string> switch_nets;
  for (const auto& sw : module.switches) {
    switch_nets.insert(sw.a);
    switch_nets.insert(sw.b);
  }
  std::unordered_set<std::string> drive_declared;
  std::unordered_map<std::string, std::string> drive_vars;
  auto drive_var_name = [&](const std::string& name) -> std::string {
    return "__gpga_drive_" + MslName(name);
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

  out << "kernel void gpga_" << MslName(module.name) << "(";
  int buffer_index = 0;
  bool first = true;
  if (pack_signals) {
    if (!first) {
      out << ",\n";
    }
    first = false;
    out << "  device uchar* gpga_state [[buffer(" << buffer_index++ << ")]]";
  }
  if (!pack_signals) {
    for (const auto& port : module.ports) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string qualifier =
          (port.dir == PortDir::kInput) ? "constant" : "device";
      std::string type = TypeForWidth(port.width);
      out << "  " << qualifier << " " << type << "* " << MslName(port.name)
          << " [[buffer(" << buffer_index++ << ")]]";
    }
    for (const auto& reg : reg_names) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg));
      out << "  device " << type << "* " << MslName(reg) << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto* reg : trireg_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg->name));
      out << "  device " << type << "* " << MslName(reg->name) << " [[buffer("
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
      out << "  device " << type << "* " << MslName(net->name) << " [[buffer("
          << buffer_index++ << ")]]";
    }
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
  if (pack_signals) {
    emit_packed_signal_setup("params.count");
  }

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
      out << "  " << type << " " << MslName(net.name) << " = "
          << ZeroForWidth(net.width) << ";\n";
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
          "__gpga_drv_" + MslName(entry.first) + "_" +
          std::to_string(idx) + "_val";
      info.drive =
          "__gpga_drv_" + MslName(entry.first) + "_" +
          std::to_string(idx) + "_drive";
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
      if (width > 64) {
        std::string drive = literal_for_width(drive_bits, width);
        std::string upper_mask =
            "gpga_wide_and_" + std::to_string(width) + "(" +
            MaskLiteralForWidth(width) + ", gpga_wide_not_" +
            std::to_string(width) + "(" +
            literal_for_width(0xFFFFFFFFFFFFFFFFull, width) + "))";
        return "gpga_wide_or_" + std::to_string(width) + "(" + drive + ", " +
               upper_mask + ")";
      }
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
      if (lhs_width > 64) {
        std::string idx = std::to_string(lo) + "u";
        std::string rhs_ext;
        std::string drive_ext;
        if (slice_width > 64) {
          rhs_ext =
              "gpga_wide_resize_" + std::to_string(lhs_width) + "_from_" +
              std::to_string(slice_width) + "(" + rhs + ")";
          drive_ext =
              "gpga_wide_resize_" + std::to_string(lhs_width) + "_from_" +
              std::to_string(slice_width) + "(" + drive + ")";
        } else {
          rhs_ext =
              "gpga_wide_from_u64_" + std::to_string(lhs_width) + "(" + rhs +
              ")";
          drive_ext =
              "gpga_wide_from_u64_" + std::to_string(lhs_width) + "(" + drive +
              ")";
        }
        out << "  " << type << " " << info.val << " = gpga_wide_shl_"
            << lhs_width << "(" << rhs_ext << ", " << idx << ");\n";
        out << "  " << type << " " << info.drive << " = gpga_wide_shl_"
            << lhs_width << "(" << drive_ext << ", " << idx << ");\n";
      } else {
        uint64_t mask = MaskForWidth64(slice_width);
        std::string mask_literal = literal_for_width(mask, lhs_width);
        std::string cast = CastForWidth(lhs_width);
        out << "  " << type << " " << info.val << " = ((" << cast << rhs
            << " & " << mask_literal << ") << " << std::to_string(lo)
            << "u);\n";
        out << "  " << type << " " << info.drive << " = ((" << cast << drive
            << " & " << mask_literal << ") << " << std::to_string(lo)
            << "u);\n";
      }
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
    std::string zero = ZeroForWidth(lhs_width);
    std::string msl_name = MslName(name);
    std::string resolved_val = "__gpga_res_" + MslName(name) + "_val";
    std::string resolved_drive = "__gpga_res_" + MslName(name) + "_drive";
    out << "  " << type << " " << resolved_val << " = " << zero << ";\n";
    out << "  " << type << " " << resolved_drive << " = " << zero << ";\n";
    if (lhs_width > 64) {
      out << "  for (uint bit = 0u; bit < " << lhs_width << "u; ++bit) {\n";
      if (wired_and || wired_or) {
        out << "    bool has0 = false;\n";
        out << "    bool has1 = false;\n";
        for (size_t idx : indices) {
          const auto& info = driver_info[idx];
          out << "    if (gpga_wide_get_bit_" << lhs_width << "(" << info.drive
              << ", bit) != 0u) {\n";
          out << "      if (gpga_wide_get_bit_" << lhs_width << "(" << info.val
              << ", bit) != 0u) {\n";
          out << "        has1 = true;\n";
          out << "      } else {\n";
          out << "        has0 = true;\n";
          out << "      }\n";
          out << "    }\n";
        }
        out << "    if (!has0 && !has1) {\n";
        out << "      continue;\n";
        out << "    }\n";
        out << "    " << resolved_drive << " = gpga_wide_set_bit_"
            << lhs_width << "(" << resolved_drive << ", bit, 1u);\n";
        if (wired_and) {
          out << "    if (!has0) {\n";
          out << "      " << resolved_val << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_val << ", bit, 1u);\n";
          out << "    }\n";
        } else {
          out << "    if (has1) {\n";
          out << "      " << resolved_val << " = gpga_wide_set_bit_"
              << lhs_width << "(" << resolved_val << ", bit, 1u);\n";
          out << "    }\n";
        }
        out << "    continue;\n";
      } else {
        out << "    uint best0 = 0u;\n";
        out << "    uint best1 = 0u;\n";
        for (size_t idx : indices) {
          const auto& info = driver_info[idx];
          out << "    if (gpga_wide_get_bit_" << lhs_width << "(" << info.drive
              << ", bit) != 0u) {\n";
          out << "      if (gpga_wide_get_bit_" << lhs_width << "(" << info.val
              << ", bit) != 0u) {\n";
          out << "        best1 = (best1 > " << info.strength1
              << ") ? best1 : " << info.strength1 << ";\n";
          out << "      } else {\n";
          out << "        best0 = (best0 > " << info.strength0
              << ") ? best0 : " << info.strength0 << ";\n";
          out << "      }\n";
          out << "    }\n";
        }
        out << "    if (best0 == 0u && best1 == 0u) {\n";
        out << "      continue;\n";
        out << "    }\n";
        out << "    " << resolved_drive << " = gpga_wide_set_bit_"
            << lhs_width << "(" << resolved_drive << ", bit, 1u);\n";
        out << "    if (best1 > best0) {\n";
        out << "      " << resolved_val << " = gpga_wide_set_bit_"
            << lhs_width << "(" << resolved_val << ", bit, 1u);\n";
        out << "    }\n";
      }
      out << "  }\n";

      if (switch_nets.count(name) > 0) {
        ensure_drive_declared(name, lhs_width, ZeroForWidth(lhs_width));
        out << "  " << drive_var_name(name) << " = " << resolved_drive
            << ";\n";
      }

      bool is_output = IsOutputPort(module, name) || regs_ctx.count(name) > 0;
      bool is_local = locals_ctx.count(name) > 0 && !is_output &&
                      regs_ctx.count(name) == 0;
      if (is_output) {
        if (is_trireg) {
          std::string decay_ref = decay_name(name) + "[gid]";
          std::string decay_delay = trireg_decay_delay(name);
          std::string drive_flag = "__gpga_trireg_drive_" + msl_name;
          std::string decay_flag = "__gpga_trireg_decay_" + msl_name;
          out << "  bool " << drive_flag << " = gpga_wide_any_" << lhs_width
              << "(" << resolved_drive << ");\n";
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
          out << "  " << msl_name << "[gid] = "
              << "gpga_wide_or_" << lhs_width << "(gpga_wide_and_" << lhs_width
              << "(" << msl_name << "[gid], gpga_wide_not_" << lhs_width << "("
              << resolved_drive << ")), gpga_wide_and_" << lhs_width << "("
              << resolved_val << ", " << resolved_drive << "));\n";
          out << "  if (" << decay_flag << ") {\n";
          out << "    " << msl_name << "[gid] = gpga_wide_or_" << lhs_width
              << "(" << msl_name << "[gid], "
              << MaskLiteralForWidth(lhs_width) << ");\n";
          out << "  }\n";
        } else {
          out << "  " << msl_name << "[gid] = " << resolved_val << ";\n";
        }
      } else if (is_local) {
        if (declared_ctx && declared_ctx->count(name) == 0) {
          out << "  " << type << " " << msl_name << ";\n";
          if (declared_ctx) {
            declared_ctx->insert(name);
          }
        }
        out << "  " << msl_name << " = " << resolved_val << ";\n";
      } else {
        out << "  // Unmapped resolved assign: " << name << "\n";
      }
      return;
    }
    std::string one = (lhs_width > 32) ? "1ul" : "1u";
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
        std::string drive_flag = "__gpga_trireg_drive_" + msl_name;
        std::string decay_flag = "__gpga_trireg_decay_" + msl_name;
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
        out << "  " << msl_name << "[gid] = (" << msl_name << "[gid] & ~"
            << resolved_drive << ") | (" << resolved_val << " & "
            << resolved_drive << ");\n";
        out << "  if (" << decay_flag << ") {\n";
        out << "    " << msl_name << "[gid] = " << zero << ";\n";
        out << "  }\n";
      } else {
        out << "  " << msl_name << "[gid] = " << resolved_val << ";\n";
      }
    } else if (is_local) {
      if (declared_ctx && declared_ctx->count(name) == 0) {
        out << "  " << type << " " << msl_name << ";\n";
        if (declared_ctx) {
          declared_ctx->insert(name);
        }
      }
      out << "  " << msl_name << " = " << resolved_val << ";\n";
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
            out << "  " << MslName(assign.lhs) << "[gid] = " << sized << ";\n";
          } else if (regs_ctx.count(assign.lhs) > 0) {
            out << "  " << MslName(assign.lhs) << "[gid] = " << sized << ";\n";
          } else if (locals_ctx.count(assign.lhs) > 0) {
            if (declared_ctx && declared_ctx->count(assign.lhs) == 0) {
              std::string type = TypeForWidth(SignalWidth(module, assign.lhs));
              out << "  " << type << " " << MslName(assign.lhs) << " = "
                  << sized << ";\n";
              declared_ctx->insert(assign.lhs);
            } else {
              out << "  " << MslName(assign.lhs) << " = " << sized << ";\n";
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
          std::string temp = target_is_local ? MslName(name)
                                             : ("__gpga_partial_" + MslName(name));
          bool track_drive = switch_nets.count(name) > 0;
          std::string temp_drive =
              "__gpga_partial_" + MslName(name) + "_drive";
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
              out << "  " << MslName(name) << "[gid] = " << temp << ";\n";
            } else if (locals_ctx.count(name) > 0) {
              if (declared_ctx && declared_ctx->count(name) == 0) {
                out << "  " << type << " " << MslName(name) << " = " << temp
                    << ";\n";
                declared_ctx->insert(name);
              } else {
                out << "  " << MslName(name) << " = " << temp << ";\n";
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
    out << "  " << type << " " << MslName(target) << ";\n";
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
      *expr = MslName(name) + "[gid]";
      return true;
    }
    if (locals.count(name) > 0) {
      *expr = MslName(name);
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

  std::function<void(int)> emit_force_overrides;
  std::vector<std::string> override_target_list;
  auto emit_sched_comb_update = [&](int indent) {
    bool has_comb = !module.assigns.empty() || !module.switches.empty();
    if (!has_comb) {
      for (const auto& block : module.always_blocks) {
        if (block.edge == EdgeKind::kCombinational) {
          has_comb = true;
          break;
        }
      }
    }
    if (!has_comb) {
      return;
    }
    std::string pad(indent, ' ');
    out << pad << "{\n";
    std::unordered_set<std::string> comb_declared;
    emit_continuous_assigns(locals, regs, &comb_declared);

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
      if (locals.count(target) == 0 || comb_declared.count(target) > 0) {
        continue;
      }
      std::string type = TypeForWidth(SignalWidth(module, target));
      out << "  " << type << " " << MslName(target) << ";\n";
      comb_declared.insert(target);
    }
    for (const auto& target : override_target_list) {
      if (locals.count(target) == 0 || comb_declared.count(target) > 0) {
        continue;
      }
      std::string type = TypeForWidth(SignalWidth(module, target));
      out << "  " << type << " " << MslName(target) << ";\n";
      comb_declared.insert(target);
    }
    for (const auto& block : module.always_blocks) {
      if (block.edge != EdgeKind::kCombinational) {
        continue;
      }
      for (const auto& stmt : block.statements) {
        emit_comb_stmt(stmt, indent + 2);
      }
    }
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
    if (emit_force_overrides) {
      emit_force_overrides(indent + 2);
    }
    out << pad << "}\n";
  };

  if (has_initial && !needs_scheduler) {
    out << "\n";
    out << "kernel void gpga_" << MslName(module.name) << "_init(";
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
      out << "  " << qualifier << " " << type << "* " << MslName(port.name)
          << " [[buffer(" << buffer_index++ << ")]]";
    }
    for (const auto& reg : init_reg_names) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg));
      out << "  device " << type << "* " << MslName(reg) << " [[buffer("
          << buffer_index++ << ")]]";
    }
    for (const auto* reg : trireg_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(SignalWidth(module, reg->name));
      out << "  device " << type << "* " << MslName(reg->name) << " [[buffer("
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
      out << "  device " << type << "* " << MslName(net->name) << " [[buffer("
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
      out << "  " << type << " " << MslName(target) << ";\n";
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
    out << "kernel void gpga_" << MslName(module.name) << "_tick(";
    buffer_index = 0;
    first = true;
    if (pack_signals) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      out << "  device uchar* gpga_state [[buffer(" << buffer_index++
          << ")]]";
    }
    if (!pack_signals) {
      for (const auto& port : module.ports) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string qualifier =
            (port.dir == PortDir::kInput) ? "constant" : "device";
        std::string type = TypeForWidth(port.width);
        out << "  " << qualifier << " " << type << "* " << MslName(port.name)
            << " [[buffer(" << buffer_index++ << ")]]";
      }
      for (const auto& reg : reg_names) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(SignalWidth(module, reg));
        out << "  device " << type << "* " << MslName(reg) << " [[buffer("
            << buffer_index++ << ")]]";
      }
      for (const auto* reg : trireg_nets) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        std::string type = TypeForWidth(SignalWidth(module, reg->name));
        out << "  device " << type << "* " << MslName(reg->name)
            << " [[buffer(" << buffer_index++ << ")]]";
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
        out << "  device " << type << "* " << MslName(net->name)
            << " [[buffer(" << buffer_index++ << ")]]";
      }
    }
    for (const auto* net : array_nets) {
      if (!first) {
        out << ",\n";
      }
      first = false;
      std::string type = TypeForWidth(net->width);
      out << "  device " << type << "* " << MslNameNext(net->name)
          << " [[buffer(" << buffer_index++ << ")]]";
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
    if (pack_signals) {
      emit_packed_signal_setup("params.count");
    }
    out << "  // Tick kernel: sequential logic (posedge/negedge in v0).\n";
    for (const auto* net : array_nets) {
      out << "  for (uint i = 0u; i < " << net->array_size << "u; ++i) {\n";
      out << "    " << MslNameNext(net->name) << "[(gid * " << net->array_size
          << "u) + i] = " << MslName(net->name) << "[(gid * "
          << net->array_size << "u) + i];\n";
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
        return MslName(name) + "[gid]";
      }
      return MslName(name);
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
        std::string temp = "nb_" + MslName(target);
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
    std::vector<const AlwaysBlock*> edge_blocks;
    for (const auto& block : module.always_blocks) {
      if (block.edge == EdgeKind::kInitial) {
        initial_blocks.push_back(&block);
      } else if (block.edge == EdgeKind::kPosedge ||
                 block.edge == EdgeKind::kNegedge) {
        edge_blocks.push_back(&block);
      }
    }

    if (!initial_blocks.empty() || !edge_blocks.empty()) {
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
      std::vector<std::unique_ptr<Statement>> always_wrappers;

      int next_pid = 0;
      for (const auto* block : initial_blocks) {
        procs.push_back(ProcDef{next_pid, &block->statements, nullptr});
        proc_parent.push_back(-1);
        proc_join_tag.push_back(-1);
        ++next_pid;
      }
      always_wrappers.reserve(edge_blocks.size());
      auto make_edge_wrapper = [&](const AlwaysBlock& block) {
        auto forever_stmt = std::make_unique<Statement>();
        forever_stmt->kind = StatementKind::kForever;
        Statement event_stmt;
        event_stmt.kind = StatementKind::kEventControl;
        event_stmt.event_edge = (block.edge == EdgeKind::kPosedge)
                                    ? EventEdgeKind::kPosedge
                                    : EventEdgeKind::kNegedge;
        auto clock_expr = std::make_unique<Expr>();
        clock_expr->kind = ExprKind::kIdentifier;
        clock_expr->ident = block.clock;
        event_stmt.event_expr = std::move(clock_expr);
        event_stmt.event_body.reserve(block.statements.size());
        for (const auto& stmt : block.statements) {
          event_stmt.event_body.push_back(CloneStatement(stmt));
        }
        forever_stmt->forever_body.push_back(std::move(event_stmt));
        return forever_stmt;
      };
      for (const auto* block : edge_blocks) {
        auto wrapper = make_edge_wrapper(*block);
        procs.push_back(ProcDef{next_pid, nullptr, wrapper.get()});
        proc_parent.push_back(-1);
        proc_join_tag.push_back(-1);
        always_wrappers.push_back(std::move(wrapper));
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
        if (procs[i].body) {
          collect_forks_in_list(*procs[i].body, procs[i].pid);
        } else if (procs[i].single) {
          collect_forks(*procs[i].single, procs[i].pid);
        }
      }

      auto for_each_proc_stmt = [&](const auto& fn) {
        for (const auto& proc : procs) {
          if (proc.body) {
            for (const auto& stmt : *proc.body) {
              fn(stmt);
            }
          } else if (proc.single) {
            fn(*proc.single);
          }
        }
      };

      std::unordered_set<std::string> force_targets;
      std::unordered_set<std::string> passign_targets;
      std::vector<const Statement*> force_stmts;
      std::vector<const Statement*> passign_stmts;
      std::function<void(const Statement&)> collect_force_stmts;
      collect_force_stmts = [&](const Statement& stmt) -> void {
        if (stmt.kind == StatementKind::kForce) {
          const std::string& target = stmt.force_target;
          if (stmt.is_procedural) {
            passign_targets.insert(target);
            passign_stmts.push_back(&stmt);
          } else {
            force_targets.insert(target);
            force_stmts.push_back(&stmt);
          }
          return;
        }
        if (stmt.kind == StatementKind::kRelease) {
          const std::string& target = stmt.release_target;
          if (stmt.is_procedural) {
            passign_targets.insert(target);
          } else {
            force_targets.insert(target);
          }
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            collect_force_stmts(inner);
          }
          for (const auto& inner : stmt.else_branch) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              collect_force_stmts(inner);
            }
          }
          for (const auto& inner : stmt.default_branch) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          for (const auto& inner : stmt.delay_body) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          for (const auto& inner : stmt.wait_body) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWhile) {
          for (const auto& inner : stmt.while_body) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kRepeat) {
          for (const auto& inner : stmt.repeat_body) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFor) {
          for (const auto& inner : stmt.for_body) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          for (const auto& inner : stmt.forever_body) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          for (const auto& inner : stmt.event_body) {
            collect_force_stmts(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFork) {
          for (const auto& inner : stmt.fork_branches) {
            collect_force_stmts(inner);
          }
          return;
        }
      };
      for_each_proc_stmt(
          [&](const Statement& stmt) { collect_force_stmts(stmt); });

      std::vector<std::string> force_target_list(force_targets.begin(),
                                                 force_targets.end());
      std::vector<std::string> passign_target_list(passign_targets.begin(),
                                                   passign_targets.end());
      std::sort(force_target_list.begin(), force_target_list.end());
      std::sort(passign_target_list.begin(), passign_target_list.end());

      std::unordered_set<std::string> override_targets(force_targets);
      override_targets.insert(passign_targets.begin(), passign_targets.end());
      override_target_list.assign(override_targets.begin(),
                                  override_targets.end());
      std::sort(override_target_list.begin(), override_target_list.end());

      std::unordered_map<std::string, uint32_t> force_target_index;
      std::unordered_map<std::string, uint32_t> passign_target_index;
      for (size_t i = 0; i < force_target_list.size(); ++i) {
        force_target_index[force_target_list[i]] = static_cast<uint32_t>(i);
      }
      for (size_t i = 0; i < passign_target_list.size(); ++i) {
        passign_target_index[passign_target_list[i]] = static_cast<uint32_t>(i);
      }

      std::unordered_map<const Statement*, uint32_t> force_stmt_ids;
      std::unordered_map<const Statement*, uint32_t> passign_stmt_ids;
      for (size_t i = 0; i < force_stmts.size(); ++i) {
        force_stmt_ids[force_stmts[i]] = static_cast<uint32_t>(i);
      }
      for (size_t i = 0; i < passign_stmts.size(); ++i) {
        passign_stmt_ids[passign_stmts[i]] = static_cast<uint32_t>(i);
      }
      std::unordered_map<std::string, std::vector<const Statement*>>
          force_stmts_by_target;
      std::unordered_map<std::string, std::vector<const Statement*>>
          passign_stmts_by_target;
      for (const auto* stmt : force_stmts) {
        force_stmts_by_target[stmt->force_target].push_back(stmt);
      }
      for (const auto* stmt : passign_stmts) {
        passign_stmts_by_target[stmt->force_target].push_back(stmt);
      }

      needs_force_shadow = !force_target_list.empty() ||
                           !passign_target_list.empty();
      std::unordered_map<std::string, bool> override_is_reg;
      for (const auto& name : override_target_list) {
        NetType net_type = SignalNetType(module, name);
        bool is_reg = (net_type == NetType::kReg || IsTriregNet(net_type));
        override_is_reg[name] = is_reg;
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
      for_each_proc_stmt([&](const Statement& stmt) { collect_waits(stmt); });

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
      for_each_proc_stmt(
          [&](const Statement& stmt) { collect_edge_waits(stmt); });

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
      for_each_proc_stmt(
          [&](const Statement& stmt) { collect_delay_assigns(stmt); });

      const int64_t kRepeatUnrollLimit = 4096;
      const std::unordered_map<std::string, int64_t> kRepeatEmptyParams;
      std::unordered_map<const Statement*, uint32_t> repeat_ids;
      uint32_t repeat_state_count = 0;
      auto repeat_const_count = [&](const Statement& stmt,
                                    int64_t* out_count) -> bool {
        if (!stmt.repeat_count || !out_count) {
          return false;
        }
        int64_t count_value = 0;
        if (!EvalConstExpr(*stmt.repeat_count, kRepeatEmptyParams, &count_value,
                           nullptr)) {
          return false;
        }
        *out_count = count_value;
        return true;
      };
      std::function<void(const Statement&)> collect_repeat_states;
      collect_repeat_states = [&](const Statement& stmt) -> void {
        if (stmt.kind == StatementKind::kRepeat && stmt.repeat_count) {
          int64_t count = 0;
          bool is_const = repeat_const_count(stmt, &count);
          if (!is_const || count > kRepeatUnrollLimit) {
            auto inserted = repeat_ids.emplace(&stmt, repeat_state_count);
            if (inserted.second) {
              repeat_state_count++;
            }
          } else if (count <= 0) {
            return;
          }
          for (const auto& inner : stmt.repeat_body) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kIf) {
          for (const auto& inner : stmt.then_branch) {
            collect_repeat_states(inner);
          }
          for (const auto& inner : stmt.else_branch) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          for (const auto& inner : stmt.block) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFor) {
          for (const auto& inner : stmt.for_body) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWhile) {
          for (const auto& inner : stmt.while_body) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kDelay) {
          for (const auto& inner : stmt.delay_body) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kEventControl) {
          for (const auto& inner : stmt.event_body) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kWait) {
          for (const auto& inner : stmt.wait_body) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kForever) {
          for (const auto& inner : stmt.forever_body) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kCase) {
          for (const auto& item : stmt.case_items) {
            for (const auto& inner : item.body) {
              collect_repeat_states(inner);
            }
          }
          for (const auto& inner : stmt.default_branch) {
            collect_repeat_states(inner);
          }
          return;
        }
        if (stmt.kind == StatementKind::kFork) {
          for (const auto& inner : stmt.fork_branches) {
            collect_repeat_states(inner);
          }
        }
      };
      for (const auto& proc : procs) {
        if (proc.body) {
          for (const auto& stmt : *proc.body) {
            collect_repeat_states(stmt);
          }
        } else if (proc.single) {
          collect_repeat_states(*proc.single);
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
      for_each_proc_stmt(
          [&](const Statement& stmt) { collect_nb_targets(stmt); });

      std::vector<std::string> nb_targets_sorted(nb_targets.begin(),
                                                 nb_targets.end());
      std::sort(nb_targets_sorted.begin(), nb_targets_sorted.end());
      packed_nb_signals.clear();
      if (pack_nb && !nb_targets_sorted.empty()) {
        packed_nb_signals.reserve(nb_targets_sorted.size());
        for (const auto& target : nb_targets_sorted) {
          PackedSignal sig;
          sig.name = "nb_" + MslName(target);
          sig.type = TypeForWidth(SignalWidth(module, target));
          sig.array_size = 1;
          packed_nb_signals.push_back(std::move(sig));
        }
      }
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
      const bool has_events = !module.events.empty();
      const bool has_edges = edge_item_count > 0;
      const bool has_edge_star = edge_star_count > 0;

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
      out << "struct GpgaSchedParams { uint count; uint max_steps; uint max_proc_steps; uint service_capacity; };\n";
      out << "constant constexpr uint GPGA_SCHED_PROC_COUNT = " << procs.size() << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_ROOT_COUNT = " << root_proc_count
          << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_EVENT_COUNT = "
          << module.events.size() << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_EDGE_COUNT = " << edge_item_count
          << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_EDGE_STAR_COUNT = " << edge_star_count
          << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_MAX_READY = " << procs.size() << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_MAX_TIME = " << procs.size() << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_MAX_NBA = " << nb_targets_sorted.size()
          << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_FORCE_COUNT = "
          << force_target_list.size() << "u;\n";
      out << "constant constexpr uint GPGA_SCHED_PCONT_COUNT = "
          << passign_target_list.size() << "u;\n";
      if (repeat_state_count > 0) {
        out << "constant constexpr uint GPGA_SCHED_REPEAT_COUNT = "
            << repeat_state_count << "u;\n";
      }
      if (has_delayed_assigns) {
        out << "constant constexpr uint GPGA_SCHED_DELAY_COUNT = "
            << delay_assigns.size() << "u;\n";
      }
      if (has_delayed_nba) {
        out << "constant constexpr uint GPGA_SCHED_MAX_DNBA = " << delayed_nba_capacity
            << "u;\n";
      }
      out << "constant constexpr uint GPGA_SCHED_NO_PARENT = 0xFFFFFFFFu;\n";
      out << "constant constexpr uint GPGA_SCHED_WAIT_NONE = 0u;\n";
      out << "constant constexpr uint GPGA_SCHED_WAIT_TIME = 1u;\n";
      out << "constant constexpr uint GPGA_SCHED_WAIT_EVENT = 2u;\n";
      out << "constant constexpr uint GPGA_SCHED_WAIT_COND = 3u;\n";
      out << "constant constexpr uint GPGA_SCHED_WAIT_JOIN = 4u;\n";
      out << "constant constexpr uint GPGA_SCHED_WAIT_DELTA = 5u;\n";
      out << "constant constexpr uint GPGA_SCHED_WAIT_EDGE = 6u;\n";
      out << "constant constexpr uint GPGA_SCHED_WAIT_SERVICE = 7u;\n";
      out << "constant constexpr uint GPGA_SCHED_EDGE_ANY = 0u;\n";
      out << "constant constexpr uint GPGA_SCHED_EDGE_POSEDGE = 1u;\n";
      out << "constant constexpr uint GPGA_SCHED_EDGE_NEGEDGE = 2u;\n";
      out << "constant constexpr uint GPGA_SCHED_EDGE_LIST = 3u;\n";
      out << "constant constexpr uint GPGA_SCHED_PROC_READY = 0u;\n";
      out << "constant constexpr uint GPGA_SCHED_PROC_BLOCKED = 1u;\n";
      out << "constant constexpr uint GPGA_SCHED_PROC_DONE = 2u;\n";
      out << "constant constexpr uint GPGA_SCHED_PHASE_ACTIVE = 0u;\n";
      out << "constant constexpr uint GPGA_SCHED_PHASE_NBA = 1u;\n";
      out << "constant constexpr uint GPGA_SCHED_STATUS_RUNNING = 0u;\n";
      out << "constant constexpr uint GPGA_SCHED_STATUS_IDLE = 1u;\n";
      out << "constant constexpr uint GPGA_SCHED_STATUS_FINISHED = 2u;\n";
      out << "constant constexpr uint GPGA_SCHED_STATUS_ERROR = 3u;\n";
      out << "constant constexpr uint GPGA_SCHED_STATUS_STOPPED = 4u;\n";
      out << "constant constexpr uint GPGA_SCHED_FLAG_INITIALIZED = 1u;\n";
      out << "constant constexpr uint GPGA_SCHED_FLAG_ACTIVE_INIT = 2u;\n";
      if (!system_task_info.monitor_stmts.empty()) {
        size_t max_args =
            std::max<size_t>(1, system_task_info.monitor_max_args);
        out << "constant constexpr uint GPGA_SCHED_MONITOR_COUNT = "
            << system_task_info.monitor_stmts.size() << "u;\n";
        out << "constant constexpr uint GPGA_SCHED_MONITOR_MAX_ARGS = " << max_args
            << "u;\n";
      }
      if (!system_task_info.strobe_stmts.empty()) {
        out << "constant constexpr uint GPGA_SCHED_STROBE_COUNT = "
            << system_task_info.strobe_stmts.size() << "u;\n";
      }
      if (system_task_info.has_system_tasks) {
        size_t max_args = std::max<size_t>(1, system_task_info.max_args);
        out << "constant constexpr uint GPGA_SCHED_SERVICE_MAX_ARGS = " << max_args
            << "u;\n";
        out << "constant constexpr uint GPGA_SCHED_SERVICE_WIDE_WORDS = "
            << service_wide_words << "u;\n";
        out << "constant constexpr uint GPGA_SCHED_STRING_COUNT = "
            << system_task_info.string_table.size() << "u;\n";
        out << "constant constexpr uint GPGA_SERVICE_INVALID_ID = 0xFFFFFFFFu;\n";
        out << "constant constexpr uint GPGA_SERVICE_ARG_VALUE = 0u;\n";
        out << "constant constexpr uint GPGA_SERVICE_ARG_IDENT = 1u;\n";
        out << "constant constexpr uint GPGA_SERVICE_ARG_STRING = 2u;\n";
        out << "constant constexpr uint GPGA_SERVICE_ARG_REAL = 3u;\n";
        out << "constant constexpr uint GPGA_SERVICE_ARG_WIDE = 4u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_DISPLAY = 0u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_MONITOR = 1u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FINISH = 2u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_DUMPFILE = 3u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_DUMPVARS = 4u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_READMEMH = 5u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_READMEMB = 6u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_STOP = 7u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_STROBE = 8u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_DUMPOFF = 9u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_DUMPON = 10u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_DUMPFLUSH = 11u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_DUMPALL = 12u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_DUMPLIMIT = 13u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FWRITE = 14u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FDISPLAY = 15u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FOPEN = 16u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FCLOSE = 17u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FGETC = 18u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FGETS = 19u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FEOF = 20u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FSCANF = 21u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_SSCANF = 22u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FTELL = 23u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_REWIND = 24u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_WRITEMEMH = 25u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_WRITEMEMB = 26u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FSEEK = 27u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FFLUSH = 28u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FERROR = 29u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FUNGETC = 30u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_FREAD = 31u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_WRITE = 32u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_SFORMAT = 33u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_TIMEFORMAT = 34u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_PRINTTIMESCALE = 35u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_TESTPLUSARGS = 36u;\n";
        out << "constant constexpr uint GPGA_SERVICE_KIND_VALUEPLUSARGS = 37u;\n";
        out << "struct GpgaServiceRecord {\n";
        out << "  uint kind;\n";
        out << "  uint pid;\n";
        out << "  uint format_id;\n";
        out << "  uint arg_count;\n";
        out << "  uint arg_kind[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
        out << "  uint arg_width[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
        out << "  ulong arg_val[GPGA_SCHED_SERVICE_MAX_ARGS];\n";
        if (service_wide_words > 0u) {
          out << "  ulong arg_wide_val[GPGA_SCHED_SERVICE_MAX_ARGS * "
                 "GPGA_SCHED_SERVICE_WIDE_WORDS];\n";
        }
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

      drive_declared.clear();

      out << "\n";
      out << "kernel void gpga_" << MslName(module.name) << "_sched_step(";
      int buffer_index = 0;
      bool first = true;
      auto emit_param = [&](const std::string& text) {
        if (!first) {
          out << ",\n";
        }
        first = false;
        out << text;
      };
      if (pack_signals) {
        emit_param("  device uchar* gpga_state [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (!pack_signals) {
        for (const auto& port : module.ports) {
          std::string qualifier =
              (port.dir == PortDir::kInput) ? "constant" : "device";
          std::string type = TypeForWidth(port.width);
          emit_param("  " + qualifier + " " + type + "* " + MslName(port.name) +
                     " [[buffer(" + std::to_string(buffer_index++) + ")]]");
        }
        for (const auto& reg : sched_reg_names) {
          std::string type = TypeForWidth(SignalWidth(module, reg));
          emit_param("  device " + type + "* " + MslName(reg) + " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        for (const auto* reg : trireg_nets) {
          emit_param("  device ulong* " + decay_name(reg->name) + " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
        for (const auto* net : array_nets) {
          std::string type = TypeForWidth(net->width);
          emit_param("  device " + type + "* " + MslName(net->name) +
                     " [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
      }
      if (pack_nb && !packed_nb_signals.empty()) {
        emit_param("  device uchar* nb_state [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (!pack_nb) {
        for (const auto& target : nb_targets_sorted) {
          std::string type = TypeForWidth(SignalWidth(module, target));
          emit_param("  device " + type + "* nb_" + MslName(target) +
                     " [[buffer(" + std::to_string(buffer_index++) + ")]]");
        }
      }
      for (const auto* net : nb_array_nets) {
        std::string type = TypeForWidth(net->width);
        emit_param("  device " + type + "* " + MslNameNext(net->name) +
                   " [[buffer(" + std::to_string(buffer_index++) + ")]]");
      };
      if (needs_force_shadow) {
        emit_param("  device uchar* sched_force_state [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (!force_target_list.empty()) {
        emit_param("  device uint* sched_force_id [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (!passign_target_list.empty()) {
        emit_param("  device uint* sched_passign_id [[buffer(" +
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
      if (has_edges) {
        emit_param("  device ulong* sched_edge_prev_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      if (has_edge_star) {
        emit_param("  device ulong* sched_edge_star_prev_val [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      emit_param("  device ulong* sched_wait_time [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_join_count [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_parent [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_join_tag [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      if (repeat_state_count > 0) {
        emit_param("  device uint* sched_repeat_left [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
        emit_param("  device uint* sched_repeat_active [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
      emit_param("  device ulong* sched_time [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_phase [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      emit_param("  device uint* sched_flags [[buffer(" +
                 std::to_string(buffer_index++) + ")]]");
      if (has_events) {
        emit_param("  device uint* sched_event_pending [[buffer(" +
                   std::to_string(buffer_index++) + ")]]");
      }
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
        if (service_wide_words > 0u) {
          emit_param("  device ulong* sched_monitor_wide_val [[buffer(" +
                     std::to_string(buffer_index++) + ")]]");
        }
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
    emit_param("  uint gid [[thread_position_in_grid]]) {\n");
    out << "  if (gid >= sched.count) {\n";
    out << "    return;\n";
    out << "  }\n";
    if (pack_signals) {
      emit_packed_signal_setup("sched.count");
    }
    emit_packed_nb_setup("sched.count");
    emit_packed_force_setup("sched.count");
    if (system_task_info.has_system_tasks) {
      out << "  sched_service_count[gid] = 0u;\n";
    }
    out << "  ulong __gpga_time = sched_time[gid];\n";
      out << "  if ((sched_flags[gid] & GPGA_SCHED_FLAG_INITIALIZED) == 0u) {\n";
      out << "    sched_time[gid] = 0ul;\n";
      out << "    __gpga_time = 0ul;\n";
      out << "    sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
      out << "    sched_flags[gid] = GPGA_SCHED_FLAG_INITIALIZED | GPGA_SCHED_FLAG_ACTIVE_INIT;\n";
      out << "    sched_error[gid] = 0u;\n";
      for (const auto* reg : trireg_nets) {
        out << "    " << decay_name(reg->name) << "[gid] = 0ul;\n";
      }
      if (has_delayed_nba) {
        out << "    sched_dnba_count[gid] = 0u;\n";
      }
    if (has_events) {
      out << "    for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
      out << "      sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
      out << "    }\n";
    }
    if (has_edges) {
      out << "    for (uint e = 0u; e < GPGA_SCHED_EDGE_COUNT; ++e) {\n";
      out << "      uint eidx = (gid * GPGA_SCHED_EDGE_COUNT) + e;\n";
      out << "      sched_edge_prev_val[eidx] = 0ul;\n";
      out << "    }\n";
    }
    if (has_edge_star) {
      out << "    for (uint s = 0u; s < GPGA_SCHED_EDGE_STAR_COUNT; ++s) {\n";
      out << "      uint sidx = (gid * GPGA_SCHED_EDGE_STAR_COUNT) + s;\n";
      out << "      sched_edge_star_prev_val[sidx] = 0ul;\n";
      out << "    }\n";
    }
    if (!system_task_info.monitor_stmts.empty()) {
      out << "    sched_monitor_enable[gid] = 1u;\n";
      out << "    for (uint m = 0u; m < GPGA_SCHED_MONITOR_COUNT; ++m) {\n";
      out << "      sched_monitor_active[(gid * GPGA_SCHED_MONITOR_COUNT) + m] = 0u;\n";
      out << "      for (uint a = 0u; a < GPGA_SCHED_MONITOR_MAX_ARGS; ++a) {\n";
      out << "        uint offset = ((gid * GPGA_SCHED_MONITOR_COUNT) + m) * GPGA_SCHED_MONITOR_MAX_ARGS + a;\n";
      out << "        sched_monitor_val[offset] = 0ul;\n";
      if (service_wide_words > 0u) {
        out << "        uint wide_offset = offset * GPGA_SCHED_SERVICE_WIDE_WORDS;\n";
        out << "        for (uint w = 0u; w < GPGA_SCHED_SERVICE_WIDE_WORDS; ++w) {\n";
        out << "          sched_monitor_wide_val[wide_offset + w] = 0ul;\n";
        out << "        }\n";
      }
      out << "      }\n";
      out << "    }\n";
    }
    if (!system_task_info.strobe_stmts.empty()) {
      out << "    for (uint s = 0u; s < GPGA_SCHED_STROBE_COUNT; ++s) {\n";
      out << "      sched_strobe_pending[(gid * GPGA_SCHED_STROBE_COUNT) + s] = 0u;\n";
      out << "    }\n";
    }
    if (!force_target_list.empty()) {
      out << "    for (uint f = 0u; f < GPGA_SCHED_FORCE_COUNT; ++f) {\n";
      out << "      sched_force_id[(gid * GPGA_SCHED_FORCE_COUNT) + f] = 0xFFFFFFFFu;\n";
      out << "    }\n";
    }
    if (!passign_target_list.empty()) {
      out << "    for (uint f = 0u; f < GPGA_SCHED_PCONT_COUNT; ++f) {\n";
      out << "      sched_passign_id[(gid * GPGA_SCHED_PCONT_COUNT) + f] = 0xFFFFFFFFu;\n";
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
      if (repeat_state_count > 0) {
        out << "    for (uint r = 0u; r < GPGA_SCHED_REPEAT_COUNT; ++r) {\n";
        out << "      uint ridx = (gid * GPGA_SCHED_REPEAT_COUNT) + r;\n";
        out << "      sched_repeat_left[ridx] = 0u;\n";
        out << "      sched_repeat_active[ridx] = 0u;\n";
        out << "    }\n";
      }
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
          out_lhs.expr = MslName(name) + "[gid]";
          out_lhs.ok = true;
          return out_lhs;
        }
        if (sched_locals.count(name) > 0) {
          out_lhs.expr = MslName(name);
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
            std::string idx = "uint(" + idx_val_expr + ")";
            std::string base = "(gid * " + std::to_string(info.array_size) +
                               "u) + " + idx;
            std::string guard =
                "(" + idx + " < " + std::to_string(info.array_size) + "u)";
            std::string name =
                use_nb ? MslNameNext(info.lhs) : MslName(info.lhs);
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
            target = "nb_" + MslName(info.lhs) + "[gid]";
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
          return "ulong(gpga_double_to_s64(" +
                 EmitRealValueExpr(expr, module, sched_locals, sched_regs) +
                 "))";
        }
        std::string delay =
            EmitExprSized(expr, 64, module, sched_locals, sched_regs);
        return "ulong(" + delay + ")";
      };

      auto force_slot_expr = [&](const std::string& target) -> std::string {
        auto it = force_target_index.find(target);
        if (it == force_target_index.end()) {
          return "";
        }
        return "(gid * GPGA_SCHED_FORCE_COUNT) + " +
               std::to_string(it->second) + "u";
      };
      auto passign_slot_expr = [&](const std::string& target) -> std::string {
        auto it = passign_target_index.find(target);
        if (it == passign_target_index.end()) {
          return "";
        }
        return "(gid * GPGA_SCHED_PCONT_COUNT) + " +
               std::to_string(it->second) + "u";
      };
      auto force_active_expr = [&](const std::string& target) -> std::string {
        std::string slot = force_slot_expr(target);
        if (slot.empty()) {
          return "false";
        }
        return "(sched_force_id[" + slot + "] != 0xFFFFFFFFu)";
      };
      auto passign_active_expr = [&](const std::string& target) -> std::string {
        std::string slot = passign_slot_expr(target);
        if (slot.empty()) {
          return "false";
        }
        return "(sched_passign_id[" + slot + "] != 0xFFFFFFFFu)";
      };
      auto override_active_expr = [&](const std::string& target) -> std::string {
        std::string force_active = force_active_expr(target);
        std::string passign_active = passign_active_expr(target);
        if (force_active == "false") {
          return passign_active;
        }
        if (passign_active == "false") {
          return force_active;
        }
        return "(" + force_active + " || " + passign_active + ")";
      };
      auto replace_prefix = [&](const std::string& ref,
                                const std::string& base,
                                const std::string& repl) -> std::string {
        if (ref.rfind(base, 0) == 0) {
          return repl + ref.substr(base.size());
        }
        return repl;
      };
      auto emit_force_value_assign =
          [&](const Statement& stmt, const std::string& target_expr,
              int indent) -> void {
            if (!stmt.assign.rhs) {
              return;
            }
            int width = SignalWidth(module, stmt.assign.lhs);
            if (width <= 0) {
              return;
            }
            bool lhs_real = SignalIsReal(module, stmt.assign.lhs);
            std::string rhs = lhs_real
                                  ? EmitRealBitsExpr(*stmt.assign.rhs, module,
                                                     sched_locals, sched_regs)
                                  : EmitExprSized(*stmt.assign.rhs, width,
                                                  module, sched_locals,
                                                  sched_regs);
            std::string pad(indent, ' ');
            out << pad << target_expr << " = " << rhs << ";\n";
          };

      emit_force_overrides = [&](int indent) -> void {
        if (override_target_list.empty()) {
          return;
        }
        std::string pad(indent, ' ');
        out << pad << "{\n";
        for (const auto& target : override_target_list) {
          auto force_it = force_target_index.find(target);
          auto passign_it = passign_target_index.find(target);
          if (force_it == force_target_index.end() &&
              passign_it == passign_target_index.end()) {
            continue;
          }
          SequentialAssign temp;
          temp.lhs = target;
          temp.nonblocking = false;
          LvalueInfo lhs = BuildLvalue(temp, module, sched_locals, sched_regs,
                                       false);
          if (!lhs.ok) {
            continue;
          }
          std::string suffix = MslName(target);
          if (force_it != force_target_index.end()) {
            std::string force_slot = force_slot_expr(target);
            out << pad << "  uint __gpga_force_id_" << suffix
                << " = sched_force_id[" << force_slot << "];\n";
            out << pad << "  if (__gpga_force_id_" << suffix
                << " != 0xFFFFFFFFu) {\n";
            out << pad << "    switch (__gpga_force_id_" << suffix << ") {\n";
            auto list_it = force_stmts_by_target.find(target);
            if (list_it != force_stmts_by_target.end()) {
              for (const auto* stmt : list_it->second) {
                auto id_it = force_stmt_ids.find(stmt);
                if (id_it == force_stmt_ids.end()) {
                  continue;
                }
                out << pad << "      case " << id_it->second << "u: {\n";
                emit_force_value_assign(*stmt, lhs.expr, indent + 8);
                out << pad << "        break;\n";
                out << pad << "      }\n";
              }
            }
            out << pad << "      default:\n";
            out << pad << "        break;\n";
            out << pad << "    }\n";
            out << pad << "  }";
            if (passign_it != passign_target_index.end()) {
              out << " else {\n";
              std::string passign_slot = passign_slot_expr(target);
              out << pad << "    uint __gpga_passign_id_" << suffix
                  << " = sched_passign_id[" << passign_slot << "];\n";
              out << pad << "    if (__gpga_passign_id_" << suffix
                  << " != 0xFFFFFFFFu) {\n";
              out << pad << "      switch (__gpga_passign_id_" << suffix
                  << ") {\n";
              auto plist_it = passign_stmts_by_target.find(target);
              if (plist_it != passign_stmts_by_target.end()) {
                for (const auto* stmt : plist_it->second) {
                  auto id_it = passign_stmt_ids.find(stmt);
                  if (id_it == passign_stmt_ids.end()) {
                    continue;
                  }
                  out << pad << "        case " << id_it->second << "u: {\n";
                  emit_force_value_assign(*stmt, lhs.expr, indent + 10);
                  out << pad << "          break;\n";
                  out << pad << "        }\n";
                }
              }
              out << pad << "        default:\n";
              out << pad << "          break;\n";
              out << pad << "      }\n";
              out << pad << "    }\n";
              out << pad << "  }\n";
            } else {
              out << "\n";
            }
            continue;
          }
          if (passign_it != passign_target_index.end()) {
            std::string passign_slot = passign_slot_expr(target);
            out << pad << "  uint __gpga_passign_id_" << suffix
                << " = sched_passign_id[" << passign_slot << "];\n";
            out << pad << "  if (__gpga_passign_id_" << suffix
                << " != 0xFFFFFFFFu) {\n";
            out << pad << "    switch (__gpga_passign_id_" << suffix << ") {\n";
            auto plist_it = passign_stmts_by_target.find(target);
            if (plist_it != passign_stmts_by_target.end()) {
              for (const auto* stmt : plist_it->second) {
                auto id_it = passign_stmt_ids.find(stmt);
                if (id_it == passign_stmt_ids.end()) {
                  continue;
                }
                out << pad << "      case " << id_it->second << "u: {\n";
                emit_force_value_assign(*stmt, lhs.expr, indent + 8);
                out << pad << "        break;\n";
                out << pad << "      }\n";
              }
            }
            out << pad << "      default:\n";
            out << pad << "        break;\n";
            out << pad << "    }\n";
            out << pad << "  }\n";
          }
        }
        out << pad << "}\n";
      };

      auto emit_passign_apply_target =
          [&](const std::string& target, const LvalueInfo& lhs,
              int indent) -> void {
            auto list_it = passign_stmts_by_target.find(target);
            if (list_it == passign_stmts_by_target.end()) {
              return;
            }
            std::string pad(indent, ' ');
            std::string slot = passign_slot_expr(target);
            std::string suffix = MslName(target);
            out << pad << "uint __gpga_passign_id_" << suffix
                << " = sched_passign_id[" << slot << "];\n";
            out << pad << "if (__gpga_passign_id_" << suffix
                << " != 0xFFFFFFFFu) {\n";
            out << pad << "  switch (__gpga_passign_id_" << suffix << ") {\n";
            for (const auto* stmt : list_it->second) {
              auto id_it = passign_stmt_ids.find(stmt);
              if (id_it == passign_stmt_ids.end()) {
                continue;
              }
              out << pad << "    case " << id_it->second << "u: {\n";
              emit_force_value_assign(*stmt, lhs.expr, indent + 6);
              out << pad << "      break;\n";
              out << pad << "    }\n";
            }
            out << pad << "    default:\n";
            out << pad << "      break;\n";
            out << pad << "  }\n";
            out << pad << "}\n";
          };

      out << "  sched_status[gid] = GPGA_SCHED_STATUS_RUNNING;\n";
      out << "  bool finished = false;\n";
      out << "  bool stopped = false;\n";
      out << "  uint steps = sched.max_steps;\n";
      out << "  while (steps > 0u) {\n";
      out << "    bool did_work = false;\n";
      out << "    if (sched_phase[gid] == GPGA_SCHED_PHASE_ACTIVE) {\n";
      out << "      if ((sched_flags[gid] & GPGA_SCHED_FLAG_ACTIVE_INIT) != 0u) {\n";
      out << "        sched_flags[gid] &= ~GPGA_SCHED_FLAG_ACTIVE_INIT;\n";
      if (!nb_targets_sorted.empty()) {
        out << "        // Initialize NBA buffers for this delta.\n";
        for (const auto& target : nb_targets_sorted) {
          out << "        nb_" << MslName(target) << "[gid] = "
              << MslName(target) << "[gid];\n";
        }
      }
      if (!nb_array_nets.empty()) {
        out << "        // Initialize array NBA buffers.\n";
        for (const auto* net : nb_array_nets) {
          out << "        for (uint i = 0u; i < " << net->array_size
              << "u; ++i) {\n";
          out << "          " << MslNameNext(net->name) << "[(gid * "
              << net->array_size << "u) + i] = " << MslName(net->name)
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
      emit_sched_comb_update(6);
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
      bool lhs_real = SignalIsReal(module, assign.lhs);
      std::string sized =
          lhs_real
              ? EmitRealBitsExpr(*assign.rhs, module, locals_override,
                                 sched_regs)
              : EmitExprSized(*assign.rhs, lhs.width, module, locals_override,
                              sched_regs);
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
            target = "nb_" + MslName(assign.lhs) + "[gid]";
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
            target = "nb_" + MslName(assign.lhs) + "[gid]";
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
        out << pad << "nb_" << MslName(assign.lhs) << "[gid] = " << sized
            << ";\n";
        return;
      }
      auto emit_store = [&](const std::string& target, int store_indent) -> void {
        std::string store_pad(store_indent, ' ');
        if (lhs.is_bit_select) {
          std::string update = EmitBitSelectUpdate(
              target, lhs.bit_index, lhs.base_width, sized);
          if (!lhs.guard.empty()) {
            out << store_pad << "if " << lhs.guard << " {\n";
            out << store_pad << "  " << target << " = " << update << ";\n";
            out << store_pad << "}\n";
          } else {
            out << store_pad << target << " = " << update << ";\n";
          }
          return;
        }
        if (lhs.is_range) {
          std::string update = EmitRangeSelectUpdate(
              target,
              lhs.is_indexed_range ? lhs.range_index
                                   : std::to_string(lhs.range_lsb),
              lhs.base_width, lhs.width, sized);
          if (!lhs.guard.empty()) {
            out << store_pad << "if " << lhs.guard << " {\n";
            out << store_pad << "  " << target << " = " << update << ";\n";
            out << store_pad << "}\n";
          } else {
            out << store_pad << target << " = " << update << ";\n";
          }
          return;
        }
        if (!lhs.guard.empty()) {
          out << store_pad << "if " << lhs.guard << " {\n";
          out << store_pad << "  " << target << " = " << sized << ";\n";
          out << store_pad << "}\n";
        } else {
          out << store_pad << target << " = " << sized << ";\n";
        }
      };

      bool is_local = locals_override.count(assign.lhs) > 0;
      bool has_override =
          !is_local &&
          (force_target_index.count(assign.lhs) > 0 ||
           passign_target_index.count(assign.lhs) > 0);
      if (has_override) {
        std::string override_cond = override_active_expr(assign.lhs);
        std::string shadow_expr =
            replace_prefix(lhs.expr, MslName(assign.lhs),
                           shadow_name(assign.lhs));
        out << pad << "if (" << override_cond << ") {\n";
        emit_store(shadow_expr, indent + 2);
        out << pad << "} else {\n";
        emit_store(lhs.expr, indent + 2);
        out << pad << "}\n";
        return;
      }
      emit_store(lhs.expr, indent);
    };

    auto emit_lvalue_value =
        [&](const SequentialAssign& assign, const std::string& value_expr,
            int value_width, int indent,
            const std::unordered_set<std::string>& locals_override) -> void {
      std::string pad(indent, ' ');
      LvalueInfo lhs =
          BuildLvalue(assign, module, locals_override, sched_regs, false);
      if (!lhs.ok) {
        return;
      }
      int target_width = lhs.width > 0 ? lhs.width : value_width;
      std::string cast_expr = (target_width > 32)
                                  ? value_expr
                                  : ("uint(" + value_expr + ")");
      std::string sized = MaskForWidthExpr(cast_expr, target_width);
      auto emit_store = [&](const std::string& target, int store_indent) -> void {
        std::string store_pad(store_indent, ' ');
        if (lhs.is_bit_select) {
          std::string update = EmitBitSelectUpdate(
              target, lhs.bit_index, lhs.base_width, sized);
          if (!lhs.guard.empty()) {
            out << store_pad << "if " << lhs.guard << " {\n";
            out << store_pad << "  " << target << " = " << update << ";\n";
            out << store_pad << "}\n";
          } else {
            out << store_pad << target << " = " << update << ";\n";
          }
          return;
        }
        if (lhs.is_range) {
          std::string update = EmitRangeSelectUpdate(
              target,
              lhs.is_indexed_range ? lhs.range_index
                                   : std::to_string(lhs.range_lsb),
              lhs.base_width, lhs.width, sized);
          if (!lhs.guard.empty()) {
            out << store_pad << "if " << lhs.guard << " {\n";
            out << store_pad << "  " << target << " = " << update << ";\n";
            out << store_pad << "}\n";
          } else {
            out << store_pad << target << " = " << update << ";\n";
          }
          return;
        }
        if (!lhs.guard.empty()) {
          out << store_pad << "if " << lhs.guard << " {\n";
          out << store_pad << "  " << target << " = " << sized << ";\n";
          out << store_pad << "}\n";
        } else {
          out << store_pad << target << " = " << sized << ";\n";
        }
      };

      bool is_local = locals_override.count(assign.lhs) > 0;
      bool has_override =
          !is_local &&
          (force_target_index.count(assign.lhs) > 0 ||
           passign_target_index.count(assign.lhs) > 0);
      if (has_override) {
        std::string override_cond = override_active_expr(assign.lhs);
        std::string shadow_expr =
            replace_prefix(lhs.expr, MslName(assign.lhs),
                           shadow_name(assign.lhs));
        out << pad << "if (" << override_cond << ") {\n";
        emit_store(shadow_expr, indent + 2);
        out << pad << "} else {\n";
        emit_store(lhs.expr, indent + 2);
        out << pad << "}\n";
        return;
      }
      emit_store(lhs.expr, indent);
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
        bool wide = false;
      };

        auto build_service_args =
            [&](const Statement& stmt, const std::string& name,
                size_t arg_start, std::string* format_id_expr,
                std::vector<ServiceArg>* args) -> bool {
          if (!format_id_expr || !args) {
            return false;
          }
          *format_id_expr = "GPGA_SERVICE_INVALID_ID";
          if (stmt.task_args.size() > arg_start &&
              stmt.task_args[arg_start]->kind == ExprKind::kString) {
            uint32_t id = 0;
            if (!string_id_for(stmt.task_args[arg_start]->string_value, &id)) {
              return false;
            }
            *format_id_expr = std::to_string(id) + "u";
          }

          std::vector<char> format_specs;
          bool has_format_specs =
              stmt.task_args.size() > arg_start &&
              stmt.task_args[arg_start] &&
              stmt.task_args[arg_start]->kind == ExprKind::kString;
          if (has_format_specs) {
            format_specs =
                ExtractFormatSpecs(stmt.task_args[arg_start]->string_value);
          }
          size_t format_arg_index = 0;

          bool requires_string =
              name == "$dumpfile" || name == "$readmemh" ||
              name == "$readmemb" || name == "$writememh" ||
              name == "$writememb";
          if (requires_string &&
              *format_id_expr == "GPGA_SERVICE_INVALID_ID") {
          return false;
        }

          bool ident_as_string = TaskTreatsIdentifierAsString(name);
          args->clear();
          if (stmt.task_args.size() > arg_start) {
            args->reserve(stmt.task_args.size() - arg_start);
          }
          for (size_t i = arg_start; i < stmt.task_args.size(); ++i) {
            const auto& arg = stmt.task_args[i];
            if (!arg) {
              continue;
            }
            bool is_format_literal = has_format_specs && i == arg_start &&
                                     arg->kind == ExprKind::kString;
            char spec = '\0';
            if (has_format_specs && !is_format_literal) {
              if (format_arg_index < format_specs.size()) {
                spec = format_specs[format_arg_index];
              }
              ++format_arg_index;
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
            if (spec == 's' && arg->kind == ExprKind::kIdentifier) {
              uint32_t id = 0;
              if (!string_id_for(arg->ident, &id)) {
                return false;
              }
              int width = SignalWidth(module, arg->ident);
              if (width <= 0) {
                width = 1;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                          std::to_string(id) + "ul"});
              continue;
            }
            if (arg->kind == ExprKind::kCall && arg->ident == "$time") {
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_VALUE", 64,
                                         "__gpga_time"});
              continue;
            }
            if (arg->kind == ExprKind::kCall && arg->ident == "$stime") {
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_VALUE", 32,
                                         "uint(__gpga_time)"});
              continue;
            }
          bool is_real = ExprIsRealValue(*arg, module);
          int width = is_real ? 64 : ExprWidth(*arg, module);
          if (width <= 0) {
            width = 1;
          }
          bool wide = !is_real && width > 64;
          std::string value =
              is_real
                  ? EmitRealBitsExpr(*arg, module, sched_locals, sched_regs)
                  : EmitExprSized(*arg, width, module, sched_locals, sched_regs);
          std::string kind = is_real
                                 ? "GPGA_SERVICE_ARG_REAL"
                                 : (wide ? "GPGA_SERVICE_ARG_WIDE"
                                         : "GPGA_SERVICE_ARG_VALUE");
          std::string val = wide ? value : to_ulong(value, width);
          args->push_back(ServiceArg{kind, width, val, wide});
        }
        return true;
      };

      auto emit_service_args =
          [&](const std::vector<ServiceArg>& args, int indent) -> void {
        std::string pad(indent, ' ');
        for (size_t i = 0; i < args.size(); ++i) {
          out << pad << "    sched_service[__gpga_svc_offset].arg_kind[" << i
              << "] = " << args[i].kind << ";\n";
          out << pad << "    sched_service[__gpga_svc_offset].arg_width[" << i
              << "] = " << args[i].width << "u;\n";
          if (args[i].wide) {
            std::string type = TypeForWidth(args[i].width);
            out << pad << "    " << type << " __gpga_wide_val" << i << " = "
                << args[i].val << ";\n";
            out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
                << "] = gpga_wide_to_u64_" << args[i].width
                << "(__gpga_wide_val" << i << ");\n";
            int word_count = (args[i].width + 63) / 64;
            out << pad << "    uint __gpga_wide_base" << i << " = " << i
                << "u * GPGA_SCHED_SERVICE_WIDE_WORDS;\n";
            out << pad << "    for (uint __gpga_wide_word = 0u; "
                       "__gpga_wide_word < "
                << word_count << "u; ++__gpga_wide_word) {\n";
            out << pad << "      sched_service[__gpga_svc_offset].arg_wide_val"
                       "[__gpga_wide_base"
                << i << " + __gpga_wide_word] = __gpga_wide_val" << i
                << ".w[__gpga_wide_word];\n";
            out << pad << "    }\n";
          } else {
            out << pad << "    sched_service[__gpga_svc_offset].arg_val[" << i
                << "] = " << args[i].val << ";\n";
          }
        }
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
        emit_service_args(args, indent);
        out << pad << "  }\n";
        out << pad << "}\n";
      };

      auto build_syscall_args =
          [&](const Expr& call, const std::string& name,
              std::string* format_id_expr,
              std::vector<ServiceArg>* args) -> bool {
        if (!format_id_expr || !args) {
          return false;
        }
        *format_id_expr = "GPGA_SERVICE_INVALID_ID";
        args->clear();
        args->reserve(call.call_args.size());
        for (size_t i = 0; i < call.call_args.size(); ++i) {
          const Expr* arg = call.call_args[i].get();
          if (!arg) {
            continue;
          }
          if (name == "$fgets" && i == 0) {
            if (arg->kind != ExprKind::kIdentifier) {
              return false;
            }
            uint32_t id = 0;
            if (!string_id_for(arg->ident, &id)) {
              return false;
            }
            int width = SignalWidth(module, arg->ident);
            if (width <= 0) {
              width = 1;
            }
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                       std::to_string(id) + "ul"});
            continue;
          }
          if ((name == "$fscanf" || name == "$sscanf") && i >= 2) {
            if (arg->kind != ExprKind::kIdentifier) {
              return false;
            }
            uint32_t id = 0;
            if (!string_id_for(arg->ident, &id)) {
              return false;
            }
            int width = SignalWidth(module, arg->ident);
            if (width <= 0) {
              width = 1;
            }
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                       std::to_string(id) + "ul"});
            continue;
          }
          if (name == "$sscanf" && i == 0) {
            if (arg->kind == ExprKind::kString) {
              uint32_t id = 0;
              if (!string_id_for(arg->string_value, &id)) {
                return false;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                         std::to_string(id) + "ul"});
              continue;
            }
            if (arg->kind == ExprKind::kIdentifier) {
              uint32_t id = 0;
              if (!string_id_for(arg->ident, &id)) {
                return false;
              }
              int width = SignalWidth(module, arg->ident);
              if (width <= 0) {
                width = 1;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", width,
                                         std::to_string(id) + "ul"});
              continue;
            }
            return false;
          }
          if (name == "$fopen" && i < 2) {
            if (arg->kind == ExprKind::kString) {
              uint32_t id = 0;
              if (!string_id_for(arg->string_value, &id)) {
                return false;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                         std::to_string(id) + "ul"});
              continue;
            }
            if (arg->kind == ExprKind::kIdentifier) {
              uint32_t id = 0;
              if (!string_id_for(arg->ident, &id)) {
                return false;
              }
              args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", 0,
                                         std::to_string(id) + "ul"});
              continue;
            }
            return false;
          }
          if ((name == "$fscanf" || name == "$sscanf") && i == 1 &&
              arg->kind == ExprKind::kString) {
            uint32_t id = 0;
            if (!string_id_for(arg->string_value, &id)) {
              return false;
            }
            *format_id_expr = std::to_string(id) + "u";
            args->push_back(ServiceArg{"GPGA_SERVICE_ARG_STRING", 0,
                                       std::to_string(id) + "ul"});
            continue;
          }
          bool is_real = ExprIsRealValue(*arg, module);
          int width = is_real ? 64 : ExprWidth(*arg, module);
          if (width <= 0) {
            width = 1;
          }
          bool wide = !is_real && width > 64;
          std::string value =
              is_real
                  ? EmitRealBitsExpr(*arg, module, sched_locals, sched_regs)
                  : EmitExprSized(*arg, width, module, sched_locals, sched_regs);
          std::string kind = is_real
                                 ? "GPGA_SERVICE_ARG_REAL"
                                 : (wide ? "GPGA_SERVICE_ARG_WIDE"
                                         : "GPGA_SERVICE_ARG_VALUE");
          std::string val = wide ? value : to_ulong(value, width);
          args->push_back(ServiceArg{kind, width, val, wide});
        }
        return true;
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
        emit_service_args(args, indent);
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
        emit_service_args(args, indent);
        out << pad << "  }\n";
        out << pad << "}\n";
      };

      struct BodyCase {
        int pc = 0;
        const Statement* owner = nullptr;
        std::vector<const Statement*> body;
        int next_pc = 0;
        int loop_pc = -1;
        bool is_forever_body = false;
        bool is_assign_delay = false;
        int delay_id = -1;
        bool is_service_resume = false;
        bool is_service_cond = false;
        int service_width = 0;
        bool service_invert = false;
        int service_true_pc = 0;
        int service_false_pc = 0;
      };
      int pc_counter = 0;
      std::vector<BodyCase> body_cases;

      auto emit_syscall_assign =
          [&](const Statement& stmt, const Expr& call, int resume_pc,
              int indent) -> bool {
        if (call.kind != ExprKind::kCall ||
            !IsFileSystemFunctionName(call.ident)) {
          return false;
        }
        const char* kind_expr = nullptr;
        if (call.ident == "$fopen") {
          kind_expr = "GPGA_SERVICE_KIND_FOPEN";
        } else if (call.ident == "$fclose") {
          kind_expr = "GPGA_SERVICE_KIND_FCLOSE";
        } else if (call.ident == "$fgetc") {
          kind_expr = "GPGA_SERVICE_KIND_FGETC";
        } else if (call.ident == "$fgets") {
          kind_expr = "GPGA_SERVICE_KIND_FGETS";
        } else if (call.ident == "$feof") {
          kind_expr = "GPGA_SERVICE_KIND_FEOF";
        } else if (call.ident == "$ftell") {
          kind_expr = "GPGA_SERVICE_KIND_FTELL";
        } else if (call.ident == "$fseek") {
          kind_expr = "GPGA_SERVICE_KIND_FSEEK";
        } else if (call.ident == "$ferror") {
          kind_expr = "GPGA_SERVICE_KIND_FERROR";
        } else if (call.ident == "$ungetc") {
          kind_expr = "GPGA_SERVICE_KIND_FUNGETC";
        } else if (call.ident == "$fread") {
          kind_expr = "GPGA_SERVICE_KIND_FREAD";
          } else if (call.ident == "$fscanf") {
            kind_expr = "GPGA_SERVICE_KIND_FSCANF";
          } else if (call.ident == "$sscanf") {
            kind_expr = "GPGA_SERVICE_KIND_SSCANF";
          } else if (call.ident == "$test$plusargs") {
            kind_expr = "GPGA_SERVICE_KIND_TESTPLUSARGS";
          } else if (call.ident == "$value$plusargs") {
            kind_expr = "GPGA_SERVICE_KIND_VALUEPLUSARGS";
          }
        if (!kind_expr) {
          return false;
        }
        std::string format_id_expr;
        std::vector<ServiceArg> args;
        if (!build_syscall_args(call, call.ident, &format_id_expr, &args)) {
          out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
          out << std::string(indent, ' ')
              << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          return true;
        }
        int width = SignalWidth(module, stmt.assign.lhs);
        if (width <= 0) {
          width = ExprWidth(call, module);
        }
        if (width <= 0) {
          width = 1;
        }
        int body_pc = pc_counter++;
        BodyCase body_case;
        body_case.pc = body_pc;
        body_case.owner = &stmt;
        body_case.next_pc = resume_pc;
        body_case.is_service_resume = true;
        body_case.service_width = width;
        body_cases.push_back(std::move(body_case));

        emit_service_record(kind_expr, format_id_expr, args, indent);
        out << std::string(indent, ' ')
            << "sched_wait_kind[idx] = GPGA_SCHED_WAIT_SERVICE;\n";
        out << std::string(indent, ' ') << "sched_wait_time[idx] = 0ul;\n";
        out << std::string(indent, ' ') << "sched_pc[idx] = " << body_pc
            << "u;\n";
        out << std::string(indent, ' ')
            << "sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
        return true;
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
          if (args[i].kind != "GPGA_SERVICE_ARG_VALUE" &&
              args[i].kind != "GPGA_SERVICE_ARG_REAL" &&
              args[i].kind != "GPGA_SERVICE_ARG_WIDE") {
            continue;
          }
          int width = args[i].width;
          if (width <= 0) {
            width = 1;
          }
          uint64_t mask = MaskForWidth64(width);
          std::string mask_literal = std::to_string(mask) + "ul";
          std::string val_expr = args[i].val;
          if (args[i].wide) {
            val_expr = "gpga_wide_to_u64_" + std::to_string(width) + "(" +
                       args[i].val + ")";
          }
          out << pad << "ulong " << prefix << "_val" << i << " = ("
              << val_expr << ") & " << mask_literal << ";\n";
          out << pad << "uint " << prefix << "_slot" << i << " = "
              << prefix << "_base + " << i << "u;\n";
          out << pad << "if (((sched_monitor_val[" << prefix << "_slot" << i
              << "] ^ " << prefix << "_val" << i << ") & " << mask_literal
              << ") != 0ul) {\n";
          out << pad << "  " << changed << " = true;\n";
          out << pad << "}\n";
          out << pad << "sched_monitor_val[" << prefix << "_slot" << i
              << "] = " << prefix << "_val" << i << ";\n";
          if (args[i].wide && service_wide_words > 0u) {
            int word_count = (width + 63) / 64;
            int last_bits = width - (word_count - 1) * 64;
            uint64_t last_mask = MaskForWidth64(last_bits);
            std::string type = TypeForWidth(width);
            out << pad << type << " " << prefix << "_wide_val" << i << " = "
                << args[i].val << ";\n";
            out << pad << "uint " << prefix << "_wbase" << i << " = "
                << prefix << "_slot" << i
                << " * GPGA_SCHED_SERVICE_WIDE_WORDS;\n";
            out << pad << "for (uint __gpga_wide_word" << i
                << " = 0u; __gpga_wide_word" << i << " < " << word_count
                << "u; ++__gpga_wide_word" << i << ") {\n";
            out << pad << "  ulong __gpga_wide_mask" << i
                << " = 0xFFFFFFFFFFFFFFFFul;\n";
            if (last_bits < 64) {
              out << pad << "  if (__gpga_wide_word" << i << " == "
                  << (word_count - 1) << "u) {\n";
              out << pad << "    __gpga_wide_mask" << i << " = "
                  << std::to_string(last_mask) << "ul;\n";
              out << pad << "  }\n";
            }
            out << pad << "  ulong __gpga_wide_val" << i << "_w = "
                << prefix << "_wide_val" << i << ".w[__gpga_wide_word" << i
                << "] & __gpga_wide_mask" << i << ";\n";
            out << pad << "  uint __gpga_wide_slot" << i << " = "
                << prefix << "_wbase" << i
                << " + __gpga_wide_word" << i << ";\n";
            out << pad << "  if (((sched_monitor_wide_val[__gpga_wide_slot" << i
                << "] ^ __gpga_wide_val" << i << "_w) & __gpga_wide_mask" << i
                << ") != 0ul) {\n";
            out << pad << "    " << changed << " = true;\n";
            out << pad << "  }\n";
            out << pad << "  sched_monitor_wide_val[__gpga_wide_slot" << i
                << "] = __gpga_wide_val" << i << "_w;\n";
            out << pad << "}\n";
          }
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
        if (name == "$sformat") {
          if (stmt.task_args.size() < 2 || !stmt.task_args[0]) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          const Expr* target = stmt.task_args[0].get();
          if (!target || target->kind != ExprKind::kIdentifier) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          std::string format_id_expr;
          std::vector<ServiceArg> args;
          if (!build_service_args(stmt, name, 1, &format_id_expr, &args) ||
              format_id_expr == "GPGA_SERVICE_INVALID_ID") {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          uint32_t target_id = 0;
          if (!string_id_for(target->ident, &target_id)) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          int width = SignalWidth(module, target->ident);
          if (width <= 0) {
            width = 1;
          }
          ServiceArg target_arg{"GPGA_SERVICE_ARG_IDENT", width,
                                std::to_string(target_id) + "ul"};
          args.insert(args.begin(), target_arg);
          emit_service_record("GPGA_SERVICE_KIND_SFORMAT", format_id_expr, args,
                              indent);
          return;
        }
        const char* kind_expr = nullptr;
        size_t arg_start = 0;
        bool guard_file_fd = false;
        std::string fd_expr;
        if (name == "$display") {
          kind_expr = "GPGA_SERVICE_KIND_DISPLAY";
        } else if (name == "$write") {
          kind_expr = "GPGA_SERVICE_KIND_WRITE";
        } else if (name == "$fdisplay") {
          kind_expr = "GPGA_SERVICE_KIND_FDISPLAY";
          arg_start = 1;
          guard_file_fd = true;
        } else if (name == "$monitor") {
          kind_expr = "GPGA_SERVICE_KIND_MONITOR";
        } else if (name == "$finish") {
          kind_expr = "GPGA_SERVICE_KIND_FINISH";
        } else if (name == "$stop") {
          kind_expr = "GPGA_SERVICE_KIND_STOP";
        } else if (name == "$fwrite") {
          kind_expr = "GPGA_SERVICE_KIND_FWRITE";
          arg_start = 1;
          guard_file_fd = true;
        } else if (name == "$fclose") {
          kind_expr = "GPGA_SERVICE_KIND_FCLOSE";
          arg_start = 1;
          guard_file_fd = true;
        } else if (name == "$fflush") {
          kind_expr = "GPGA_SERVICE_KIND_FFLUSH";
          guard_file_fd = !stmt.task_args.empty();
        } else if (name == "$ftell") {
          kind_expr = "GPGA_SERVICE_KIND_FTELL";
          guard_file_fd = true;
        } else if (name == "$rewind") {
          kind_expr = "GPGA_SERVICE_KIND_REWIND";
          guard_file_fd = true;
        } else if (name == "$dumpfile") {
          kind_expr = "GPGA_SERVICE_KIND_DUMPFILE";
        } else if (name == "$dumpvars") {
          kind_expr = "GPGA_SERVICE_KIND_DUMPVARS";
          } else if (name == "$readmemh") {
            kind_expr = "GPGA_SERVICE_KIND_READMEMH";
          } else if (name == "$readmemb") {
            kind_expr = "GPGA_SERVICE_KIND_READMEMB";
          } else if (name == "$writememh") {
            kind_expr = "GPGA_SERVICE_KIND_WRITEMEMH";
          } else if (name == "$writememb") {
            kind_expr = "GPGA_SERVICE_KIND_WRITEMEMB";
          } else if (name == "$dumpoff") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPOFF";
          } else if (name == "$dumpon") {
            kind_expr = "GPGA_SERVICE_KIND_DUMPON";
        } else if (name == "$dumpflush") {
          kind_expr = "GPGA_SERVICE_KIND_DUMPFLUSH";
        } else if (name == "$dumpall") {
          kind_expr = "GPGA_SERVICE_KIND_DUMPALL";
        } else if (name == "$dumplimit") {
          kind_expr = "GPGA_SERVICE_KIND_DUMPLIMIT";
        } else if (name == "$timeformat") {
          kind_expr = "GPGA_SERVICE_KIND_TIMEFORMAT";
        } else if (name == "$printtimescale") {
          kind_expr = "GPGA_SERVICE_KIND_PRINTTIMESCALE";
        } else {
          out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
          out << std::string(indent, ' ')
              << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          return;
        }

        std::string format_id_expr;
        std::vector<ServiceArg> args;
        std::string fd_guard;
        if (guard_file_fd) {
          if (stmt.task_args.empty() || !stmt.task_args[0]) {
            out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
            out << std::string(indent, ' ')
                << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          fd_expr = EmitExprSized(*stmt.task_args[0], 32, module, sched_locals,
                                  sched_regs);
          fd_expr = MaskForWidthExpr(fd_expr, 32);
          fd_guard = "(" + fd_expr + " != 0u)";
        }
        if (!build_service_args(stmt, name, arg_start, &format_id_expr,
                                &args)) {
          out << std::string(indent, ' ') << "sched_error[gid] = 1u;\n";
          out << std::string(indent, ' ')
              << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
          return;
        }
        if (guard_file_fd && arg_start > 0) {
          args.insert(args.begin(),
                      ServiceArg{"GPGA_SERVICE_ARG_VALUE", 32,
                                 to_ulong(fd_expr, 32)});
        }

        bool dump_control =
            name == "$dumpfile" || name == "$dumpvars" ||
            name == "$dumpoff" || name == "$dumpon" ||
            name == "$dumpflush" || name == "$dumpall" ||
            name == "$dumplimit" || name == "$writememh" ||
            name == "$writememb";

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
        } else if (dump_control) {
          std::string pad(indent, ' ');
          out << pad << "if (gid == 0u) {\n";
          emit_service_record(kind_expr, format_id_expr, args, indent + 2);
          out << pad << "}\n";
        } else if (guard_file_fd) {
          std::string pad(indent, ' ');
          out << pad << "if (" << fd_guard << ") {\n";
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
                int resume_pc, const auto& self) -> void {
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
        if (stmt.kind == StatementKind::kForce ||
            stmt.kind == StatementKind::kRelease) {
          bool is_proc = stmt.is_procedural;
          const std::string& target =
              (stmt.kind == StatementKind::kForce) ? stmt.force_target
                                                   : stmt.release_target;
          auto target_it = is_proc ? passign_target_index.find(target)
                                   : force_target_index.find(target);
          if (target_it == (is_proc ? passign_target_index.end()
                                    : force_target_index.end())) {
            out << pad << "sched_error[gid] = 1u;\n";
            out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          if (stmt.kind == StatementKind::kForce) {
            if (stmt.assign.delay) {
              out << pad << "sched_error[gid] = 1u;\n";
              out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            auto id_it = is_proc ? passign_stmt_ids.find(&stmt)
                                 : force_stmt_ids.find(&stmt);
            if (id_it == (is_proc ? passign_stmt_ids.end()
                                  : force_stmt_ids.end())) {
              out << pad << "sched_error[gid] = 1u;\n";
              out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            LvalueInfo lhs =
                BuildLvalue(stmt.assign, module, locals_override, sched_regs,
                            false);
            if (!lhs.ok) {
              out << pad << "sched_error[gid] = 1u;\n";
              out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              return;
            }
            if (override_is_reg[target]) {
              out << pad << "if (!(" << override_active_expr(target)
                  << ")) {\n";
              out << pad << "  " << shadow_name(target) << "[gid] = "
                  << lhs.expr << ";\n";
              out << pad << "}\n";
            }
            std::string slot =
                is_proc ? passign_slot_expr(target) : force_slot_expr(target);
            if (is_proc) {
              out << pad << "sched_passign_id[" << slot << "] = "
                  << id_it->second << "u;\n";
              std::string force_active = force_active_expr(target);
              if (force_active != "false") {
                out << pad << "if (!" << force_active << ") {\n";
                emit_force_value_assign(stmt, lhs.expr, indent + 2);
                out << pad << "}\n";
              } else {
                emit_force_value_assign(stmt, lhs.expr, indent);
              }
            } else {
              out << pad << "sched_force_id[" << slot << "] = "
                  << id_it->second << "u;\n";
              emit_force_value_assign(stmt, lhs.expr, indent);
            }
            return;
          }
          std::string slot =
              is_proc ? passign_slot_expr(target) : force_slot_expr(target);
          if (is_proc) {
            out << pad << "sched_passign_id[" << slot
                << "] = 0xFFFFFFFFu;\n";
            if (override_is_reg[target]) {
              std::string force_active = force_active_expr(target);
              if (force_active != "false") {
                out << pad << "if (!" << force_active << ") {\n";
                out << pad << "  " << MslName(target) << "[gid] = "
                    << shadow_name(target) << "[gid];\n";
                out << pad << "}\n";
              } else {
                out << pad << MslName(target) << "[gid] = "
                    << shadow_name(target) << "[gid];\n";
              }
            }
            return;
          }
          out << pad << "sched_force_id[" << slot << "] = 0xFFFFFFFFu;\n";
          LvalueInfo lhs =
              BuildLvalue(stmt.assign, module, locals_override, sched_regs,
                          false);
          if (!lhs.ok) {
            out << pad << "sched_error[gid] = 1u;\n";
            out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          if (passign_target_index.count(target) > 0) {
            std::string passign_active = passign_active_expr(target);
            out << pad << "if (" << passign_active << ") {\n";
            emit_passign_apply_target(target, lhs, indent + 2);
            out << pad << "} else {\n";
            if (override_is_reg[target]) {
              out << pad << "  " << MslName(target) << "[gid] = "
                  << shadow_name(target) << "[gid];\n";
            }
            out << pad << "}\n";
          } else if (override_is_reg[target]) {
            out << pad << MslName(target) << "[gid] = "
                << shadow_name(target) << "[gid];\n";
          }
          return;
        }
        if (stmt.kind == StatementKind::kAssign) {
          if (stmt.assign.delay) {
            out << pad << "sched_error[gid] = 1u;\n";
            out << pad << "sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            return;
          }
          if (stmt.assign.rhs &&
              stmt.assign.rhs->kind == ExprKind::kCall &&
              IsFileSystemFunctionName(stmt.assign.rhs->ident)) {
            emit_syscall_assign(stmt, *stmt.assign.rhs, resume_pc, indent);
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
            out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
            self(inner, indent + 4, locals_override, resume_pc, self);
            out << pad << "  }\n";
          }
          if (!stmt.else_branch.empty()) {
            out << pad << "} else {\n";
            for (const auto& inner : stmt.else_branch) {
              out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
              self(inner, indent + 4, locals_override, resume_pc, self);
              out << pad << "  }\n";
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
              out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
              self(inner, indent + 4, locals_override, resume_pc, self);
              out << pad << "  }\n";
            }
          }
          if (!stmt.default_branch.empty()) {
            out << pad << "} else {\n";
            for (const auto& inner : stmt.default_branch) {
              out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
              self(inner, indent + 4, locals_override, resume_pc, self);
              out << pad << "  }\n";
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
            out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
            self(inner, indent + 4, locals_override, resume_pc, self);
            out << pad << "  }\n";
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
            out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
            self(inner, indent + 4, locals_override, resume_pc, self);
            out << pad << "  }\n";
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
            out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
            self(inner, indent + 4, locals_override, resume_pc, self);
            out << pad << "  }\n";
          }
          out << pad << "}\n";
          return;
        }
        if (stmt.kind == StatementKind::kBlock) {
          out << pad << "{\n";
          for (const auto& inner : stmt.block) {
            out << pad << "  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
            self(inner, indent + 4, locals_override, resume_pc, self);
            out << pad << "  }\n";
          }
          out << pad << "}\n";
          return;
        }
      };

    auto emit_task_call =
        [&](const Statement& stmt, int indent, int resume_pc,
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
            out << std::string(indent, ' ') << type << " " << MslName(arg.name)
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
          out << std::string(indent, ' ') << type << " " << MslName(arg.name)
              << " = "
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
          emit_inline_stmt_fn(inner, indent, task_locals, resume_pc,
                              emit_inline_stmt_fn);
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
        std::unordered_map<const Statement*,
                           std::pair<const Statement*, const Statement*>>
            repeat_spans;
        std::function<void(const Statement&)> append_stmt;
        append_stmt = [&](const Statement& stmt) -> void {
          if (stmt.kind == StatementKind::kBlock && stmt.block_label.empty()) {
            for (const auto& inner : stmt.block) {
              append_stmt(inner);
            }
            return;
          }
          if (stmt.kind == StatementKind::kRepeat && stmt.repeat_count) {
            int64_t count = 0;
            if (repeat_const_count(stmt, &count) &&
                count <= kRepeatUnrollLimit) {
              if (count <= 0) {
                return;
              }
              for (int64_t rep = 0; rep < count; ++rep) {
                for (const auto& inner : stmt.repeat_body) {
                  append_stmt(inner);
                }
              }
              return;
            }
            stmts.push_back(&stmt);
            size_t body_start = stmts.size();
            for (const auto& inner : stmt.repeat_body) {
              append_stmt(inner);
            }
            size_t body_end = stmts.size();
            const Statement* first =
                (body_end > body_start) ? stmts[body_start] : nullptr;
            const Statement* last =
                (body_end > body_start) ? stmts[body_end - 1] : nullptr;
            repeat_spans[&stmt] = std::make_pair(first, last);
            return;
          }
          stmts.push_back(&stmt);
        };
        if (proc.body) {
          for (const auto& stmt : *proc.body) {
            append_stmt(stmt);
          }
        } else if (proc.single) {
          append_stmt(*proc.single);
        }
        std::unordered_map<const Statement*, int> pc_for_stmt;
        pc_counter = 0;
        for (const auto* stmt : stmts) {
          pc_for_stmt[stmt] = pc_counter++;
        }
        const int pc_done = pc_counter++;
        std::unordered_map<const Statement*, size_t> stmt_index;
        for (size_t i = 0; i < stmts.size(); ++i) {
          stmt_index[stmts[i]] = i;
        }
        struct RepeatRuntime {
          uint32_t id = 0u;
          int body_pc = -1;
          int after_pc = -1;
        };
        std::unordered_map<const Statement*, RepeatRuntime> repeat_runtime;
        std::unordered_map<const Statement*, int> next_pc_override;
        for (const auto& entry : repeat_spans) {
          const Statement* stmt_ptr = entry.first;
          auto id_it = repeat_ids.find(stmt_ptr);
          if (id_it == repeat_ids.end()) {
            continue;
          }
          const Statement* first = entry.second.first;
          const Statement* last = entry.second.second;
          size_t after_index = 0;
          auto stmt_it = stmt_index.find(stmt_ptr);
          if (stmt_it == stmt_index.end()) {
            continue;
          }
          if (last) {
            auto last_it = stmt_index.find(last);
            if (last_it == stmt_index.end()) {
              continue;
            }
            after_index = last_it->second + 1;
            next_pc_override[last] = pc_for_stmt[stmt_ptr];
          } else {
            after_index = stmt_it->second + 1;
          }
          int after_pc =
              (after_index < stmts.size()) ? pc_for_stmt[stmts[after_index]]
                                           : pc_done;
          int body_pc = first ? pc_for_stmt[first] : after_pc;
          repeat_runtime[stmt_ptr] =
              RepeatRuntime{id_it->second, body_pc, after_pc};
        }
        body_cases.clear();

        std::unordered_map<std::string, int> block_end_pc;
        for (size_t i = 0; i < stmts.size(); ++i) {
          const auto* stmt = stmts[i];
          if (stmt->kind == StatementKind::kBlock &&
              !stmt->block_label.empty()) {
            int next_pc = (i + 1 < stmts.size()) ? pc_for_stmt[stmts[i + 1]]
                                                 : pc_done;
            auto next_override_it = next_pc_override.find(stmt);
            if (next_override_it != next_pc_override.end()) {
              next_pc = next_override_it->second;
            }
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
          auto next_override_it = next_pc_override.find(&stmt);
          if (next_override_it != next_pc_override.end()) {
            next_pc = next_override_it->second;
          }
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
            emit_inline_stmt(stmt, 18, sched_locals, next_pc, emit_inline_stmt);
            out << "                  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
            out << "                    sched_pc[idx] = " << next_pc << "u;\n";
            out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            out << "                  }\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kRepeat) {
            auto rep_it = repeat_runtime.find(&stmt);
            if (rep_it == repeat_runtime.end()) {
              out << "                  sched_error[gid] = 1u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            const RepeatRuntime& rep = rep_it->second;
            out << "                  uint __gpga_rep_slot = (gid * "
                << "GPGA_SCHED_REPEAT_COUNT) + " << rep.id << "u;\n";
            out << "                  uint __gpga_rep_left = sched_repeat_left[__gpga_rep_slot];\n";
            out << "                  uint __gpga_rep_active = sched_repeat_active[__gpga_rep_slot];\n";
            if (stmt.repeat_count) {
              std::string rep_expr =
                  EmitExprSized(*stmt.repeat_count, 32, module, sched_locals,
                                sched_regs);
              out << "                  if (__gpga_rep_active == 0u) {\n";
              out << "                    uint __gpga_rep_count = uint("
                  << rep_expr << ");\n";
              out << "                    sched_repeat_left[__gpga_rep_slot] = __gpga_rep_count;\n";
              out << "                    sched_repeat_active[__gpga_rep_slot] = 1u;\n";
              out << "                    __gpga_rep_left = __gpga_rep_count;\n";
              out << "                  }\n";
            } else {
              out << "                  if (__gpga_rep_active == 0u) {\n";
              out << "                    sched_repeat_left[__gpga_rep_slot] = 0u;\n";
              out << "                    sched_repeat_active[__gpga_rep_slot] = 1u;\n";
              out << "                    __gpga_rep_left = 0u;\n";
              out << "                  }\n";
            }
            out << "                  if (__gpga_rep_left == 0u) {\n";
            out << "                    sched_repeat_active[__gpga_rep_slot] = 0u;\n";
            out << "                    sched_pc[idx] = " << rep.after_pc << "u;\n";
            if (rep.after_pc == pc_done) {
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            } else {
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            }
            out << "                    break;\n";
            out << "                  }\n";
            if (rep.body_pc == rep.after_pc) {
              out << "                  sched_repeat_left[__gpga_rep_slot] = 0u;\n";
              out << "                  sched_repeat_active[__gpga_rep_slot] = 0u;\n";
              out << "                  sched_pc[idx] = " << rep.after_pc << "u;\n";
              if (rep.after_pc == pc_done) {
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              } else {
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
              }
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            out << "                  sched_repeat_left[__gpga_rep_slot] = __gpga_rep_left - 1u;\n";
            out << "                  sched_pc[idx] = " << rep.body_pc << "u;\n";
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
                  out << "                  {\n";
                  out << "                    ulong __gpga_edge_val = ((ulong)("
                      << curr << ")) & " << mask << ";\n";
                  out << "                    sched_edge_prev_val[__gpga_edge_base + "
                      << i << "u] = __gpga_edge_val;\n";
                  out << "                  }\n";
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
            if (body_stmt.kind != StatementKind::kDelay &&
                body_stmt.kind != StatementKind::kEventControl) {
              out << "                  sched_error[gid] = 1u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
            if (body_stmt.kind == StatementKind::kDelay) {
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
            int body_pc = -1;
            if (!body_stmt.event_body.empty()) {
              body_pc = pc_counter++;
              BodyCase body_case;
              body_case.pc = body_pc;
              body_case.owner = &stmt;
              body_case.next_pc = pc;
              body_case.loop_pc = pc;
              body_case.is_forever_body = true;
              for (const auto& inner : body_stmt.event_body) {
                body_case.body.push_back(&inner);
              }
              body_cases.push_back(std::move(body_case));
            }
            int event_id = -1;
            bool named_event = false;
            const Expr* named_expr = nullptr;
            if (!body_stmt.event_items.empty()) {
              if (body_stmt.event_items.size() == 1 &&
                  body_stmt.event_items[0].edge == EventEdgeKind::kAny &&
                  body_stmt.event_items[0].expr) {
                named_expr = body_stmt.event_items[0].expr.get();
              }
            } else if (body_stmt.event_expr &&
                       body_stmt.event_edge == EventEdgeKind::kAny) {
              named_expr = body_stmt.event_expr.get();
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
              auto it = edge_wait_ids.find(&body_stmt);
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
              } else if (body_stmt.event_edge == EventEdgeKind::kPosedge) {
                edge_kind = "GPGA_SCHED_EDGE_POSEDGE";
              } else if (body_stmt.event_edge == EventEdgeKind::kNegedge) {
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
                  out << "                  {\n";
                  out << "                    ulong __gpga_edge_val = ((ulong)("
                      << curr << ")) & " << mask << ";\n";
                  out << "                    sched_edge_prev_val[__gpga_edge_base + "
                      << i << "u] = __gpga_edge_val;\n";
                  out << "                  }\n";
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
                                 : std::to_string(pc) + "u")
                << ";\n";
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
            emit_task_call(stmt, 18, next_pc, emit_inline_stmt);
            out << "                  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
            out << "                    sched_pc[idx] = " << next_pc << "u;\n";
            out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            out << "                  }\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          }
          if (stmt.kind == StatementKind::kWhile && stmt.while_condition) {
            const Expr* fd_expr = nullptr;
            bool invert = false;
            if (ExtractFeofCondition(*stmt.while_condition, &fd_expr,
                                     &invert)) {
              const Expr* call_expr =
                  invert && stmt.while_condition->operand
                      ? stmt.while_condition->operand.get()
                      : stmt.while_condition.get();
              if (!call_expr || call_expr->kind != ExprKind::kCall) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              int body_pc = -1;
              if (!stmt.while_body.empty()) {
                body_pc = pc_counter++;
                BodyCase body_case;
                body_case.pc = body_pc;
                body_case.owner = &stmt;
                for (const auto& inner : stmt.while_body) {
                  body_case.body.push_back(&inner);
                }
                body_case.next_pc = pc;
                body_cases.push_back(std::move(body_case));
              }
              int cond_pc = pc_counter++;
              BodyCase cond_case;
              cond_case.pc = cond_pc;
              cond_case.is_service_cond = true;
              cond_case.service_invert = invert;
              cond_case.service_true_pc = (body_pc >= 0) ? body_pc : pc;
              cond_case.service_false_pc = next_pc;
              body_cases.push_back(std::move(cond_case));

              std::string format_id_expr;
              std::vector<ServiceArg> args;
              if (!build_syscall_args(*call_expr, call_expr->ident,
                                      &format_id_expr, &args)) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              emit_service_record("GPGA_SERVICE_KIND_FEOF", format_id_expr,
                                  args, 18);
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_SERVICE;\n";
              out << "                  sched_wait_time[idx] = 0ul;\n";
              out << "                  sched_pc[idx] = " << cond_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
          }
          if (stmt.kind == StatementKind::kIf && stmt.condition) {
            const Expr* fd_expr = nullptr;
            bool invert = false;
            if (ExtractFeofCondition(*stmt.condition, &fd_expr, &invert)) {
              const Expr* call_expr =
                  invert && stmt.condition->operand
                      ? stmt.condition->operand.get()
                      : stmt.condition.get();
              if (!call_expr || call_expr->kind != ExprKind::kCall) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              int then_pc = -1;
              int else_pc = -1;
              if (!stmt.then_branch.empty()) {
                then_pc = pc_counter++;
                BodyCase then_case;
                then_case.pc = then_pc;
                then_case.owner = &stmt;
                then_case.next_pc = next_pc;
                for (const auto& inner : stmt.then_branch) {
                  then_case.body.push_back(&inner);
                }
                body_cases.push_back(std::move(then_case));
              }
              if (!stmt.else_branch.empty()) {
                else_pc = pc_counter++;
                BodyCase else_case;
                else_case.pc = else_pc;
                else_case.owner = &stmt;
                else_case.next_pc = next_pc;
                for (const auto& inner : stmt.else_branch) {
                  else_case.body.push_back(&inner);
                }
                body_cases.push_back(std::move(else_case));
              }
              int cond_pc = pc_counter++;
              BodyCase cond_case;
              cond_case.pc = cond_pc;
              cond_case.is_service_cond = true;
              cond_case.service_invert = invert;
              cond_case.service_true_pc = (then_pc >= 0) ? then_pc : next_pc;
              cond_case.service_false_pc = (else_pc >= 0) ? else_pc : next_pc;
              body_cases.push_back(std::move(cond_case));

              std::string format_id_expr;
              std::vector<ServiceArg> args;
              if (!build_syscall_args(*call_expr, call_expr->ident,
                                      &format_id_expr, &args)) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              emit_service_record("GPGA_SERVICE_KIND_FEOF", format_id_expr,
                                  args, 18);
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_SERVICE;\n";
              out << "                  sched_wait_time[idx] = 0ul;\n";
              out << "                  sched_pc[idx] = " << cond_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
          }
          if (stmt.kind == StatementKind::kIf && stmt.condition) {
            const Expr* call_expr = nullptr;
            bool invert = false;
            if (ExtractPlusargsCondition(*stmt.condition, &call_expr, &invert)) {
              int then_pc = -1;
              int else_pc = -1;
              if (!stmt.then_branch.empty()) {
                then_pc = pc_counter++;
                BodyCase then_case;
                then_case.pc = then_pc;
                then_case.owner = &stmt;
                then_case.next_pc = next_pc;
                for (const auto& inner : stmt.then_branch) {
                  then_case.body.push_back(&inner);
                }
                body_cases.push_back(std::move(then_case));
              }
              if (!stmt.else_branch.empty()) {
                else_pc = pc_counter++;
                BodyCase else_case;
                else_case.pc = else_pc;
                else_case.owner = &stmt;
                else_case.next_pc = next_pc;
                for (const auto& inner : stmt.else_branch) {
                  else_case.body.push_back(&inner);
                }
                body_cases.push_back(std::move(else_case));
              }
              int cond_pc = pc_counter++;
              BodyCase cond_case;
              cond_case.pc = cond_pc;
              cond_case.is_service_cond = true;
              cond_case.service_invert = invert;
              cond_case.service_true_pc = (then_pc >= 0) ? then_pc : next_pc;
              cond_case.service_false_pc = (else_pc >= 0) ? else_pc : next_pc;
              body_cases.push_back(std::move(cond_case));

              if (!call_expr || call_expr->kind != ExprKind::kCall) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              const char* kind_expr = nullptr;
              if (call_expr->ident == "$test$plusargs") {
                kind_expr = "GPGA_SERVICE_KIND_TESTPLUSARGS";
              } else if (call_expr->ident == "$value$plusargs") {
                kind_expr = "GPGA_SERVICE_KIND_VALUEPLUSARGS";
              }
              if (!kind_expr) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              std::string format_id_expr;
              std::vector<ServiceArg> args;
              if (!build_syscall_args(*call_expr, call_expr->ident,
                                      &format_id_expr, &args)) {
                out << "                  sched_error[gid] = 1u;\n";
                out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
                out << "                  break;\n";
                out << "                }\n";
                continue;
              }
              emit_service_record(kind_expr, format_id_expr, args, 18);
              out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_SERVICE;\n";
              out << "                  sched_wait_time[idx] = 0ul;\n";
              out << "                  sched_pc[idx] = " << cond_pc << "u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_BLOCKED;\n";
              out << "                  break;\n";
              out << "                }\n";
              continue;
            }
          }
          int inline_resume_pc = next_pc;
          if (stmt.kind == StatementKind::kWhile ||
              stmt.kind == StatementKind::kFor ||
              stmt.kind == StatementKind::kRepeat) {
            inline_resume_pc = pc;
          }
          emit_inline_stmt(stmt, 18, sched_locals, inline_resume_pc,
                           emit_inline_stmt);
          out << "                  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
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
          } else if (body_case.is_service_cond) {
            out << "                  ulong __gpga_ret = sched_wait_time[idx];\n";
            out << "                  bool __gpga_cond = ((__gpga_ret & 1ul) != 0ul);\n";
            if (body_case.service_invert) {
              out << "                  __gpga_cond = !__gpga_cond;\n";
            }
            out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
            out << "                  if (__gpga_cond) {\n";
            out << "                    sched_pc[idx] = " << body_case.service_true_pc
                << "u;\n";
            out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            out << "                  } else {\n";
            out << "                    sched_pc[idx] = " << body_case.service_false_pc
                << "u;\n";
            if (body_case.service_false_pc == pc_done) {
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            } else {
              out << "                    sched_state[idx] = GPGA_SCHED_PROC_READY;\n";
            }
            out << "                  }\n";
            out << "                  break;\n";
            out << "                }\n";
            continue;
          } else if (body_case.is_service_resume) {
            int width = body_case.service_width;
            if (width <= 0) {
              width = 1;
            }
            out << "                  ulong __gpga_ret = sched_wait_time[idx];\n";
            out << "                  sched_wait_kind[idx] = GPGA_SCHED_WAIT_NONE;\n";
            if (!body_case.owner ||
                body_case.owner->kind != StatementKind::kAssign) {
              out << "                  sched_error[gid] = 1u;\n";
              out << "                  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
            } else {
              emit_lvalue_value(body_case.owner->assign, "__gpga_ret", width,
                                18, sched_locals);
            }
          } else {
            int inline_resume_pc = body_case.next_pc;
            for (const auto* inner : body_case.body) {
              emit_inline_stmt(*inner, 18, sched_locals, inline_resume_pc,
                               emit_inline_stmt);
            }
          }
          int next_pc = body_case.is_forever_body ? body_case.loop_pc
                                                   : body_case.next_pc;
          out << "                  if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
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
      if (has_events) {
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
      }
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
            out << "                {\n";
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
            out << "                }\n";
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
      if (has_events) {
        out << "        for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
        out << "          sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
        out << "        }\n";
      }
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
          bool has_override =
              force_target_index.count(target) > 0 ||
              passign_target_index.count(target) > 0;
          if (has_override) {
            std::string override_cond = override_active_expr(target);
            out << "      if (" << override_cond << ") {\n";
            out << "        " << shadow_name(target) << "[gid] = nb_"
                << MslName(target) << "[gid];\n";
            out << "      } else {\n";
            out << "        " << MslName(target) << "[gid] = nb_"
                << MslName(target) << "[gid];\n";
            out << "      }\n";
          } else {
            out << "      " << MslName(target) << "[gid] = nb_"
                << MslName(target) << "[gid];\n";
          }
        }
      }
      if (!nb_array_nets.empty()) {
        out << "      // Commit array NBAs.\n";
        for (const auto* net : nb_array_nets) {
          out << "      for (uint i = 0u; i < " << net->array_size << "u; ++i) {\n";
          out << "        " << MslName(net->name) << "[(gid * "
              << net->array_size << "u) + i] = " << MslNameNext(net->name)
              << "[(gid * " << net->array_size << "u) + i];\n";
          out << "      }\n";
        }
      }
      emit_sched_comb_update(6);
      if (!system_task_info.monitor_stmts.empty()) {
        out << "      // Monitor change detection.\n";
        for (size_t i = 0; i < system_task_info.monitor_stmts.size(); ++i) {
          const Statement* monitor_stmt = system_task_info.monitor_stmts[i];
          std::string format_id_expr;
          std::vector<ServiceArg> args;
        build_service_args(*monitor_stmt, monitor_stmt->task_name, 0,
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
        build_service_args(*strobe_stmt, strobe_stmt->task_name, 0,
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
      out << "        if (sched_state[idx] == GPGA_SCHED_PROC_READY) {\n";
      out << "          any_ready = true;\n";
      out << "          continue;\n";
      out << "        }\n";
      out << "        if (sched_state[idx] != GPGA_SCHED_PROC_BLOCKED) {\n";
      out << "          continue;\n";
      out << "        }\n";
      if (has_events) {
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
      }
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
            out << "              {\n";
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
            out << "              }\n";
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
      if (has_events) {
        out << "      for (uint e = 0u; e < GPGA_SCHED_EVENT_COUNT; ++e) {\n";
        out << "        sched_event_pending[(gid * GPGA_SCHED_EVENT_COUNT) + e] = 0u;\n";
        out << "      }\n";
      }
      out << "      if (any_ready) {\n";
      out << "        sched_flags[gid] |= GPGA_SCHED_FLAG_ACTIVE_INIT;\n";
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
      if (has_delayed_nba) {
        out << "      if (sched_dnba_count[gid] != 0u) {\n";
        out << "        uint __gpga_dnba_base = gid * GPGA_SCHED_MAX_DNBA;\n";
        out << "        uint __gpga_dnba_count = sched_dnba_count[gid];\n";
        out << "        for (uint __gpga_dnba_i = 0u; __gpga_dnba_i < __gpga_dnba_count; ++__gpga_dnba_i) {\n";
        out << "          ulong __gpga_dnba_time = sched_dnba_time[__gpga_dnba_base + __gpga_dnba_i];\n";
        out << "          if (!have_time || __gpga_dnba_time < next_time) {\n";
        out << "            have_time = true;\n";
        out << "            next_time = __gpga_dnba_time;\n";
        out << "          }\n";
        out << "        }\n";
        out << "      }\n";
      }
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
      out << "        sched_flags[gid] |= GPGA_SCHED_FLAG_ACTIVE_INIT;\n";
      out << "        sched_phase[gid] = GPGA_SCHED_PHASE_ACTIVE;\n";
      out << "        continue;\n";
      out << "      }\n";
      out << "      bool have_service = false;\n";
      out << "      for (uint pid = 0u; pid < GPGA_SCHED_PROC_COUNT; ++pid) {\n";
      out << "        uint idx = gpga_sched_index(gid, pid);\n";
      out << "        if (sched_wait_kind[idx] == GPGA_SCHED_WAIT_SERVICE) {\n";
      out << "          have_service = true;\n";
      out << "          break;\n";
      out << "        }\n";
      out << "      }\n";
      out << "      if (have_service) {\n";
      out << "        break;\n";
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
