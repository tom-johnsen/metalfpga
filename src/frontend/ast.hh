#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gpga {

enum class PortDir {
  kInput,
  kOutput,
  kInout,
};

struct Expr;

struct Port {
  PortDir dir = PortDir::kInput;
  std::string name;
  int width = 1;
  bool is_signed = false;
  bool is_real = false;
  bool is_declared = false;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
};

struct Expr;

enum class NetType {
  kWire,
  kReg,
  kWand,
  kWor,
  kTri0,
  kTri1,
  kTriand,
  kTrior,
  kTrireg,
  kSupply0,
  kSupply1,
};

enum class Strength {
  kHighZ,
  kWeak,
  kPull,
  kStrong,
  kSupply,
};

enum class ChargeStrength {
  kNone,
  kSmall,
  kMedium,
  kLarge,
};

enum class UnconnectedDrive {
  kNone,
  kPull0,
  kPull1,
};

enum class SwitchKind {
  kTran,
  kTranif1,
  kTranif0,
  kCmos,
};

struct Switch {
  SwitchKind kind = SwitchKind::kTran;
  std::string a;
  std::string b;
  std::unique_ptr<Expr> control;
  std::unique_ptr<Expr> control_n;
  Strength strength0 = Strength::kStrong;
  Strength strength1 = Strength::kStrong;
  bool has_strength = false;
};

struct ArrayDim {
  int size = 0;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
};

struct Net {
  NetType type = NetType::kWire;
  std::string name;
  int width = 1;
  bool is_signed = false;
  bool is_real = false;
  ChargeStrength charge = ChargeStrength::kNone;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
  int array_size = 0;
  std::vector<ArrayDim> array_dims;
};

enum class ExprKind {
  kIdentifier,
  kNumber,
  kString,
  kUnary,
  kBinary,
  kTernary,
  kSelect,
  kIndex,
  kCall,
  kConcat,
};

struct Expr {
  ExprKind kind = ExprKind::kIdentifier;
  std::string ident;
  std::string string_value;
  uint64_t number = 0;
  uint64_t value_bits = 0;
  uint64_t x_bits = 0;
  uint64_t z_bits = 0;
  int number_width = 0;
  bool has_width = false;
  bool has_base = false;
  char base_char = 'd';
  bool is_signed = false;
  bool is_real_literal = false;
  char op = 0;
  char unary_op = 0;
  std::unique_ptr<Expr> operand;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Expr> then_expr;
  std::unique_ptr<Expr> else_expr;
  std::unique_ptr<Expr> base;
  std::unique_ptr<Expr> index;
  int msb = 0;
  int lsb = 0;
  bool has_range = false;
  bool indexed_range = false;
  bool indexed_desc = false;
  int indexed_width = 0;
  std::unique_ptr<Expr> msb_expr;
  std::unique_ptr<Expr> lsb_expr;
  std::vector<std::unique_ptr<Expr>> elements;
  int repeat = 1;
  std::unique_ptr<Expr> repeat_expr;
  std::vector<std::unique_ptr<Expr>> call_args;

  bool HasX() const { return x_bits != 0; }
  bool HasZ() const { return z_bits != 0; }
  bool IsFullyDetermined() const { return x_bits == 0 && z_bits == 0; }
};

struct Parameter {
  std::string name;
  std::unique_ptr<Expr> value;
  bool is_local = false;
  bool is_real = false;
};

struct Statement;

struct FunctionArg {
  std::string name;
  int width = 1;
  bool is_signed = false;
  bool is_real = false;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
};

struct LocalVar {
  std::string name;
  int width = 1;
  bool is_signed = false;
  bool is_real = false;
};

struct Function {
  std::string name;
  int width = 1;
  bool is_signed = false;
  bool is_real = false;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
  std::vector<FunctionArg> args;
  std::vector<LocalVar> locals;
  std::vector<Statement> body;
  std::unique_ptr<Expr> body_expr;
};

struct Assign {
  std::string lhs;
  int lhs_msb = 0;
  int lhs_lsb = 0;
  bool lhs_has_range = false;
  std::unique_ptr<Expr> rhs;
  Strength strength0 = Strength::kStrong;
  Strength strength1 = Strength::kStrong;
  bool has_strength = false;
  bool is_implicit = false;
  bool is_derived = false;
  int origin_depth = 0;
};

struct SequentialAssign {
  std::string lhs;
  std::unique_ptr<Expr> lhs_index;
  std::vector<std::unique_ptr<Expr>> lhs_indices;
  bool lhs_has_range = false;
  bool lhs_indexed_range = false;
  bool lhs_indexed_desc = false;
  int lhs_indexed_width = 0;
  int lhs_msb = 0;
  int lhs_lsb = 0;
  std::unique_ptr<Expr> lhs_msb_expr;
  std::unique_ptr<Expr> lhs_lsb_expr;
  std::unique_ptr<Expr> rhs;
  std::unique_ptr<Expr> delay;
  bool nonblocking = true;
};

enum class StatementKind {
  kAssign,
  kIf,
  kBlock,
  kCase,
  kFor,
  kWhile,
  kRepeat,
  kDelay,
  kEventControl,
  kEventTrigger,
  kWait,
  kForever,
  kFork,
  kDisable,
  kTaskCall,
  kForce,
  kRelease,
};

enum class CaseKind {
  kCase,
  kCaseZ,
  kCaseX,
};

enum class EventEdgeKind {
  kAny,
  kPosedge,
  kNegedge,
};

enum class TimingCheckKind {
  kSetup,
  kHold,
  kSetupHold,
  kRecovery,
  kRemoval,
  kRecRem,
  kSkew,
  kTimeSkew,
  kFullSkew,
  kWidth,
  kPeriod,
  kPulseWidth,
  kNoChange,
};

enum class TimingEdgeState {
  k0,
  k1,
  kX,
  kZ,
};

struct TimingEdgePattern {
  TimingEdgeState from = TimingEdgeState::k0;
  TimingEdgeState to = TimingEdgeState::k0;
  std::string raw;
};

struct TimingCheckLimit {
  std::unique_ptr<Expr> min;
  std::unique_ptr<Expr> typ;
  std::unique_ptr<Expr> max;
};

struct TimingCheckEvent {
  EventEdgeKind edge = EventEdgeKind::kAny;
  bool has_edge_list = false;
  std::vector<TimingEdgePattern> edge_list;
  std::unique_ptr<Expr> expr;
  std::unique_ptr<Expr> cond;
  std::string raw_expr;
  std::string raw_cond;
};

struct EventItem {
  EventEdgeKind edge = EventEdgeKind::kAny;
  std::unique_ptr<Expr> expr;
};

struct Statement;

struct CaseItem {
  std::vector<std::unique_ptr<Expr>> labels;
  std::vector<Statement> body;
};

struct Statement {
  StatementKind kind = StatementKind::kAssign;
  CaseKind case_kind = CaseKind::kCase;
  SequentialAssign assign;
  bool is_procedural = false;
  std::string for_init_lhs;
  std::unique_ptr<Expr> for_init_rhs;
  std::unique_ptr<Expr> for_condition;
  std::string for_step_lhs;
  std::unique_ptr<Expr> for_step_rhs;
  std::vector<Statement> for_body;
  std::unique_ptr<Expr> while_condition;
  std::vector<Statement> while_body;
  std::unique_ptr<Expr> repeat_count;
  std::vector<Statement> repeat_body;
  std::unique_ptr<Expr> delay;
  std::vector<Statement> delay_body;
  EventEdgeKind event_edge = EventEdgeKind::kAny;
  std::unique_ptr<Expr> event_expr;
  std::vector<EventItem> event_items;
  std::vector<Statement> event_body;
  std::unique_ptr<Expr> wait_condition;
  std::vector<Statement> wait_body;
  std::vector<Statement> forever_body;
  std::vector<Statement> fork_branches;
  std::string disable_target;
  std::string task_name;
  std::vector<std::unique_ptr<Expr>> task_args;
  std::string trigger_target;
  std::string force_target;
  std::string release_target;
  std::unique_ptr<Expr> condition;
  std::vector<Statement> then_branch;
  std::vector<Statement> else_branch;
  std::vector<Statement> block;
  std::string block_label;
  std::unique_ptr<Expr> case_expr;
  std::vector<CaseItem> case_items;
  std::vector<Statement> default_branch;
};

enum class EdgeKind {
  kPosedge,
  kNegedge,
  kCombinational,
  kInitial,
};

struct AlwaysBlock {
  EdgeKind edge = EdgeKind::kPosedge;
  std::string clock;
  std::string sensitivity;
  bool is_synthesized = false;
  bool is_decl_init = false;
  int origin_depth = 0;
  std::vector<Statement> statements;
};

enum class TaskArgDir {
  kInput,
  kOutput,
  kInout,
};

struct TaskArg {
  TaskArgDir dir = TaskArgDir::kInput;
  std::string name;
  int width = 1;
  bool is_signed = false;
  bool is_real = false;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
};

struct Task {
  std::string name;
  std::vector<TaskArg> args;
  std::vector<Statement> body;
};

struct EventDecl {
  std::string name;
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

struct DefParam {
  std::string instance;
  std::string param;
  std::unique_ptr<Expr> expr;
  int line = 0;
  int column = 0;
};

struct TimingCheck {
  std::string name;
  std::string edge;
  std::string signal;
  std::string condition;
  TimingCheckKind kind = TimingCheckKind::kSetup;
  TimingCheckEvent data_event;
  TimingCheckEvent ref_event;
  TimingCheckLimit limit;
  TimingCheckLimit limit2;
  std::unique_ptr<Expr> threshold;
  std::unique_ptr<Expr> check_cond;
  std::unique_ptr<Expr> event_based_flag;
  std::unique_ptr<Expr> remain_active_flag;
  std::string notifier;
  std::string delayed_ref;
  std::string delayed_data;
  int line = 0;
  int column = 0;
};

struct Module {
  std::string name;
  std::string timescale;
  std::vector<Port> ports;
  std::vector<Net> nets;
  std::vector<Assign> assigns;
  std::vector<Switch> switches;
  std::vector<Instance> instances;
  std::vector<AlwaysBlock> always_blocks;
  std::vector<Parameter> parameters;
  std::vector<Function> functions;
  std::vector<Task> tasks;
  std::vector<EventDecl> events;
  std::vector<DefParam> defparams;
  std::vector<TimingCheck> timing_checks;
  std::unordered_set<std::string> generate_labels;
  UnconnectedDrive unconnected_drive = UnconnectedDrive::kNone;
};

struct Program {
  std::vector<Module> modules;
};

struct FourStateValue {
  uint64_t value_bits = 0;
  uint64_t x_bits = 0;
  uint64_t z_bits = 0;
  int width = 0;

  bool HasXorZ() const { return x_bits != 0 || z_bits != 0; }
};

std::unique_ptr<Expr> CloneExpr(const Expr& expr);
bool EvalConstExpr(const Expr& expr,
                   const std::unordered_map<std::string, int64_t>& params,
                   int64_t* out_value, std::string* error);
bool EvalConstExpr4State(const Expr& expr,
                         const std::unordered_map<std::string, int64_t>& params,
                         FourStateValue* out_value, std::string* error);

}  // namespace gpga
