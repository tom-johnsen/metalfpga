#include "codegen/msl_codegen.hh"

#include <algorithm>
#include <cstdint>
#include <functional>
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

std::string ZeroForWidth(int width) {
  return (width > 32) ? "0ul" : "0u";
}

std::string CastForWidth(int width) {
  return (width > 32) ? "(ulong)" : "";
}

std::string MaskForWidthExpr(const std::string& expr, int width) {
  if (width >= 64) {
    return expr;
  }
  uint64_t mask = MaskForWidth64(width);
  std::string suffix = (width > 32) ? "ul" : "u";
  return "((" + expr + ") & " + std::to_string(mask) + suffix + ")";
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
      return expr.operand ? ExprWidth(*expr.operand, module) : 32;
    case ExprKind::kBinary: {
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G') {
        return 1;
      }
      if (expr.op == 'l' || expr.op == 'r') {
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
      return (hi - lo + 1);
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
      uint64_t mask = MaskForWidth64(width);
      std::string mask_suffix = wide ? "ul" : "u";
      std::string cast = wide ? "(ulong)" : "";
      acc = "(" + acc + " | ((" + cast + part + " & " +
            std::to_string(mask) + mask_suffix + ") << " +
            std::to_string(shift) + "u))";
    }
  }
  return acc;
}

bool IsOutputPort(const Module& module, const std::string& name) {
  const Port* port = FindPort(module, name);
  return port && (port->dir == PortDir::kOutput || port->dir == PortDir::kInout);
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
      if (expr.op == 'l' || expr.op == 'r') {
        int width = lhs_width;
        std::string zero = ZeroForWidth(width);
        std::string cast = CastForWidth(width);
        std::string lhs_masked = MaskForWidthExpr(lhs, width);
        std::string op = (expr.op == 'l') ? "<<" : ">>";
        return "((" + rhs + ") >= " + std::to_string(width) + "u ? " + zero +
               " : (" + cast + lhs_masked + " " + op + " " + rhs + "))";
      }
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G') {
        std::string lhs_ext = ExtendExpr(lhs, lhs_width, target_width);
        std::string rhs_ext = ExtendExpr(rhs, rhs_width, target_width);
        return "((" + lhs_ext + " " + BinaryOpString(expr.op) + " " + rhs_ext +
               ") ? 1u : 0u)";
      }
      std::string lhs_ext = ExtendExpr(lhs, lhs_width, target_width);
      std::string rhs_ext = ExtendExpr(rhs, rhs_width, target_width);
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
      int lo = std::min(expr.msb, expr.lsb);
      int hi = std::max(expr.msb, expr.lsb);
      int width = hi - lo + 1;
      int base_width = ExprWidth(*expr.base, module);
      bool wide = base_width > 32 || width > 32;
      uint64_t mask = MaskForWidth64(width);
      std::string mask_suffix = wide ? "ul" : "u";
      return "((" + base + " >> " + std::to_string(lo) + "u) & " +
             std::to_string(mask) + mask_suffix + ")";
    }
    case ExprKind::kConcat:
      return EmitConcatExpr(expr, module, locals, regs);
  }
  return "0u";
}

}  // namespace

std::string EmitMSLStub(const Module& module) {
  std::ostringstream out;
  out << "#include <metal_stdlib>\n";
  out << "using namespace metal;\n\n";
  out << "struct GpgaParams { uint count; };\n\n";
  out << "// Placeholder MSL emitted by GPGA.\n\n";
  std::vector<std::string> reg_names;
  for (const auto& net : module.nets) {
    if (net.type == NetType::kReg && !IsOutputPort(module, net.name)) {
      reg_names.push_back(net.name);
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
    if (net.type == NetType::kWire) {
      locals.insert(net.name);
    } else if (net.type == NetType::kReg) {
      regs.insert(net.name);
    }
  }

  for (const auto& assign : module.assigns) {
    if (!assign.rhs) {
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
  out << "}\n";

  if (!module.always_blocks.empty()) {
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
    if (!first) {
      out << ",\n";
    }
    out << "  constant GpgaParams& params [[buffer(" << buffer_index++
        << ")]],\n";
    out << "  uint gid [[thread_position_in_grid]]) {\n";
    out << "  if (gid >= params.count) {\n";
    out << "    return;\n";
    out << "  }\n";
    out << "  // Tick kernel: sequential logic (posedge only in v0).\n";

    auto lvalue_expr = [&](const std::string& name) -> std::string {
      if (IsOutputPort(module, name) || regs.count(name) > 0) {
        return name + "[gid]";
      }
      return name;
    };

    std::function<void(const Statement&, int,
                       const std::unordered_map<std::string, std::string>&)>
        emit_stmt;
    emit_stmt = [&](const Statement& stmt, int indent,
                    const std::unordered_map<std::string, std::string>& nb_map) {
      std::string pad(indent, ' ');
      if (stmt.kind == StatementKind::kAssign) {
        if (!stmt.assign.rhs) {
          return;
        }
        std::string expr = EmitExpr(*stmt.assign.rhs, module, locals, regs);
        const std::string& lhs = stmt.assign.lhs;
        if (!IsOutputPort(module, lhs) && regs.count(lhs) == 0) {
          out << pad << "// Unmapped sequential assign: " << lhs << " = "
              << expr << ";\n";
          return;
        }
        int lhs_width = SignalWidth(module, lhs);
        std::string sized = EmitExprSized(*stmt.assign.rhs, lhs_width, module, locals, regs);
        if (stmt.assign.nonblocking) {
          auto it = nb_map.find(lhs);
          if (it != nb_map.end()) {
            out << pad << it->second << " = " << sized << ";\n";
          } else {
            out << pad << lvalue_expr(lhs) << " = " << sized << ";\n";
          }
        } else {
          out << pad << lvalue_expr(lhs) << " = " << sized << ";\n";
        }
        return;
      }
      if (stmt.kind == StatementKind::kIf) {
        std::string cond = stmt.condition
                               ? EmitExpr(*stmt.condition, module, locals, regs)
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
      if (stmt.kind == StatementKind::kAssign && stmt.assign.nonblocking) {
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
      }
    };

    for (const auto& block : module.always_blocks) {
      out << "  // always @(";
      out << (block.edge == EdgeKind::kPosedge ? "posedge " : "negedge ");
      out << block.clock << ")\n";

      std::unordered_set<std::string> nb_targets;
      for (const auto& stmt : block.statements) {
        collect_nb_targets(stmt, &nb_targets, collect_nb_targets);
      }
      std::unordered_map<std::string, std::string> nb_map;
      for (const auto& target : nb_targets) {
        if (!IsOutputPort(module, target) && regs.count(target) == 0) {
          continue;
        }
        std::string temp = "nb_" + target;
        nb_map[target] = temp;
        std::string type = TypeForWidth(SignalWidth(module, target));
        out << "  " << type << " " << temp << " = " << lvalue_expr(target)
            << ";\n";
      }

      for (const auto& stmt : block.statements) {
        emit_stmt(stmt, 2, nb_map);
      }

      for (const auto& entry : nb_map) {
        out << "  " << lvalue_expr(entry.first) << " = " << entry.second
            << ";\n";
      }
    }
    out << "}\n";
  }
  return out.str();
}

}  // namespace gpga
