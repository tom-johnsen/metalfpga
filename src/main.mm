#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "codegen/host_codegen.hh"
#include "codegen/msl_codegen.hh"
#include "core/elaboration.hh"
#include "frontend/verilog_parser.hh"
#include "utils/diagnostics.hh"

namespace {

void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " <input.v> [--emit-msl <path>] [--emit-host <path>]"
            << " [--dump-flat] [--top <module>]\n";
}

bool WriteFile(const std::string& path, const std::string& content,
               gpga::Diagnostics* diagnostics) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to open output file",
                     gpga::SourceLocation{path});
    return false;
  }
  out << content;
  if (!out) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to write output file",
                     gpga::SourceLocation{path});
    return false;
  }
  return true;
}

std::string DirLabel(gpga::PortDir dir) {
  switch (dir) {
    case gpga::PortDir::kInput:
      return "input";
    case gpga::PortDir::kOutput:
      return "output";
    case gpga::PortDir::kInout:
      return "inout";
  }
  return "unknown";
}

int SignalWidth(const gpga::Module& module, const std::string& name) {
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

int ExprWidth(const gpga::Expr& expr, const gpga::Module& module) {
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      return SignalWidth(module, expr.ident);
    case gpga::ExprKind::kNumber:
      if (expr.has_width && expr.number_width > 0) {
        return expr.number_width;
      }
      return MinimalWidth(expr.number);
    case gpga::ExprKind::kUnary:
      return expr.operand ? ExprWidth(*expr.operand, module) : 32;
    case gpga::ExprKind::kBinary:
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G') {
        return 1;
      }
      if (expr.op == 'l' || expr.op == 'r') {
        return expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      }
      return std::max(expr.lhs ? ExprWidth(*expr.lhs, module) : 32,
                      expr.rhs ? ExprWidth(*expr.rhs, module) : 32);
    case gpga::ExprKind::kTernary:
      return std::max(expr.then_expr ? ExprWidth(*expr.then_expr, module) : 32,
                      expr.else_expr ? ExprWidth(*expr.else_expr, module) : 32);
    case gpga::ExprKind::kSelect: {
      int lo = std::min(expr.msb, expr.lsb);
      int hi = std::max(expr.msb, expr.lsb);
      return hi - lo + 1;
    }
    case gpga::ExprKind::kConcat: {
      int total = 0;
      for (const auto& element : expr.elements) {
        total += ExprWidth(*element, module);
      }
      return total * std::max(1, expr.repeat);
    }
  }
  return 32;
}

bool IsAllOnesExpr(const gpga::Expr& expr, const gpga::Module& module,
                   int* width_out) {
  switch (expr.kind) {
    case gpga::ExprKind::kNumber: {
      int width = expr.has_width && expr.number_width > 0
                      ? expr.number_width
                      : MinimalWidth(expr.number);
      if (width_out) {
        *width_out = width;
      }
      if (width <= 0) {
        return false;
      }
      if (width > 64) {
        return false;
      }
      uint64_t mask = (width == 64)
                          ? std::numeric_limits<uint64_t>::max()
                          : ((1ULL << width) - 1ULL);
      return expr.number == mask;
    }
    case gpga::ExprKind::kConcat: {
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

const gpga::Expr* SimplifyAssignMask(const gpga::Expr& expr,
                                     const gpga::Module& module,
                                     int lhs_width) {
  if (expr.kind != gpga::ExprKind::kBinary || expr.op != '&') {
    return nullptr;
  }
  if (!expr.lhs || !expr.rhs) {
    return nullptr;
  }
  int lhs_expr_width = ExprWidth(*expr.lhs, module);
  int rhs_expr_width = ExprWidth(*expr.rhs, module);
  int target = std::max(lhs_expr_width, rhs_expr_width);
  if (lhs_expr_width == lhs_width &&
      IsAllOnesExpr(*expr.rhs, module, nullptr) &&
      ExprWidth(*expr.rhs, module) == target) {
    return expr.lhs.get();
  }
  if (rhs_expr_width == lhs_width &&
      IsAllOnesExpr(*expr.lhs, module, nullptr) &&
      ExprWidth(*expr.lhs, module) == target) {
    return expr.rhs.get();
  }
  return nullptr;
}

bool IsCompareOp(char op) {
  return op == 'E' || op == 'N' || op == '<' || op == '>' || op == 'L' ||
         op == 'G';
}

bool IsShiftOp(char op) { return op == 'l' || op == 'r'; }

std::string ExprToString(const gpga::Expr& expr, const gpga::Module& module) {
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      return expr.ident;
    case gpga::ExprKind::kNumber:
    if (expr.has_base) {
      uint64_t value = expr.number;
      int base = 10;
      const char* digits = "0123456789ABCDEF";
        switch (expr.base_char) {
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
            base = 10;
            break;
        }
        std::string repr;
        if (value == 0) {
          repr = "0";
        } else {
          while (value > 0) {
            int digit = static_cast<int>(value % base);
            repr.insert(repr.begin(), digits[digit]);
            value /= static_cast<uint64_t>(base);
          }
        }
        std::string prefix;
        if (expr.has_width && expr.number_width > 0) {
          prefix = std::to_string(expr.number_width);
        }
        return prefix + "'" + std::string(1, expr.base_char) + repr;
      }
      if (expr.has_width && expr.number_width > 0) {
        return std::to_string(expr.number_width) + "'d" +
               std::to_string(expr.number);
      }
      return std::to_string(expr.number);
    case gpga::ExprKind::kUnary: {
      std::string operand =
          expr.operand ? ExprToString(*expr.operand, module) : "0";
      return std::string(1, expr.unary_op) + operand;
    }
    case gpga::ExprKind::kBinary:
      {
        int lhs_width = expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
        int rhs_width = expr.rhs ? ExprWidth(*expr.rhs, module) : 32;
        int target = std::max(lhs_width, rhs_width);
        std::string lhs = expr.lhs ? ExprToString(*expr.lhs, module) : "0";
        std::string rhs = expr.rhs ? ExprToString(*expr.rhs, module) : "0";
        if (!IsShiftOp(expr.op)) {
          if (lhs_width < target) {
            lhs = "zext(" + lhs + ", " + std::to_string(target) + ")";
          }
          if (rhs_width < target) {
            rhs = "zext(" + rhs + ", " + std::to_string(target) + ")";
          }
        }
        if (expr.op == 'E') {
          return "(" + lhs + " == " + rhs + ")";
        }
        if (expr.op == 'N') {
          return "(" + lhs + " != " + rhs + ")";
        }
        if (expr.op == 'L') {
          return "(" + lhs + " <= " + rhs + ")";
        }
        if (expr.op == 'G') {
          return "(" + lhs + " >= " + rhs + ")";
        }
        if (expr.op == 'l') {
          return "(" + lhs + " << " + rhs + ")";
        }
        if (expr.op == 'r') {
          return "(" + lhs + " >> " + rhs + ")";
        }
        return "(" + lhs + " " + expr.op + " " + rhs + ")";
      }
    case gpga::ExprKind::kTernary: {
      std::string cond =
          expr.condition ? ExprToString(*expr.condition, module) : "0";
      std::string then_expr =
          expr.then_expr ? ExprToString(*expr.then_expr, module) : "0";
      std::string else_expr =
          expr.else_expr ? ExprToString(*expr.else_expr, module) : "0";
      return "(" + cond + " ? " + then_expr + " : " + else_expr + ")";
    }
    case gpga::ExprKind::kSelect: {
      std::string base = ExprToString(*expr.base, module);
      if (expr.has_range) {
        return base + "[" + std::to_string(expr.msb) + ":" +
               std::to_string(expr.lsb) + "]";
      }
      return base + "[" + std::to_string(expr.msb) + "]";
    }
    case gpga::ExprKind::kConcat: {
      std::string inner;
      for (size_t i = 0; i < expr.elements.size(); ++i) {
        if (i > 0) {
          inner += ", ";
        }
        inner += ExprToString(*expr.elements[i], module);
      }
      if (expr.repeat > 1) {
        return "{" + std::to_string(expr.repeat) + "{" + inner + "}}";
      }
      return "{" + inner + "}";
    }
  }
  return "<expr>";
}

void DumpStatement(const gpga::Statement& stmt, const gpga::Module& module,
                   int indent, std::ostream& os) {
  std::string pad(static_cast<size_t>(indent), ' ');
  if (stmt.kind == gpga::StatementKind::kAssign) {
    if (stmt.assign.rhs) {
      os << pad << stmt.assign.lhs
         << (stmt.assign.nonblocking ? " <= " : " = ")
         << ExprToString(*stmt.assign.rhs, module) << ";\n";
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kIf) {
    std::string cond =
        stmt.condition ? ExprToString(*stmt.condition, module) : "0";
    os << pad << "if (" << cond << ") {\n";
    for (const auto& inner : stmt.then_branch) {
      DumpStatement(inner, module, indent + 2, os);
    }
    if (!stmt.else_branch.empty()) {
      os << pad << "} else {\n";
      for (const auto& inner : stmt.else_branch) {
        DumpStatement(inner, module, indent + 2, os);
      }
    }
    os << pad << "}\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kBlock) {
    os << pad << "begin\n";
    for (const auto& inner : stmt.block) {
      DumpStatement(inner, module, indent + 2, os);
    }
    os << pad << "end\n";
  }
}

void DumpFlat(const gpga::ElaboratedDesign& design, std::ostream& os) {
  const auto& top = design.top;
  os << "Top: " << top.name << "\n";
  if (!top.parameters.empty()) {
    os << "Parameters:\n";
    for (const auto& param : top.parameters) {
      if (param.value) {
        os << "  - " << param.name << " = "
           << ExprToString(*param.value, top) << "\n";
      } else {
        os << "  - " << param.name << "\n";
      }
    }
  }
  os << "Ports:\n";
  for (const auto& port : top.ports) {
    os << "  - " << DirLabel(port.dir) << " " << port.name
       << " [" << port.width << "]\n";
  }
  os << "Nets:\n";
  for (const auto& net : top.nets) {
    const char* type = (net.type == gpga::NetType::kReg) ? "reg" : "wire";
    os << "  - " << type << " " << net.name << " [" << net.width << "]\n";
  }
  os << "Assigns:\n";
  for (const auto& assign : top.assigns) {
    if (assign.rhs) {
      int lhs_width = SignalWidth(top, assign.lhs);
      const gpga::Expr* rhs_expr = assign.rhs.get();
      if (const gpga::Expr* simplified =
              SimplifyAssignMask(*assign.rhs, top, lhs_width)) {
        rhs_expr = simplified;
      }
      int rhs_width = ExprWidth(*rhs_expr, top);
      std::string rhs = ExprToString(*rhs_expr, top);
      if (rhs_width < lhs_width) {
        rhs = "zext(" + rhs + ", " + std::to_string(lhs_width) + ")";
      } else if (rhs_width > lhs_width) {
        rhs = "trunc(" + rhs + ", " + std::to_string(lhs_width) + ")";
      }
      os << "  - " << assign.lhs << " = " << rhs << "\n";
    }
  }
  os << "Always blocks:\n";
  for (const auto& block : top.always_blocks) {
    os << "  - always @("
       << (block.edge == gpga::EdgeKind::kPosedge ? "posedge " : "negedge ")
       << block.clock << ")\n";
    for (const auto& stmt : block.statements) {
      DumpStatement(stmt, top, 4, os);
    }
  }
  os << "Flat name map:\n";
  for (const auto& entry : design.flat_to_hier) {
    os << "  - " << entry.first << " -> " << entry.second << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 2;
  }

  std::string input_path;
  std::string msl_out;
  std::string host_out;
  std::string top_name;
  bool dump_flat = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--emit-msl") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      msl_out = argv[++i];
    } else if (arg == "--dump-flat") {
      dump_flat = true;
    } else if (arg == "--top") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      top_name = argv[++i];
    } else if (arg == "--emit-host") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      host_out = argv[++i];
    } else if (!arg.empty() && arg[0] == '-') {
      PrintUsage(argv[0]);
      return 2;
    } else if (input_path.empty()) {
      input_path = arg;
    } else {
      PrintUsage(argv[0]);
      return 2;
    }
  }

  if (input_path.empty()) {
    PrintUsage(argv[0]);
    return 2;
  }

  gpga::Diagnostics diagnostics;
  gpga::Program program;
  if (!gpga::ParseVerilogFile(input_path, &program, &diagnostics) ||
      diagnostics.HasErrors()) {
    diagnostics.RenderTo(std::cerr);
    return 1;
  }

  gpga::ElaboratedDesign design;
  bool elaborated = false;
  if (!top_name.empty()) {
    elaborated = gpga::Elaborate(program, top_name, &design, &diagnostics);
  } else {
    elaborated = gpga::Elaborate(program, &design, &diagnostics);
  }
  if (!elaborated || diagnostics.HasErrors()) {
    diagnostics.RenderTo(std::cerr);
    return 1;
  }

  if (!msl_out.empty()) {
    std::string msl = gpga::EmitMSLStub(design.top);
    if (!WriteFile(msl_out, msl, &diagnostics)) {
      diagnostics.RenderTo(std::cerr);
      return 1;
    }
  }

  if (!host_out.empty()) {
    std::string host = gpga::EmitHostStub(design.top);
    if (!WriteFile(host_out, host, &diagnostics)) {
      diagnostics.RenderTo(std::cerr);
      return 1;
    }
  }

  if (msl_out.empty() && host_out.empty()) {
    std::cout << "Elaborated top module '" << design.top.name
              << "'. Use --emit-msl/--emit-host to write stubs.\n";
  }
  if (dump_flat) {
    DumpFlat(design, std::cout);
  }

  return 0;
}
