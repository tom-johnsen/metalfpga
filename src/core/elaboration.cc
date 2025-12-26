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

const Net* FindNet(const Module& module, const std::string& name) {
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return &net;
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

const std::unordered_map<std::string, std::string>* g_task_renames = nullptr;

std::unique_ptr<Expr> SimplifyExpr(std::unique_ptr<Expr> expr,
                                   const Module& module);
std::unique_ptr<Expr> MakeNumberExpr(uint64_t value);
std::unique_ptr<Expr> MakeNumberExprSignedWidth(int64_t value, int width);
using BindingMap = std::unordered_map<std::string, const Expr*>;
std::unique_ptr<Expr> CloneExprWithParams(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module* module,
    Diagnostics* diagnostics, const BindingMap* bindings);
void CollectIdentifiers(const Expr& expr,
                        std::unordered_set<std::string>* out);
void CollectAssignedSignals(const Statement& statement,
                            std::unordered_set<std::string>* out);
int SignalWidth(const Module& module, const std::string& name);
bool CloneStatement(
    const Statement& statement,
    const std::function<std::string(const std::string&)>& rename,
    const ParamBindings& params, const Module& source_module,
    const Module& flat_module, Statement* out, Diagnostics* diagnostics);

ParamBindings CloneParamBindings(const ParamBindings& params) {
  ParamBindings out;
  out.values = params.values;
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

void UpdateBindingsFromStatement(const Statement& statement,
                                 const Module& flat_module,
                                 ParamBindings* params) {
  if (statement.kind == StatementKind::kAssign) {
    const auto& assign = statement.assign;
    if (assign.nonblocking || assign.lhs_index ||
        !assign.lhs_indices.empty() || assign.lhs_has_range || !assign.rhs) {
      params->values.erase(assign.lhs);
      params->exprs.erase(assign.lhs);
      return;
    }
    int64_t value = 0;
    if (TryEvalConstExprWithParams(*assign.rhs, *params, &value)) {
      params->values[assign.lhs] = value;
      int width = SignalWidth(flat_module, assign.lhs);
      params->exprs[assign.lhs] = MakeNumberExprSignedWidth(value, width);
    } else {
      params->values.erase(assign.lhs);
      params->exprs.erase(assign.lhs);
    }
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
    diagnostics->Add(Severity::kError, error + " in " + context);
    return false;
  }
  return true;
}

bool ContainsAssignToVar(const Statement& statement,
                          const std::string& name) {
  if (statement.kind == StatementKind::kAssign) {
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

std::unique_ptr<Expr> MakeBinaryExpr(char op, std::unique_ptr<Expr> lhs,
                                     std::unique_ptr<Expr> rhs) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kBinary;
  expr->op = op;
  expr->lhs = std::move(lhs);
  expr->rhs = std::move(rhs);
  return expr;
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
        auto cloned = CloneExprWithParams(*arg, rename, params, module,
                                          diagnostics, bindings);
        if (!cloned) {
          return nullptr;
        }
        out->call_args.push_back(std::move(cloned));
      }
      return out;
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
      auto cloned = CloneExprWithParams(*expr.call_args[i], rename, params,
                                        module, diagnostics, bindings);
      if (!cloned) {
        return nullptr;
      }
      const std::string& arg_name = func->args[i].name;
      arg_bindings[arg_name] = cloned.get();
      arg_clones.push_back(std::move(cloned));
    }
    if (!func->body_expr) {
      diagnostics->Add(Severity::kError,
                       "function '" + expr.ident + "' has no body");
      return nullptr;
    }
    return CloneExprWithParams(*func->body_expr, rename, params, module,
                               diagnostics, &arg_bindings);
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
    out->operand = CloneExprWithParams(*expr.operand, rename, params, module,
                                       diagnostics, bindings);
    if (!out->operand) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kBinary) {
    out->lhs = CloneExprWithParams(*expr.lhs, rename, params, module,
                                   diagnostics, bindings);
    out->rhs = CloneExprWithParams(*expr.rhs, rename, params, module,
                                   diagnostics, bindings);
    if (!out->lhs || !out->rhs) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kTernary) {
    out->condition =
        CloneExprWithParams(*expr.condition, rename, params, module,
                            diagnostics, bindings);
    out->then_expr =
        CloneExprWithParams(*expr.then_expr, rename, params, module,
                            diagnostics, bindings);
    out->else_expr =
        CloneExprWithParams(*expr.else_expr, rename, params, module,
                            diagnostics, bindings);
    if (!out->condition || !out->then_expr || !out->else_expr) {
      return nullptr;
    }
    return out;
  }
  if (expr.kind == ExprKind::kSelect) {
    out->base = CloneExprWithParams(*expr.base, rename, params, module,
                                    diagnostics, bindings);
    if (!out->base) {
      return nullptr;
    }
    out->has_range = expr.has_range;
    out->indexed_range = expr.indexed_range;
    out->indexed_desc = expr.indexed_desc;
    out->indexed_width = expr.indexed_width;
    if (expr.msb_expr) {
      out->msb_expr = CloneExprWithParams(*expr.msb_expr, rename, params,
                                          module, diagnostics, bindings);
      if (!out->msb_expr) {
        return nullptr;
      }
    }
    if (expr.lsb_expr) {
      out->lsb_expr = CloneExprWithParams(*expr.lsb_expr, rename, params,
                                          module, diagnostics, bindings);
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
          if (dims.size() != indices.size()) {
            diagnostics->Add(
                Severity::kError,
                "array '" + base_name +
                    "' requires " + std::to_string(dims.size()) +
                    " index(es) in v0");
            return nullptr;
          }
          std::vector<std::unique_ptr<Expr>> cloned_indices;
          cloned_indices.reserve(indices.size());
          for (const auto* index_expr : indices) {
            auto cloned =
                CloneExprWithParams(*index_expr, rename, params, module,
                                    diagnostics, bindings);
            if (!cloned) {
              return nullptr;
            }
            cloned_indices.push_back(std::move(cloned));
          }
          auto flat_index = BuildFlatIndexExpr(dims, std::move(cloned_indices));
          auto base_ident = std::make_unique<Expr>();
          base_ident->kind = ExprKind::kIdentifier;
          base_ident->ident = rename(base_name);
          out->base = std::move(base_ident);
          out->index = std::move(flat_index);
          return out;
        }
      }
    }
    out->base = CloneExprWithParams(*expr.base, rename, params, module,
                                    diagnostics, bindings);
    out->index = CloneExprWithParams(*expr.index, rename, params, module,
                                     diagnostics, bindings);
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
          CloneExprWithParams(*element, rename, params, module, diagnostics,
                              bindings);
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
  if (statement.kind == StatementKind::kAssign) {
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
      if (dims.size() != statement.assign.lhs_indices.size()) {
        diagnostics->Add(Severity::kError,
                         "array '" + statement.assign.lhs + "' requires " +
                             std::to_string(dims.size()) +
                             " index(es) in v0");
        return false;
      }
      std::vector<std::unique_ptr<Expr>> cloned_indices;
      cloned_indices.reserve(statement.assign.lhs_indices.size());
      for (const auto& index_expr : statement.assign.lhs_indices) {
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
      if (!out->assign.lhs_msb_expr || !out->assign.lhs_lsb_expr ||
          !TryEvalConstExprWithParams(*out->assign.lhs_msb_expr, params, &msb) ||
          !TryEvalConstExprWithParams(*out->assign.lhs_lsb_expr, params, &lsb)) {
        diagnostics->Add(Severity::kError,
                         "part-select assignment indices must be constant in v0");
        return false;
      }
      out->assign.lhs_msb = static_cast<int>(msb);
      out->assign.lhs_lsb = static_cast<int>(lsb);
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
    out->kind = StatementKind::kBlock;
    if (!statement.repeat_count) {
      diagnostics->Add(Severity::kError, "malformed repeat in v0");
      return false;
    }
    int64_t count = 0;
    if (!EvalConstExprWithParams(*statement.repeat_count, params, &count,
                                 diagnostics, "repeat count")) {
      return false;
    }
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
                NetType type, const std::vector<int>& array_dims,
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
        &module, diagnostics, nullptr);
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
      break;
  }
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

  for (const auto& block : flat.always_blocks) {
    std::unordered_set<std::string> block_drives;
    for (const auto& stmt : block.statements) {
      CollectAssignedSignalsNoIndex(stmt, &block_drives);
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
                  bool enable_4state) {
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
      if (!AddFlatNet(net.name, width, net.is_signed, net.type, array_dims,
                      hier_prefix + "." + net.name, out, net_names,
                      flat_to_hier, diagnostics)) {
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
      if (!AddFlatNet(prefix + port.name, width, port.is_signed, type, {},
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
                      array_dims,
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

    ParamBindings child_params;
    if (!BuildParamBindings(*child, &instance, &params, &child_params,
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
    std::unordered_map<std::string, NetType> child_port_types;
    for (const auto& port : child->ports) {
      int width = port.width;
      if (!ResolveRangeWidth(port.width, port.msb_expr, port.lsb_expr,
                             child_params, &width, diagnostics,
                             "port '" + port.name + "'")) {
        return false;
      }
      NetType port_type = NetType::kWire;
      if (const Net* net = FindNet(*child, port.name)) {
        port_type = net->type;
      }
      child_ports.insert(port.name);
      child_port_dirs[port.name] = port.dir;
      child_port_widths[port.name] = width;
      child_port_signed[port.name] = port.is_signed;
      child_port_types[port.name] = port_type;
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

      std::string base_name;
      int msb = 0;
      int lsb = 0;
      if (resolved_expr->kind == ExprKind::kSelect &&
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
      out_assign.lhs_has_range = true;
      out_assign.lhs_msb = msb;
      out_assign.lhs_lsb = lsb;
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
                      child_port_signed[port_name], type, {},
                      child_hier + "." + port_name, out, net_names,
                      flat_to_hier, diagnostics)) {
        return false;
      }
      if (connected_ports.count(port_name) == 0) {
        if (port.dir == PortDir::kInput) {
          diagnostics->Add(Severity::kWarning,
                           "unconnected input '" + port.name +
                               "' in instance '" + instance.name +
                               "' (defaulting to " +
                               std::string(enable_4state ? "X" : "0") + ")");
          Assign default_assign;
          default_assign.lhs = port_net;
          default_assign.rhs = enable_4state
                                   ? MakeAllXExpr(child_port_widths[port_name])
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
                    enable_4state)) {
    return false;
  }

  if (!ValidateSwitches(flat, diagnostics)) {
    return false;
  }
  if (!ValidateSingleDrivers(flat, diagnostics, enable_4state)) {
    return false;
  }
  if (!ValidateCombinationalAcyclic(flat, diagnostics)) {
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
