#include "core/elaboration.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
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

const Net* FindNet(const Module& module, const std::string& name) {
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return &net;
    }
  }
  return nullptr;
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

Instance CloneInstance(const Instance& instance) {
  Instance out;
  out.module_name = instance.module_name;
  out.name = instance.name;
  for (const auto& override_item : instance.param_overrides) {
    ParamOverride param;
    param.name = override_item.name;
    if (override_item.expr) {
      param.expr = CloneExpr(*override_item.expr);
    }
    out.param_overrides.push_back(std::move(param));
  }
  for (const auto& conn : instance.connections) {
    Connection connection;
    connection.port = conn.port;
    if (conn.expr) {
      connection.expr = CloneExpr(*conn.expr);
    }
    out.connections.push_back(std::move(connection));
  }
  return out;
}

bool MatchDefparamInstance(const std::string& instance,
                           const std::string& instance_name,
                           std::string* tail) {
  if (tail) {
    tail->clear();
  }
  if (instance.empty()) {
    return false;
  }
  std::vector<std::string> parts;
  size_t start = 0;
  while (start < instance.size()) {
    size_t next = instance.find('.', start);
    if (next == std::string::npos) {
      parts.push_back(instance.substr(start));
      break;
    }
    parts.push_back(instance.substr(start, next - start));
    start = next + 1;
  }
  if (parts.empty()) {
    return false;
  }
  std::string flat = parts[0];
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      flat += "__";
      flat += parts[i];
    }
    if (flat == instance_name) {
      if (tail && i + 1 < parts.size()) {
        std::string suffix;
        for (size_t j = i + 1; j < parts.size(); ++j) {
          if (!suffix.empty()) {
            suffix += ".";
          }
          suffix += parts[j];
        }
        *tail = suffix;
      }
      return true;
    }
  }
  return false;
}

bool ValidateDefparamsForModule(
    const std::vector<DefParam>& defparams,
    const std::unordered_set<std::string>& instance_names,
    Diagnostics* diagnostics) {
  for (const auto& defparam : defparams) {
    bool matched = false;
    for (const auto& instance_name : instance_names) {
      if (MatchDefparamInstance(defparam.instance, instance_name, nullptr)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      diagnostics->Add(Severity::kError,
                       "unknown instance '" + defparam.instance +
                           "' in defparam");
      return false;
    }
  }
  return true;
}

bool ApplyDefparamsToInstance(
    const std::vector<DefParam>& defparams,
    const Instance& instance, Instance* out_instance,
    std::vector<DefParam>* child_defparams, Diagnostics* diagnostics) {
  if (!out_instance) {
    return false;
  }
  bool has_positional = false;
  for (const auto& override_item : out_instance->param_overrides) {
    if (override_item.name.empty()) {
      has_positional = true;
      break;
    }
  }
  for (const auto& defparam : defparams) {
    std::string tail;
    if (!MatchDefparamInstance(defparam.instance, instance.name, &tail)) {
      continue;
    }
    if (tail.empty()) {
      if (has_positional) {
        diagnostics->Add(
            Severity::kError,
            "defparam cannot target instance with positional overrides '" +
                instance.name + "'");
        return false;
      }
      bool replaced = false;
      for (auto& override_item : out_instance->param_overrides) {
        if (override_item.name == defparam.param) {
          override_item.expr =
              defparam.expr ? CloneExpr(*defparam.expr) : nullptr;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        ParamOverride override_item;
        override_item.name = defparam.param;
        if (defparam.expr) {
          override_item.expr = CloneExpr(*defparam.expr);
        }
        out_instance->param_overrides.push_back(std::move(override_item));
      }
      continue;
    }
    if (child_defparams) {
      DefParam child;
      child.instance = tail;
      child.param = defparam.param;
      child.line = defparam.line;
      child.column = defparam.column;
      if (defparam.expr) {
        child.expr = CloneExpr(*defparam.expr);
      }
      child_defparams->push_back(std::move(child));
    }
  }
  return true;
}

void ForceUnsizedWidth(Expr* expr, int width) {
  if (!expr) {
    return;
  }
  switch (expr->kind) {
    case ExprKind::kNumber:
      if (!expr->has_width) {
        expr->has_width = true;
        expr->number_width = width;
        if (width > 0 && width < 64) {
          uint64_t mask = (1ull << width) - 1ull;
          expr->number &= mask;
          expr->value_bits &= mask;
          expr->x_bits &= mask;
          expr->z_bits &= mask;
        }
      }
      return;
    case ExprKind::kString:
      return;
    case ExprKind::kUnary:
      ForceUnsizedWidth(expr->operand.get(), width);
      return;
    case ExprKind::kBinary:
      ForceUnsizedWidth(expr->lhs.get(), width);
      ForceUnsizedWidth(expr->rhs.get(), width);
      return;
    case ExprKind::kTernary:
      ForceUnsizedWidth(expr->condition.get(), width);
      ForceUnsizedWidth(expr->then_expr.get(), width);
      ForceUnsizedWidth(expr->else_expr.get(), width);
      return;
    case ExprKind::kSelect:
      ForceUnsizedWidth(expr->base.get(), width);
      ForceUnsizedWidth(expr->msb_expr.get(), width);
      ForceUnsizedWidth(expr->lsb_expr.get(), width);
      return;
    case ExprKind::kIndex:
      ForceUnsizedWidth(expr->base.get(), width);
      ForceUnsizedWidth(expr->index.get(), width);
      return;
    case ExprKind::kCall:
      for (auto& arg : expr->call_args) {
        ForceUnsizedWidth(arg.get(), width);
      }
      return;
    case ExprKind::kConcat:
      ForceUnsizedWidth(expr->repeat_expr.get(), width);
      for (auto& element : expr->elements) {
        ForceUnsizedWidth(element.get(), width);
      }
      return;
    case ExprKind::kIdentifier:
      return;
  }
}

bool FindTopModule(const Program& program, std::string* top_name,
                   Diagnostics* diagnostics) {
  std::unordered_set<std::string> instantiated;
  for (const auto& module : program.modules) {
    for (const auto& instance : module.instances) {
      instantiated.insert(instance.module_name);
    }
  }

  std::vector<const Module*> roots;
  roots.reserve(program.modules.size());
  for (const auto& module : program.modules) {
    if (instantiated.count(module.name) == 0) {
      roots.push_back(&module);
    }
  }

  if (roots.empty()) {
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
  if (roots.size() > 1) {
    auto has_initial = [](const Module& module) -> bool {
      for (const auto& block : module.always_blocks) {
        if (block.edge == EdgeKind::kInitial) {
          return true;
        }
      }
      return false;
    };
    auto is_test = [](const std::string& name) -> bool {
      return name.rfind("test_", 0) == 0;
    };
    const Module* chosen = nullptr;
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
    diagnostics->Add(Severity::kWarning,
                     "multiple top-level modules found; using '" +
                         chosen->name + "' (use --top <name> to override)");
    *top_name = chosen->name;
    return true;
  }
  *top_name = roots.front()->name;
  return true;
}

struct PortBinding {
  std::string signal;
};

struct ParamBindings {
  std::unordered_map<std::string, int64_t> values;
  std::unordered_map<std::string, uint64_t> real_values;
  std::unordered_map<std::string, std::unique_ptr<Expr>> exprs;
};

const std::unordered_map<std::string, std::string>* g_task_renames = nullptr;

std::unique_ptr<Expr> SimplifyExpr(std::unique_ptr<Expr> expr,
                                   const Module& module);
std::unique_ptr<Expr> MakeNumberExpr(uint64_t value);
std::unique_ptr<Expr> MakeNumberExprWidth(uint64_t value, int width);
std::unique_ptr<Expr> MakeNumberExprSignedWidth(int64_t value, int width);
double BitsToDouble(uint64_t bits);
uint64_t DoubleToBits(double value);
uint64_t MaskForWidth64(int width);
std::unique_ptr<Expr> MakeIdentifierExpr(const std::string& name);
std::unique_ptr<Expr> MakeUnaryExpr(char op, std::unique_ptr<Expr> operand);
std::unique_ptr<Expr> MakeBinaryExpr(char op, std::unique_ptr<Expr> lhs,
                                     std::unique_ptr<Expr> rhs);
std::unique_ptr<Expr> MakeTernaryExpr(std::unique_ptr<Expr> condition,
                                      std::unique_ptr<Expr> then_expr,
                                      std::unique_ptr<Expr> else_expr);
std::unique_ptr<Expr> MakeAllXExpr(int width);
std::unique_ptr<Expr> MakeRealLiteralExpr(double value);
using BindingMap = std::unordered_map<std::string, const Expr*>;
std::unique_ptr<Expr> CloneExprWithParams(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module* module,
    Diagnostics* diagnostics, const BindingMap* bindings);
std::unique_ptr<Expr> CloneExprWithParamsImpl(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module* module,
    Diagnostics* diagnostics, const BindingMap* bindings, int inline_depth);
void CollectIdentifiers(const Expr& expr,
                        std::unordered_set<std::string>* out);
void CollectAssignedSignals(const Statement& statement,
                            std::unordered_set<std::string>* out);
int SignalWidth(const Module& module, const std::string& name);
bool SignalIsReal(const Module& module, const std::string& name);
int ExprWidth(const Expr& expr, const Module& module);
bool ExprHasSystemCall(const Expr& expr);
bool StatementHasSystemCall(const Statement& statement);
bool CloneStatement(
    const Statement& statement,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module& source_module,
    const Module& flat_module, Statement* out, Diagnostics* diagnostics);
bool EvalConstExprValueWithFunctions(const Expr& expr,
                                     const ParamBindings& params,
                                     const Module& module, int64_t* out_value,
                                     Diagnostics* diagnostics,
                                     const std::string& context);

ParamBindings CloneParamBindings(const ParamBindings& params) {
  ParamBindings out;
  out.values = params.values;
  out.real_values = params.real_values;
  out.exprs.reserve(params.exprs.size());
  for (const auto& entry : params.exprs) {
    out.exprs[entry.first] = gpga::CloneExpr(*entry.second);
  }
  return out;
}

bool TryEvalConstExprWithParams(const Expr& expr, const ParamBindings& params,
                                int64_t* out_value) {
  Diagnostics scratch;
  auto resolved = CloneExprWithParams(
      expr, [](const std::string& ident) { return ident; }, params, nullptr,
      &scratch, nullptr);
  if (!resolved) {
    return false;
  }
  ForceUnsizedWidth(resolved.get(), 32);
  std::string error;
  if (!gpga::EvalConstExpr(*resolved, {}, out_value, &error)) {
    return false;
  }
  return true;
}

bool ExprUsesRealConst(const Expr& expr, const ParamBindings& params) {
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      return params.real_values.count(expr.ident) > 0;
    case ExprKind::kNumber:
      return expr.is_real_literal;
    case ExprKind::kUnary:
      if (expr.unary_op == '+' || expr.unary_op == '-' ||
          expr.unary_op == '!' || expr.unary_op == 'B') {
        return expr.operand ? ExprUsesRealConst(*expr.operand, params) : false;
      }
      return false;
    case ExprKind::kBinary:
      if (expr.op == '+' || expr.op == '-' || expr.op == '*' ||
          expr.op == '/' || expr.op == 'p' || expr.op == 'E' ||
          expr.op == 'N' || expr.op == 'C' || expr.op == 'c' ||
          expr.op == 'W' || expr.op == 'w' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G' ||
          expr.op == 'A' || expr.op == 'O') {
        return (expr.lhs && ExprUsesRealConst(*expr.lhs, params)) ||
               (expr.rhs && ExprUsesRealConst(*expr.rhs, params));
      }
      return false;
    case ExprKind::kTernary:
      return (expr.then_expr && ExprUsesRealConst(*expr.then_expr, params)) ||
             (expr.else_expr && ExprUsesRealConst(*expr.else_expr, params));
    case ExprKind::kCall:
      return expr.ident == "$realtime" || expr.ident == "$itor" ||
             expr.ident == "$bitstoreal" || expr.ident == "$rtoi";
    case ExprKind::kString:
    case ExprKind::kSelect:
    case ExprKind::kIndex:
    case ExprKind::kConcat:
      return false;
  }
  return false;
}

bool EvalConstExprRealValue(const Expr& expr, const ParamBindings& params,
                            const Module& module, double* out_value,
                            Diagnostics* diagnostics) {
  (void)module;
  if (!out_value) {
    return false;
  }
  if (!ExprUsesRealConst(expr, params)) {
    int64_t value = 0;
    if (!EvalConstExprValueWithFunctions(expr, params, module, &value,
                                         diagnostics,
                                         "real constant expression")) {
      return false;
    }
    *out_value = static_cast<double>(value);
    return true;
  }
  switch (expr.kind) {
    case ExprKind::kNumber: {
      if (expr.is_real_literal) {
        *out_value = BitsToDouble(expr.value_bits);
        return true;
      }
      if (expr.x_bits != 0 || expr.z_bits != 0) {
        diagnostics->Add(Severity::kError,
                         "x/z not allowed in real constant expression");
        return false;
      }
      *out_value = static_cast<double>(expr.number);
      return true;
    }
    case ExprKind::kIdentifier: {
      auto it = params.real_values.find(expr.ident);
      if (it != params.real_values.end()) {
        *out_value = BitsToDouble(it->second);
        return true;
      }
      auto int_it = params.values.find(expr.ident);
      if (int_it != params.values.end()) {
        *out_value = static_cast<double>(int_it->second);
        return true;
      }
      diagnostics->Add(Severity::kError,
                       "unknown parameter '" + expr.ident + "'");
      return false;
    }
    case ExprKind::kString:
      diagnostics->Add(Severity::kError,
                       "string literal not allowed in real constant expression");
      return false;
    case ExprKind::kUnary: {
      if (!expr.operand) {
        diagnostics->Add(Severity::kError,
                         "missing operand in real constant expression");
        return false;
      }
      double value = 0.0;
      if (!EvalConstExprRealValue(*expr.operand, params, module, &value,
                                  diagnostics)) {
        return false;
      }
      if (expr.unary_op == '+') {
        *out_value = value;
        return true;
      }
      if (expr.unary_op == '-') {
        *out_value = -value;
        return true;
      }
      if (expr.unary_op == '!' || expr.unary_op == 'B') {
        *out_value = (value == 0.0) ? 1.0 : 0.0;
        return true;
      }
      diagnostics->Add(Severity::kError,
                       "unsupported unary operator in real constant expression");
      return false;
    }
    case ExprKind::kBinary: {
      if (!expr.lhs || !expr.rhs) {
        diagnostics->Add(Severity::kError,
                         "missing operand in real constant expression");
        return false;
      }
      double lhs = 0.0;
      double rhs = 0.0;
      if (!EvalConstExprRealValue(*expr.lhs, params, module, &lhs,
                                  diagnostics) ||
          !EvalConstExprRealValue(*expr.rhs, params, module, &rhs,
                                  diagnostics)) {
        return false;
      }
      if (expr.op == '+' || expr.op == '-' || expr.op == '*' ||
          expr.op == '/') {
        switch (expr.op) {
          case '+':
            *out_value = lhs + rhs;
            break;
          case '-':
            *out_value = lhs - rhs;
            break;
          case '*':
            *out_value = lhs * rhs;
            break;
          case '/':
            *out_value = lhs / rhs;
            break;
        }
        return true;
      }
      if (expr.op == 'p') {
        *out_value = std::pow(lhs, rhs);
        return true;
      }
      if (expr.op == 'E' || expr.op == 'C' || expr.op == 'W') {
        *out_value = (lhs == rhs) ? 1.0 : 0.0;
        return true;
      }
      if (expr.op == 'N' || expr.op == 'c' || expr.op == 'w') {
        *out_value = (lhs != rhs) ? 1.0 : 0.0;
        return true;
      }
      if (expr.op == '<') {
        *out_value = (lhs < rhs) ? 1.0 : 0.0;
        return true;
      }
      if (expr.op == '>') {
        *out_value = (lhs > rhs) ? 1.0 : 0.0;
        return true;
      }
      if (expr.op == 'L') {
        *out_value = (lhs <= rhs) ? 1.0 : 0.0;
        return true;
      }
      if (expr.op == 'G') {
        *out_value = (lhs >= rhs) ? 1.0 : 0.0;
        return true;
      }
      if (expr.op == 'A') {
        *out_value = ((lhs != 0.0) && (rhs != 0.0)) ? 1.0 : 0.0;
        return true;
      }
      if (expr.op == 'O') {
        *out_value = ((lhs != 0.0) || (rhs != 0.0)) ? 1.0 : 0.0;
        return true;
      }
      diagnostics->Add(Severity::kError,
                       "unsupported binary operator in real constant expression");
      return false;
    }
    case ExprKind::kTernary: {
      double cond_value = 0.0;
      if (expr.condition &&
          !EvalConstExprRealValue(*expr.condition, params, module, &cond_value,
                                  diagnostics)) {
        return false;
      }
      bool cond = cond_value != 0.0;
      if (cond) {
        if (!expr.then_expr) {
          diagnostics->Add(
              Severity::kError,
              "missing then branch in real constant expression");
          return false;
        }
        return EvalConstExprRealValue(*expr.then_expr, params, module,
                                      out_value, diagnostics);
      }
      if (!expr.else_expr) {
        diagnostics->Add(Severity::kError,
                         "missing else branch in real constant expression");
        return false;
      }
      return EvalConstExprRealValue(*expr.else_expr, params, module, out_value,
                                    diagnostics);
    }
    case ExprKind::kCall: {
      if (expr.ident == "$realtime") {
        diagnostics->Add(Severity::kError,
                         "$realtime not allowed in real constant expression");
        return false;
      }
      if (expr.ident == "$itor") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          return EvalConstExprRealValue(*expr.call_args.front(), params, module,
                                        out_value, diagnostics);
        }
        *out_value = 0.0;
        return true;
      }
      if (expr.ident == "$bitstoreal") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          int64_t bits = 0;
          if (!TryEvalConstExprWithParams(*expr.call_args.front(), params,
                                          &bits)) {
            diagnostics->Add(
                Severity::kError,
                "$bitstoreal requires integer constant in real expression");
            return false;
          }
          *out_value = BitsToDouble(static_cast<uint64_t>(bits));
          return true;
        }
        *out_value = 0.0;
        return true;
      }
      if (expr.ident == "$rtoi") {
        if (!expr.call_args.empty() && expr.call_args.front()) {
          double value = 0.0;
          if (!EvalConstExprRealValue(*expr.call_args.front(), params, module,
                                      &value, diagnostics)) {
            return false;
          }
          *out_value = static_cast<double>(static_cast<int64_t>(value));
          return true;
        }
        *out_value = 0.0;
        return true;
      }
      diagnostics->Add(Severity::kError,
                       "unsupported function '" + expr.ident +
                           "' in real constant expression");
      return false;
    }
    case ExprKind::kSelect:
    case ExprKind::kIndex:
    case ExprKind::kConcat:
      diagnostics->Add(Severity::kError,
                       "unsupported expression in real constant expression");
      return false;
  }
  return false;
}

struct ConstVar {
  int64_t value = 0;
  int width = 32;
  bool is_signed = false;
  bool initialized = false;
};

struct ConstScope {
  std::unordered_map<std::string, ConstVar> vars;
};

bool EvalConstExprInScope(const Expr& expr, const Module& module,
                          const ParamBindings& params, const ConstScope& scope,
                          int64_t* out_value, Diagnostics* diagnostics,
                          std::unordered_set<std::string>* call_stack);

bool EvalConstFunction(const Function& func, const Module& module,
                       const ParamBindings& params,
                       const std::vector<int64_t>& arg_values,
                       int64_t* out_value, Diagnostics* diagnostics,
                       std::unordered_set<std::string>* call_stack);

void ReplaceExprWithNumber(Expr* expr, int64_t value, int width) {
  if (!expr) {
    return;
  }
  uint64_t bits = static_cast<uint64_t>(value);
  if (width < 64) {
    bits &= MaskForWidth64(width);
  }
  expr->kind = ExprKind::kNumber;
  expr->number = bits;
  expr->value_bits = bits;
  expr->x_bits = 0;
  expr->z_bits = 0;
  expr->has_width = true;
  expr->number_width = width;
  expr->is_signed = false;
  expr->ident.clear();
  expr->call_args.clear();
  expr->elements.clear();
  expr->operand.reset();
  expr->lhs.reset();
  expr->rhs.reset();
  expr->condition.reset();
  expr->then_expr.reset();
  expr->else_expr.reset();
  expr->base.reset();
  expr->msb_expr.reset();
  expr->lsb_expr.reset();
  expr->index.reset();
  expr->repeat_expr.reset();
}

bool ResolveConstFunctionCalls(Expr* expr, const Module& module,
                               const ParamBindings& params,
                               const ConstScope& scope, Diagnostics* diagnostics,
                               std::unordered_set<std::string>* call_stack) {
  if (!expr) {
    return true;
  }
  switch (expr->kind) {
    case ExprKind::kCall: {
      if (!expr->ident.empty() && expr->ident.front() == '$') {
        if (expr->ident == "$rtoi") {
          if (expr->call_args.size() != 1 || !expr->call_args.front()) {
            diagnostics->Add(Severity::kError,
                             "$rtoi expects 1 argument in constant function");
            return false;
          }
          double value = 0.0;
          if (!EvalConstExprRealValue(*expr->call_args.front(), params, module,
                                      &value, diagnostics)) {
            return false;
          }
          ReplaceExprWithNumber(expr, static_cast<int64_t>(value), 32);
          return true;
        }
        diagnostics->Add(Severity::kError,
                         "system function '" + expr->ident +
                             "' not allowed in constant function");
        return false;
      }
      const Function* func = FindFunction(module, expr->ident);
      if (!func) {
        diagnostics->Add(Severity::kError,
                         "unknown function '" + expr->ident + "'");
        return false;
      }
      std::vector<int64_t> arg_values;
      arg_values.reserve(expr->call_args.size());
      for (const auto& arg : expr->call_args) {
        int64_t value = 0;
        if (!arg || !EvalConstExprInScope(*arg, module, params, scope, &value,
                                          diagnostics, call_stack)) {
          return false;
        }
        arg_values.push_back(value);
      }
      int64_t result = 0;
      if (!EvalConstFunction(*func, module, params, arg_values, &result,
                             diagnostics, call_stack)) {
        return false;
      }
      ReplaceExprWithNumber(expr, result, func->width);
      return true;
    }
    case ExprKind::kUnary:
      return ResolveConstFunctionCalls(expr->operand.get(), module, params,
                                       scope, diagnostics, call_stack);
    case ExprKind::kBinary:
      return ResolveConstFunctionCalls(expr->lhs.get(), module, params, scope,
                                       diagnostics, call_stack) &&
             ResolveConstFunctionCalls(expr->rhs.get(), module, params, scope,
                                       diagnostics, call_stack);
    case ExprKind::kTernary:
      return ResolveConstFunctionCalls(expr->condition.get(), module, params,
                                       scope, diagnostics, call_stack) &&
             ResolveConstFunctionCalls(expr->then_expr.get(), module, params,
                                       scope, diagnostics, call_stack) &&
             ResolveConstFunctionCalls(expr->else_expr.get(), module, params,
                                       scope, diagnostics, call_stack);
    case ExprKind::kSelect:
      return ResolveConstFunctionCalls(expr->base.get(), module, params, scope,
                                       diagnostics, call_stack) &&
             ResolveConstFunctionCalls(expr->msb_expr.get(), module, params,
                                       scope, diagnostics, call_stack) &&
             ResolveConstFunctionCalls(expr->lsb_expr.get(), module, params,
                                       scope, diagnostics, call_stack);
    case ExprKind::kIndex:
      return ResolveConstFunctionCalls(expr->base.get(), module, params, scope,
                                       diagnostics, call_stack) &&
             ResolveConstFunctionCalls(expr->index.get(), module, params, scope,
                                       diagnostics, call_stack);
    case ExprKind::kConcat:
      for (const auto& element : expr->elements) {
        if (!ResolveConstFunctionCalls(element.get(), module, params, scope,
                                       diagnostics, call_stack)) {
          return false;
        }
      }
      return ResolveConstFunctionCalls(expr->repeat_expr.get(), module, params,
                                       scope, diagnostics, call_stack);
    case ExprKind::kIdentifier:
    case ExprKind::kNumber:
    case ExprKind::kString:
      return true;
  }
  return true;
}

bool EvalConstExprInScope(const Expr& expr, const Module& module,
                          const ParamBindings& params, const ConstScope& scope,
                          int64_t* out_value, Diagnostics* diagnostics,
                          std::unordered_set<std::string>* call_stack) {
  std::unordered_set<std::string> idents;
  CollectIdentifiers(expr, &idents);
  for (const auto& name : idents) {
    auto it = scope.vars.find(name);
    if (it != scope.vars.end()) {
      if (!it->second.initialized) {
        diagnostics->Add(Severity::kError,
                         "use of uninitialized variable '" + name +
                             "' in constant function");
        return false;
      }
      continue;
    }
    if (params.values.count(name) > 0) {
      continue;
    }
    if (params.real_values.count(name) > 0) {
      continue;
    }
    diagnostics->Add(Severity::kError,
                     "unknown identifier '" + name +
                         "' in constant function");
    return false;
  }

  auto resolved = CloneExpr(expr);
  if (!ResolveConstFunctionCalls(resolved.get(), module, params, scope,
                                 diagnostics, call_stack)) {
    return false;
  }
  std::unordered_map<std::string, int64_t> scope_values = params.values;
  for (const auto& entry : scope.vars) {
    if (entry.second.initialized) {
      scope_values[entry.first] = entry.second.value;
    }
  }
  std::string error;
  if (!gpga::EvalConstExpr(*resolved, scope_values, out_value, &error)) {
    diagnostics->Add(Severity::kError, error + " in constant function");
    return false;
  }
  return true;
}

bool AssignConstVarValue(ConstScope* scope, const std::string& name,
                         int64_t value, Diagnostics* diagnostics) {
  if (!scope) {
    return false;
  }
  auto it = scope->vars.find(name);
  if (it == scope->vars.end()) {
    diagnostics->Add(Severity::kError,
                     "assignment to non-local '" + name +
                         "' in constant function");
    return false;
  }
  uint64_t bits = static_cast<uint64_t>(value);
  if (it->second.width < 64) {
    bits &= MaskForWidth64(it->second.width);
  }
  it->second.value = static_cast<int64_t>(bits);
  it->second.initialized = true;
  return true;
}

bool AssignConstVar(const SequentialAssign& assign, const Module& module,
                    const ParamBindings& params, ConstScope* scope,
                    Diagnostics* diagnostics,
                    std::unordered_set<std::string>* call_stack) {
  if (!scope || !assign.rhs) {
    return false;
  }
  if (!assign.lhs_indices.empty() || assign.lhs_indexed_range) {
    diagnostics->Add(Severity::kError,
                     "array assignment not supported in constant function");
    return false;
  }
  int64_t rhs_value = 0;
  if (!EvalConstExprInScope(*assign.rhs, module, params, *scope, &rhs_value,
                            diagnostics, call_stack)) {
    return false;
  }
  auto it = scope->vars.find(assign.lhs);
  if (it == scope->vars.end()) {
    diagnostics->Add(Severity::kError,
                     "assignment to non-local '" + assign.lhs +
                         "' in constant function");
    return false;
  }
  ConstVar& var = it->second;
  uint64_t bits = static_cast<uint64_t>(var.value);
  if (var.width < 64) {
    bits &= MaskForWidth64(var.width);
  }
  if (assign.lhs_index) {
    int64_t index = 0;
    if (!EvalConstExprInScope(*assign.lhs_index, module, params, *scope, &index,
                              diagnostics, call_stack)) {
      return false;
    }
    if (index < 0 || index >= var.width) {
      return true;
    }
    uint64_t mask = 1ull << static_cast<uint64_t>(index);
    if ((rhs_value & 1ll) != 0) {
      bits |= mask;
    } else {
      bits &= ~mask;
    }
    var.value = static_cast<int64_t>(bits);
    var.initialized = true;
    return true;
  }
  if (assign.lhs_has_range) {
    int64_t msb = assign.lhs_msb;
    int64_t lsb = assign.lhs_lsb;
    if (assign.lhs_indexed_range) {
      if (!assign.lhs_msb_expr || !assign.lhs_lsb_expr) {
        diagnostics->Add(Severity::kError,
                         "indexed part select missing bounds");
        return false;
      }
      if (!EvalConstExprInScope(*assign.lhs_msb_expr, module, params, *scope,
                                &msb, diagnostics, call_stack) ||
          !EvalConstExprInScope(*assign.lhs_lsb_expr, module, params, *scope,
                                &lsb, diagnostics, call_stack)) {
        return false;
      }
    }
    int64_t lo = std::min(msb, lsb);
    int64_t hi = std::max(msb, lsb);
    int width = static_cast<int>(hi - lo + 1);
    if (lo < 0 || hi >= var.width) {
      return true;
    }
    uint64_t mask = MaskForWidth64(width);
    uint64_t insert = static_cast<uint64_t>(rhs_value) & mask;
    bits &= ~(mask << static_cast<uint64_t>(lo));
    bits |= (insert << static_cast<uint64_t>(lo));
    var.value = static_cast<int64_t>(bits);
    var.initialized = true;
    return true;
  }
  return AssignConstVarValue(scope, assign.lhs, rhs_value, diagnostics);
}

bool EvalConstStatements(const std::vector<Statement>& statements,
                         const Module& module, const ParamBindings& params,
                         ConstScope* scope, Diagnostics* diagnostics,
                         std::unordered_set<std::string>* call_stack);

bool EvalConstStatement(const Statement& stmt, const Module& module,
                        const ParamBindings& params, ConstScope* scope,
                        Diagnostics* diagnostics,
                        std::unordered_set<std::string>* call_stack) {
  const int kMaxIterations = 1 << 20;
  switch (stmt.kind) {
    case StatementKind::kAssign:
      if (stmt.assign.nonblocking) {
        diagnostics->Add(Severity::kError,
                         "nonblocking assignment not allowed in constant "
                         "function");
        return false;
      }
      return AssignConstVar(stmt.assign, module, params, scope, diagnostics,
                            call_stack);
    case StatementKind::kIf: {
      int64_t cond = 0;
      if (stmt.condition &&
          !EvalConstExprInScope(*stmt.condition, module, params, *scope, &cond,
                                diagnostics, call_stack)) {
        return false;
      }
      const auto& branch = (cond != 0) ? stmt.then_branch : stmt.else_branch;
      return EvalConstStatements(branch, module, params, scope, diagnostics,
                                 call_stack);
    }
    case StatementKind::kBlock:
      return EvalConstStatements(stmt.block, module, params, scope, diagnostics,
                                 call_stack);
    case StatementKind::kFor: {
      if (!stmt.for_init_rhs) {
        diagnostics->Add(Severity::kError,
                         "missing for init in constant function");
        return false;
      }
      int64_t init_value = 0;
      if (!EvalConstExprInScope(*stmt.for_init_rhs, module, params, *scope,
                                &init_value, diagnostics, call_stack)) {
        return false;
      }
      if (!AssignConstVarValue(scope, stmt.for_init_lhs, init_value,
                               diagnostics)) {
        return false;
      }
      int iterations = 0;
      while (true) {
        int64_t cond = 0;
        if (stmt.for_condition &&
            !EvalConstExprInScope(*stmt.for_condition, module, params, *scope,
                                  &cond, diagnostics, call_stack)) {
          return false;
        }
        if (cond == 0) {
          break;
        }
        if (!EvalConstStatements(stmt.for_body, module, params, scope,
                                 diagnostics, call_stack)) {
          return false;
        }
        if (!stmt.for_step_rhs) {
          diagnostics->Add(Severity::kError,
                           "missing for step in constant function");
          return false;
        }
        int64_t step_value = 0;
        if (!EvalConstExprInScope(*stmt.for_step_rhs, module, params, *scope,
                                  &step_value, diagnostics, call_stack)) {
          return false;
        }
        if (!AssignConstVarValue(scope, stmt.for_step_lhs, step_value,
                                 diagnostics)) {
          return false;
        }
        if (++iterations > kMaxIterations) {
          diagnostics->Add(Severity::kError,
                           "for loop exceeded iteration limit in constant "
                           "function");
          return false;
        }
      }
      return true;
    }
    case StatementKind::kWhile: {
      int iterations = 0;
      while (true) {
        int64_t cond = 0;
        if (stmt.while_condition &&
            !EvalConstExprInScope(*stmt.while_condition, module, params, *scope,
                                  &cond, diagnostics, call_stack)) {
          return false;
        }
        if (cond == 0) {
          break;
        }
        if (!EvalConstStatements(stmt.while_body, module, params, scope,
                                 diagnostics, call_stack)) {
          return false;
        }
        if (++iterations > kMaxIterations) {
          diagnostics->Add(Severity::kError,
                           "while loop exceeded iteration limit in constant "
                           "function");
          return false;
        }
      }
      return true;
    }
    case StatementKind::kRepeat: {
      if (!stmt.repeat_count) {
        diagnostics->Add(Severity::kError,
                         "missing repeat count in constant function");
        return false;
      }
      int64_t count = 0;
      if (!EvalConstExprInScope(*stmt.repeat_count, module, params, *scope,
                                &count, diagnostics, call_stack)) {
        return false;
      }
      if (count < 0) {
        count = 0;
      }
      for (int64_t i = 0; i < count; ++i) {
        if (!EvalConstStatements(stmt.repeat_body, module, params, scope,
                                 diagnostics, call_stack)) {
          return false;
        }
      }
      return true;
    }
    case StatementKind::kCase:
    default:
      diagnostics->Add(Severity::kError,
                       "unsupported statement in constant function");
      return false;
  }
}

bool EvalConstStatements(const std::vector<Statement>& statements,
                         const Module& module, const ParamBindings& params,
                         ConstScope* scope, Diagnostics* diagnostics,
                         std::unordered_set<std::string>* call_stack) {
  for (const auto& stmt : statements) {
    if (!EvalConstStatement(stmt, module, params, scope, diagnostics,
                            call_stack)) {
      return false;
    }
  }
  return true;
}

bool EvalConstFunction(const Function& func, const Module& module,
                       const ParamBindings& params,
                       const std::vector<int64_t>& arg_values,
                       int64_t* out_value, Diagnostics* diagnostics,
                       std::unordered_set<std::string>* call_stack) {
  if (!call_stack) {
    return false;
  }
  std::string key = func.name;
  key.push_back('(');
  for (size_t i = 0; i < arg_values.size(); ++i) {
    if (i != 0) {
      key.push_back(',');
    }
    key += std::to_string(arg_values[i]);
  }
  key.push_back(')');
  if (call_stack->count(key) > 0) {
    diagnostics->Add(Severity::kError,
                     "recursive function '" + func.name +
                         "' not supported in constant evaluation");
    return false;
  }
  if (call_stack->size() > 1024) {
    diagnostics->Add(Severity::kError,
                     "function recursion too deep in constant evaluation");
    return false;
  }
  call_stack->insert(key);
  ConstScope scope;
  ConstVar out_var;
  out_var.width = func.width;
  out_var.is_signed = func.is_signed;
  out_var.initialized = false;
  scope.vars[func.name] = out_var;
  if (func.args.size() != arg_values.size()) {
    diagnostics->Add(Severity::kError,
                     "function '" + func.name + "' expects " +
                         std::to_string(func.args.size()) + " argument(s)");
    call_stack->erase(key);
    return false;
  }
  for (size_t i = 0; i < func.args.size(); ++i) {
    ConstVar arg;
    arg.width = func.args[i].width;
    arg.is_signed = func.args[i].is_signed;
    arg.value = arg_values[i];
    if (arg.width < 64) {
      arg.value =
          static_cast<int64_t>(static_cast<uint64_t>(arg.value) &
                               MaskForWidth64(arg.width));
    }
    arg.initialized = true;
    scope.vars[func.args[i].name] = arg;
  }
  for (const auto& local : func.locals) {
    ConstVar local_var;
    local_var.width = local.width;
    local_var.is_signed = local.is_signed;
    local_var.initialized = false;
    scope.vars[local.name] = local_var;
  }
  if (!EvalConstStatements(func.body, module, params, &scope, diagnostics,
                           call_stack)) {
    call_stack->erase(key);
    return false;
  }
  auto it = scope.vars.find(func.name);
  if (it == scope.vars.end() || !it->second.initialized) {
    diagnostics->Add(Severity::kError,
                     "function '" + func.name +
                         "' missing return assignment");
    call_stack->erase(key);
    return false;
  }
  if (out_value) {
    *out_value = it->second.value;
  }
  call_stack->erase(key);
  return true;
}

struct SymbolicVar {
  std::unique_ptr<Expr> expr;
  int width = 0;
  bool is_signed = false;
  bool is_real = false;
};

using SymbolicEnv = std::unordered_map<std::string, SymbolicVar>;

SymbolicEnv CloneSymbolicEnv(const SymbolicEnv& env) {
  SymbolicEnv out;
  out.reserve(env.size());
  for (const auto& entry : env) {
    SymbolicVar var;
    var.width = entry.second.width;
    var.is_signed = entry.second.is_signed;
    var.is_real = entry.second.is_real;
    if (entry.second.expr) {
      var.expr = gpga::CloneExpr(*entry.second.expr);
    }
    out.emplace(entry.first, std::move(var));
  }
  return out;
}

std::unique_ptr<Expr> CloneExprWithParamsImpl(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module* module,
    Diagnostics* diagnostics, const BindingMap* bindings, int inline_depth);

std::unique_ptr<Expr> CloneExprWithEnv(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module& module, const SymbolicEnv& env,
    Diagnostics* diagnostics, int inline_depth) {
  BindingMap bindings;
  bindings.reserve(env.size());
  for (const auto& entry : env) {
    if (entry.second.expr) {
      bindings[entry.first] = entry.second.expr.get();
    }
  }
  return CloneExprWithParamsImpl(expr, rename, params, &module, diagnostics,
                                 &bindings, inline_depth);
}

std::unique_ptr<Expr> MakeBoolExpr(std::unique_ptr<Expr> expr) {
  return MakeUnaryExpr('B', std::move(expr));
}

std::unique_ptr<Expr> MakeMaskExpr(int width, int target_width) {
  uint64_t mask = MaskForWidth64(width);
  return MakeNumberExprWidth(mask, target_width);
}

std::unique_ptr<Expr> BuildRangeAssignExpr(
    const Expr& base_expr, std::unique_ptr<Expr> rhs_expr,
    std::unique_ptr<Expr> lsb_expr, int slice_width, int base_width) {
  if (!rhs_expr || !lsb_expr) {
    return nullptr;
  }
  auto mask = MakeMaskExpr(slice_width, base_width);
  auto mask_shifted = MakeBinaryExpr('l', gpga::CloneExpr(*mask),
                                     gpga::CloneExpr(*lsb_expr));
  auto cleared = MakeBinaryExpr('&', gpga::CloneExpr(base_expr),
                                MakeUnaryExpr('~', std::move(mask_shifted)));
  auto rhs_masked = MakeBinaryExpr('&', std::move(rhs_expr), std::move(mask));
  auto shifted = MakeBinaryExpr('l', std::move(rhs_masked), std::move(lsb_expr));
  return MakeBinaryExpr('|', std::move(cleared), std::move(shifted));
}

bool TryEvalConstExprWithEnv(const Expr& expr, const ParamBindings& params,
                             const Module& module, const SymbolicEnv& env,
                             int64_t* out_value, Diagnostics* diagnostics,
                             int inline_depth) {
  auto resolved = CloneExprWithEnv(expr, [](const std::string& ident) { return ident; },
                                   params, module, env, diagnostics, inline_depth);
  if (!resolved) {
    return false;
  }
  std::string error;
  if (!gpga::EvalConstExpr(*resolved, params.values, out_value, &error)) {
    return false;
  }
  return true;
}

bool AssignSymbolic(const SequentialAssign& assign, const Module& module,
                    const ParamBindings& params,
                    const std::function<std::string(const std::string&)>& rename,
                    SymbolicEnv* env, Diagnostics* diagnostics,
                    int inline_depth) {
  if (!env) {
    return false;
  }
  auto it = env->find(assign.lhs);
  if (it == env->end()) {
    diagnostics->Add(Severity::kError,
                     "assignment to non-local '" + assign.lhs +
                         "' in function body");
    return false;
  }
  if (!assign.rhs) {
    it->second.expr = it->second.is_real ? MakeRealLiteralExpr(0.0)
                                         : MakeAllXExpr(it->second.width);
    return true;
  }
  auto rhs = CloneExprWithEnv(*assign.rhs, rename, params, module, *env,
                              diagnostics, inline_depth);
  if (!rhs) {
    return false;
  }
  if (!assign.lhs_indices.empty() || assign.lhs_indexed_range) {
    diagnostics->Add(Severity::kError,
                     "array assignment not supported in function body");
    return false;
  }
  if (it->second.is_real &&
      (assign.lhs_index || assign.lhs_has_range)) {
    diagnostics->Add(Severity::kError,
                     "bit/part select not allowed on real in function body");
    return false;
  }
  if (assign.lhs_index) {
    auto idx = CloneExprWithEnv(*assign.lhs_index, rename, params, module, *env,
                                diagnostics, inline_depth);
    if (!idx) {
      return false;
    }
    auto updated = BuildRangeAssignExpr(*it->second.expr, std::move(rhs),
                                        std::move(idx), 1, it->second.width);
    if (!updated) {
      return false;
    }
    it->second.expr = std::move(updated);
    return true;
  }
  if (assign.lhs_has_range) {
    int64_t msb = assign.lhs_msb;
    int64_t lsb = assign.lhs_lsb;
    std::unique_ptr<Expr> lsb_expr;
    int width = 0;
    if (assign.lhs_indexed_range) {
      if (!assign.lhs_lsb_expr) {
        diagnostics->Add(Severity::kError,
                         "indexed part select missing lsb");
        return false;
      }
      lsb_expr = CloneExprWithEnv(*assign.lhs_lsb_expr, rename, params, module,
                                  *env, diagnostics, inline_depth);
      width = assign.lhs_indexed_width;
    } else {
      int lo = std::min(msb, lsb);
      int hi = std::max(msb, lsb);
      width = hi - lo + 1;
      lsb_expr = MakeNumberExpr(static_cast<uint64_t>(lo));
    }
    auto updated = BuildRangeAssignExpr(*it->second.expr, std::move(rhs),
                                        std::move(lsb_expr), width,
                                        it->second.width);
    if (!updated) {
      return false;
    }
    it->second.expr = std::move(updated);
    return true;
  }
  it->second.expr = std::move(rhs);
  return true;
}

bool EvalSymbolicStatements(const std::vector<Statement>& statements,
                            const Module& module, const ParamBindings& params,
                            const std::function<std::string(const std::string&)>& rename,
                            SymbolicEnv* env, Diagnostics* diagnostics,
                            int inline_depth);

bool EvalSymbolicStatement(const Statement& stmt, const Module& module,
                           const ParamBindings& params,
                           const std::function<std::string(const std::string&)>& rename,
                           SymbolicEnv* env, Diagnostics* diagnostics,
                           int inline_depth) {
  const int kMaxIterations = 1 << 20;
  switch (stmt.kind) {
    case StatementKind::kAssign:
      if (stmt.assign.nonblocking) {
        diagnostics->Add(Severity::kError,
                         "nonblocking assignment not allowed in function body");
        return false;
      }
      return AssignSymbolic(stmt.assign, module, params, rename, env,
                            diagnostics, inline_depth);
    case StatementKind::kIf: {
      std::unique_ptr<Expr> cond_expr;
      if (stmt.condition) {
        cond_expr = CloneExprWithEnv(*stmt.condition, rename, params, module,
                                     *env, diagnostics, inline_depth);
      } else {
        cond_expr = MakeNumberExpr(0u);
      }
      if (!cond_expr) {
        return false;
      }
      SymbolicEnv then_env = CloneSymbolicEnv(*env);
      SymbolicEnv else_env = CloneSymbolicEnv(*env);
      if (!EvalSymbolicStatements(stmt.then_branch, module, params, rename,
                                  &then_env, diagnostics, inline_depth)) {
        return false;
      }
      if (!EvalSymbolicStatements(stmt.else_branch, module, params, rename,
                                  &else_env, diagnostics, inline_depth)) {
        return false;
      }
      auto cond_bool = MakeBoolExpr(std::move(cond_expr));
      for (auto& entry : *env) {
        auto then_it = then_env.find(entry.first);
        auto else_it = else_env.find(entry.first);
        if (then_it == then_env.end() || else_it == else_env.end()) {
          continue;
        }
        if (!then_it->second.expr || !else_it->second.expr) {
          continue;
        }
        auto cond_clone = gpga::CloneExpr(*cond_bool);
        entry.second.expr = MakeTernaryExpr(
            std::move(cond_clone), gpga::CloneExpr(*then_it->second.expr),
            gpga::CloneExpr(*else_it->second.expr));
      }
      return true;
    }
    case StatementKind::kBlock:
      return EvalSymbolicStatements(stmt.block, module, params, rename, env,
                                    diagnostics, inline_depth);
    case StatementKind::kFor: {
      if (!stmt.for_init_rhs || !stmt.for_step_rhs || !stmt.for_condition) {
        diagnostics->Add(Severity::kError,
                         "incomplete for loop in function body");
        return false;
      }
      int64_t init_value = 0;
      if (!TryEvalConstExprWithEnv(*stmt.for_init_rhs, params, module, *env,
                                   &init_value, diagnostics, inline_depth)) {
        diagnostics->Add(Severity::kError,
                         "for init must be constant in function body");
        return false;
      }
      SequentialAssign init_assign;
      init_assign.lhs = stmt.for_init_lhs;
      init_assign.rhs = MakeNumberExprSignedWidth(init_value, 32);
      if (!AssignSymbolic(init_assign, module, params, rename, env, diagnostics,
                          inline_depth)) {
        return false;
      }
      int iterations = 0;
      while (true) {
        int64_t cond_value = 0;
        if (!TryEvalConstExprWithEnv(*stmt.for_condition, params, module, *env,
                                     &cond_value, diagnostics, inline_depth)) {
          diagnostics->Add(Severity::kError,
                           "for condition must be constant in function body");
          return false;
        }
        if (cond_value == 0) {
          break;
        }
        if (!EvalSymbolicStatements(stmt.for_body, module, params, rename, env,
                                    diagnostics, inline_depth)) {
          return false;
        }
        int64_t step_value = 0;
        if (!TryEvalConstExprWithEnv(*stmt.for_step_rhs, params, module, *env,
                                     &step_value, diagnostics, inline_depth)) {
          diagnostics->Add(Severity::kError,
                           "for step must be constant in function body");
          return false;
        }
        SequentialAssign step_assign;
        step_assign.lhs = stmt.for_step_lhs;
        step_assign.rhs = MakeNumberExprSignedWidth(step_value, 32);
        if (!AssignSymbolic(step_assign, module, params, rename, env,
                            diagnostics, inline_depth)) {
          return false;
        }
        if (++iterations > kMaxIterations) {
          diagnostics->Add(Severity::kError,
                           "for loop exceeded iteration limit in function body");
          return false;
        }
      }
      return true;
    }
    case StatementKind::kWhile: {
      int iterations = 0;
      while (true) {
        if (!stmt.while_condition) {
          diagnostics->Add(Severity::kError,
                           "missing while condition in function body");
          return false;
        }
        int64_t cond_value = 0;
        if (!TryEvalConstExprWithEnv(*stmt.while_condition, params, module,
                                     *env, &cond_value, diagnostics,
                                     inline_depth)) {
          diagnostics->Add(Severity::kError,
                           "while condition must be constant in function body");
          return false;
        }
        if (cond_value == 0) {
          break;
        }
        if (!EvalSymbolicStatements(stmt.while_body, module, params, rename,
                                    env, diagnostics, inline_depth)) {
          return false;
        }
        if (++iterations > kMaxIterations) {
          diagnostics->Add(Severity::kError,
                           "while loop exceeded iteration limit in function body");
          return false;
        }
      }
      return true;
    }
    case StatementKind::kRepeat: {
      if (!stmt.repeat_count) {
        diagnostics->Add(Severity::kError,
                         "missing repeat count in function body");
        return false;
      }
      int64_t count = 0;
      if (!TryEvalConstExprWithEnv(*stmt.repeat_count, params, module, *env,
                                   &count, diagnostics, inline_depth)) {
        diagnostics->Add(Severity::kError,
                         "repeat count must be constant in function body");
        return false;
      }
      if (count < 0) {
        count = 0;
      }
      for (int64_t i = 0; i < count; ++i) {
        if (!EvalSymbolicStatements(stmt.repeat_body, module, params, rename,
                                    env, diagnostics, inline_depth)) {
          return false;
        }
      }
      return true;
    }
    case StatementKind::kCase:
    default:
      diagnostics->Add(Severity::kError,
                       "unsupported statement in function body");
      return false;
  }
}

bool EvalSymbolicStatements(const std::vector<Statement>& statements,
                            const Module& module, const ParamBindings& params,
                            const std::function<std::string(const std::string&)>& rename,
                            SymbolicEnv* env, Diagnostics* diagnostics,
                            int inline_depth) {
  for (const auto& stmt : statements) {
    if (!EvalSymbolicStatement(stmt, module, params, rename, env, diagnostics,
                               inline_depth)) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<Expr> InlineFunctionExpr(
    const Function& func, std::vector<std::unique_ptr<Expr>> arg_exprs,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module& module,
    Diagnostics* diagnostics, int inline_depth) {
  const int kMaxInlineDepth = 32;
  if (inline_depth > kMaxInlineDepth) {
    diagnostics->Add(Severity::kError,
                     "function call nesting too deep in '" + func.name + "'");
    return nullptr;
  }
  if (func.args.size() != arg_exprs.size()) {
    diagnostics->Add(Severity::kError,
                     "function '" + func.name + "' expects " +
                         std::to_string(func.args.size()) + " argument(s)");
    return nullptr;
  }
  SymbolicEnv env;
  env.reserve(func.args.size() + func.locals.size() + 1);
  for (size_t i = 0; i < func.args.size(); ++i) {
    SymbolicVar arg;
    arg.width = func.args[i].width;
    arg.is_signed = func.args[i].is_signed;
    arg.is_real = func.args[i].is_real;
    arg.expr = std::move(arg_exprs[i]);
    env.emplace(func.args[i].name, std::move(arg));
  }
  for (const auto& local : func.locals) {
    SymbolicVar var;
    var.width = local.width;
    var.is_signed = local.is_signed;
    var.is_real = local.is_real;
    var.expr =
        local.is_real ? MakeRealLiteralExpr(0.0) : MakeAllXExpr(local.width);
    env.emplace(local.name, std::move(var));
  }
  SymbolicVar ret;
  ret.width = func.width;
  ret.is_signed = func.is_signed;
  ret.is_real = func.is_real;
  ret.expr = func.is_real ? MakeRealLiteralExpr(0.0)
                          : MakeAllXExpr(func.width);
  env.emplace(func.name, std::move(ret));

  if (!EvalSymbolicStatements(func.body, module, params, rename, &env,
                              diagnostics, inline_depth)) {
    return nullptr;
  }
  auto it = env.find(func.name);
  if (it == env.end() || !it->second.expr) {
    return func.is_real ? MakeRealLiteralExpr(0.0)
                        : MakeAllXExpr(func.width);
  }
  return gpga::CloneExpr(*it->second.expr);
}

void UpdateBindingsFromStatement(const Statement& statement,
                                 const Module& flat_module,
                                 ParamBindings* params) {
  if (statement.kind == StatementKind::kAssign ||
      statement.kind == StatementKind::kForce ||
      statement.kind == StatementKind::kRelease) {
    const auto& assign = statement.assign;
    if (params->values.count(assign.lhs) == 0 &&
        params->real_values.count(assign.lhs) == 0 &&
        params->exprs.count(assign.lhs) == 0) {
      return;
    }
    bool lhs_real = SignalIsReal(flat_module, assign.lhs);
    if (assign.nonblocking || assign.lhs_index ||
        !assign.lhs_indices.empty() || assign.lhs_has_range || !assign.rhs) {
      params->values.erase(assign.lhs);
      params->real_values.erase(assign.lhs);
      params->exprs.erase(assign.lhs);
      return;
    }
    if (assign.rhs && ExprHasSystemCall(*assign.rhs)) {
      params->values.erase(assign.lhs);
      params->real_values.erase(assign.lhs);
      params->exprs.erase(assign.lhs);
      return;
    }
    if (lhs_real) {
      Diagnostics scratch;
      double value = 0.0;
      if (EvalConstExprRealValue(*assign.rhs, *params, flat_module, &value,
                                 &scratch)) {
        params->real_values[assign.lhs] = DoubleToBits(value);
        params->exprs[assign.lhs] = MakeRealLiteralExpr(value);
      } else {
        params->real_values.erase(assign.lhs);
        params->exprs.erase(assign.lhs);
      }
      params->values.erase(assign.lhs);
      return;
    }
    int64_t value = 0;
    bool rhs_real = ExprUsesRealConst(*assign.rhs, *params);
    if (rhs_real) {
      Diagnostics scratch;
      double real_value = 0.0;
      if (EvalConstExprRealValue(*assign.rhs, *params, flat_module,
                                 &real_value, &scratch)) {
        value = static_cast<int64_t>(real_value);
        params->values[assign.lhs] = value;
        int width = SignalWidth(flat_module, assign.lhs);
        params->exprs[assign.lhs] = MakeNumberExprSignedWidth(value, width);
      } else {
        params->values.erase(assign.lhs);
        params->exprs.erase(assign.lhs);
      }
    } else if (TryEvalConstExprWithParams(*assign.rhs, *params, &value)) {
      params->values[assign.lhs] = value;
      int width = SignalWidth(flat_module, assign.lhs);
      params->exprs[assign.lhs] = MakeNumberExprSignedWidth(value, width);
    } else {
      params->values.erase(assign.lhs);
      params->exprs.erase(assign.lhs);
    }
    params->real_values.erase(assign.lhs);
    return;
  }
  if (statement.kind == StatementKind::kBlock) {
    for (const auto& inner : statement.block) {
      UpdateBindingsFromStatement(inner, flat_module, params);
    }
    return;
  }
  std::unordered_set<std::string> assigned;
  CollectAssignedSignals(statement, &assigned);
  for (const auto& name : assigned) {
    params->values.erase(name);
    params->real_values.erase(name);
    params->exprs.erase(name);
  }
}

bool CloneStatementList(
    const std::vector<Statement>& statements,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module& source_module,
    const Module& flat_module,
    std::vector<Statement>* out, Diagnostics* diagnostics) {
  ParamBindings current = CloneParamBindings(params);
  for (const auto& stmt : statements) {
    Statement cloned;
    if (!CloneStatement(stmt, rename, current, source_module, flat_module,
                        &cloned, diagnostics)) {
      return false;
    }
    out->push_back(std::move(cloned));
    UpdateBindingsFromStatement(stmt, flat_module, &current);
  }
  return true;
}

bool EvalConstExprValue(const Expr& expr, const ParamBindings& params,
                        int64_t* out_value, Diagnostics* diagnostics,
                        const std::string& context) {
  auto widened = gpga::CloneExpr(expr);
  ForceUnsizedWidth(widened.get(), 32);
  std::string error;
  if (!gpga::EvalConstExpr(*widened, params.values, out_value, &error)) {
    diagnostics->Add(Severity::kError, error + " in " + context);
    return false;
  }
  return true;
}

bool EvalConstExprValueWithFunctions(const Expr& expr,
                                     const ParamBindings& params,
                                     const Module& module, int64_t* out_value,
                                     Diagnostics* diagnostics,
                                     const std::string& context) {
  ConstScope scope;
  std::unordered_set<std::string> call_stack;
  if (!EvalConstExprInScope(expr, module, params, scope, out_value, diagnostics,
                            &call_stack)) {
    diagnostics->Add(Severity::kError, "failed to evaluate " + context);
    return false;
  }
  return true;
}

bool EvalConstExprRealValueWithFunctions(
    const Expr& expr, const ParamBindings& params, const Module& module,
    double* out_value, Diagnostics* diagnostics, const std::string& context) {
  if (!EvalConstExprRealValue(expr, params, module, out_value, diagnostics)) {
    diagnostics->Add(Severity::kError, "failed to evaluate " + context);
    return false;
  }
  return true;
}

bool EvalConstExprWithParams(const Expr& expr, const ParamBindings& params,
                             int64_t* out_value, Diagnostics* diagnostics,
                             const std::string& context) {
  auto resolved = CloneExprWithParams(
      expr, [](const std::string& ident) { return ident; }, params, nullptr,
      diagnostics, nullptr);
  if (!resolved) {
    return false;
  }
  ForceUnsizedWidth(resolved.get(), 32);
  std::string error;
  if (!gpga::EvalConstExpr(*resolved, {}, out_value, &error)) {
    if (context == "repeat count" &&
        error.rfind("unknown parameter '", 0) == 0) {
      return false;
    }
    diagnostics->Add(Severity::kError, error + " in " + context);
    return false;
  }
  return true;
}

bool ContainsAssignToVar(const Statement& statement,
                          const std::string& name) {
  if (statement.kind == StatementKind::kAssign ||
      statement.kind == StatementKind::kForce ||
      statement.kind == StatementKind::kRelease) {
    return statement.assign.lhs == name;
  }
  if (statement.kind == StatementKind::kIf) {
    for (const auto& inner : statement.then_branch) {
      if (ContainsAssignToVar(inner, name)) {
        return true;
      }
    }
    for (const auto& inner : statement.else_branch) {
      if (ContainsAssignToVar(inner, name)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kBlock) {
    for (const auto& inner : statement.block) {
      if (ContainsAssignToVar(inner, name)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kCase) {
    for (const auto& item : statement.case_items) {
      for (const auto& inner : item.body) {
        if (ContainsAssignToVar(inner, name)) {
          return true;
        }
      }
    }
    for (const auto& inner : statement.default_branch) {
      if (ContainsAssignToVar(inner, name)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kFor) {
    for (const auto& inner : statement.for_body) {
      if (ContainsAssignToVar(inner, name)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kWhile) {
    for (const auto& inner : statement.while_body) {
      if (ContainsAssignToVar(inner, name)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kRepeat) {
    for (const auto& inner : statement.repeat_body) {
      if (ContainsAssignToVar(inner, name)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

bool FindLoopVarUpdate(const std::vector<Statement>& body,
                       const std::string& loop_var,
                       const ParamBindings& params, int64_t* next_value,
                       bool* found, Diagnostics* diagnostics) {
  for (const auto& stmt : body) {
    if (stmt.kind == StatementKind::kAssign) {
      if (stmt.assign.lhs != loop_var) {
        continue;
      }
      if (stmt.assign.lhs_index || !stmt.assign.lhs_indices.empty() ||
          stmt.assign.lhs_has_range) {
        diagnostics->Add(Severity::kError,
                         "while-loop step cannot use indexed assignment in v0");
        return false;
      }
      if (!stmt.assign.rhs) {
        diagnostics->Add(Severity::kError,
                         "while-loop step missing rhs in v0");
        return false;
      }
      int64_t value = 0;
      if (!EvalConstExprWithParams(*stmt.assign.rhs, params, &value,
                                   diagnostics, "while-loop step")) {
        return false;
      }
      *next_value = value;
      *found = true;
      continue;
    }
    if (stmt.kind == StatementKind::kIf || stmt.kind == StatementKind::kCase) {
      if (ContainsAssignToVar(stmt, loop_var)) {
        diagnostics->Add(Severity::kError,
                         "while-loop step must be unconditional in v0");
        return false;
      }
      continue;
    }
    if (stmt.kind == StatementKind::kBlock) {
      if (!FindLoopVarUpdate(stmt.block, loop_var, params, next_value, found,
                             diagnostics)) {
        return false;
      }
      continue;
    }
    if (stmt.kind == StatementKind::kFor || stmt.kind == StatementKind::kWhile ||
        stmt.kind == StatementKind::kRepeat) {
      if (ContainsAssignToVar(stmt, loop_var)) {
        diagnostics->Add(Severity::kError,
                         "while-loop step cannot be inside a nested loop in v0");
        return false;
      }
      continue;
    }
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

bool ResolveArrayDims(const Net& net, const ParamBindings& params,
                      std::vector<int>* dims_out, Diagnostics* diagnostics,
                      const std::string& context) {
  dims_out->clear();
  if (net.array_dims.empty()) {
    return true;
  }
  dims_out->reserve(net.array_dims.size());
  for (size_t i = 0; i < net.array_dims.size(); ++i) {
    const auto& dim = net.array_dims[i];
    int size = dim.size;
    if (!ResolveRangeWidth(dim.size, dim.msb_expr, dim.lsb_expr, params, &size,
                           diagnostics,
                           context + " dim[" + std::to_string(i) + "]")) {
      return false;
    }
    if (size <= 0) {
      diagnostics->Add(Severity::kError,
                       "invalid array dimension in " + context);
      return false;
    }
    dims_out->push_back(size);
  }
  return true;
}

bool ResolveRangeBounds(const std::shared_ptr<Expr>& msb_expr,
                        const std::shared_ptr<Expr>& lsb_expr, int width,
                        const ParamBindings& params, const Module& module,
                        int* msb_out, int* lsb_out, Diagnostics* diagnostics,
                        const std::string& context) {
  if (msb_expr && lsb_expr) {
    int64_t msb = 0;
    int64_t lsb = 0;
    if (!EvalConstExprValueWithFunctions(*msb_expr, params, module, &msb,
                                         diagnostics, context + " msb")) {
      return false;
    }
    if (!EvalConstExprValueWithFunctions(*lsb_expr, params, module, &lsb,
                                         diagnostics, context + " lsb")) {
      return false;
    }
    *msb_out = static_cast<int>(msb);
    *lsb_out = static_cast<int>(lsb);
    return true;
  }
  if (width > 0) {
    *msb_out = width - 1;
    *lsb_out = 0;
    return true;
  }
  diagnostics->Add(Severity::kError,
                   "invalid range bounds in " + context);
  return false;
}

bool ResolvePackedBounds(const Module& module, const ParamBindings& params,
                         const std::string& name, int* msb_out, int* lsb_out,
                         Diagnostics* diagnostics, const std::string& context) {
  if (const Net* net = FindNet(module, name)) {
    return ResolveRangeBounds(net->msb_expr, net->lsb_expr, net->width, params,
                              module, msb_out, lsb_out, diagnostics, context);
  }
  if (const Port* port = FindPort(module, name)) {
    return ResolveRangeBounds(port->msb_expr, port->lsb_expr, port->width,
                              params, module, msb_out, lsb_out, diagnostics,
                              context);
  }
  diagnostics->Add(Severity::kError,
                   "unknown signal '" + name + "' in " + context);
  return false;
}

bool ResolveArrayBounds(const Module& module, const ParamBindings& params,
                        const std::string& name, int* msb_out, int* lsb_out,
                        Diagnostics* diagnostics, const std::string& context) {
  const Net* net = FindNet(module, name);
  if (!net || net->array_dims.empty()) {
    return false;
  }
  const ArrayDim& dim = net->array_dims.front();
  std::shared_ptr<Expr> msb_expr = dim.msb_expr;
  std::shared_ptr<Expr> lsb_expr = dim.lsb_expr;
  int size = dim.size;
  if (msb_expr && lsb_expr) {
    return ResolveRangeBounds(msb_expr, lsb_expr, size, params, module, msb_out,
                              lsb_out, diagnostics, context);
  }
  if (size > 0) {
    *msb_out = size - 1;
    *lsb_out = 0;
    return true;
  }
  diagnostics->Add(Severity::kError,
                   "invalid array bounds in " + context);
  return false;
}

bool ResolveArraySize(const Net& net, const ParamBindings& params,
                      int* size_out, Diagnostics* diagnostics,
                      const std::string& context) {
  if (net.array_dims.empty()) {
    *size_out = net.array_size;
    return true;
  }
  std::vector<int> dims;
  if (!ResolveArrayDims(net, params, &dims, diagnostics, context)) {
    return false;
  }
  int64_t total = 1;
  for (int dim : dims) {
    if (dim <= 0 || total > (0x7FFFFFFF / dim)) {
      diagnostics->Add(Severity::kError,
                       "array size overflow in " + context);
      return false;
    }
    total *= dim;
  }
  if (total <= 0 || total > 0x7FFFFFFF) {
    diagnostics->Add(Severity::kError,
                     "array size overflow in " + context);
    return false;
  }
  *size_out = static_cast<int>(total);
  return true;
}

std::unique_ptr<Expr> LowerSystemFunctionCall(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module& module,
    Diagnostics* diagnostics, const BindingMap* bindings, int inline_depth) {
  auto make_u32 = [&](uint64_t value) -> std::unique_ptr<Expr> {
    return MakeNumberExprWidth(value, 32);
  };
  auto make_u64 = [&](uint64_t value) -> std::unique_ptr<Expr> {
    return MakeNumberExprWidth(value, 64);
  };
  auto make_zero = [&]() -> std::unique_ptr<Expr> { return make_u32(0u); };

  std::vector<std::unique_ptr<Expr>> arg_clones;
  arg_clones.reserve(expr.call_args.size());
  for (const auto& arg : expr.call_args) {
    auto cloned = CloneExprWithParamsImpl(*arg, rename, params, &module,
                                          diagnostics, bindings, inline_depth);
    if (!cloned) {
      return nullptr;
    }
    arg_clones.push_back(std::move(cloned));
  }

  auto get_identity_arg = [&](size_t index) -> std::unique_ptr<Expr> {
    if (index >= expr.call_args.size()) {
      return nullptr;
    }
    return CloneExprWithParamsImpl(*expr.call_args[index],
                                   [](const std::string& ident) { return ident; },
                                   params, &module, diagnostics, nullptr,
                                   inline_depth);
  };

  if (expr.ident == "$bits") {
    if (expr.call_args.size() != 1) {
      diagnostics->Add(Severity::kError,
                       "$bits expects 1 argument");
      return nullptr;
    }
    auto resolved = get_identity_arg(0);
    if (!resolved) {
      return nullptr;
    }
    int width = ExprWidth(*resolved, module);
    return make_u32(static_cast<uint64_t>(width));
  }
  if (expr.ident == "$size") {
    if (expr.call_args.size() != 1) {
      diagnostics->Add(Severity::kError,
                       "$size expects 1 argument");
      return nullptr;
    }
    const Expr* arg = expr.call_args[0].get();
    if (arg && arg->kind == ExprKind::kIdentifier) {
      const Net* net = FindNet(module, arg->ident);
      if (net && !net->array_dims.empty()) {
        std::vector<int> dims;
        if (!ResolveArrayDims(*net, params, &dims, diagnostics,
                              "$size")) {
          return nullptr;
        }
        if (!dims.empty()) {
          return make_u32(static_cast<uint64_t>(dims.front()));
        }
      }
    }
    return make_u32(1u);
  }
  if (expr.ident == "$dimensions") {
    if (expr.call_args.size() != 1) {
      diagnostics->Add(Severity::kError,
                       "$dimensions expects 1 argument");
      return nullptr;
    }
    const Expr* arg = expr.call_args[0].get();
    if (arg && arg->kind == ExprKind::kIdentifier) {
      const Net* net = FindNet(module, arg->ident);
      if (net && !net->array_dims.empty()) {
        return make_u32(static_cast<uint64_t>(net->array_dims.size()));
      }
    }
    return make_u32(1u);
  }
  if (expr.ident == "$left" || expr.ident == "$right" ||
      expr.ident == "$low" || expr.ident == "$high") {
    if (expr.call_args.size() != 1) {
      diagnostics->Add(Severity::kError,
                       expr.ident + " expects 1 argument");
      return nullptr;
    }
    const Expr* arg = expr.call_args[0].get();
    int msb = 0;
    int lsb = 0;
    bool ok = false;
    if (arg && arg->kind == ExprKind::kIdentifier) {
      if (ResolveArrayBounds(module, params, arg->ident, &msb, &lsb,
                             diagnostics, expr.ident)) {
        ok = true;
      } else if (ResolvePackedBounds(module, params, arg->ident, &msb, &lsb,
                                     diagnostics, expr.ident)) {
        ok = true;
      }
    }
    if (!ok) {
      return make_u32(0u);
    }
    int low = std::min(msb, lsb);
    int high = std::max(msb, lsb);
    if (expr.ident == "$left") {
      return make_u32(static_cast<uint64_t>(msb));
    }
    if (expr.ident == "$right") {
      return make_u32(static_cast<uint64_t>(lsb));
    }
    if (expr.ident == "$low") {
      return make_u32(static_cast<uint64_t>(low));
    }
    return make_u32(static_cast<uint64_t>(high));
  }
  if (expr.ident == "$random") {
    if (arg_clones.empty()) {
      return make_u32(0u);
    }
    auto mul = MakeBinaryExpr(
        '*', std::move(arg_clones[0]), MakeNumberExprWidth(1103515245u, 32));
    auto add =
        MakeBinaryExpr('+', std::move(mul), MakeNumberExprWidth(12345u, 32));
    return add;
  }
  if (expr.ident == "$urandom") {
    if (arg_clones.empty()) {
      return make_u32(0u);
    }
    auto mul = MakeBinaryExpr(
        '*', std::move(arg_clones[0]), MakeNumberExprWidth(1664525u, 32));
    auto add =
        MakeBinaryExpr('+', std::move(mul), MakeNumberExprWidth(1013904223u, 32));
    return add;
  }
  if (expr.ident == "$urandom_range") {
    if (arg_clones.size() >= 1) {
      return std::move(arg_clones[0]);
    }
    return make_u32(0u);
  }
  if (expr.ident == "$realtime" || expr.ident == "$realtobits" ||
      expr.ident == "$bitstoreal" || expr.ident == "$rtoi" ||
      expr.ident == "$itor") {
    auto call = std::make_unique<Expr>();
    call->kind = ExprKind::kCall;
    call->ident = expr.ident;
    call->call_args = std::move(arg_clones);
    return call;
  }
  if (expr.ident == "$fopen" || expr.ident == "$fgetc" ||
      expr.ident == "$feof" || expr.ident == "$ftell" ||
      expr.ident == "$fgets" || expr.ident == "$fscanf" ||
      expr.ident == "$sscanf") {
    auto call = std::make_unique<Expr>();
    call->kind = ExprKind::kCall;
    call->ident = expr.ident;
    call->call_args = std::move(arg_clones);
    return call;
  }
  if (expr.ident == "$test$plusargs" || expr.ident == "$value$plusargs") {
    return make_u32(0u);
  }
  return make_zero();
}

bool CollectIndexChain(const Expr& expr, std::string* base_name,
                       std::vector<const Expr*>* indices) {
  if (expr.kind == ExprKind::kIndex) {
    if (!expr.base || !expr.index) {
      return false;
    }
    if (!CollectIndexChain(*expr.base, base_name, indices)) {
      return false;
    }
    indices->push_back(expr.index.get());
    return true;
  }
  if (expr.kind == ExprKind::kIdentifier) {
    *base_name = expr.ident;
    return true;
  }
  return false;
}

std::unique_ptr<Expr> BuildFlatIndexExpr(
    const std::vector<int>& dims,
    std::vector<std::unique_ptr<Expr>> indices) {
  if (indices.empty()) {
    return MakeNumberExpr(0);
  }
  std::unique_ptr<Expr> acc = std::move(indices[0]);
  for (size_t i = 1; i < indices.size(); ++i) {
    auto dim_expr = MakeNumberExpr(static_cast<uint64_t>(dims[i]));
    acc = MakeBinaryExpr(
        '+', MakeBinaryExpr('*', std::move(acc), std::move(dim_expr)),
        std::move(indices[i]));
  }
  // Ensure unsized literals don't collapse index math to tiny widths.
  ForceUnsizedWidth(acc.get(), 32);
  return acc;
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
    const ParamBindings& params, const Module* module,
    Diagnostics* diagnostics, const BindingMap* bindings) {
  return CloneExprWithParamsImpl(expr, rename, params, module, diagnostics,
                                 bindings, 0);
}

std::unique_ptr<Expr> CloneExprWithParamsImpl(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module* module,
    Diagnostics* diagnostics, const BindingMap* bindings, int inline_depth) {
  if (expr.kind == ExprKind::kIdentifier) {
    if (bindings) {
      auto it = bindings->find(expr.ident);
      if (it != bindings->end()) {
        return gpga::CloneExpr(*it->second);
      }
    }
    auto it = params.exprs.find(expr.ident);
    if (it != params.exprs.end()) {
      return gpga::CloneExpr(*it->second);
    }
    auto out = std::make_unique<Expr>();
    out->kind = ExprKind::kIdentifier;
    out->ident = rename(expr.ident);
    return out;
  }
  if (expr.kind == ExprKind::kCall) {
    if (expr.ident == "$time") {
      auto out = std::make_unique<Expr>();
      out->kind = ExprKind::kCall;
      out->ident = expr.ident;
      out->call_args.reserve(expr.call_args.size());
      for (const auto& arg : expr.call_args) {
        auto cloned = CloneExprWithParamsImpl(*arg, rename, params, module,
                                              diagnostics, bindings,
                                              inline_depth);
        if (!cloned) {
          return nullptr;
        }
        out->call_args.push_back(std::move(cloned));
      }
      return out;
    }
    if (!expr.ident.empty() && expr.ident.front() == '$') {
      if (!module) {
        if (expr.ident == "$fopen" || expr.ident == "$fgetc" ||
            expr.ident == "$feof" || expr.ident == "$ftell" ||
            expr.ident == "$fgets" || expr.ident == "$fscanf" ||
            expr.ident == "$sscanf") {
          auto out = std::make_unique<Expr>();
          out->kind = ExprKind::kCall;
          out->ident = expr.ident;
          out->call_args.reserve(expr.call_args.size());
          for (const auto& arg : expr.call_args) {
            auto cloned = CloneExprWithParamsImpl(
                *arg, rename, params, module, diagnostics, bindings,
                inline_depth);
            if (!cloned) {
              return nullptr;
            }
            out->call_args.push_back(std::move(cloned));
          }
          return out;
        }
        return MakeNumberExprWidth(0u, 32);
      }
      return LowerSystemFunctionCall(expr, rename, params, *module, diagnostics,
                                     bindings, inline_depth);
    }
    if (!module) {
      diagnostics->Add(Severity::kError,
                       "function call requires module context");
      return nullptr;
    }
    const Function* func = FindFunction(*module, expr.ident);
    if (!func) {
      diagnostics->Add(Severity::kError,
                       "unknown function '" + expr.ident + "'");
      return nullptr;
    }
    if (expr.call_args.size() != func->args.size()) {
      diagnostics->Add(Severity::kError,
                       "function '" + expr.ident + "' expects " +
                           std::to_string(func->args.size()) +
                           " argument(s)");
      return nullptr;
    }
    std::vector<std::unique_ptr<Expr>> arg_clones;
    BindingMap arg_bindings;
    arg_clones.reserve(expr.call_args.size());
    for (size_t i = 0; i < expr.call_args.size(); ++i) {
      auto cloned = CloneExprWithParamsImpl(*expr.call_args[i], rename, params,
                                            module, diagnostics, bindings,
                                            inline_depth);
      if (!cloned) {
        return nullptr;
      }
      const std::string& arg_name = func->args[i].name;
      arg_bindings[arg_name] = cloned.get();
      arg_clones.push_back(std::move(cloned));
    }
    if (func->body_expr) {
      return CloneExprWithParamsImpl(*func->body_expr, rename, params, module,
                                     diagnostics, &arg_bindings,
                                     inline_depth);
    }
    std::vector<int64_t> arg_values;
    bool all_const = true;
    arg_values.reserve(arg_clones.size());
    for (const auto& arg : arg_clones) {
      int64_t value = 0;
      if (!TryEvalConstExprWithParams(*arg, params, &value)) {
        all_const = false;
        break;
      }
      arg_values.push_back(value);
    }
    if (all_const) {
      int64_t result = 0;
      std::unordered_set<std::string> call_stack;
      if (EvalConstFunction(*func, *module, params, arg_values, &result,
                            diagnostics, &call_stack)) {
        if (func->is_signed) {
          return MakeNumberExprSignedWidth(result, func->width);
        }
        return MakeNumberExprWidth(static_cast<uint64_t>(result), func->width);
      }
      return nullptr;
    }
    auto inlined = InlineFunctionExpr(*func, std::move(arg_clones), rename,
                                      params, *module, diagnostics,
                                      inline_depth + 1);
    if (!inlined) {
      return nullptr;
    }
    return inlined;
  }

  auto out = std::make_unique<Expr>();
  out->kind = expr.kind;
  out->ident = expr.ident;
  out->string_value = expr.string_value;
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

  if (expr.kind == ExprKind::kNumber || expr.kind == ExprKind::kString) {
    return out;
  }
  if (expr.kind == ExprKind::kUnary) {
    out->operand = CloneExprWithParamsImpl(*expr.operand, rename, params,
                                           module, diagnostics, bindings,
                                           inline_depth);
    if (!out->operand) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kBinary) {
    out->lhs = CloneExprWithParamsImpl(*expr.lhs, rename, params, module,
                                       diagnostics, bindings, inline_depth);
    out->rhs = CloneExprWithParamsImpl(*expr.rhs, rename, params, module,
                                       diagnostics, bindings, inline_depth);
    if (!out->lhs || !out->rhs) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kTernary) {
    out->condition =
        CloneExprWithParamsImpl(*expr.condition, rename, params, module,
                                diagnostics, bindings, inline_depth);
    out->then_expr =
        CloneExprWithParamsImpl(*expr.then_expr, rename, params, module,
                                diagnostics, bindings, inline_depth);
    out->else_expr =
        CloneExprWithParamsImpl(*expr.else_expr, rename, params, module,
                                diagnostics, bindings, inline_depth);
    if (!out->condition || !out->then_expr || !out->else_expr) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kSelect) {
    out->base = CloneExprWithParamsImpl(*expr.base, rename, params, module,
                                        diagnostics, bindings, inline_depth);
    if (!out->base) {
      return nullptr;
    }
    out->has_range = expr.has_range;
    out->indexed_range = expr.indexed_range;
    out->indexed_desc = expr.indexed_desc;
    out->indexed_width = expr.indexed_width;
    if (expr.msb_expr) {
      out->msb_expr = CloneExprWithParamsImpl(*expr.msb_expr, rename, params,
                                              module, diagnostics, bindings,
                                              inline_depth);
      if (!out->msb_expr) {
        return nullptr;
      }
    }
    if (expr.lsb_expr) {
      out->lsb_expr = CloneExprWithParamsImpl(*expr.lsb_expr, rename, params,
                                              module, diagnostics, bindings,
                                              inline_depth);
      if (!out->lsb_expr) {
        return nullptr;
      }
    }
    if (!expr.indexed_range) {
      int msb = 0;
      int lsb = 0;
      if (!ResolveSelectIndices(expr, params, &msb, &lsb, diagnostics,
                                "select")) {
        return nullptr;
      }
      out->msb = msb;
      out->lsb = lsb;
    } else if (out->msb_expr && out->lsb_expr) {
      int64_t msb = 0;
      int64_t lsb = 0;
      if (TryEvalConstExprWithParams(*out->msb_expr, params, &msb) &&
          TryEvalConstExprWithParams(*out->lsb_expr, params, &lsb)) {
        out->msb = static_cast<int>(msb);
        out->lsb = static_cast<int>(lsb);
      }
    }
    return out;
  }
  if (expr.kind == ExprKind::kIndex) {
    if (module) {
      std::string base_name;
      std::vector<const Expr*> indices;
      if (CollectIndexChain(expr, &base_name, &indices)) {
        const Net* net = FindNet(*module, base_name);
        if (net && !net->array_dims.empty()) {
          std::vector<int> dims;
          if (!ResolveArrayDims(*net, params, &dims, diagnostics,
                                "array '" + base_name + "'")) {
            return nullptr;
          }
          if (indices.size() == dims.size()) {
            std::vector<std::unique_ptr<Expr>> cloned_indices;
            cloned_indices.reserve(indices.size());
            for (const auto* index_expr : indices) {
              auto cloned =
                  CloneExprWithParamsImpl(*index_expr, rename, params, module,
                                          diagnostics, bindings, inline_depth);
              if (!cloned) {
                return nullptr;
              }
              cloned_indices.push_back(std::move(cloned));
            }
            auto flat_index =
                BuildFlatIndexExpr(dims, std::move(cloned_indices));
            auto base_ident = std::make_unique<Expr>();
            base_ident->kind = ExprKind::kIdentifier;
            base_ident->ident = rename(base_name);
            out->base = std::move(base_ident);
            out->index = std::move(flat_index);
            return out;
          }
          if (indices.size() == dims.size() + 1) {
            std::vector<std::unique_ptr<Expr>> cloned_indices;
            cloned_indices.reserve(dims.size());
            for (size_t i = 0; i < dims.size(); ++i) {
              auto cloned =
                  CloneExprWithParamsImpl(*indices[i], rename, params, module,
                                          diagnostics, bindings, inline_depth);
              if (!cloned) {
                return nullptr;
              }
              cloned_indices.push_back(std::move(cloned));
            }
            auto flat_index =
                BuildFlatIndexExpr(dims, std::move(cloned_indices));
            auto base_ident = std::make_unique<Expr>();
            base_ident->kind = ExprKind::kIdentifier;
            base_ident->ident = rename(base_name);
            auto array_index = std::make_unique<Expr>();
            array_index->kind = ExprKind::kIndex;
            array_index->base = std::move(base_ident);
            array_index->index = std::move(flat_index);
            auto bit_index =
                CloneExprWithParamsImpl(*indices.back(), rename, params, module,
                                        diagnostics, bindings, inline_depth);
            if (!bit_index) {
              return nullptr;
            }
            out->base = std::move(array_index);
            out->index = std::move(bit_index);
            return out;
          }
          {
            diagnostics->Add(
                Severity::kError,
                "array '" + base_name +
                    "' requires " + std::to_string(dims.size()) +
                    " index(es) in v0");
            return nullptr;
          }
        }
      }
    }
    out->base = CloneExprWithParamsImpl(*expr.base, rename, params, module,
                                        diagnostics, bindings, inline_depth);
    out->index = CloneExprWithParamsImpl(*expr.index, rename, params, module,
                                         diagnostics, bindings, inline_depth);
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
          CloneExprWithParamsImpl(*element, rename, params, module, diagnostics,
                                  bindings, inline_depth);
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
    const ParamBindings& params, const Module& source_module,
    const Module& flat_module, Statement* out, Diagnostics* diagnostics) {
  out->kind = statement.kind;
  out->block_label = statement.block_label;
  if (statement.kind == StatementKind::kAssign ||
      statement.kind == StatementKind::kForce ||
      statement.kind == StatementKind::kRelease) {
    out->assign.lhs = rename(statement.assign.lhs);
    out->assign.lhs_has_range = statement.assign.lhs_has_range;
    out->assign.lhs_indexed_range = statement.assign.lhs_indexed_range;
    out->assign.lhs_indexed_desc = statement.assign.lhs_indexed_desc;
    out->assign.lhs_indexed_width = statement.assign.lhs_indexed_width;
    out->assign.lhs_msb = statement.assign.lhs_msb;
    out->assign.lhs_lsb = statement.assign.lhs_lsb;
    if (!statement.assign.lhs_indices.empty()) {
      const Net* net = FindNet(source_module, statement.assign.lhs);
      if (!net || net->array_dims.empty()) {
        diagnostics->Add(Severity::kError,
                         "indexed assignment target is not an array");
        return false;
      }
      std::vector<int> dims;
      if (!ResolveArrayDims(*net, params, &dims, diagnostics,
                            "array '" + statement.assign.lhs + "'")) {
        return false;
      }
      const size_t dims_count = dims.size();
      const size_t index_count = statement.assign.lhs_indices.size();
      if (index_count != dims_count && index_count != dims_count + 1) {
        diagnostics->Add(Severity::kError,
                         "array '" + statement.assign.lhs + "' requires " +
                             std::to_string(dims.size()) +
                             " index(es) in v0");
        return false;
      }
      std::vector<std::unique_ptr<Expr>> cloned_indices;
      cloned_indices.reserve(dims_count);
      for (size_t i = 0; i < dims_count; ++i) {
        const auto& index_expr = statement.assign.lhs_indices[i];
        auto cloned = CloneExprWithParams(*index_expr, rename, params,
                                          &source_module, diagnostics, nullptr);
        if (!cloned) {
          return false;
        }
        cloned_indices.push_back(std::move(cloned));
      }
      out->assign.lhs_index =
          SimplifyExpr(BuildFlatIndexExpr(dims, std::move(cloned_indices)),
                       flat_module);
      if (index_count == dims_count + 1) {
        const auto& bit_expr = statement.assign.lhs_indices.back();
        auto cloned_bit = CloneExprWithParams(*bit_expr, rename, params,
                                              &source_module, diagnostics,
                                              nullptr);
        if (!cloned_bit) {
          return false;
        }
        cloned_bit = SimplifyExpr(std::move(cloned_bit), flat_module);
        out->assign.lhs_has_range = true;
        out->assign.lhs_msb_expr = std::move(cloned_bit);
        int64_t bit_value = 0;
        if (out->assign.lhs_msb_expr &&
            TryEvalConstExprWithParams(*out->assign.lhs_msb_expr, params,
                                       &bit_value)) {
          out->assign.lhs_msb = static_cast<int>(bit_value);
          out->assign.lhs_lsb = static_cast<int>(bit_value);
        }
      }
    } else if (statement.assign.lhs_index) {
      out->assign.lhs_index =
          CloneExprWithParams(*statement.assign.lhs_index, rename, params,
                              &source_module, diagnostics, nullptr);
      if (!out->assign.lhs_index) {
        return false;
      }
      out->assign.lhs_index =
          SimplifyExpr(std::move(out->assign.lhs_index), flat_module);
    }
    if (statement.assign.lhs_msb_expr) {
      out->assign.lhs_msb_expr =
          CloneExprWithParams(*statement.assign.lhs_msb_expr, rename, params,
                              &source_module, diagnostics, nullptr);
      if (!out->assign.lhs_msb_expr) {
        return false;
      }
      out->assign.lhs_msb_expr =
          SimplifyExpr(std::move(out->assign.lhs_msb_expr), flat_module);
    }
    if (statement.assign.lhs_lsb_expr) {
      out->assign.lhs_lsb_expr =
          CloneExprWithParams(*statement.assign.lhs_lsb_expr, rename, params,
                              &source_module, diagnostics, nullptr);
      if (!out->assign.lhs_lsb_expr) {
        return false;
      }
      out->assign.lhs_lsb_expr =
          SimplifyExpr(std::move(out->assign.lhs_lsb_expr), flat_module);
    }
    if (out->assign.lhs_has_range && !out->assign.lhs_indexed_range) {
      int64_t msb = 0;
      int64_t lsb = 0;
      if (!out->assign.lhs_msb_expr) {
        diagnostics->Add(Severity::kError,
                         "part-select assignment indices must be constant in v0");
        return false;
      }
      if (!out->assign.lhs_lsb_expr) {
        if (TryEvalConstExprWithParams(*out->assign.lhs_msb_expr, params,
                                       &msb)) {
          out->assign.lhs_msb = static_cast<int>(msb);
          out->assign.lhs_lsb = static_cast<int>(msb);
        }
      } else {
        if (!TryEvalConstExprWithParams(*out->assign.lhs_msb_expr, params,
                                        &msb) ||
            !TryEvalConstExprWithParams(*out->assign.lhs_lsb_expr, params,
                                        &lsb)) {
          diagnostics->Add(Severity::kError,
                           "part-select assignment indices must be constant in v0");
          return false;
        }
        out->assign.lhs_msb = static_cast<int>(msb);
        out->assign.lhs_lsb = static_cast<int>(lsb);
      }
    }
    if (statement.assign.rhs) {
      out->assign.rhs =
          CloneExprWithParams(*statement.assign.rhs, rename, params,
                              &source_module, diagnostics, nullptr);
      if (!out->assign.rhs) {
        return false;
      }
      out->assign.rhs = SimplifyExpr(std::move(out->assign.rhs), flat_module);
    } else {
      out->assign.rhs = nullptr;
    }
    if (statement.assign.delay) {
      out->assign.delay =
          CloneExprWithParams(*statement.assign.delay, rename, params,
                              &source_module, diagnostics, nullptr);
      if (!out->assign.delay) {
        return false;
      }
      out->assign.delay =
          SimplifyExpr(std::move(out->assign.delay), flat_module);
    } else {
      out->assign.delay = nullptr;
    }
    out->assign.nonblocking = statement.assign.nonblocking;
    if (statement.kind == StatementKind::kForce) {
      out->force_target = rename(statement.force_target);
    }
    if (statement.kind == StatementKind::kRelease) {
      out->release_target = rename(statement.release_target);
    }
    return true;
  }
  if (statement.kind == StatementKind::kIf) {
    if (statement.condition) {
      out->condition =
          CloneExprWithParams(*statement.condition, rename, params,
                              &source_module, diagnostics, nullptr);
      if (!out->condition) {
        return false;
      }
    }
    for (const auto& stmt : statement.then_branch) {
      Statement cloned;
      if (!CloneStatement(stmt, rename, params, source_module, flat_module,
                          &cloned, diagnostics)) {
        return false;
      }
      out->then_branch.push_back(std::move(cloned));
    }
    for (const auto& stmt : statement.else_branch) {
      Statement cloned;
      if (!CloneStatement(stmt, rename, params, source_module, flat_module,
                          &cloned, diagnostics)) {
        return false;
      }
      out->else_branch.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kBlock) {
    return CloneStatementList(statement.block, rename, params, source_module,
                              flat_module, &out->block, diagnostics);
  }
  if (statement.kind == StatementKind::kCase) {
    out->case_kind = statement.case_kind;
    out->case_expr =
        CloneExprWithParams(*statement.case_expr, rename, params,
                            &source_module, diagnostics, nullptr);
    if (!out->case_expr) {
      return false;
    }
    for (const auto& item : statement.case_items) {
      CaseItem cloned_item;
      for (const auto& label : item.labels) {
        auto cloned_label =
            CloneExprWithParams(*label, rename, params, &source_module,
                                diagnostics, nullptr);
        if (!cloned_label) {
          return false;
        }
        cloned_item.labels.push_back(std::move(cloned_label));
      }
      for (const auto& stmt : item.body) {
        Statement cloned_stmt;
        if (!CloneStatement(stmt, rename, params, source_module, flat_module,
                            &cloned_stmt, diagnostics)) {
          return false;
        }
        cloned_item.body.push_back(std::move(cloned_stmt));
      }
      out->case_items.push_back(std::move(cloned_item));
    }
    for (const auto& stmt : statement.default_branch) {
      Statement cloned_stmt;
      if (!CloneStatement(stmt, rename, params, source_module, flat_module,
                          &cloned_stmt, diagnostics)) {
        return false;
      }
      out->default_branch.push_back(std::move(cloned_stmt));
    }
    return true;
  }
  if (statement.kind == StatementKind::kFor) {
    out->kind = StatementKind::kBlock;
    if (!statement.for_init_rhs || !statement.for_condition ||
        !statement.for_step_rhs) {
      diagnostics->Add(Severity::kError, "malformed for-loop in v0");
      return false;
    }
    if (statement.for_step_lhs != statement.for_init_lhs) {
      diagnostics->Add(Severity::kError,
                       "for-loop step must update loop variable in v0");
      return false;
    }
    int64_t init_value = 0;
    if (!EvalConstExprWithParams(*statement.for_init_rhs, params, &init_value,
                                 diagnostics, "for-loop init")) {
      return false;
    }
    int64_t current = init_value;
    int iterations = 0;
    const int kMaxIterations = 100000;
    while (iterations++ < kMaxIterations) {
      ParamBindings iter_params;
      iter_params.values = params.values;
      iter_params.exprs.reserve(params.exprs.size());
      for (const auto& entry : params.exprs) {
        iter_params.exprs[entry.first] = gpga::CloneExpr(*entry.second);
      }
      iter_params.values[statement.for_init_lhs] = current;
      iter_params.exprs[statement.for_init_lhs] =
          MakeNumberExprSignedWidth(current, 32);
      int64_t cond_value = 0;
      if (!EvalConstExprWithParams(*statement.for_condition, iter_params,
                                   &cond_value, diagnostics,
                                   "for-loop condition")) {
        return false;
      }
      if (cond_value == 0) {
        return true;
      }
      for (const auto& body_stmt : statement.for_body) {
        Statement cloned;
        if (!CloneStatement(body_stmt, rename, iter_params, source_module,
                            flat_module, &cloned, diagnostics)) {
          return false;
        }
        out->block.push_back(std::move(cloned));
      }
      int64_t step_value = 0;
      if (!EvalConstExprWithParams(*statement.for_step_rhs, iter_params,
                                   &step_value, diagnostics,
                                   "for-loop step")) {
        return false;
      }
      current = step_value;
    }
    diagnostics->Add(Severity::kError, "for-loop exceeds iteration limit");
    return false;
  }
  if (statement.kind == StatementKind::kWhile) {
    bool has_system_call = false;
    if (statement.while_condition &&
        ExprHasSystemCall(*statement.while_condition)) {
      has_system_call = true;
    } else {
      for (const auto& stmt : statement.while_body) {
        if (StatementHasSystemCall(stmt)) {
          has_system_call = true;
          break;
        }
      }
    }
    if (has_system_call) {
      if (statement.while_condition) {
        out->while_condition =
            CloneExprWithParams(*statement.while_condition, rename, params,
                                &source_module, diagnostics, nullptr);
        if (!out->while_condition) {
          return false;
        }
      }
      for (const auto& body_stmt : statement.while_body) {
        Statement cloned;
        if (!CloneStatement(body_stmt, rename, params, source_module,
                            flat_module, &cloned, diagnostics)) {
          return false;
        }
        out->while_body.push_back(std::move(cloned));
      }
      return true;
    }
    out->kind = StatementKind::kBlock;
    if (!statement.while_condition) {
      diagnostics->Add(Severity::kError, "malformed while-loop in v0");
      return false;
    }
    std::unordered_set<std::string> cond_idents;
    CollectIdentifiers(*statement.while_condition, &cond_idents);
    if (cond_idents.empty()) {
      int64_t cond_value = 0;
      if (!EvalConstExprWithParams(*statement.while_condition, params,
                                   &cond_value, diagnostics,
                                   "while-loop condition")) {
        return false;
      }
      if (cond_value == 0) {
        return true;
      }
      diagnostics->Add(Severity::kError,
                       "while-loop condition is constant true in v0");
      return false;
    }
    std::unordered_map<std::string, int64_t> current_values;
    for (const auto& ident : cond_idents) {
      auto it = params.values.find(ident);
      if (it == params.values.end()) {
        diagnostics->Add(Severity::kWarning,
                         "assuming 0 for while-loop variable '" + ident + "'");
        current_values[ident] = 0;
      } else {
        current_values[ident] = it->second;
      }
    }
    int iterations = 0;
    const int kMaxIterations = 100000;
    while (iterations++ < kMaxIterations) {
      ParamBindings iter_params = CloneParamBindings(params);
      for (const auto& entry : current_values) {
        int width = SignalWidth(flat_module, entry.first);
        iter_params.values[entry.first] = entry.second;
        iter_params.exprs[entry.first] =
            MakeNumberExprSignedWidth(entry.second, width);
      }
      int64_t cond_value = 0;
      if (!EvalConstExprWithParams(*statement.while_condition, iter_params,
                                   &cond_value, diagnostics,
                                   "while-loop condition")) {
        return false;
      }
      if (cond_value == 0) {
        return true;
      }
      ParamBindings body_params = CloneParamBindings(iter_params);
      for (const auto& body_stmt : statement.while_body) {
        Statement cloned;
        if (!CloneStatement(body_stmt, rename, body_params, source_module,
                            flat_module, &cloned, diagnostics)) {
          return false;
        }
        out->block.push_back(std::move(cloned));
        UpdateBindingsFromStatement(body_stmt, flat_module, &body_params);
      }
      bool any_update = false;
      std::unordered_map<std::string, int64_t> next_values;
      for (const auto& entry : current_values) {
        auto it = body_params.values.find(entry.first);
        if (it == body_params.values.end()) {
          diagnostics->Add(Severity::kError,
                           "while-loop variable '" + entry.first +
                               "' is not constant in v0");
          return false;
        }
        next_values[entry.first] = it->second;
        if (it->second != entry.second) {
          any_update = true;
        }
      }
      if (!any_update) {
        diagnostics->Add(Severity::kError,
                         "while-loop does not update condition variables in v0");
        return false;
      }
      current_values = std::move(next_values);
    }
    diagnostics->Add(Severity::kError, "while-loop exceeds iteration limit");
    return false;
  }
  if (statement.kind == StatementKind::kRepeat) {
    if (!statement.repeat_count) {
      diagnostics->Add(Severity::kError, "malformed repeat in v0");
      return false;
    }
    int64_t count = 0;
    if (TryEvalConstExprWithParams(*statement.repeat_count, params, &count)) {
      out->kind = StatementKind::kBlock;
      if (count < 0) {
        diagnostics->Add(Severity::kError, "repeat count must be >= 0");
        return false;
      }
      const int64_t kMaxIterations = 100000;
      if (count > kMaxIterations) {
        diagnostics->Add(Severity::kError, "repeat exceeds iteration limit");
        return false;
      }
      for (int64_t i = 0; i < count; ++i) {
        for (const auto& body_stmt : statement.repeat_body) {
          Statement cloned;
          if (!CloneStatement(body_stmt, rename, params, source_module,
                              flat_module, &cloned, diagnostics)) {
            return false;
          }
          out->block.push_back(std::move(cloned));
        }
      }
      return true;
    }
    out->kind = StatementKind::kRepeat;
    out->repeat_count =
        CloneExprWithParams(*statement.repeat_count, rename, params,
                            &source_module, diagnostics, nullptr);
    if (!out->repeat_count) {
      return false;
    }
    out->repeat_count = SimplifyExpr(std::move(out->repeat_count), flat_module);
    for (const auto& body_stmt : statement.repeat_body) {
      Statement cloned;
      if (!CloneStatement(body_stmt, rename, params, source_module,
                          flat_module, &cloned, diagnostics)) {
        return false;
      }
      out->repeat_body.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kDelay) {
    out->kind = StatementKind::kDelay;
    if (statement.delay) {
      out->delay =
          CloneExprWithParams(*statement.delay, rename, params, &source_module,
                              diagnostics, nullptr);
      if (!out->delay) {
        return false;
      }
      out->delay = SimplifyExpr(std::move(out->delay), flat_module);
    }
    for (const auto& body_stmt : statement.delay_body) {
      Statement cloned;
      if (!CloneStatement(body_stmt, rename, params, source_module,
                          flat_module, &cloned, diagnostics)) {
        return false;
      }
      out->delay_body.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kEventControl) {
    out->kind = StatementKind::kEventControl;
    out->event_edge = statement.event_edge;
    for (const auto& item : statement.event_items) {
      EventItem cloned_item;
      cloned_item.edge = item.edge;
      if (item.expr) {
        cloned_item.expr =
            CloneExprWithParams(*item.expr, rename, params, &source_module,
                                diagnostics, nullptr);
        if (!cloned_item.expr) {
          return false;
        }
        cloned_item.expr =
            SimplifyExpr(std::move(cloned_item.expr), flat_module);
      }
      out->event_items.push_back(std::move(cloned_item));
    }
    if (statement.event_expr) {
      out->event_expr =
          CloneExprWithParams(*statement.event_expr, rename, params,
                              &source_module, diagnostics, nullptr);
      if (!out->event_expr) {
        return false;
      }
      out->event_expr =
          SimplifyExpr(std::move(out->event_expr), flat_module);
    }
    for (const auto& body_stmt : statement.event_body) {
      Statement cloned;
      if (!CloneStatement(body_stmt, rename, params, source_module,
                          flat_module, &cloned, diagnostics)) {
        return false;
      }
      out->event_body.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kEventTrigger) {
    out->kind = StatementKind::kEventTrigger;
    out->trigger_target = rename(statement.trigger_target);
    return true;
  }
  if (statement.kind == StatementKind::kWait) {
    out->kind = StatementKind::kWait;
    if (statement.wait_condition) {
      out->wait_condition =
          CloneExprWithParams(*statement.wait_condition, rename, params,
                              &source_module, diagnostics, nullptr);
      if (!out->wait_condition) {
        return false;
      }
      out->wait_condition =
          SimplifyExpr(std::move(out->wait_condition), flat_module);
    }
    for (const auto& body_stmt : statement.wait_body) {
      Statement cloned;
      if (!CloneStatement(body_stmt, rename, params, source_module,
                          flat_module, &cloned, diagnostics)) {
        return false;
      }
      out->wait_body.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kForever) {
    out->kind = StatementKind::kForever;
    for (const auto& body_stmt : statement.forever_body) {
      Statement cloned;
      if (!CloneStatement(body_stmt, rename, params, source_module,
                          flat_module, &cloned, diagnostics)) {
        return false;
      }
      out->forever_body.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kFork) {
    out->kind = StatementKind::kFork;
    for (const auto& body_stmt : statement.fork_branches) {
      Statement cloned;
      if (!CloneStatement(body_stmt, rename, params, source_module,
                          flat_module, &cloned, diagnostics)) {
        return false;
      }
      out->fork_branches.push_back(std::move(cloned));
    }
    return true;
  }
  if (statement.kind == StatementKind::kDisable) {
    out->kind = StatementKind::kDisable;
    out->disable_target = rename(statement.disable_target);
    return true;
  }
  if (statement.kind == StatementKind::kTaskCall) {
    out->kind = StatementKind::kTaskCall;
    out->task_name = statement.task_name;
    if (g_task_renames) {
      auto it = g_task_renames->find(statement.task_name);
      if (it != g_task_renames->end()) {
        out->task_name = it->second;
      }
    }
    for (const auto& arg : statement.task_args) {
      auto cloned_arg =
          CloneExprWithParams(*arg, rename, params, &source_module,
                              diagnostics, nullptr);
      if (!cloned_arg) {
        return false;
      }
      out->task_args.push_back(std::move(cloned_arg));
    }
    return true;
  }
  return true;
}

bool AddFlatNet(const std::string& name, int width, bool is_signed,
                NetType type, ChargeStrength charge,
                const std::vector<int>& array_dims, bool is_real,
                const std::string& hier_path, Module* out,
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
  net.is_real = is_real;
  net.charge = charge;
  int total = 0;
  if (!array_dims.empty()) {
    int64_t product = 1;
    for (int dim : array_dims) {
      if (dim <= 0 || product > (0x7FFFFFFF / dim)) {
        diagnostics->Add(Severity::kError,
                         "array size overflow in '" + name + "'");
        return false;
      }
      product *= dim;
    }
    total = static_cast<int>(product);
    for (int dim : array_dims) {
      net.array_dims.push_back(ArrayDim{dim, nullptr, nullptr});
    }
  }
  net.array_size = total;
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

std::unique_ptr<Expr> MakeNumberExprWidth(uint64_t value, int width) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kNumber;
  expr->number = value;
  expr->value_bits = value;
  expr->has_width = true;
  expr->number_width = width;
  if (width >= 0 && width < 64) {
    uint64_t mask = MaskForWidth64(width);
    expr->number &= mask;
    expr->value_bits &= mask;
  }
  return expr;
}

std::unique_ptr<Expr> MakeNumberExprSignedWidth(int64_t value, int width) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kNumber;
  expr->is_signed = true;
  expr->has_width = true;
  expr->number_width = width;
  uint64_t bits = static_cast<uint64_t>(value);
  if (width < 64) {
    uint64_t mask = (width <= 0) ? 0ull : ((1ull << width) - 1ull);
    bits &= mask;
  }
  expr->number = bits;
  expr->value_bits = bits;
  return expr;
}

double BitsToDouble(uint64_t bits) {
  double value = 0.0;
  static_assert(sizeof(bits) == sizeof(value), "double size mismatch");
  std::memcpy(&value, &bits, sizeof(bits));
  return value;
}

uint64_t DoubleToBits(double value) {
  uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "double size mismatch");
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::unique_ptr<Expr> MakeRealLiteralExpr(double value) {
  uint64_t bits = DoubleToBits(value);
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kNumber;
  expr->number = bits;
  expr->value_bits = bits;
  expr->has_width = true;
  expr->number_width = 64;
  expr->is_real_literal = true;
  return expr;
}

std::unique_ptr<Expr> MakeIdentifierExpr(const std::string& name) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kIdentifier;
  expr->ident = name;
  return expr;
}

std::unique_ptr<Expr> MakeUnaryExpr(char op, std::unique_ptr<Expr> operand) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kUnary;
  expr->unary_op = op;
  expr->operand = std::move(operand);
  return expr;
}

std::unique_ptr<Expr> MakeBinaryExpr(char op, std::unique_ptr<Expr> lhs,
                                     std::unique_ptr<Expr> rhs) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kBinary;
  expr->op = op;
  expr->lhs = std::move(lhs);
  expr->rhs = std::move(rhs);
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

bool SignalIsReal(const Module& module, const std::string& name) {
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return port.is_real;
    }
  }
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return net.is_real;
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

int ExprWidth(const Expr& expr, const Module& module) {
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
          expr.unary_op == '|' || expr.unary_op == '^' ||
          expr.unary_op == 'B') {
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
    case ExprKind::kString:
      return false;
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
    if (expr->has_range && !expr->indexed_range) {
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
    double real_value = 0.0;
    if (param.is_real) {
      if (eval_params == &bindings) {
        if (!EvalConstExprRealValueWithFunctions(
                *expr, bindings, module, &real_value, diagnostics,
                "parameter '" + param.name + "'")) {
          return false;
        }
      } else {
        if (!EvalConstExprRealValueWithFunctions(
                *expr, *eval_params, module, &real_value, diagnostics,
                "parameter '" + param.name + "'")) {
          return false;
        }
      }
      bindings.real_values[param.name] = DoubleToBits(real_value);
    } else {
      int64_t value = 0;
      if (eval_params == &bindings) {
        if (!EvalConstExprValueWithFunctions(
                *expr, bindings, module, &value, diagnostics,
                "parameter '" + param.name + "'")) {
          return false;
        }
      } else {
        if (!EvalConstExprValueWithFunctions(
                *expr, *eval_params, module, &value, diagnostics,
                "parameter '" + param.name + "'")) {
          return false;
        }
      }
      bindings.values[param.name] = value;
    }
    const ParamBindings* expr_params =
        eval_params == &bindings ? &bindings : eval_params;
    auto resolved = CloneExprWithParams(
        *expr, [](const std::string& ident) { return ident; }, *expr_params,
        &module, diagnostics, nullptr);
    if (!resolved) {
      return false;
    }
    ConstScope empty_scope;
    std::unordered_set<std::string> call_stack;
    if (param.is_real) {
      resolved = MakeRealLiteralExpr(real_value);
    } else {
      if (!ResolveConstFunctionCalls(resolved.get(), module, *expr_params,
                                     empty_scope, diagnostics, &call_stack)) {
        return false;
      }
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
    case StatementKind::kFor:
      for (const auto& stmt : statement.for_body) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kWhile:
      for (const auto& stmt : statement.while_body) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kRepeat:
      for (const auto& stmt : statement.repeat_body) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kDelay:
      for (const auto& stmt : statement.delay_body) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kEventControl:
      for (const auto& stmt : statement.event_body) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kWait:
      for (const auto& stmt : statement.wait_body) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kForever:
      for (const auto& stmt : statement.forever_body) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kFork:
      for (const auto& stmt : statement.fork_branches) {
        CollectAssignedSignals(stmt, out);
      }
      break;
    case StatementKind::kDisable:
    case StatementKind::kTaskCall:
    case StatementKind::kEventTrigger:
    case StatementKind::kForce:
    case StatementKind::kRelease:
      break;
  }
}

bool ExprHasUnsupportedCall(const Expr& expr, std::string* name_out) {
  if (expr.kind == ExprKind::kCall) {
    if (expr.ident != "$time" && expr.ident != "$realtime" &&
        expr.ident != "$realtobits" && expr.ident != "$bitstoreal" &&
        expr.ident != "$rtoi" && expr.ident != "$itor" &&
        expr.ident != "$fopen" && expr.ident != "$fclose" &&
        expr.ident != "$fgetc" && expr.ident != "$fgets" &&
        expr.ident != "$feof" && expr.ident != "$ftell" &&
        expr.ident != "$fscanf" && expr.ident != "$sscanf") {
      if (name_out) {
        *name_out = expr.ident;
      }
      return true;
    }
    return false;
  }
  switch (expr.kind) {
    case ExprKind::kUnary:
      return expr.operand && ExprHasUnsupportedCall(*expr.operand, name_out);
    case ExprKind::kBinary:
      return (expr.lhs && ExprHasUnsupportedCall(*expr.lhs, name_out)) ||
             (expr.rhs && ExprHasUnsupportedCall(*expr.rhs, name_out));
    case ExprKind::kTernary:
      return (expr.condition &&
              ExprHasUnsupportedCall(*expr.condition, name_out)) ||
             (expr.then_expr &&
              ExprHasUnsupportedCall(*expr.then_expr, name_out)) ||
             (expr.else_expr &&
              ExprHasUnsupportedCall(*expr.else_expr, name_out));
    case ExprKind::kSelect:
      return (expr.base && ExprHasUnsupportedCall(*expr.base, name_out)) ||
             (expr.msb_expr &&
              ExprHasUnsupportedCall(*expr.msb_expr, name_out)) ||
             (expr.lsb_expr &&
              ExprHasUnsupportedCall(*expr.lsb_expr, name_out));
    case ExprKind::kIndex:
      return (expr.base && ExprHasUnsupportedCall(*expr.base, name_out)) ||
             (expr.index && ExprHasUnsupportedCall(*expr.index, name_out));
    case ExprKind::kConcat:
      if (expr.repeat_expr &&
          ExprHasUnsupportedCall(*expr.repeat_expr, name_out)) {
        return true;
      }
      for (const auto& element : expr.elements) {
        if (element && ExprHasUnsupportedCall(*element, name_out)) {
          return true;
        }
      }
      return false;
    case ExprKind::kIdentifier:
    case ExprKind::kNumber:
    case ExprKind::kString:
      return false;
    case ExprKind::kCall:
      break;
  }
  return false;
}

bool ExprHasSystemCall(const Expr& expr) {
  if (expr.kind == ExprKind::kCall) {
    return !expr.ident.empty() && expr.ident.front() == '$' &&
           expr.ident != "$time";
  }
  switch (expr.kind) {
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
    case ExprKind::kConcat:
      if (expr.repeat_expr && ExprHasSystemCall(*expr.repeat_expr)) {
        return true;
      }
      for (const auto& element : expr.elements) {
        if (element && ExprHasSystemCall(*element)) {
          return true;
        }
      }
      return false;
    case ExprKind::kIdentifier:
    case ExprKind::kNumber:
    case ExprKind::kString:
      return false;
    case ExprKind::kCall:
      return !expr.ident.empty() && expr.ident.front() == '$' &&
             expr.ident != "$time";
  }
  return false;
}

bool StatementHasSystemCall(const Statement& statement) {
  if (statement.kind == StatementKind::kAssign ||
      statement.kind == StatementKind::kForce ||
      statement.kind == StatementKind::kRelease) {
    if (statement.assign.rhs && ExprHasSystemCall(*statement.assign.rhs)) {
      return true;
    }
    if (statement.assign.lhs_index &&
        ExprHasSystemCall(*statement.assign.lhs_index)) {
      return true;
    }
    for (const auto& idx : statement.assign.lhs_indices) {
      if (idx && ExprHasSystemCall(*idx)) {
        return true;
      }
    }
    if (statement.assign.lhs_msb_expr &&
        ExprHasSystemCall(*statement.assign.lhs_msb_expr)) {
      return true;
    }
    if (statement.assign.lhs_lsb_expr &&
        ExprHasSystemCall(*statement.assign.lhs_lsb_expr)) {
      return true;
    }
    if (statement.assign.delay &&
        ExprHasSystemCall(*statement.assign.delay)) {
      return true;
    }
    return false;
  }
  if (statement.kind == StatementKind::kIf) {
    if (statement.condition &&
        ExprHasSystemCall(*statement.condition)) {
      return true;
    }
    for (const auto& stmt : statement.then_branch) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    for (const auto& stmt : statement.else_branch) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kBlock) {
    for (const auto& stmt : statement.block) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kCase) {
    if (statement.case_expr &&
        ExprHasSystemCall(*statement.case_expr)) {
      return true;
    }
    for (const auto& item : statement.case_items) {
      for (const auto& stmt : item.body) {
        if (StatementHasSystemCall(stmt)) {
          return true;
        }
      }
    }
    for (const auto& stmt : statement.default_branch) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kFor) {
    if (statement.for_init_rhs &&
        ExprHasSystemCall(*statement.for_init_rhs)) {
      return true;
    }
    if (statement.for_condition &&
        ExprHasSystemCall(*statement.for_condition)) {
      return true;
    }
    if (statement.for_step_rhs &&
        ExprHasSystemCall(*statement.for_step_rhs)) {
      return true;
    }
    for (const auto& stmt : statement.for_body) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kWhile) {
    if (statement.while_condition &&
        ExprHasSystemCall(*statement.while_condition)) {
      return true;
    }
    for (const auto& stmt : statement.while_body) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kRepeat) {
    if (statement.repeat_count &&
        ExprHasSystemCall(*statement.repeat_count)) {
      return true;
    }
    for (const auto& stmt : statement.repeat_body) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kDelay) {
    if (statement.delay && ExprHasSystemCall(*statement.delay)) {
      return true;
    }
    for (const auto& stmt : statement.delay_body) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kEventControl) {
    if (!statement.event_items.empty()) {
      for (const auto& item : statement.event_items) {
        if (item.expr && ExprHasSystemCall(*item.expr)) {
          return true;
        }
      }
    } else if (statement.event_expr &&
               ExprHasSystemCall(*statement.event_expr)) {
      return true;
    }
    for (const auto& stmt : statement.event_body) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kWait) {
    if (statement.wait_condition &&
        ExprHasSystemCall(*statement.wait_condition)) {
      return true;
    }
    for (const auto& stmt : statement.wait_body) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kForever) {
    for (const auto& stmt : statement.forever_body) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  if (statement.kind == StatementKind::kFork) {
    for (const auto& stmt : statement.fork_branches) {
      if (StatementHasSystemCall(stmt)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

bool StatementHasUnsupportedCall(const Statement& statement,
                                 std::string* name_out) {
  switch (statement.kind) {
    case StatementKind::kAssign: {
      const auto& assign = statement.assign;
      if (assign.lhs_index && ExprHasUnsupportedCall(*assign.lhs_index, name_out)) {
        return true;
      }
      for (const auto& idx : assign.lhs_indices) {
        if (idx && ExprHasUnsupportedCall(*idx, name_out)) {
          return true;
        }
      }
      if (assign.lhs_msb_expr &&
          ExprHasUnsupportedCall(*assign.lhs_msb_expr, name_out)) {
        return true;
      }
      if (assign.lhs_lsb_expr &&
          ExprHasUnsupportedCall(*assign.lhs_lsb_expr, name_out)) {
        return true;
      }
      if (assign.rhs && ExprHasUnsupportedCall(*assign.rhs, name_out)) {
        return true;
      }
      if (assign.delay && ExprHasUnsupportedCall(*assign.delay, name_out)) {
        return true;
      }
      return false;
    }
    case StatementKind::kIf:
      if (statement.condition &&
          ExprHasUnsupportedCall(*statement.condition, name_out)) {
        return true;
      }
      for (const auto& stmt : statement.then_branch) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      for (const auto& stmt : statement.else_branch) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      return false;
    case StatementKind::kBlock:
      for (const auto& stmt : statement.block) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      return false;
    case StatementKind::kCase:
      if (statement.case_expr &&
          ExprHasUnsupportedCall(*statement.case_expr, name_out)) {
        return true;
      }
      for (const auto& item : statement.case_items) {
        for (const auto& label : item.labels) {
          if (label && ExprHasUnsupportedCall(*label, name_out)) {
            return true;
          }
        }
        for (const auto& stmt : item.body) {
          if (StatementHasUnsupportedCall(stmt, name_out)) {
            return true;
          }
        }
      }
      for (const auto& stmt : statement.default_branch) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      return false;
    case StatementKind::kFor:
      if (statement.for_init_rhs &&
          ExprHasUnsupportedCall(*statement.for_init_rhs, name_out)) {
        return true;
      }
      if (statement.for_condition &&
          ExprHasUnsupportedCall(*statement.for_condition, name_out)) {
        return true;
      }
      if (statement.for_step_rhs &&
          ExprHasUnsupportedCall(*statement.for_step_rhs, name_out)) {
        return true;
      }
      for (const auto& stmt : statement.for_body) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      return false;
    case StatementKind::kWhile:
      if (statement.while_condition &&
          ExprHasUnsupportedCall(*statement.while_condition, name_out)) {
        return true;
      }
      for (const auto& stmt : statement.while_body) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      return false;
    case StatementKind::kRepeat:
      if (statement.repeat_count &&
          ExprHasUnsupportedCall(*statement.repeat_count, name_out)) {
        return true;
      }
      for (const auto& stmt : statement.repeat_body) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      return false;
    case StatementKind::kDelay:
      if (statement.delay &&
          ExprHasUnsupportedCall(*statement.delay, name_out)) {
        return true;
      }
      for (const auto& stmt : statement.delay_body) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      return false;
    case StatementKind::kEventControl:
      if (!statement.event_items.empty()) {
        for (const auto& item : statement.event_items) {
          if (item.expr &&
              ExprHasUnsupportedCall(*item.expr, name_out)) {
            return true;
          }
        }
      } else if (statement.event_expr &&
                 ExprHasUnsupportedCall(*statement.event_expr, name_out)) {
        return true;
      }
      for (const auto& stmt : statement.event_body) {
        if (StatementHasUnsupportedCall(stmt, name_out)) {
          return true;
        }
      }
      return false;
    case StatementKind::kEventTrigger:
    case StatementKind::kWait:
    case StatementKind::kForever:
    case StatementKind::kFork:
    case StatementKind::kDisable:
    case StatementKind::kTaskCall:
    case StatementKind::kForce:
    case StatementKind::kRelease:
      return false;
  }
  return false;
}

bool ValidateNoFunctionCalls(const Module& module, Diagnostics* diagnostics) {
  std::string call_name;
  for (const auto& assign : module.assigns) {
    if (assign.rhs && ExprHasUnsupportedCall(*assign.rhs, &call_name)) {
      diagnostics->Add(Severity::kError,
                       "function call '" + call_name +
                           "' not supported in runtime expressions");
      return false;
    }
  }
  for (const auto& sw : module.switches) {
    if (sw.control && ExprHasUnsupportedCall(*sw.control, &call_name)) {
      diagnostics->Add(Severity::kError,
                       "function call '" + call_name +
                           "' not supported in runtime expressions");
      return false;
    }
    if (sw.control_n && ExprHasUnsupportedCall(*sw.control_n, &call_name)) {
      diagnostics->Add(Severity::kError,
                       "function call '" + call_name +
                           "' not supported in runtime expressions");
      return false;
    }
  }
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      if (StatementHasUnsupportedCall(stmt, &call_name)) {
        diagnostics->Add(Severity::kError,
                         "function call '" + call_name +
                             "' not supported in runtime expressions");
        return false;
      }
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      if (StatementHasUnsupportedCall(stmt, &call_name)) {
        diagnostics->Add(Severity::kError,
                         "function call '" + call_name +
                             "' not supported in runtime expressions");
        return false;
      }
    }
  }
  return true;
}

void CollectAssignedSignalsNoIndex(const Statement& statement,
                                   std::unordered_set<std::string>* out) {
  switch (statement.kind) {
    case StatementKind::kAssign:
      if (!statement.assign.lhs_index &&
          statement.assign.lhs_indices.empty() &&
          !statement.assign.lhs_has_range) {
        out->insert(statement.assign.lhs);
      }
      break;
    case StatementKind::kIf:
      for (const auto& stmt : statement.then_branch) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      for (const auto& stmt : statement.else_branch) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kBlock:
      for (const auto& stmt : statement.block) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kCase:
      for (const auto& item : statement.case_items) {
        for (const auto& stmt : item.body) {
          CollectAssignedSignalsNoIndex(stmt, out);
        }
      }
      for (const auto& stmt : statement.default_branch) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kFor:
      for (const auto& stmt : statement.for_body) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kWhile:
      for (const auto& stmt : statement.while_body) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kRepeat:
      for (const auto& stmt : statement.repeat_body) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kDelay:
      for (const auto& stmt : statement.delay_body) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kEventControl:
      for (const auto& stmt : statement.event_body) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kWait:
      for (const auto& stmt : statement.wait_body) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kForever:
      for (const auto& stmt : statement.forever_body) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kFork:
      for (const auto& stmt : statement.fork_branches) {
        CollectAssignedSignalsNoIndex(stmt, out);
      }
      break;
    case StatementKind::kDisable:
    case StatementKind::kTaskCall:
    case StatementKind::kEventTrigger:
    case StatementKind::kForce:
    case StatementKind::kRelease:
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

struct SignalRef {
  std::string name;
  bool has_range = false;
  int lo = 0;
  int hi = 0;
};

void CollectSignalRefs(const Expr& expr, const ParamBindings& params,
                       std::vector<SignalRef>* out) {
  if (!out) {
    return;
  }
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      out->push_back(SignalRef{expr.ident, false, 0, 0});
      return;
    case ExprKind::kSelect: {
      bool added = false;
      if (expr.base && expr.base->kind == ExprKind::kIdentifier) {
        int64_t msb = expr.msb;
        int64_t lsb = expr.lsb;
        bool ok = true;
        if (expr.msb_expr) {
          ok = TryEvalConstExprWithParams(*expr.msb_expr, params, &msb);
        }
        if (expr.has_range) {
          if (expr.lsb_expr) {
            ok = ok && TryEvalConstExprWithParams(*expr.lsb_expr, params, &lsb);
          } else {
            lsb = msb;
          }
        } else {
          lsb = msb;
        }
        if (ok) {
          int lo = static_cast<int>(std::min(msb, lsb));
          int hi = static_cast<int>(std::max(msb, lsb));
          out->push_back(SignalRef{expr.base->ident, true, lo, hi});
        } else {
          out->push_back(SignalRef{expr.base->ident, false, 0, 0});
        }
        added = true;
      }
      if (!added && expr.base) {
        CollectSignalRefs(*expr.base, params, out);
      }
      if (expr.msb_expr) {
        CollectSignalRefs(*expr.msb_expr, params, out);
      }
      if (expr.lsb_expr) {
        CollectSignalRefs(*expr.lsb_expr, params, out);
      }
      return;
    }
    case ExprKind::kIndex: {
      bool added = false;
      if (expr.base && expr.base->kind == ExprKind::kIdentifier &&
          expr.index) {
        int64_t index = 0;
        if (TryEvalConstExprWithParams(*expr.index, params, &index)) {
          out->push_back(
              SignalRef{expr.base->ident, true, static_cast<int>(index),
                        static_cast<int>(index)});
        } else {
          out->push_back(SignalRef{expr.base->ident, false, 0, 0});
        }
        added = true;
      }
      if (!added && expr.base) {
        CollectSignalRefs(*expr.base, params, out);
      }
      if (expr.index) {
        CollectSignalRefs(*expr.index, params, out);
      }
      return;
    }
    case ExprKind::kUnary:
      if (expr.operand) {
        CollectSignalRefs(*expr.operand, params, out);
      }
      return;
    case ExprKind::kBinary:
      if (expr.lhs) {
        CollectSignalRefs(*expr.lhs, params, out);
      }
      if (expr.rhs) {
        CollectSignalRefs(*expr.rhs, params, out);
      }
      return;
    case ExprKind::kTernary:
      if (expr.condition) {
        CollectSignalRefs(*expr.condition, params, out);
      }
      if (expr.then_expr) {
        CollectSignalRefs(*expr.then_expr, params, out);
      }
      if (expr.else_expr) {
        CollectSignalRefs(*expr.else_expr, params, out);
      }
      return;
    case ExprKind::kCall:
      for (const auto& arg : expr.call_args) {
        CollectSignalRefs(*arg, params, out);
      }
      return;
    case ExprKind::kConcat:
      for (const auto& element : expr.elements) {
        CollectSignalRefs(*element, params, out);
      }
      return;
    case ExprKind::kNumber:
      return;
    case ExprKind::kString:
      return;
  }
}

bool ValidateSingleDrivers(const Module& flat, Diagnostics* diagnostics,
                           bool allow_multi_driver) {
  struct Range {
    int lo = 0;
    int hi = 0;
  };
  auto is_wire = [&](const std::string& name) -> bool {
    const Net* net = FindNet(flat, name);
    if (!net) {
      return true;
    }
    return net->type != NetType::kReg;
  };
  std::unordered_map<std::string, std::string> drivers;
  std::unordered_map<std::string, std::vector<Range>> partial_ranges;
  for (const auto& assign : flat.assigns) {
    if (!assign.lhs_has_range) {
      bool can_multi = allow_multi_driver && is_wire(assign.lhs);
      if (drivers.count(assign.lhs) > 0 || partial_ranges.count(assign.lhs) > 0) {
        if (!can_multi || drivers[assign.lhs] == "always" ||
            partial_ranges.count(assign.lhs) > 0) {
          diagnostics->Add(Severity::kError,
                           "multiple drivers for signal '" + assign.lhs + "'");
          return false;
        }
      }
      drivers[assign.lhs] = "assign";
      continue;
    }
    bool can_multi = allow_multi_driver && is_wire(assign.lhs);
    if (drivers.count(assign.lhs) > 0) {
      if (!can_multi || drivers[assign.lhs] == "always") {
        diagnostics->Add(Severity::kError,
                         "multiple drivers for signal '" + assign.lhs + "'");
        return false;
      }
    }
    int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
    int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
    auto& ranges = partial_ranges[assign.lhs];
    if (!can_multi) {
      for (const auto& range : ranges) {
        if (hi >= range.lo && lo <= range.hi) {
          diagnostics->Add(Severity::kError,
                           "overlapping drivers for signal '" + assign.lhs +
                               "' (" + std::to_string(lo) + ":" +
                               std::to_string(hi) + " overlaps " +
                               std::to_string(range.lo) + ":" +
                               std::to_string(range.hi) + ")");
          return false;
        }
      }
    }
    ranges.push_back(Range{lo, hi});
  }

  auto is_reg = [&](const std::string& name) -> bool {
    const Net* net = FindNet(flat, name);
    if (!net) {
      return false;
    }
    return net->type == NetType::kReg;
  };
  for (const auto& block : flat.always_blocks) {
    std::unordered_set<std::string> block_drives;
    for (const auto& stmt : block.statements) {
      CollectAssignedSignalsNoIndex(stmt, &block_drives);
    }
    for (const auto& name : block_drives) {
      if (drivers.count(name) > 0 || partial_ranges.count(name) > 0) {
        auto driver_it = drivers.find(name);
        if ((driver_it != drivers.end() && driver_it->second == "assign") ||
            partial_ranges.count(name) > 0) {
          diagnostics->Add(Severity::kError,
                           "multiple drivers for signal '" + name + "'");
          return false;
        }
        if (!is_reg(name)) {
          diagnostics->Add(Severity::kError,
                           "multiple drivers for signal '" + name + "'");
          return false;
        }
      }
      drivers[name] = "always";
    }
  }
  return true;
}

bool ValidateSwitches(const Module& flat, Diagnostics* diagnostics) {
  for (const auto& sw : flat.switches) {
    int a_width = SignalWidth(flat, sw.a);
    if (a_width <= 0) {
      diagnostics->Add(Severity::kError,
                       "unknown switch terminal '" + sw.a + "'");
      return false;
    }
    int b_width = SignalWidth(flat, sw.b);
    if (b_width <= 0) {
      diagnostics->Add(Severity::kError,
                       "unknown switch terminal '" + sw.b + "'");
      return false;
    }
    if (a_width != b_width) {
      diagnostics->Add(Severity::kError,
                       "switch terminals '" + sw.a + "' and '" + sw.b +
                           "' must have matching widths");
      return false;
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
  struct AssignInfo {
    size_t index = 0;
    bool has_range = false;
    int lo = 0;
    int hi = 0;
  };
  std::unordered_map<std::string, std::vector<AssignInfo>> lhs_map;
  lhs_map.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const auto& assign = flat.assigns[i];
    AssignInfo info;
    info.index = i;
    info.has_range = assign.lhs_has_range;
    if (assign.lhs_has_range) {
      info.lo = std::min(assign.lhs_msb, assign.lhs_lsb);
      info.hi = std::max(assign.lhs_msb, assign.lhs_lsb);
    }
    lhs_map[assign.lhs].push_back(info);
  }

  std::vector<int> indegree(count, 0);
  std::vector<std::vector<size_t>> edges(count);
  ParamBindings empty_params;
  for (size_t i = 0; i < count; ++i) {
    const auto& assign = flat.assigns[i];
    if (!assign.rhs) {
      continue;
    }
    std::vector<SignalRef> deps;
    CollectSignalRefs(*assign.rhs, empty_params, &deps);
    std::unordered_set<size_t> seen;
    for (const auto& dep : deps) {
      auto it = lhs_map.find(dep.name);
      if (it == lhs_map.end()) {
        continue;
      }
      for (const auto& driver : it->second) {
        if (dep.has_range && driver.has_range) {
          if (dep.hi < driver.lo || dep.lo > driver.hi) {
            continue;
          }
        }
        if (!seen.insert(driver.index).second) {
          continue;
        }
        edges[driver.index].push_back(i);
        indegree[i]++;
      }
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
    if (stmt.kind == StatementKind::kFor) {
      for (const auto& inner : stmt.for_body) {
        walk(inner);
      }
    }
    if (stmt.kind == StatementKind::kWhile) {
      for (const auto& inner : stmt.while_body) {
        walk(inner);
      }
    }
    if (stmt.kind == StatementKind::kRepeat) {
      for (const auto& inner : stmt.repeat_body) {
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

void WarnUndrivenWires(const Module& flat, Diagnostics* diagnostics,
                       bool enable_4state) {
  std::unordered_set<std::string> driven;
  for (const auto& assign : flat.assigns) {
    driven.insert(assign.lhs);
  }
  for (const auto& block : flat.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectAssignedSignals(stmt, &driven);
    }
  }
  for (const auto& net : flat.nets) {
    if (net.type == NetType::kReg || net.array_size > 0) {
      continue;
    }
    if (net.type == NetType::kTri0 || net.type == NetType::kTri1 ||
        net.type == NetType::kSupply0 || net.type == NetType::kSupply1) {
      continue;
    }
    if (driven.count(net.name) > 0) {
      continue;
    }
    diagnostics->Add(Severity::kWarning,
                     "undriven wire '" + net.name + "' defaults to " +
                         std::string(enable_4state ? "X" : "0") +
                         " in v0");
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

bool IsDeclaredEvent(const Module& module, const std::string& name) {
  for (const auto& event_decl : module.events) {
    if (event_decl.name == name) {
      return true;
    }
  }
  return false;
}

bool IsDeclaredLocal(const std::unordered_set<std::string>* locals,
                     const std::string& name) {
  return locals && locals->count(name) > 0;
}

bool IsDeclaredSignalOrEventOrLocal(
    const Module& module, const std::string& name,
    const std::unordered_set<std::string>* locals) {
  if (IsDeclaredLocal(locals, name)) {
    return true;
  }
  return IsDeclaredSignal(module, name) || IsDeclaredEvent(module, name);
}

bool IsSystemTaskName(const std::string& name) {
  return !name.empty() && name[0] == '$';
}

bool SystemTaskAllowsScope(const std::string& name) {
  return name == "$dumpvars" || name == "$printtimescale";
}

bool IsModuleOrInstanceName(const Module& module, const std::string& name) {
  if (module.name == name) {
    return true;
  }
  for (const auto& inst : module.instances) {
    if (inst.name == name) {
      return true;
    }
  }
  return false;
}

bool ValidateExprIdentifiers(const Expr* expr, const Module& module,
                             Diagnostics* diagnostics,
                             const std::unordered_set<std::string>* locals) {
  if (!expr) {
    return true;
  }
  bool ok = true;
  switch (expr->kind) {
    case ExprKind::kIdentifier:
      if (!IsDeclaredSignalOrEventOrLocal(module, expr->ident, locals)) {
        diagnostics->Add(Severity::kError,
                         "unknown signal '" + expr->ident + "'");
        ok = false;
      }
      break;
    case ExprKind::kNumber:
    case ExprKind::kString:
      break;
    case ExprKind::kUnary:
      ok &= ValidateExprIdentifiers(expr->operand.get(), module, diagnostics,
                                    locals);
      break;
    case ExprKind::kBinary:
      ok &= ValidateExprIdentifiers(expr->lhs.get(), module, diagnostics,
                                    locals);
      ok &= ValidateExprIdentifiers(expr->rhs.get(), module, diagnostics,
                                    locals);
      break;
    case ExprKind::kTernary:
      ok &= ValidateExprIdentifiers(expr->condition.get(), module, diagnostics,
                                    locals);
      ok &= ValidateExprIdentifiers(expr->then_expr.get(), module, diagnostics,
                                    locals);
      ok &= ValidateExprIdentifiers(expr->else_expr.get(), module, diagnostics,
                                    locals);
      break;
    case ExprKind::kSelect:
      ok &= ValidateExprIdentifiers(expr->base.get(), module, diagnostics,
                                    locals);
      ok &= ValidateExprIdentifiers(expr->msb_expr.get(), module, diagnostics,
                                    locals);
      ok &= ValidateExprIdentifiers(expr->lsb_expr.get(), module, diagnostics,
                                    locals);
      break;
    case ExprKind::kIndex:
      ok &= ValidateExprIdentifiers(expr->base.get(), module, diagnostics,
                                    locals);
      ok &= ValidateExprIdentifiers(expr->index.get(), module, diagnostics,
                                    locals);
      break;
    case ExprKind::kCall:
      for (const auto& arg : expr->call_args) {
        ok &= ValidateExprIdentifiers(arg.get(), module, diagnostics, locals);
      }
      break;
    case ExprKind::kConcat:
      ok &= ValidateExprIdentifiers(expr->repeat_expr.get(), module,
                                    diagnostics, locals);
      for (const auto& element : expr->elements) {
        ok &= ValidateExprIdentifiers(element.get(), module, diagnostics,
                                      locals);
      }
      break;
  }
  return ok;
}

bool ValidateAssignTarget(const Module& module, const std::string& name,
                          Diagnostics* diagnostics,
                          const std::unordered_set<std::string>* locals) {
  if (IsDeclaredLocal(locals, name)) {
    return true;
  }
  if (!IsDeclaredSignal(module, name)) {
    diagnostics->Add(Severity::kError,
                     "assignment target '" + name + "' is not declared");
    return false;
  }
  return true;
}

bool ValidateStatementIdentifiers(const Statement& stmt, const Module& module,
                                  Diagnostics* diagnostics,
                                  const std::unordered_set<std::string>* locals) {
  bool ok = true;
  switch (stmt.kind) {
    case StatementKind::kAssign:
    case StatementKind::kForce:
    case StatementKind::kRelease:
      ok &=
          ValidateAssignTarget(module, stmt.assign.lhs, diagnostics, locals);
      for (const auto& index_expr : stmt.assign.lhs_indices) {
        ok &= ValidateExprIdentifiers(index_expr.get(), module, diagnostics,
                                      locals);
      }
      ok &= ValidateExprIdentifiers(stmt.assign.lhs_index.get(), module,
                                    diagnostics, locals);
      ok &= ValidateExprIdentifiers(stmt.assign.lhs_msb_expr.get(), module,
                                    diagnostics, locals);
      ok &= ValidateExprIdentifiers(stmt.assign.lhs_lsb_expr.get(), module,
                                    diagnostics, locals);
      ok &= ValidateExprIdentifiers(stmt.assign.rhs.get(), module, diagnostics,
                                    locals);
      ok &= ValidateExprIdentifiers(stmt.assign.delay.get(), module,
                                    diagnostics, locals);
      break;
    case StatementKind::kIf:
      ok &=
          ValidateExprIdentifiers(stmt.condition.get(), module, diagnostics,
                                  locals);
      for (const auto& inner : stmt.then_branch) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      for (const auto& inner : stmt.else_branch) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kBlock:
      for (const auto& inner : stmt.block) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kCase:
      ok &=
          ValidateExprIdentifiers(stmt.case_expr.get(), module, diagnostics,
                                  locals);
      for (const auto& item : stmt.case_items) {
        for (const auto& label : item.labels) {
          ok &= ValidateExprIdentifiers(label.get(), module, diagnostics,
                                        locals);
        }
        for (const auto& inner : item.body) {
          ok &= ValidateStatementIdentifiers(inner, module, diagnostics,
                                             locals);
        }
      }
      for (const auto& inner : stmt.default_branch) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kFor:
      if (!stmt.for_init_lhs.empty()) {
        ok &= ValidateAssignTarget(module, stmt.for_init_lhs, diagnostics,
                                   locals);
      }
      ok &= ValidateExprIdentifiers(stmt.for_init_rhs.get(), module,
                                    diagnostics, locals);
      ok &= ValidateExprIdentifiers(stmt.for_condition.get(), module,
                                    diagnostics, locals);
      if (!stmt.for_step_lhs.empty()) {
        ok &= ValidateAssignTarget(module, stmt.for_step_lhs, diagnostics,
                                   locals);
      }
      ok &= ValidateExprIdentifiers(stmt.for_step_rhs.get(), module,
                                    diagnostics, locals);
      for (const auto& inner : stmt.for_body) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kWhile:
      ok &= ValidateExprIdentifiers(stmt.while_condition.get(), module,
                                    diagnostics, locals);
      for (const auto& inner : stmt.while_body) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kRepeat:
      ok &= ValidateExprIdentifiers(stmt.repeat_count.get(), module,
                                    diagnostics, locals);
      for (const auto& inner : stmt.repeat_body) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kDelay:
      ok &=
          ValidateExprIdentifiers(stmt.delay.get(), module, diagnostics, locals);
      for (const auto& inner : stmt.delay_body) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kEventControl:
      ok &= ValidateExprIdentifiers(stmt.event_expr.get(), module, diagnostics,
                                    locals);
      for (const auto& item : stmt.event_items) {
        ok &= ValidateExprIdentifiers(item.expr.get(), module, diagnostics,
                                      locals);
      }
      for (const auto& inner : stmt.event_body) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kEventTrigger:
      if (!IsDeclaredEvent(module, stmt.trigger_target)) {
        diagnostics->Add(
            Severity::kError,
            "event '" + stmt.trigger_target + "' is not declared");
        ok = false;
      }
      break;
    case StatementKind::kWait:
      ok &= ValidateExprIdentifiers(stmt.wait_condition.get(), module,
                                    diagnostics, locals);
      for (const auto& inner : stmt.wait_body) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kForever:
      for (const auto& inner : stmt.forever_body) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kFork:
      for (const auto& inner : stmt.fork_branches) {
        ok &= ValidateStatementIdentifiers(inner, module, diagnostics, locals);
      }
      break;
    case StatementKind::kDisable:
      break;
    case StatementKind::kTaskCall:
      if (IsSystemTaskName(stmt.task_name) &&
          SystemTaskAllowsScope(stmt.task_name)) {
        for (const auto& arg : stmt.task_args) {
          if (!arg) {
            continue;
          }
          if (arg->kind == ExprKind::kIdentifier &&
              !IsDeclaredSignalOrEventOrLocal(module, arg->ident, locals)) {
            if (!IsModuleOrInstanceName(module, arg->ident)) {
              diagnostics->Add(Severity::kError,
                               "unknown signal '" + arg->ident + "'");
              ok = false;
            }
            continue;
          }
          ok &= ValidateExprIdentifiers(arg.get(), module, diagnostics, locals);
        }
      } else {
        for (const auto& arg : stmt.task_args) {
          ok &= ValidateExprIdentifiers(arg.get(), module, diagnostics, locals);
        }
      }
      break;
  }
  return ok;
}

bool ValidateModuleIdentifiers(const Module& module,
                               Diagnostics* diagnostics) {
  bool ok = true;
  for (const auto& assign : module.assigns) {
    ok &= ValidateAssignTarget(module, assign.lhs, diagnostics, nullptr);
    ok &= ValidateExprIdentifiers(assign.rhs.get(), module, diagnostics,
                                  nullptr);
  }
  for (const auto& sw : module.switches) {
    ok &= ValidateAssignTarget(module, sw.a, diagnostics, nullptr);
    ok &= ValidateAssignTarget(module, sw.b, diagnostics, nullptr);
    ok &= ValidateExprIdentifiers(sw.control.get(), module, diagnostics,
                                  nullptr);
    ok &= ValidateExprIdentifiers(sw.control_n.get(), module, diagnostics,
                                  nullptr);
  }
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      ok &= ValidateStatementIdentifiers(stmt, module, diagnostics, nullptr);
    }
  }
  for (const auto& task : module.tasks) {
    std::unordered_set<std::string> locals;
    for (const auto& arg : task.args) {
      locals.insert(arg.name);
    }
    for (const auto& stmt : task.body) {
      ok &= ValidateStatementIdentifiers(stmt, module, diagnostics, &locals);
    }
  }
  return ok;
}

void WarnUndeclaredClocks(const Module& flat, Diagnostics* diagnostics) {
  for (const auto& block : flat.always_blocks) {
    if (block.edge == EdgeKind::kCombinational ||
        block.edge == EdgeKind::kInitial) {
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
  if (stmt.kind == StatementKind::kFor) {
    for (const auto& inner : stmt.for_body) {
      if (HasNonblockingAssign(inner)) {
        return true;
      }
    }
  }
  if (stmt.kind == StatementKind::kWhile) {
    for (const auto& inner : stmt.while_body) {
      if (HasNonblockingAssign(inner)) {
        return true;
      }
    }
  }
  if (stmt.kind == StatementKind::kRepeat) {
    for (const auto& inner : stmt.repeat_body) {
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
                  bool enable_4state,
                  const std::vector<DefParam>* inherited_defparams) {
    if (stack->count(module.name) > 0) {
    diagnostics->Add(Severity::kError,
                     "recursive module instantiation detected");
    return false;
  }
  stack->insert(module.name);

  std::unordered_set<std::string> port_names;
  std::unordered_set<std::string> local_net_names;
  std::unordered_set<std::string> local_event_names;
  for (const auto& port : module.ports) {
    port_names.insert(port.name);
  }
  for (const auto& net : module.nets) {
    local_net_names.insert(net.name);
  }
  for (const auto& event_decl : module.events) {
    local_event_names.insert(event_decl.name);
  }
  std::unordered_set<std::string> instance_names;
  for (const auto& instance : module.instances) {
    instance_names.insert(instance.name);
  }
  if (!ValidateDefparamsForModule(module.defparams, instance_names,
                                  diagnostics)) {
    return false;
  }
  if (inherited_defparams &&
      !ValidateDefparamsForModule(*inherited_defparams, instance_names,
                                  diagnostics)) {
    return false;
  }

  auto rename = [&](const std::string& ident) -> std::string {
    if (ident.find('.') != std::string::npos) {
      std::string top_name = hier_prefix;
      size_t top_dot = top_name.find('.');
      if (top_dot != std::string::npos) {
        top_name = top_name.substr(0, top_dot);
      }
      bool absolute = (!top_name.empty() &&
                       ident.rfind(top_name + ".", 0) == 0);
      std::string path = ident;
      if (absolute) {
        path = ident.substr(top_name.size() + 1);
      }
      std::string flat;
      size_t start = 0;
      while (start < path.size()) {
        size_t next = path.find('.', start);
        if (next == std::string::npos) {
          flat += path.substr(start);
          break;
        }
        flat += path.substr(start, next - start);
        flat += "__";
        start = next + 1;
      }
      if (!absolute && !prefix.empty()) {
        flat = prefix + flat;
      }
      return flat;
    }
    auto it = port_map.find(ident);
    if (it != port_map.end()) {
      return it->second.signal;
    }
    if (!prefix.empty() &&
        (port_names.count(ident) > 0 || local_net_names.count(ident) > 0 ||
         local_event_names.count(ident) > 0)) {
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
  auto lookup_real = [&](const std::string& ident) -> bool {
    for (const auto& net : module.nets) {
      if (net.name == ident) {
        return net.is_real;
      }
    }
    return false;
  };
  auto lookup_charge = [&](const std::string& ident) -> ChargeStrength {
    for (const auto& net : module.nets) {
      if (net.name == ident) {
        return net.charge;
      }
    }
    return ChargeStrength::kNone;
  };

  auto register_event = [&](const std::string& name,
                            const std::string& hier_path) -> bool {
    auto it = flat_to_hier->find(name);
    if (it != flat_to_hier->end() && it->second != hier_path) {
      diagnostics->Add(Severity::kError,
                       "flattened event name collision for '" + name + "'");
      return false;
    }
    (*flat_to_hier)[name] = hier_path;
    return true;
  };

  std::unordered_map<std::string, std::string> task_renames;
  for (const auto& task : module.tasks) {
    task_renames[task.name] =
        prefix.empty() ? task.name : (prefix + task.name);
  }
  const auto* prev_task_renames = g_task_renames;
  g_task_renames = &task_renames;

  if (prefix.empty()) {
    out->name = module.name;
    out->unconnected_drive = module.unconnected_drive;
    out->parameters.clear();
    for (const auto& param : module.parameters) {
      Parameter flat_param;
      flat_param.name = param.name;
      flat_param.is_local = param.is_local;
      flat_param.is_real = param.is_real;
      auto it = params.exprs.find(param.name);
      if (it != params.exprs.end()) {
        flat_param.value = gpga::CloneExpr(*it->second);
      } else if (param.value) {
        flat_param.value = CloneExprWithParams(
            *param.value, [](const std::string& ident) { return ident; },
            params, &module, diagnostics, nullptr);
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
      flat_port.is_real = port.is_real;
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
      std::vector<int> array_dims;
      if (!ResolveArrayDims(net, params, &array_dims, diagnostics,
                            "net '" + net.name + "' array range")) {
        return false;
      }
      if (!AddFlatNet(net.name, width, net.is_signed, net.type, net.charge,
                      array_dims, net.is_real, hier_prefix + "." + net.name,
                      out, net_names, flat_to_hier, diagnostics)) {
        return false;
      }
    }
    out->events.clear();
    for (const auto& event_decl : module.events) {
      out->events.push_back(event_decl);
      if (!register_event(event_decl.name,
                          hier_prefix + "." + event_decl.name)) {
        return false;
      }
    }
    out->tasks.clear();
    for (const auto& task : module.tasks) {
      Task flat_task;
      flat_task.name = task_renames[task.name];
      for (const auto& arg : task.args) {
        int width = arg.width;
        if (!ResolveRangeWidth(arg.width, arg.msb_expr, arg.lsb_expr, params,
                               &width, diagnostics,
                               "task '" + task.name + "' arg '" + arg.name +
                                   "'")) {
          return false;
        }
        TaskArg flat_arg;
        flat_arg.dir = arg.dir;
        flat_arg.name = arg.name;
        flat_arg.width = width;
        flat_arg.is_signed = arg.is_signed;
        flat_arg.is_real = arg.is_real;
        flat_task.args.push_back(std::move(flat_arg));
      }
      if (!CloneStatementList(task.body, rename, params, module, *out,
                              &flat_task.body, diagnostics)) {
        return false;
      }
      out->tasks.push_back(std::move(flat_task));
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
      if (!AddFlatNet(prefix + port.name, width, port.is_signed, type,
                      lookup_charge(port.name), {}, lookup_real(port.name),
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
      std::vector<int> array_dims;
      if (!ResolveArrayDims(net, params, &array_dims, diagnostics,
                            "net '" + net.name + "' array range")) {
        return false;
      }
      if (!AddFlatNet(prefix + net.name, width, net.is_signed, net.type,
                      net.charge, array_dims, net.is_real,
                      hier_prefix + "." + net.name, out, net_names,
                      flat_to_hier, diagnostics)) {
        return false;
      }
    }
    for (const auto& event_decl : module.events) {
      EventDecl flat_event;
      flat_event.name = prefix + event_decl.name;
      out->events.push_back(std::move(flat_event));
      if (!register_event(prefix + event_decl.name,
                          hier_prefix + "." + event_decl.name)) {
        return false;
      }
    }
    for (const auto& task : module.tasks) {
      Task flat_task;
      flat_task.name = task_renames[task.name];
      for (const auto& arg : task.args) {
        int width = arg.width;
        if (!ResolveRangeWidth(arg.width, arg.msb_expr, arg.lsb_expr, params,
                               &width, diagnostics,
                               "task '" + task.name + "' arg '" + arg.name +
                                   "'")) {
          return false;
        }
        TaskArg flat_arg;
        flat_arg.dir = arg.dir;
        flat_arg.name = arg.name;
        flat_arg.width = width;
        flat_arg.is_signed = arg.is_signed;
        flat_arg.is_real = arg.is_real;
        flat_task.args.push_back(std::move(flat_arg));
      }
      if (!CloneStatementList(task.body, rename, params, module, *out,
                              &flat_task.body, diagnostics)) {
        return false;
      }
      out->tasks.push_back(std::move(flat_task));
    }
  }

  for (const auto& assign : module.assigns) {
    Assign flattened;
    flattened.lhs = rename(assign.lhs);
    flattened.lhs_has_range = assign.lhs_has_range;
    flattened.lhs_msb = assign.lhs_msb;
    flattened.lhs_lsb = assign.lhs_lsb;
    flattened.strength0 = assign.strength0;
    flattened.strength1 = assign.strength1;
    flattened.has_strength = assign.has_strength;
    if (assign.rhs) {
      flattened.rhs =
          CloneExprWithParams(*assign.rhs, rename, params, &module,
                              diagnostics, nullptr);
      if (!flattened.rhs) {
        return false;
      }
      flattened.rhs = SimplifyExpr(std::move(flattened.rhs), *out);
    } else {
      flattened.rhs = nullptr;
    }
    out->assigns.push_back(std::move(flattened));
  }
  for (const auto& sw : module.switches) {
    Switch flattened;
    flattened.kind = sw.kind;
    flattened.a = rename(sw.a);
    flattened.b = rename(sw.b);
    flattened.strength0 = sw.strength0;
    flattened.strength1 = sw.strength1;
    flattened.has_strength = sw.has_strength;
    if (sw.control) {
      flattened.control = CloneExprWithParams(*sw.control, rename, params,
                                              &module, diagnostics, nullptr);
      if (!flattened.control) {
        return false;
      }
      flattened.control = SimplifyExpr(std::move(flattened.control), *out);
    }
    if (sw.control_n) {
      flattened.control_n = CloneExprWithParams(*sw.control_n, rename, params,
                                                &module, diagnostics, nullptr);
      if (!flattened.control_n) {
        return false;
      }
      flattened.control_n = SimplifyExpr(std::move(flattened.control_n), *out);
    }
    out->switches.push_back(std::move(flattened));
  }
  for (const auto& block : module.always_blocks) {
    AlwaysBlock flattened;
    flattened.edge = block.edge;
    flattened.clock = rename(block.clock);
    flattened.sensitivity = block.sensitivity;
    if (!CloneStatementList(block.statements, rename, params, module, *out,
                            &flattened.statements, diagnostics)) {
      return false;
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

    Instance effective_instance = CloneInstance(instance);
    std::vector<DefParam> child_defparams;
    if (!ApplyDefparamsToInstance(module.defparams, instance,
                                  &effective_instance, &child_defparams,
                                  diagnostics)) {
      return false;
    }
    if (inherited_defparams &&
        !ApplyDefparamsToInstance(*inherited_defparams, instance,
                                  &effective_instance, &child_defparams,
                                  diagnostics)) {
      return false;
    }

    ParamBindings child_params;
    if (!BuildParamBindings(*child, &effective_instance, &params, &child_params,
                            diagnostics)) {
      return false;
    }

    std::string child_prefix = prefix + instance.name + "__";
    std::string child_hier = hier_prefix + "." + instance.name;
    std::unordered_map<std::string, PortBinding> child_port_map;
    std::unordered_set<std::string> child_ports;
    std::unordered_map<std::string, PortDir> child_port_dirs;
    std::unordered_map<std::string, int> child_port_widths;
    std::unordered_map<std::string, bool> child_port_signed;
    std::unordered_map<std::string, bool> child_port_real;
    std::unordered_map<std::string, NetType> child_port_types;
    std::unordered_map<std::string, ChargeStrength> child_port_charge;
    for (const auto& port : child->ports) {
      int width = port.width;
      if (!ResolveRangeWidth(port.width, port.msb_expr, port.lsb_expr,
                             child_params, &width, diagnostics,
                             "port '" + port.name + "'")) {
        return false;
      }
      NetType port_type = NetType::kWire;
      bool port_is_real = false;
      ChargeStrength port_charge = ChargeStrength::kNone;
      if (const Net* net = FindNet(*child, port.name)) {
        port_type = net->type;
        port_is_real = net->is_real;
        port_charge = net->charge;
      }
      child_ports.insert(port.name);
      child_port_dirs[port.name] = port.dir;
      child_port_widths[port.name] = width;
      child_port_signed[port.name] = port.is_signed;
      child_port_real[port.name] = port_is_real;
      child_port_types[port.name] = port_type;
      child_port_charge[port.name] = port_charge;
      child_port_map[port.name] = PortBinding{child_prefix + port.name};
    }
    std::unordered_set<std::string> seen_ports;
    std::unordered_set<std::string> connected_ports;
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
      if (seen_ports.count(port_name) > 0) {
        diagnostics->Add(Severity::kError,
                         "duplicate connection for port '" + port_name +
                             "' in instance '" + instance.name + "'");
        return false;
      }
      seen_ports.insert(port_name);
      if (!connection.expr) {
        continue;
      }
      connected_ports.insert(port_name);
      auto resolved_expr = CloneExprWithParams(*connection.expr, rename, params,
                                               &module, diagnostics, nullptr);
      if (!resolved_expr) {
        return false;
      }
      const std::string& port_signal = child_port_map[port_name].signal;
      if (child_port_dirs[port_name] == PortDir::kInput) {
        Assign expr_assign;
        expr_assign.lhs = port_signal;
        expr_assign.rhs = std::move(resolved_expr);
        out->assigns.push_back(std::move(expr_assign));
        continue;
      }

      resolved_expr = SimplifyExpr(std::move(resolved_expr), *out);
      if (resolved_expr && resolved_expr->kind == ExprKind::kTernary &&
          resolved_expr->condition) {
        int64_t cond_value = 0;
        if (TryEvalConstExprWithParams(*resolved_expr->condition, params,
                                       &cond_value)) {
          if (cond_value != 0 && resolved_expr->then_expr) {
            resolved_expr = std::move(resolved_expr->then_expr);
          } else if (resolved_expr->else_expr) {
            resolved_expr = std::move(resolved_expr->else_expr);
          }
        }
      }

      std::string base_name;
      int msb = 0;
      int lsb = 0;
      if (resolved_expr->kind == ExprKind::kIdentifier) {
        base_name = resolved_expr->ident;
      } else if (resolved_expr->kind == ExprKind::kSelect &&
          resolved_expr->base &&
          resolved_expr->base->kind == ExprKind::kIdentifier) {
        base_name = resolved_expr->base->ident;
        if (!ResolveSelectIndices(*resolved_expr, params, &msb, &lsb,
                                  diagnostics, "port connection")) {
          return false;
        }
      } else if (resolved_expr->kind == ExprKind::kIndex &&
                 resolved_expr->base &&
                 resolved_expr->base->kind == ExprKind::kIdentifier &&
                 resolved_expr->index) {
        base_name = resolved_expr->base->ident;
        int64_t index = 0;
        if (!EvalConstExprWithParams(*resolved_expr->index, params, &index,
                                     diagnostics, "port connection index")) {
          return false;
        }
        msb = static_cast<int>(index);
        lsb = static_cast<int>(index);
      } else {
        diagnostics->Add(
            Severity::kError,
            "output port connections must be identifiers or constant selects in v0");
        return false;
      }
      Assign out_assign;
      out_assign.lhs = base_name;
      if (resolved_expr->kind != ExprKind::kIdentifier) {
        out_assign.lhs_has_range = true;
        out_assign.lhs_msb = msb;
        out_assign.lhs_lsb = lsb;
      }
      auto rhs = std::make_unique<Expr>();
      rhs->kind = ExprKind::kIdentifier;
      rhs->ident = port_signal;
      out_assign.rhs = std::move(rhs);
      out->assigns.push_back(std::move(out_assign));
    }

    for (const auto& port : child->ports) {
      std::string port_name = port.name;
      std::string port_net = child_prefix + port_name;
      NetType type = child_port_types[port_name];
      if (!AddFlatNet(port_net, child_port_widths[port_name],
                      child_port_signed[port_name], type,
                      child_port_charge[port_name], {},
                      child_port_real[port_name],
                      child_hier + "." + port_name, out, net_names,
                      flat_to_hier, diagnostics)) {
        return false;
      }
      if (connected_ports.count(port_name) == 0) {
        if (port.dir == PortDir::kInput) {
          std::string default_label;
          Assign default_assign;
          default_assign.lhs = port_net;
          switch (module.unconnected_drive) {
            case UnconnectedDrive::kPull0:
              default_label = "pull0";
              default_assign.rhs = MakeNumberExpr(0u);
              default_assign.has_strength = true;
              default_assign.strength0 = Strength::kPull;
              default_assign.strength1 = Strength::kHighZ;
              break;
            case UnconnectedDrive::kPull1:
              default_label = "pull1";
              default_assign.rhs = MakeNumberExpr(1u);
              default_assign.has_strength = true;
              default_assign.strength0 = Strength::kHighZ;
              default_assign.strength1 = Strength::kPull;
              break;
            case UnconnectedDrive::kNone:
              default_label = enable_4state ? "X" : "0";
              default_assign.rhs =
                  enable_4state
                      ? MakeAllXExpr(child_port_widths[port_name])
                      : MakeNumberExpr(0u);
              break;
          }
          diagnostics->Add(Severity::kWarning,
                           "unconnected input '" + port.name +
                               "' in instance '" + instance.name +
                               "' (defaulting to " + default_label + ")");
          out->assigns.push_back(std::move(default_assign));
        } else {
          diagnostics->Add(Severity::kWarning,
                           "unconnected output '" + port.name +
                               "' in instance '" + instance.name + "'");
        }
      }
    }

    const std::vector<DefParam>* child_defparam_ptr =
        child_defparams.empty() ? nullptr : &child_defparams;
    if (!InlineModule(program, *child, child_prefix, child_hier, child_params,
                      child_port_map, out, diagnostics, stack, net_names,
                      flat_to_hier, enable_4state, child_defparam_ptr)) {
      return false;
    }
  }

  g_task_renames = prev_task_renames;
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
                    enable_4state, nullptr)) {
    return false;
  }

  if (!ValidateSwitches(flat, diagnostics)) {
    return false;
  }
  if (!ValidateSingleDrivers(flat, diagnostics, true)) {
    return false;
  }
  if (!ValidateNoFunctionCalls(flat, diagnostics)) {
    return false;
  }
  if (!ValidateCombinationalAcyclic(flat, diagnostics)) {
    return false;
  }
  if (!ValidateModuleIdentifiers(flat, diagnostics)) {
    return false;
  }
  WarnUndeclaredClocks(flat, diagnostics);
  WarnNonblockingInCombAlways(flat, diagnostics);
  WarnNonblockingArrayWrites(flat, diagnostics);
  WarnUndrivenWires(flat, diagnostics, enable_4state);

  out_design->top = std::move(flat);
  out_design->flat_to_hier = std::move(flat_to_hier);
  return true;
}

}  // namespace gpga
