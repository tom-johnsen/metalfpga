#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gpga {

enum class PortDir {
  kInput,
  kOutput,
  kInout,
};

struct Port {
  PortDir dir = PortDir::kInput;
  std::string name;
  int width = 1;
};

struct Expr;

enum class NetType {
  kWire,
  kReg,
};

struct Net {
  NetType type = NetType::kWire;
  std::string name;
  int width = 1;
};

enum class ExprKind {
  kIdentifier,
  kNumber,
  kUnary,
  kBinary,
  kTernary,
  kSelect,
  kConcat,
};

struct Expr {
  ExprKind kind = ExprKind::kIdentifier;
  std::string ident;
  uint64_t number = 0;
  int number_width = 0;
  bool has_width = false;
  bool has_base = false;
  char base_char = 'd';
  char op = 0;
  char unary_op = 0;
  std::unique_ptr<Expr> operand;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Expr> then_expr;
  std::unique_ptr<Expr> else_expr;
  std::unique_ptr<Expr> base;
  int msb = 0;
  int lsb = 0;
  bool has_range = false;
  std::vector<std::unique_ptr<Expr>> elements;
  int repeat = 1;
};

struct Parameter {
  std::string name;
  std::unique_ptr<Expr> value;
};

struct Assign {
  std::string lhs;
  std::unique_ptr<Expr> rhs;
};

struct SequentialAssign {
  std::string lhs;
  std::unique_ptr<Expr> rhs;
  bool nonblocking = true;
};

enum class StatementKind {
  kAssign,
  kIf,
  kBlock,
};

struct Statement {
  StatementKind kind = StatementKind::kAssign;
  SequentialAssign assign;
  std::unique_ptr<Expr> condition;
  std::vector<Statement> then_branch;
  std::vector<Statement> else_branch;
  std::vector<Statement> block;
};

enum class EdgeKind {
  kPosedge,
  kNegedge,
};

struct AlwaysBlock {
  EdgeKind edge = EdgeKind::kPosedge;
  std::string clock;
  std::vector<Statement> statements;
};

struct Connection {
  std::string port;
  std::unique_ptr<Expr> expr;
};

struct ParamOverride {
  std::string name;
  std::unique_ptr<Expr> expr;
};

struct Instance {
  std::string module_name;
  std::string name;
  std::vector<ParamOverride> param_overrides;
  std::vector<Connection> connections;
};

struct Module {
  std::string name;
  std::vector<Port> ports;
  std::vector<Net> nets;
  std::vector<Assign> assigns;
  std::vector<Instance> instances;
  std::vector<AlwaysBlock> always_blocks;
  std::vector<Parameter> parameters;
};

struct Program {
  std::vector<Module> modules;
};

}  // namespace gpga
