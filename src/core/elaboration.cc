#include "core/elaboration.hh"

#include <cctype>
#include <functional>
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
    diagnostics->Add(Severity::kError, "no top-level module found");
    return false;
  }
  *top_name = candidate;
  return true;
}

struct PortBinding {
  std::string signal;
};

std::unique_ptr<Expr> CloneExpr(
    const Expr& expr,
    const std::function<std::string(const std::string&)>& rename) {
  auto out = std::make_unique<Expr>();
  out->kind = expr.kind;
  out->number = expr.number;
  out->number_width = expr.number_width;
  out->has_width = expr.has_width;
  out->has_base = expr.has_base;
  out->base_char = expr.base_char;
  out->op = expr.op;
  out->unary_op = expr.unary_op;
  if (expr.kind == ExprKind::kIdentifier) {
    out->ident = rename(expr.ident);
  } else if (expr.kind == ExprKind::kNumber) {
    out->number = expr.number;
    out->number_width = expr.number_width;
    out->has_width = expr.has_width;
  } else if (expr.kind == ExprKind::kUnary) {
    out->operand = CloneExpr(*expr.operand, rename);
  } else if (expr.kind == ExprKind::kBinary) {
    out->lhs = CloneExpr(*expr.lhs, rename);
    out->rhs = CloneExpr(*expr.rhs, rename);
  } else if (expr.kind == ExprKind::kTernary) {
    out->condition = CloneExpr(*expr.condition, rename);
    out->then_expr = CloneExpr(*expr.then_expr, rename);
    out->else_expr = CloneExpr(*expr.else_expr, rename);
  } else if (expr.kind == ExprKind::kSelect) {
    out->base = CloneExpr(*expr.base, rename);
    out->msb = expr.msb;
    out->lsb = expr.lsb;
    out->has_range = expr.has_range;
  } else if (expr.kind == ExprKind::kConcat) {
    out->repeat = expr.repeat;
    for (const auto& element : expr.elements) {
      out->elements.push_back(CloneExpr(*element, rename));
    }
  }
  return out;
}

Statement CloneStatement(
    const Statement& statement,
    const std::function<std::string(const std::string&)>& rename) {
  Statement out;
  out.kind = statement.kind;
  if (statement.kind == StatementKind::kAssign) {
    out.assign.lhs = rename(statement.assign.lhs);
    out.assign.rhs = statement.assign.rhs
                         ? CloneExpr(*statement.assign.rhs, rename)
                         : nullptr;
    out.assign.nonblocking = statement.assign.nonblocking;
  } else if (statement.kind == StatementKind::kIf) {
    out.condition = statement.condition
                        ? CloneExpr(*statement.condition, rename)
                        : nullptr;
    for (const auto& stmt : statement.then_branch) {
      out.then_branch.push_back(CloneStatement(stmt, rename));
    }
    for (const auto& stmt : statement.else_branch) {
      out.else_branch.push_back(CloneStatement(stmt, rename));
    }
  } else if (statement.kind == StatementKind::kBlock) {
    for (const auto& stmt : statement.block) {
      out.block.push_back(CloneStatement(stmt, rename));
    }
  }
  return out;
}

bool AddFlatNet(const std::string& name, int width, NetType type,
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
  out->nets.push_back(Net{type, name, width});
  net_names->insert(name);
  (*flat_to_hier)[name] = hier_path;
  return true;
}

std::unique_ptr<Expr> MakeNumberExpr(uint64_t value) {
  auto expr = std::make_unique<Expr>();
  expr->kind = ExprKind::kNumber;
  expr->number = value;
  return expr;
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
  }
}

bool ValidateSingleDrivers(const Module& flat, Diagnostics* diagnostics) {
  std::unordered_map<std::string, std::string> drivers;
  for (const auto& assign : flat.assigns) {
    if (drivers.count(assign.lhs) > 0) {
      diagnostics->Add(Severity::kError,
                       "multiple drivers for signal '" + assign.lhs + "'");
      return false;
    }
    drivers[assign.lhs] = "assign";
  }

  for (const auto& block : flat.always_blocks) {
    std::unordered_set<std::string> block_drives;
    for (const auto& stmt : block.statements) {
      CollectAssignedSignals(stmt, &block_drives);
    }
    for (const auto& name : block_drives) {
      if (drivers.count(name) > 0) {
        diagnostics->Add(Severity::kError,
                         "multiple drivers for signal '" + name + "'");
        return false;
      }
      drivers[name] = "always";
    }
  }
  return true;
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
    if (!IsDeclaredSignal(flat, block.clock)) {
      diagnostics->Add(Severity::kWarning,
                       "clock '" + block.clock +
                           "' in always block is not declared");
    }
  }
}

bool InlineModule(const Program& program, const Module& module,
                  const std::string& prefix, const std::string& hier_prefix,
                  const std::unordered_map<std::string, PortBinding>& port_map,
                  Module* out, Diagnostics* diagnostics,
                  std::unordered_set<std::string>* stack,
                  std::unordered_set<std::string>* net_names,
                  std::unordered_map<std::string, std::string>* flat_to_hier) {
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
    out->ports = module.ports;
    out->parameters.clear();
    for (const auto& param : module.parameters) {
      Parameter flat_param;
      flat_param.name = param.name;
      flat_param.value =
          param.value ? CloneExpr(*param.value, rename) : nullptr;
      out->parameters.push_back(std::move(flat_param));
    }
    for (const auto& port : module.ports) {
      (*flat_to_hier)[port.name] = hier_prefix + "." + port.name;
    }
    for (const auto& net : module.nets) {
      if (!AddFlatNet(net.name, net.width, net.type,
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
      NetType type = lookup_type(port.name);
      if (!AddFlatNet(prefix + port.name, port.width, type,
                      hier_prefix + "." + port.name, out, net_names,
                      flat_to_hier, diagnostics)) {
        return false;
      }
    }
    for (const auto& net : module.nets) {
      if (!AddFlatNet(prefix + net.name, net.width, net.type,
                      hier_prefix + "." + net.name, out, net_names,
                      flat_to_hier, diagnostics)) {
        return false;
      }
    }
  }

  for (const auto& assign : module.assigns) {
    Assign flattened;
    flattened.lhs = rename(assign.lhs);
    flattened.rhs = assign.rhs ? CloneExpr(*assign.rhs, rename) : nullptr;
    out->assigns.push_back(std::move(flattened));
  }
  for (const auto& block : module.always_blocks) {
    AlwaysBlock flattened;
    flattened.edge = block.edge;
    flattened.clock = rename(block.clock);
    for (const auto& stmt : block.statements) {
      flattened.statements.push_back(CloneStatement(stmt, rename));
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

    std::unordered_map<std::string, PortBinding> child_port_map;
    std::unordered_set<std::string> child_ports;
    std::unordered_map<std::string, PortDir> child_port_dirs;
    std::unordered_map<std::string, int> child_port_widths;
    for (const auto& port : child->ports) {
      child_ports.insert(port.name);
      child_port_dirs[port.name] = port.dir;
      child_port_widths[port.name] = port.width;
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
        diagnostics->Add(Severity::kError,
                         "missing connection expression in instance '" +
                             instance.name + "'");
        return false;
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
        if (!AddFlatNet(literal_name, width, NetType::kWire,
                        hier_prefix + "." + instance.name + "." + port_name +
                            ".__lit",
                        out, net_names, flat_to_hier, diagnostics)) {
          return false;
        }
        child_port_map[port_name] = PortBinding{literal_name};
        out->assigns.push_back(
            Assign{literal_name, MakeNumberExpr(connection.expr->number)});
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
                               "' (defaulting to 0)");
          std::string default_name = child_prefix + port.name;
          if (!AddFlatNet(default_name, child_port_widths[port.name],
                          NetType::kWire, child_hier + "." + port.name, out,
                          net_names, flat_to_hier, diagnostics)) {
            return false;
          }
          child_port_map[port.name] = PortBinding{default_name};
          out->assigns.push_back(Assign{default_name, MakeNumberExpr(0)});
        } else {
          diagnostics->Add(Severity::kWarning,
                           "unconnected output '" + port.name +
                               "' in instance '" + instance.name + "'");
        }
      }
    }

    if (!InlineModule(program, *child, child_prefix, child_hier,
                      child_port_map, out, diagnostics, stack, net_names,
                      flat_to_hier)) {
      return false;
    }
  }

  stack->erase(module.name);
  return true;
}

}  // namespace

bool Elaborate(const Program& program, ElaboratedDesign* out_design,
               Diagnostics* diagnostics) {
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
  return Elaborate(program, top_name, out_design, diagnostics);
}

bool Elaborate(const Program& program, const std::string& top_name,
               ElaboratedDesign* out_design, Diagnostics* diagnostics) {
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
  std::unordered_map<std::string, PortBinding> port_map;
  std::unordered_set<std::string> stack;
  std::unordered_set<std::string> net_names;
  std::unordered_map<std::string, std::string> flat_to_hier;
  if (!InlineModule(program, *top, "", top->name, port_map, &flat, diagnostics,
                    &stack, &net_names, &flat_to_hier)) {
    return false;
  }

  if (!ValidateSingleDrivers(flat, diagnostics)) {
    return false;
  }
  WarnUndeclaredClocks(flat, diagnostics);

  out_design->top = std::move(flat);
  out_design->flat_to_hier = std::move(flat_to_hier);
  return true;
}

}  // namespace gpga
