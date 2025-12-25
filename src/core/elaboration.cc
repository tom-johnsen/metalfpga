#include "core/elaboration.hh"

#include <algorithm>
#include <cctype>
#include <functional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace gpga {

namespace {

const Module* FindModule(const Program& program, const std::string& name) {
  for (const auto& module : program.modules) {
    if (module.name == name) {
      return &module;
    }
  }
  return nullptr;
}

bool FindTopModule(const Program& program, std::string* top_name,
                   Diagnostics* diagnostics) {
  std::unordered_set<std::string> instantiated;
  for (const auto& module : program.modules) {
    for (const auto& instance : module.instances) {
      instantiated.insert(instance.module_name);
    }
  }

  std::string candidate;
  for (const auto& module : program.modules) {
    if (instantiated.count(module.name) == 0) {
      if (!candidate.empty()) {
        diagnostics->Add(Severity::kError,
                         "multiple top-level modules found (use --top <name>)");
        return false;
      }
      candidate = module.name;
    }
  }

  if (candidate.empty()) {
    std::unordered_set<std::string> names;
    names.reserve(program.modules.size());
    for (const auto& module : program.modules) {
      names.insert(module.name);
    }
    std::unordered_map<std::string, std::vector<std::string>> graph;
    for (const auto& module : program.modules) {
      auto& edges = graph[module.name];
      for (const auto& instance : module.instances) {
        if (names.count(instance.module_name) > 0) {
          edges.push_back(instance.module_name);
        }
      }
    }

    enum class VisitState { kUnseen, kVisiting, kDone };
    std::unordered_map<std::string, VisitState> state;
    for (const auto& name : names) {
      state[name] = VisitState::kUnseen;
    }
    std::function<bool(const std::string&)> has_cycle =
        [&](const std::string& name) -> bool {
      auto it = state.find(name);
      if (it == state.end()) {
        return false;
      }
      if (it->second == VisitState::kVisiting) {
        return true;
      }
      if (it->second == VisitState::kDone) {
        return false;
      }
      it->second = VisitState::kVisiting;
      auto edge_it = graph.find(name);
      if (edge_it != graph.end()) {
        for (const auto& child : edge_it->second) {
          if (has_cycle(child)) {
            return true;
          }
        }
      }
      it->second = VisitState::kDone;
      return false;
    };

    for (const auto& name : names) {
      if (has_cycle(name)) {
        diagnostics->Add(Severity::kError,
                         "recursive module instantiation detected");
        return false;
      }
    }

    diagnostics->Add(Severity::kError, "no top-level module found");
    return false;
  }
  *top_name = candidate;
  return true;
}

struct PortBinding {
  std::string signal;
};

struct ParamBindings {
  std::unordered_map<std::string, int64_t> values;
  std::unordered_map<std::string, std::unique_ptr<Expr>> exprs;
};

std::unique_ptr<Expr> SimplifyExpr(std::unique_ptr<Expr> expr,
                                   const Module& module);

bool EvalConstExprValue(const Expr& expr, const ParamBindings& params,
                        int64_t* out_value, Diagnostics* diagnostics,
                        const std::string& context) {
  std::string error;
  if (!gpga::EvalConstExpr(expr, params.values, out_value, &error)) {
    diagnostics->Add(Severity::kError, error + " in " + context);
    return false;
  }
  return true;
}

bool ResolveRangeWidth(int default_width,
                       const std::shared_ptr<Expr>& msb_expr,
                       const std::shared_ptr<Expr>& lsb_expr,
                       const ParamBindings& params, int* width_out,
                       Diagnostics* diagnostics, const std::string& context) {
  if (!msb_expr || !lsb_expr) {
    *width_out = default_width;
    return true;
  }
  int64_t msb = 0;
  int64_t lsb = 0;
  if (!EvalConstExprValue(*msb_expr, params, &msb, diagnostics,
                          context + " msb")) {
    return false;
  }
  if (!EvalConstExprValue(*lsb_expr, params, &lsb, diagnostics,
                          context + " lsb")) {
    return false;
  }
  int64_t width64 = msb >= lsb ? (msb - lsb + 1) : (lsb - msb + 1);
  if (width64 <= 0 || width64 > 0x7FFFFFFF) {
    diagnostics->Add(Severity::kError,
                     "invalid range width in " + context);
    return false;
  }
  *width_out = static_cast<int>(width64);
  return true;
}

bool ResolveArraySize(const Net& net, const ParamBindings& params,
                      int* size_out, Diagnostics* diagnostics,
                      const std::string& context) {
  if (!net.array_msb_expr || !net.array_lsb_expr) {
    *size_out = net.array_size;
    return true;
  }
  return ResolveRangeWidth(net.array_size, net.array_msb_expr,
                           net.array_lsb_expr, params, size_out, diagnostics,
                           context);
}

bool ResolveSelectIndices(const Expr& expr, const ParamBindings& params,
                          int* msb_out, int* lsb_out, Diagnostics* diagnostics,
                          const std::string& context) {
  int64_t msb = expr.msb;
  int64_t lsb = expr.lsb;
  if (expr.msb_expr) {
    if (!EvalConstExprValue(*expr.msb_expr, params, &msb, diagnostics,
                            context + " msb")) {
      return false;
    }
  }
  if (expr.has_range) {
    if (expr.lsb_expr) {
      if (!EvalConstExprValue(*expr.lsb_expr, params, &lsb, diagnostics,
                              context + " lsb")) {
        return false;
      }
    }
  } else {
    lsb = msb;
  }
  *msb_out = static_cast<int>(msb);
  *lsb_out = static_cast<int>(lsb);
  return true;
}

bool ResolveRepeatCount(const Expr& expr, const ParamBindings& params,
                        int* repeat_out, Diagnostics* diagnostics,
                        const std::string& context) {
  int64_t repeat = expr.repeat;
  if (expr.repeat_expr) {
    if (!EvalConstExprValue(*expr.repeat_expr, params, &repeat, diagnostics,
                            context + " repeat")) {
      return false;
    }
  }
  if (repeat <= 0 || repeat > 0x7FFFFFFF) {
    diagnostics->Add(Severity::kError,
                     "invalid replication count in " + context);
    return false;
  }
  *repeat_out = static_cast<int>(repeat);
  return true;
}

std::unique_ptr<Expr> CloneExprWithParams(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, Diagnostics* diagnostics) {
  if (expr.kind == ExprKind::kIdentifier) {
    auto it = params.exprs.find(expr.ident);
    if (it != params.exprs.end()) {
      return gpga::CloneExpr(*it->second);
    }
    auto out = std::make_unique<Expr>();
    out->kind = ExprKind::kIdentifier;
    out->ident = rename(expr.ident);
    return out;
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

  if (expr.kind == ExprKind::kNumber) {
    return out;
  }
  if (expr.kind == ExprKind::kUnary) {
    out->operand = CloneExprWithParams(*expr.operand, rename, params, diagnostics);
    if (!out->operand) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kBinary) {
    out->lhs = CloneExprWithParams(*expr.lhs, rename, params, diagnostics);
    out->rhs = CloneExprWithParams(*expr.rhs, rename, params, diagnostics);
    if (!out->lhs || !out->rhs) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kTernary) {
    out->condition =
        CloneExprWithParams(*expr.condition, rename, params, diagnostics);
    out->then_expr =
        CloneExprWithParams(*expr.then_expr, rename, params, diagnostics);
    out->else_expr =
        CloneExprWithParams(*expr.else_expr, rename, params, diagnostics);
    if (!out->condition || !out->then_expr || !out->else_expr) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kSelect) {
    out->base = CloneExprWithParams(*expr.base, rename, params, diagnostics);
    if (!out->base) {
      return nullptr;
    }
    int msb = 0;
    int lsb = 0;
    if (!ResolveSelectIndices(expr, params, &msb, &lsb, diagnostics,
                              "select")) {
      return nullptr;
    }
    out->msb = msb;
    out->lsb = lsb;
    out->has_range = expr.has_range;
    return out;
  }
  if (expr.kind == ExprKind::kIndex) {
    out->base = CloneExprWithParams(*expr.base, rename, params, diagnostics);
    out->index = CloneExprWithParams(*expr.index, rename, params, diagnostics);
    if (!out->base || !out->index) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kConcat) {
    int repeat = 1;
    if (!ResolveRepeatCount(expr, params, &repeat, diagnostics, "concat")) {
      return nullptr;
    }
    out->repeat = repeat;
    for (const auto& element : expr.elements) {
      auto cloned =
          CloneExprWithParams(*element, rename, params, diagnostics);
      if (!cloned) {
        return nullptr;
      }
      out->elements.push_back(std::move(cloned));
    }
    return out;
  }
  return out;
}

bool CloneStatement(
    const Statement& statement,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module& flat_module, Statement* out,
    Diagnostics* diagnostics) {
  out->kind = statement.kind;
  if (statement.kind == StatementKind::kAssign) {
    out->assign.lhs = rename(statement.assign.lhs);
    if (statement.assign.lhs_index) {
      out->assign.lhs_index =
          CloneExprWithParams(*statement.assign.lhs_index, rename, params,
                              diagnostics);
      if (!out->assign.lhs_index) {
        return false;
      }
      out->assign.lhs_index =
          SimplifyExpr(std::move(out->assign.lhs_index), flat_module);
    }
    if (statement.assign.rhs) {
      out->assign.rhs =
          CloneExprWithParams(*statement.assign.rhs, rename, params,
                              diagnostics);
      if (!out->assign.rhs) {
        return false;
      }
      out->assign.rhs = SimplifyExpr(std::move(out->assign.rhs), flat_module);
    } else {
      out->assign.rhs = nullptr;
    }
    out->assign.nonblocking = statement.assign.nonblocking;
    return true;
  }
  if (statement.kind == StatementKind::kIf) {
    if (statement.condition) {
      out->condition =
          CloneExprWithParams(*statement.condition, rename, params,
                              diagnostics);
      if (!out->condition) {
        return false;
      }
    }
    for (const auto& stmt : statement.then_branch) {
      Statement cloned;
      if (!CloneStatement(stmt, rename, params, flat_module, &cloned,
                          diagnostics)) {
        return false;
      }
      out->then_branch.push_back(std::move(cloned));
    }
    for (const auto& stmt : statement.else_branch) {
      Statement cloned;
      if (!CloneStatement(stmt, rename, params, flat_module, &cloned,
                          diagnostics)) {
        return false;
      }
      out->else_branch.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kBlock) {
    for (const auto& stmt : statement.block) {
      Statement cloned;
      if (!CloneStatement(stmt, rename, params, flat_module, &cloned,
                          diagnostics)) {
        return false;
      }
      out->block.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kCase) {
    out->case_kind = statement.case_kind;
    out->case_expr =
        CloneExprWithParams(*statement.case_expr, rename, params, diagnostics);
    if (!out->case_expr) {
      return false;
    }
    for (const auto& item : statement.case_items) {
      CaseItem cloned_item;
      for (const auto& label : item.labels) {
        auto cloned_label =
            CloneExprWithParams(*label, rename, params, diagnostics);
        if (!cloned_label) {
          return false;
        }
        cloned_item.labels.push_back(std::move(cloned_label));
      }
      for (const auto& stmt : item.body) {
        Statement cloned_stmt;
        if (!CloneStatement(stmt, rename, params, flat_module, &cloned_stmt,
                            diagnostics)) {
          return false;
        }
        cloned_item.body.push_back(std::move(cloned_stmt));
      }
      out->case_items.push_back(std::move(cloned_item));
    }
    for (const auto& stmt : statement.default_branch) {
      Statement cloned_stmt;
      if (!CloneStatement(stmt, rename, params, flat_module, &cloned_stmt,
                          diagnostics)) {
        return false;
      }
      out->default_branch.push_back(std::move(cloned_stmt));
    }
    return true;
  }
  return true;
}

bool AddFlatNet(const std::string& name, int width, bool is_signed,
                NetType type, int array_size, const std::string& hier_path,
                Module* out,
                std::unordered_set<std::string>* net_names,
                std::unordered_map<std::string, std::string>* flat_to_hier,
                Diagnostics* diagnostics) {
  if (net_names->count(name) > 0) {
    auto it = flat_to_hier->find(name);
    if (it != flat_to_hier->end() && it->second != hier_path) {
      diagnostics->Add(Severity::kError,
                       "flattened net name collision for '" + name + "'");
      return false;
    }
    return true;
  }
  Net net;
  net.type = type;
  net.name = name;
  net.width = width;
  net.is_signed = is_signed;
  net.array_size = array_size;
  out->nets.push_back(std::move(net));
  net_names->insert(name);
  (*flat_to_hier)[name] = hier_path;
  return true;
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

std::unique_ptr<Expr> MakeNumberExpr(uint64_t value) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kNumber;
  expr->number = value;
  expr->value_bits = value;
  return expr;
}

std::unique_ptr<Expr> MakeAllXExpr(int width) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kNumber;
  expr->number = 0;
  expr->value_bits = 0;
  expr->x_bits = MaskForWidth64(width);
  expr->z_bits = 0;
  expr->has_width = true;
  expr->number_width = width;
  return expr;
}

bool IsArrayNet(const Module& module, const std::string& name,
                int* element_width) {
  for (const auto& net : module.nets) {
    if (net.name == name && net.array_size > 0) {
      if (element_width) {
        *element_width = net.width;
      }
      return true;
    }
  }
  return false;
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
      int lo = std::min(expr.msb, expr.lsb);
      int hi = std::max(expr.msb, expr.lsb);
      return hi - lo + 1;
    }
    case ExprKind::kIndex: {
      if (expr.base && expr.base->kind == ExprKind::kIdentifier) {
        int element_width = 0;
        if (IsArrayNet(module, expr.base->ident, &element_width)) {
          return element_width;
        }
      }
      return 1;
    }
    case ExprKind::kConcat: {
      int total = 0;
      for (const auto& element : expr.elements) {
        total += ExprWidth(*element, module);
      }
      return total * std::max(1, expr.repeat);
    }
  }
  return 32;
}

bool IsAllOnesExpr(const Expr& expr, const Module& module, int* width_out) {
  switch (expr.kind) {
    case ExprKind::kNumber: {
      if (expr.x_bits != 0 || expr.z_bits != 0) {
        return false;
      }
      int width = expr.has_width && expr.number_width > 0
                      ? expr.number_width
                      : MinimalWidth(expr.number);
      if (width_out) {
        *width_out = width;
      }
      if (width <= 0 || width > 64) {
        return false;
      }
      uint64_t mask =
          (width == 64) ? 0xFFFFFFFFFFFFFFFFull : ((1ull << width) - 1ull);
      return expr.number == mask;
    }
    case ExprKind::kConcat: {
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
      int total = base_width * std::max(1, expr.repeat);
      if (width_out) {
        *width_out = total;
      }
      return true;
    }
    default:
      return false;
  }
}

std::unique_ptr<Expr> SimplifyExpr(std::unique_ptr<Expr> expr,
                                   const Module& module) {
  if (!expr) {
    return nullptr;
  }
  if (expr->kind == ExprKind::kUnary) {
    expr->operand = SimplifyExpr(std::move(expr->operand), module);
    if (expr->unary_op == '+' && expr->operand) {
      return std::move(expr->operand);
    }
    return expr;
  }
  if (expr->kind == ExprKind::kBinary) {
    expr->lhs = SimplifyExpr(std::move(expr->lhs), module);
    expr->rhs = SimplifyExpr(std::move(expr->rhs), module);
    if (!expr->lhs || !expr->rhs) {
      return expr;
    }
    if (expr->op == '&') {
      int lhs_width = ExprWidth(*expr->lhs, module);
      int rhs_width = ExprWidth(*expr->rhs, module);
      int expr_width = std::max(lhs_width, rhs_width);
      int mask_width = 0;
      if (IsAllOnesExpr(*expr->lhs, module, &mask_width) &&
          mask_width == expr_width && rhs_width == expr_width) {
        return std::move(expr->rhs);
      }
      if (IsAllOnesExpr(*expr->rhs, module, &mask_width) &&
          mask_width == expr_width && lhs_width == expr_width) {
        return std::move(expr->lhs);
      }
    }
    return expr;
  }
  if (expr->kind == ExprKind::kTernary) {
    expr->condition = SimplifyExpr(std::move(expr->condition), module);
    expr->then_expr = SimplifyExpr(std::move(expr->then_expr), module);
    expr->else_expr = SimplifyExpr(std::move(expr->else_expr), module);
    return expr;
  }
  if (expr->kind == ExprKind::kSelect) {
    expr->base = SimplifyExpr(std::move(expr->base), module);
    if (!expr->base) {
      return expr;
    }
    if (expr->has_range) {
      int base_width = ExprWidth(*expr->base, module);
      int lo = std::min(expr->msb, expr->lsb);
      int hi = std::max(expr->msb, expr->lsb);
      if (lo == 0 && hi == base_width - 1) {
        return std::move(expr->base);
      }
    }
    return expr;
  }
  if (expr->kind == ExprKind::kIndex) {
    expr->base = SimplifyExpr(std::move(expr->base), module);
    expr->index = SimplifyExpr(std::move(expr->index), module);
    return expr;
  }
  if (expr->kind == ExprKind::kConcat) {
    for (auto& element : expr->elements) {
      element = SimplifyExpr(std::move(element), module);
    }
    if (expr->repeat == 1 && expr->elements.size() == 1 &&
        expr->elements[0]) {
      return std::move(expr->elements[0]);
    }
    return expr;
  }
  return expr;
}

bool BuildParamBindings(const Module& module, const Instance* instance,
                        const ParamBindings* outer_params, ParamBindings* out,
                        Diagnostics* diagnostics) {
  ParamBindings bindings;
  std::unordered_map<std::string, bool> param_is_local;
  std::vector<const Parameter*> overridable;
  for (const auto& param : module.parameters) {
    param_is_local[param.name] = param.is_local;
    if (!param.is_local) {
      overridable.push_back(&param);
    }
  }

  std::vector<const ParamOverride*> positional_overrides;
  std::unordered_map<std::string, const ParamOverride*> named_overrides;
  bool has_positional = false;
  bool has_named = false;
  if (instance) {
    for (const auto& override_item : instance->param_overrides) {
      if (override_item.name.empty()) {
        has_positional = true;
        positional_overrides.push_back(&override_item);
      } else {
        has_named = true;
        if (named_overrides.count(override_item.name) > 0) {
          diagnostics->Add(Severity::kError,
                           "duplicate parameter override '" +
                               override_item.name + "' in instance '" +
                               instance->name + "'");
          return false;
        }
        auto it = param_is_local.find(override_item.name);
        if (it == param_is_local.end()) {
          diagnostics->Add(Severity::kError,
                           "unknown parameter '" + override_item.name +
                               "' in instance '" + instance->name + "'");
          return false;
        }
        if (it->second) {
          diagnostics->Add(Severity::kError,
                           "cannot override localparam '" +
                               override_item.name + "' in instance '" +
                               instance->name + "'");
          return false;
        }
        named_overrides[override_item.name] = &override_item;
      }
    }
  }

  if (has_positional && has_named) {
    diagnostics->Add(Severity::kError,
                     "cannot mix named and positional parameter overrides");
    return false;
  }
  if (has_positional &&
      positional_overrides.size() > overridable.size()) {
    diagnostics->Add(Severity::kError,
                     "too many positional parameter overrides");
    return false;
  }

  size_t positional_index = 0;
  for (const auto& param : module.parameters) {
    const Expr* expr = param.value.get();
    const ParamBindings* eval_params = &bindings;
    if (!param.is_local) {
      if (has_positional && positional_index < positional_overrides.size()) {
        expr = positional_overrides[positional_index++]->expr.get();
        eval_params = outer_params ? outer_params : &bindings;
      } else if (has_named) {
        auto it = named_overrides.find(param.name);
        if (it != named_overrides.end()) {
          expr = it->second->expr.get();
          eval_params = outer_params ? outer_params : &bindings;
        }
      }
    }
    if (!expr) {
      diagnostics->Add(Severity::kError,
                       "missing value for parameter '" + param.name + "'");
      return false;
    }
    int64_t value = 0;
    if (eval_params == &bindings) {
      if (!EvalConstExprValue(*expr, bindings, &value, diagnostics,
                              "parameter '" + param.name + "'")) {
        return false;
      }
    } else {
      std::unordered_map<std::string, int64_t> scope = eval_params->values;
      std::string error;
      if (!gpga::EvalConstExpr(*expr, scope, &value, &error)) {
        diagnostics->Add(Severity::kError,
                         error + " in parameter '" + param.name + "'");
        return false;
      }
    }
    bindings.values[param.name] = value;
    const ParamBindings* expr_params =
        eval_params == &bindings ? &bindings : eval_params;
    auto resolved = CloneExprWithParams(
        *expr, [](const std::string& ident) { return ident; }, *expr_params,
        diagnostics);
    if (!resolved) {
      return false;
    }
    bindings.exprs[param.name] = std::move(resolved);
  }

  if (has_positional &&
      positional_index < positional_overrides.size()) {
    diagnostics->Add(Severity::kError,
                     "too many positional parameter overrides");
    return false;
  }

  *out = std::move(bindings);
  return true;
}

void CollectAssignedSignals(const Statement& statement,
                            std::unordered_set<std::string>* out) {
  switch (statement.kind) {
    case StatementKind::kAssign:
      out->insert(statement.assign.lhs);
      break;
    case StatementKind::kIf:
      for (const auto& stmt : statement.then_branch) {
        CollectAssignedSignals(stmt, out);
      }
      for (const auto& stmt : statement.else_branch) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kBlock:
      for (const auto& stmt : statement.block) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kCase:
      for (const auto& item : statement.case_items) {
        for (const auto& stmt : item.body) {
          CollectAssignedSignals(stmt, out);
        }
      }
      for (const auto& stmt : statement.default_branch) {
        CollectAssignedSignals(stmt, out);
      }
      break;
  }
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
      return;
    case ExprKind::kIndex:
      if (expr.base) {
        CollectIdentifiers(*expr.base, out);
      }
      if (expr.index) {
        CollectIdentifiers(*expr.index, out);
      }
      return;
    case ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        CollectIdentifiers(*element, out);
      }
      return;
  }
}

bool ValidateSingleDrivers(const Module& flat, Diagnostics* diagnostics) {
  struct Range {
    int lo = 0;
    int hi = 0;
  };
  std::unordered_map<std::string, std::string> drivers;
  std::unordered_map<std::string, std::vector<Range>> partial_ranges;
  for (const auto& assign : flat.assigns) {
    if (!assign.lhs_has_range) {
      if (drivers.count(assign.lhs) > 0 || partial_ranges.count(assign.lhs) > 0) {
        diagnostics->Add(Severity::kError,
                         "multiple drivers for signal '" + assign.lhs + "'");
        return false;
      }
      drivers[assign.lhs] = "assign";
      continue;
    }
    if (drivers.count(assign.lhs) > 0) {
      diagnostics->Add(Severity::kError,
                       "multiple drivers for signal '" + assign.lhs + "'");
      return false;
    }
    int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
    int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
    auto& ranges = partial_ranges[assign.lhs];
    for (const auto& range : ranges) {
      if (hi >= range.lo && lo <= range.hi) {
        diagnostics->Add(Severity::kError,
                         "overlapping drivers for signal '" + assign.lhs + "'");
        return false;
      }
    }
    ranges.push_back(Range{lo, hi});
  }

  for (const auto& block : flat.always_blocks) {
    std::unordered_set<std::string> block_drives;
    for (const auto& stmt : block.statements) {
      CollectAssignedSignals(stmt, &block_drives);
    }
    for (const auto& name : block_drives) {
      if (drivers.count(name) > 0 || partial_ranges.count(name) > 0) {
        diagnostics->Add(Severity::kError,
                         "multiple drivers for signal '" + name + "'");
        return false;
      }
      drivers[name] = "always";
    }
  }
  return true;
}

bool ValidateCombinationalAcyclic(const Module& flat,
                                  Diagnostics* diagnostics) {
  const size_t count = flat.assigns.size();
  if (count == 0) {
    return true;
  }
  std::unordered_map<std::string, size_t> lhs_to_index;
  lhs_to_index.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    lhs_to_index[flat.assigns[i].lhs] = i;
  }

  std::vector<int> indegree(count, 0);
  std::vector<std::vector<size_t>> edges(count);
  for (size_t i = 0; i < count; ++i) {
    const auto& assign = flat.assigns[i];
    if (!assign.rhs) {
      continue;
    }
    std::unordered_set<std::string> deps;
    CollectIdentifiers(*assign.rhs, &deps);
    if (deps.count(assign.lhs) > 0) {
      diagnostics->Add(Severity::kError,
                       "combinational self-dependency on '" + assign.lhs + "'");
      return false;
    }
    for (const auto& dep : deps) {
      auto it = lhs_to_index.find(dep);
      if (it == lhs_to_index.end()) {
        continue;
      }
      edges[it->second].push_back(i);
      indegree[i]++;
    }
  }

  std::queue<size_t> ready;
  for (size_t i = 0; i < count; ++i) {
    if (indegree[i] == 0) {
      ready.push(i);
    }
  }

  size_t visited = 0;
  while (!ready.empty()) {
    size_t node = ready.front();
    ready.pop();
    visited++;
    for (size_t next : edges[node]) {
      indegree[next]--;
      if (indegree[next] == 0) {
        ready.push(next);
      }
    }
  }

  if (visited != count) {
    diagnostics->Add(Severity::kError,
                     "combinational cycle detected in continuous assigns");
    return false;
  }
  return true;
}

void WarnNonblockingArrayWrites(const Module& flat,
                                Diagnostics* diagnostics) {
  std::unordered_set<std::string> warned;
  std::function<void(const Statement&)> walk;
  walk = [&](const Statement& stmt) {
    if (stmt.kind == StatementKind::kAssign) {
      if (stmt.assign.lhs_index && stmt.assign.nonblocking) {
        if (warned.insert(stmt.assign.lhs).second) {
          diagnostics->Add(
              Severity::kWarning,
              "nonblocking array write to '" + stmt.assign.lhs +
                  "' requires mem/mem_next swap after tick");
        }
      }
      return;
    }
    if (stmt.kind == StatementKind::kIf) {
      for (const auto& inner : stmt.then_branch) {
        walk(inner);
      }
      for (const auto& inner : stmt.else_branch) {
        walk(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kBlock) {
      for (const auto& inner : stmt.block) {
        walk(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kCase) {
      for (const auto& item : stmt.case_items) {
        for (const auto& inner : item.body) {
          walk(inner);
        }
      }
      for (const auto& inner : stmt.default_branch) {
        walk(inner);
      }
    }
  };
  for (const auto& block : flat.always_blocks) {
    for (const auto& stmt : block.statements) {
      walk(stmt);
    }
  }
}

bool IsDeclaredSignal(const Module& module, const std::string& name) {
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return true;
    }
  }
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return true;
    }
  }
  return false;
}

void WarnUndeclaredClocks(const Module& flat, Diagnostics* diagnostics) {
  for (const auto& block : flat.always_blocks) {
    if (block.edge == EdgeKind::kCombinational) {
      continue;
    }
    if (!IsDeclaredSignal(flat, block.clock)) {
      diagnostics->Add(Severity::kWarning,
                       "clock '" + block.clock +
                           "' in always block is not declared");
    }
  }
}

bool HasNonblockingAssign(const Statement& stmt) {
  if (stmt.kind == StatementKind::kAssign) {
    return stmt.assign.nonblocking;
  }
  if (stmt.kind == StatementKind::kIf) {
    for (const auto& inner : stmt.then_branch) {
      if (HasNonblockingAssign(inner)) {
        return true;
      }
    }
    for (const auto& inner : stmt.else_branch) {
      if (HasNonblockingAssign(inner)) {
        return true;
      }
    }
    return false;
  }
  if (stmt.kind == StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      if (HasNonblockingAssign(inner)) {
        return true;
      }
    }
  }
  if (stmt.kind == StatementKind::kCase) {
    for (const auto& item : stmt.case_items) {
      for (const auto& inner : item.body) {
        if (HasNonblockingAssign(inner)) {
          return true;
        }
      }
    }
    for (const auto& inner : stmt.default_branch) {
      if (HasNonblockingAssign(inner)) {
        return true;
      }
    }
  }
  return false;
}

void WarnNonblockingInCombAlways(const Module& flat,
                                 Diagnostics* diagnostics) {
  for (const auto& block : flat.always_blocks) {
    if (block.edge != EdgeKind::kCombinational) {
      continue;
    }
    for (const auto& stmt : block.statements) {
      if (HasNonblockingAssign(stmt)) {
        diagnostics->Add(
            Severity::kWarning,
            "nonblocking assignment in always @* (prefer blocking '=')");
        break;
      }
    }
  }
}

bool InlineModule(const Program& program, const Module& module,
                  const std::string& prefix, const std::string& hier_prefix,
                  const ParamBindings& params,
                  const std::unordered_map<std::string, PortBinding>& port_map,
                  Module* out, Diagnostics* diagnostics,
                  std::unordered_set<std::string>* stack,
                  std::unordered_set<std::string>* net_names,
                  std::unordered_map<std::string, std::string>* flat_to_hier,
                  bool enable_4state) {
    if (stack->count(module.name) > 0) {
    diagnostics->Add(Severity::kError,
                     "recursive module instantiation detected");
    return false;
  }
  stack->insert(module.name);

  std::unordered_set<std::string> port_names;
  std::unordered_set<std::string> local_net_names;
  for (const auto& port : module.ports) {
    port_names.insert(port.name);
  }
  for (const auto& net : module.nets) {
    local_net_names.insert(net.name);
  }

  auto rename = [&](const std::string& ident) -> std::string {
    auto it = port_map.find(ident);
    if (it != port_map.end()) {
      return it->second.signal;
    }
    if (!prefix.empty() &&
        (port_names.count(ident) > 0 || local_net_names.count(ident) > 0)) {
      return prefix + ident;
    }
    return ident;
  };

  auto lookup_type = [&](const std::string& ident) -> NetType {
    for (const auto& net : module.nets) {
      if (net.name == ident) {
        return net.type;
      }
    }
    return NetType::kWire;
  };

  if (prefix.empty()) {
    out->name = module.name;
    out->parameters.clear();
    for (const auto& param : module.parameters) {
      Parameter flat_param;
      flat_param.name = param.name;
      flat_param.is_local = param.is_local;
      auto it = params.exprs.find(param.name);
      if (it != params.exprs.end()) {
        flat_param.value = gpga::CloneExpr(*it->second);
      } else if (param.value) {
        flat_param.value = CloneExprWithParams(
            *param.value, [](const std::string& ident) { return ident; },
            params, diagnostics);
        if (!flat_param.value) {
          return false;
        }
      }
      out->parameters.push_back(std::move(flat_param));
    }
    out->ports.clear();
    for (const auto& port : module.ports) {
      int width = port.width;
      if (!ResolveRangeWidth(port.width, port.msb_expr, port.lsb_expr, params,
                             &width, diagnostics,
                             "port '" + port.name + "'")) {
        return false;
      }
      Port flat_port;
      flat_port.dir = port.dir;
      flat_port.name = port.name;
      flat_port.width = width;
      flat_port.is_signed = port.is_signed;
      out->ports.push_back(std::move(flat_port));
      (*flat_to_hier)[port.name] = hier_prefix + "." + port.name;
    }
    for (const auto& net : module.nets) {
      int width = net.width;
      if (!ResolveRangeWidth(net.width, net.msb_expr, net.lsb_expr, params,
                             &width, diagnostics,
                             "net '" + net.name + "'")) {
        return false;
      }
      int array_size = 0;
      if (!ResolveArraySize(net, params, &array_size, diagnostics,
                            "net '" + net.name + "' array range")) {
        return false;
      }
      if (!AddFlatNet(net.name, width, net.is_signed, net.type, array_size,
                      hier_prefix + "." + net.name, out, net_names,
                      flat_to_hier, diagnostics)) {
        return false;
      }
    }
  } else {
    for (const auto& port : module.ports) {
      if (port_map.find(port.name) != port_map.end()) {
        continue;
      }
      int width = port.width;
      if (!ResolveRangeWidth(port.width, port.msb_expr, port.lsb_expr, params,
                             &width, diagnostics,
                             "port '" + port.name + "'")) {
        return false;
      }
      NetType type = lookup_type(port.name);
      if (!AddFlatNet(prefix + port.name, width, port.is_signed, type, 0,
                      hier_prefix + "." + port.name, out, net_names,
                      flat_to_hier, diagnostics)) {
        return false;
      }
    }
    for (const auto& net : module.nets) {
      int width = net.width;
      if (!ResolveRangeWidth(net.width, net.msb_expr, net.lsb_expr, params,
                             &width, diagnostics,
                             "net '" + net.name + "'")) {
        return false;
      }
      int array_size = 0;
      if (!ResolveArraySize(net, params, &array_size, diagnostics,
                            "net '" + net.name + "' array range")) {
        return false;
      }
      if (!AddFlatNet(prefix + net.name, width, net.is_signed, net.type,
                      array_size,
                      hier_prefix + "." + net.name, out, net_names,
                      flat_to_hier, diagnostics)) {
        return false;
      }
    }
  }

  for (const auto& assign : module.assigns) {
    Assign flattened;
    flattened.lhs = rename(assign.lhs);
    flattened.lhs_has_range = assign.lhs_has_range;
    flattened.lhs_msb = assign.lhs_msb;
    flattened.lhs_lsb = assign.lhs_lsb;
    if (assign.rhs) {
      flattened.rhs =
          CloneExprWithParams(*assign.rhs, rename, params, diagnostics);
      if (!flattened.rhs) {
        return false;
      }
      flattened.rhs = SimplifyExpr(std::move(flattened.rhs), *out);
    } else {
      flattened.rhs = nullptr;
    }
    out->assigns.push_back(std::move(flattened));
  }
  for (const auto& block : module.always_blocks) {
    AlwaysBlock flattened;
    flattened.edge = block.edge;
    flattened.clock = rename(block.clock);
    for (const auto& stmt : block.statements) {
      Statement cloned;
      if (!CloneStatement(stmt, rename, params, *out, &cloned, diagnostics)) {
        return false;
      }
      flattened.statements.push_back(std::move(cloned));
    }
    out->always_blocks.push_back(std::move(flattened));
  }

  for (const auto& instance : module.instances) {
    const Module* child = FindModule(program, instance.module_name);
    if (!child) {
      diagnostics->Add(Severity::kError,
                       "unknown module '" + instance.module_name + "'");
      return false;
    }

    ParamBindings child_params;
    if (!BuildParamBindings(*child, &instance, &params, &child_params,
                            diagnostics)) {
      return false;
    }

    std::unordered_map<std::string, PortBinding> child_port_map;
    std::unordered_set<std::string> child_ports;
    std::unordered_map<std::string, PortDir> child_port_dirs;
    std::unordered_map<std::string, int> child_port_widths;
    std::unordered_map<std::string, bool> child_port_signed;
    for (const auto& port : child->ports) {
      int width = port.width;
      if (!ResolveRangeWidth(port.width, port.msb_expr, port.lsb_expr,
                             child_params, &width, diagnostics,
                             "port '" + port.name + "'")) {
        return false;
      }
      child_ports.insert(port.name);
      child_port_dirs[port.name] = port.dir;
      child_port_widths[port.name] = width;
      child_port_signed[port.name] = port.is_signed;
    }
    const bool positional = !instance.connections.empty() &&
                            !instance.connections.front().port.empty() &&
                            std::isdigit(static_cast<unsigned char>(
                                instance.connections.front().port[0]));
    size_t position = 0;
    for (const auto& connection : instance.connections) {
      std::string port_name;
      if (positional) {
        if (position >= child->ports.size()) {
          diagnostics->Add(Severity::kError,
                           "too many positional connections in instance '" +
                               instance.name + "'");
          return false;
        }
        port_name = child->ports[position].name;
        ++position;
      } else {
        port_name = connection.port;
      }

      if (child_ports.count(port_name) == 0) {
        diagnostics->Add(Severity::kError,
                         "unknown port '" + port_name +
                             "' in instance '" + instance.name + "'");
        return false;
      }
      if (child_port_map.find(port_name) != child_port_map.end()) {
        diagnostics->Add(Severity::kError,
                         "duplicate connection for port '" + port_name +
                             "' in instance '" + instance.name + "'");
        return false;
      }
      if (!connection.expr) {
        continue;
      }
      if (connection.expr->kind == ExprKind::kIdentifier) {
        const std::string signal = rename(connection.expr->ident);
        child_port_map[port_name] = PortBinding{signal};
      } else if (connection.expr->kind == ExprKind::kNumber) {
        if (child_port_dirs[port_name] != PortDir::kInput) {
          diagnostics->Add(Severity::kError,
                           "literal connection only allowed for input port '" +
                               port_name + "' in instance '" +
                               instance.name + "'");
          return false;
        }
        std::string literal_name =
            prefix + instance.name + "__" + port_name + "__lit";
        int width = child_port_widths[port_name];
        if (!AddFlatNet(literal_name, width, child_port_signed[port_name],
                        NetType::kWire, 0,
                        hier_prefix + "." + instance.name + "." + port_name +
                            ".__lit",
                        out, net_names, flat_to_hier, diagnostics)) {
          return false;
        }
        child_port_map[port_name] = PortBinding{literal_name};
        auto literal_expr = CloneExprWithParams(*connection.expr,
                                                 [](const std::string& ident) {
                                                   return ident;
                                                 },
                                                 params, diagnostics);
        if (!literal_expr) {
          return false;
        }
        Assign literal_assign;
        literal_assign.lhs = literal_name;
        literal_assign.rhs = std::move(literal_expr);
        out->assigns.push_back(std::move(literal_assign));
      } else {
        diagnostics->Add(Severity::kError,
                         "port connections must be identifiers or literals in v0");
        return false;
      }
    }

    std::string child_prefix = prefix + instance.name + "__";
    std::string child_hier = hier_prefix + "." + instance.name;

    for (const auto& port : child->ports) {
      if (child_port_map.find(port.name) == child_port_map.end()) {
        if (port.dir == PortDir::kInput) {
          diagnostics->Add(Severity::kWarning,
                           "unconnected input '" + port.name +
                               "' in instance '" + instance.name +
                               "' (defaulting to " +
                               std::string(enable_4state ? "X" : "0") + ")");
          std::string default_name = child_prefix + port.name;
          if (!AddFlatNet(default_name, child_port_widths[port.name],
                          child_port_signed[port.name], NetType::kWire, 0,
                          child_hier + "." + port.name, out, net_names,
                          flat_to_hier, diagnostics)) {
            return false;
          }
          child_port_map[port.name] = PortBinding{default_name};
          Assign default_assign;
          default_assign.lhs = default_name;
          default_assign.rhs = enable_4state
                                   ? MakeAllXExpr(child_port_widths[port.name])
                                   : MakeNumberExpr(0);
          out->assigns.push_back(std::move(default_assign));
        } else {
          diagnostics->Add(Severity::kWarning,
                           "unconnected output '" + port.name +
                               "' in instance '" + instance.name + "'");
        }
      }
    }

    if (!InlineModule(program, *child, child_prefix, child_hier, child_params,
                      child_port_map, out, diagnostics, stack, net_names,
                      flat_to_hier, enable_4state)) {
      return false;
    }
  }

  stack->erase(module.name);
  return true;
}

}  // namespace

bool Elaborate(const Program& program, ElaboratedDesign* out_design,
               Diagnostics* diagnostics, bool enable_4state) {
  if (!out_design || !diagnostics) {
    return false;
  }
  if (program.modules.empty()) {
    diagnostics->Add(Severity::kError, "no modules to elaborate");
    return false;
  }

  std::string top_name;
  if (!FindTopModule(program, &top_name, diagnostics)) {
    return false;
  }
  return Elaborate(program, top_name, out_design, diagnostics, enable_4state);
}

bool Elaborate(const Program& program, const std::string& top_name,
               ElaboratedDesign* out_design, Diagnostics* diagnostics,
               bool enable_4state) {
  if (!out_design || !diagnostics) {
    return false;
  }
  if (program.modules.empty()) {
    diagnostics->Add(Severity::kError, "no modules to elaborate");
    return false;
  }

  const Module* top = FindModule(program, top_name);
  if (!top) {
    diagnostics->Add(Severity::kError,
                     "top module '" + top_name + "' not found");
    return false;
  }

  Module flat;
  ParamBindings top_params;
  if (!BuildParamBindings(*top, nullptr, nullptr, &top_params, diagnostics)) {
    return false;
  }
  std::unordered_map<std::string, PortBinding> port_map;
  std::unordered_set<std::string> stack;
  std::unordered_set<std::string> net_names;
  std::unordered_map<std::string, std::string> flat_to_hier;
  if (!InlineModule(program, *top, "", top->name, top_params, port_map, &flat,
                    diagnostics, &stack, &net_names, &flat_to_hier,
                    enable_4state)) {
    return false;
  }

  if (!ValidateSingleDrivers(flat, diagnostics)) {
    return false;
  }
  if (!ValidateCombinationalAcyclic(flat, diagnostics)) {
    return false;
  }
  WarnUndeclaredClocks(flat, diagnostics);
  WarnNonblockingInCombAlways(flat, diagnostics);
  WarnNonblockingArrayWrites(flat, diagnostics);

  out_design->top = std::move(flat);
  out_design->flat_to_hier = std::move(flat_to_hier);
  return true;
}

}  // namespace gpga
