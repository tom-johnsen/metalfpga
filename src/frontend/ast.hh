/**
 * @file ast.hh
 * @brief Verilog frontend AST declarations.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gpga {

/// Direction for a module port.
enum class PortDir {
  kInput,  ///< Input port.
  kOutput, ///< Output port.
  kInout,  ///< Inout port.
};

struct Expr;

/// Port declaration in a module header or body.
struct Port {
  PortDir dir = PortDir::kInput; ///< Port direction.
  std::string name; ///< Port name.
  int width = 1; ///< Declared width in bits.
  bool is_signed = false; ///< Signedness flag.
  bool is_real = false; ///< True for real-typed ports.
  bool is_declared = false; ///< True if explicitly declared.
  std::shared_ptr<Expr> msb_expr; ///< MSB expression for parametric widths.
  std::shared_ptr<Expr> lsb_expr; ///< LSB expression for parametric widths.
};

struct Expr;

/// Net data type.
enum class NetType {
  kWire,    ///< wire
  kReg,     ///< reg
  kWand,    ///< wand
  kWor,     ///< wor
  kTri0,    ///< tri0
  kTri1,    ///< tri1
  kTriand,  ///< triand
  kTrior,   ///< trior
  kTrireg,  ///< trireg
  kSupply0, ///< supply0
  kSupply1, ///< supply1
};

/// Drive strength.
enum class Strength {
  kHighZ,  ///< High impedance.
  kWeak,   ///< Weak drive.
  kPull,   ///< Pull strength.
  kStrong, ///< Strong drive.
  kSupply, ///< Supply strength.
};

/// Charge strength for trireg nets.
enum class ChargeStrength {
  kNone,   ///< No charge.
  kSmall,  ///< Small charge.
  kMedium, ///< Medium charge.
  kLarge,  ///< Large charge.
};

/// Unconnected drive behavior.
enum class UnconnectedDrive {
  kNone,  ///< No implicit drive.
  kPull0, ///< Pull unconnected signals to 0.
  kPull1, ///< Pull unconnected signals to 1.
};

/// Switch primitive kind.
enum class SwitchKind {
  kTran,    ///< tran
  kTranif1, ///< tranif1
  kTranif0, ///< tranif0
  kCmos,    ///< cmos
};

/// Switch primitive instance.
struct Switch {
  SwitchKind kind = SwitchKind::kTran; ///< Primitive kind.
  std::string a; ///< First terminal.
  std::string b; ///< Second terminal.
  std::unique_ptr<Expr> control; ///< Control expression.
  std::unique_ptr<Expr> control_n; ///< Complement control expression.
  Strength strength0 = Strength::kStrong; ///< Strength for driving 0.
  Strength strength1 = Strength::kStrong; ///< Strength for driving 1.
  bool has_strength = false; ///< True when strengths are specified.
};

/// Packed or unpacked array dimension.
struct ArrayDim {
  int size = 0; ///< Size in elements (if known).
  std::shared_ptr<Expr> msb_expr; ///< MSB expression for the dimension.
  std::shared_ptr<Expr> lsb_expr; ///< LSB expression for the dimension.
};

/// Net declaration.
struct Net {
  NetType type = NetType::kWire; ///< Net kind.
  std::string name; ///< Net name.
  int width = 1; ///< Net width in bits.
  bool is_signed = false; ///< Signedness flag.
  bool is_real = false; ///< True for real-typed nets.
  ChargeStrength charge = ChargeStrength::kNone; ///< Charge strength for trireg.
  std::shared_ptr<Expr> msb_expr; ///< MSB expression for parametric width.
  std::shared_ptr<Expr> lsb_expr; ///< LSB expression for parametric width.
  int array_size = 0; ///< Simple array size (if present).
  std::vector<ArrayDim> array_dims; ///< Additional array dimensions.
};

/// Expression node kind.
enum class ExprKind {
  kIdentifier, ///< Identifier reference.
  kNumber,     ///< Numeric literal.
  kString,     ///< String literal.
  kUnary,      ///< Unary operation.
  kBinary,     ///< Binary operation.
  kTernary,    ///< Ternary operation.
  kSelect,     ///< Bit or part select.
  kIndex,      ///< Array indexing.
  kCall,       ///< Function or task call.
  kConcat,     ///< Concatenation.
};

/// Expression node.
struct Expr {
  ExprKind kind = ExprKind::kIdentifier; ///< Expression kind.
  std::string ident; ///< Identifier name.
  std::string string_value; ///< String literal contents.
  uint64_t number = 0; ///< Parsed numeric value (raw).
  uint64_t value_bits = 0; ///< Known value bits for 4-state literals.
  uint64_t x_bits = 0; ///< X bits for 4-state literals.
  uint64_t z_bits = 0; ///< Z bits for 4-state literals.
  int number_width = 0; ///< Declared literal width.
  bool has_width = false; ///< True if a width was provided.
  bool has_base = false; ///< True if a base was specified.
  char base_char = 'd'; ///< Base character (b, o, d, h).
  bool is_signed = false; ///< Signedness flag.
  bool is_real_literal = false; ///< True for real literals.
  char op = 0; ///< Binary operator token.
  char unary_op = 0; ///< Unary operator token.
  std::unique_ptr<Expr> operand; ///< Operand for unary operations.
  std::unique_ptr<Expr> lhs; ///< Left operand for binary operations.
  std::unique_ptr<Expr> rhs; ///< Right operand for binary operations.
  std::unique_ptr<Expr> condition; ///< Condition for ternary expressions.
  std::unique_ptr<Expr> then_expr; ///< True branch of ternary.
  std::unique_ptr<Expr> else_expr; ///< False branch of ternary.
  std::unique_ptr<Expr> base; ///< Base expression for selects or indices.
  std::unique_ptr<Expr> index; ///< Index expression for selects.
  int msb = 0; ///< Constant MSB for ranges.
  int lsb = 0; ///< Constant LSB for ranges.
  bool has_range = false; ///< True if a range is present.
  bool indexed_range = false; ///< True for indexed part-selects.
  bool indexed_desc = false; ///< True if indexed range is descending.
  int indexed_width = 0; ///< Width for indexed part-selects.
  std::unique_ptr<Expr> msb_expr; ///< MSB expression for ranges.
  std::unique_ptr<Expr> lsb_expr; ///< LSB expression for ranges.
  std::vector<std::unique_ptr<Expr>> elements; ///< Concatenation elements.
  int repeat = 1; ///< Repeat count for concatenation.
  std::unique_ptr<Expr> repeat_expr; ///< Repeat count expression.
  std::vector<std::unique_ptr<Expr>> call_args; ///< Call arguments.

  /// True if X bits are present.
  bool HasX() const { return x_bits != 0; }
  /// True if Z bits are present.
  bool HasZ() const { return z_bits != 0; }
  /// True if no X/Z bits are present.
  bool IsFullyDetermined() const { return x_bits == 0 && z_bits == 0; }
};

/// Parameter declaration.
struct Parameter {
  std::string name; ///< Parameter name.
  std::unique_ptr<Expr> value; ///< Parameter value expression.
  bool is_local = false; ///< True for localparam.
  bool is_real = false; ///< True for real parameter.
};

struct Statement;

/// Function argument declaration.
struct FunctionArg {
  std::string name; ///< Argument name.
  int width = 1; ///< Argument width in bits.
  bool is_signed = false; ///< Signedness flag.
  bool is_real = false; ///< True for real-typed args.
  std::shared_ptr<Expr> msb_expr; ///< MSB expression for parametric width.
  std::shared_ptr<Expr> lsb_expr; ///< LSB expression for parametric width.
};

/// Local variable declaration.
struct LocalVar {
  std::string name; ///< Variable name.
  int width = 1; ///< Width in bits.
  bool is_signed = false; ///< Signedness flag.
  bool is_real = false; ///< True for real-typed locals.
};

/// Function declaration and body.
struct Function {
  std::string name; ///< Function name.
  int width = 1; ///< Return width in bits.
  bool is_signed = false; ///< Return signedness.
  bool is_real = false; ///< True if return type is real.
  std::shared_ptr<Expr> msb_expr; ///< MSB expression for parametric width.
  std::shared_ptr<Expr> lsb_expr; ///< LSB expression for parametric width.
  std::vector<FunctionArg> args; ///< Function arguments.
  std::vector<LocalVar> locals; ///< Local variables.
  std::vector<Statement> body; ///< Statement body.
  std::unique_ptr<Expr> body_expr; ///< Expression body for simple functions.
};

/// Continuous assignment.
struct Assign {
  std::string lhs; ///< Left-hand identifier.
  int lhs_msb = 0; ///< Constant MSB for LHS range.
  int lhs_lsb = 0; ///< Constant LSB for LHS range.
  bool lhs_has_range = false; ///< True if LHS has an explicit range.
  std::unique_ptr<Expr> rhs; ///< Right-hand expression.
  Strength strength0 = Strength::kStrong; ///< Drive strength for 0.
  Strength strength1 = Strength::kStrong; ///< Drive strength for 1.
  bool has_strength = false; ///< True when strengths are specified.
  bool is_implicit = false; ///< True for implicit assignments.
  bool is_derived = false; ///< True when derived from higher-level forms.
  int origin_depth = 0; ///< Depth of origin for derived assigns.
};

/// Procedural assignment description.
struct SequentialAssign {
  std::string lhs; ///< Left-hand identifier.
  std::unique_ptr<Expr> lhs_index; ///< Single index expression.
  std::vector<std::unique_ptr<Expr>> lhs_indices; ///< Multi-dimensional indices.
  bool lhs_has_range = false; ///< True if LHS has an explicit range.
  bool lhs_indexed_range = false; ///< True for indexed part-select.
  bool lhs_indexed_desc = false; ///< True if indexed range is descending.
  int lhs_indexed_width = 0; ///< Width for indexed part-select.
  int lhs_msb = 0; ///< Constant MSB for range.
  int lhs_lsb = 0; ///< Constant LSB for range.
  std::unique_ptr<Expr> lhs_msb_expr; ///< MSB expression for range.
  std::unique_ptr<Expr> lhs_lsb_expr; ///< LSB expression for range.
  std::unique_ptr<Expr> rhs; ///< Right-hand expression.
  std::unique_ptr<Expr> delay; ///< Optional delay expression.
  bool nonblocking = true; ///< True for nonblocking assignment.
};

/// Statement node kind.
enum class StatementKind {
  kAssign,       ///< Procedural assignment.
  kIf,           ///< If/else statement.
  kBlock,        ///< Begin/end block.
  kCase,         ///< Case statement.
  kFor,          ///< For loop.
  kWhile,        ///< While loop.
  kRepeat,       ///< Repeat loop.
  kDelay,        ///< Delay control.
  kEventControl, ///< Event control (@).
  kEventTrigger, ///< Event trigger (->).
  kWait,         ///< Wait statement.
  kForever,      ///< Forever loop.
  kFork,         ///< Fork/join.
  kDisable,      ///< Disable statement.
  kTaskCall,     ///< Task call statement.
  kForce,        ///< Force statement.
  kRelease,      ///< Release statement.
};

/// Case statement variant.
enum class CaseKind {
  kCase,  ///< case
  kCaseZ, ///< casez
  kCaseX, ///< casex
};

/// Edge qualifier for event controls.
enum class EventEdgeKind {
  kAny,     ///< Any edge.
  kPosedge, ///< Positive edge.
  kNegedge, ///< Negative edge.
};

/// Timing check type.
enum class TimingCheckKind {
  kSetup,      ///< $setup
  kHold,       ///< $hold
  kSetupHold,  ///< $setuphold
  kRecovery,   ///< $recovery
  kRemoval,    ///< $removal
  kRecRem,     ///< $recrem
  kSkew,       ///< $skew
  kTimeSkew,   ///< $timeskew
  kFullSkew,   ///< $fullskew
  kWidth,      ///< $width
  kPeriod,     ///< $period
  kPulseWidth, ///< $pulsewidth
  kNoChange,   ///< $nochange
};

/// Specify path type.
enum class SpecifyPathKind {
  kParallel, ///< Parallel path.
  kFull,     ///< Full path.
};

/// Polarity for specify paths.
enum class SpecifyPathPolarity {
  kNone,     ///< No polarity.
  kPositive, ///< Positive polarity.
  kNegative, ///< Negative polarity.
};

/// Edge state used in timing checks.
enum class TimingEdgeState {
  k0, ///< Logic 0.
  k1, ///< Logic 1.
  kX, ///< Unknown.
  kZ, ///< High impedance.
};

/// Edge pattern for timing checks.
struct TimingEdgePattern {
  TimingEdgeState from = TimingEdgeState::k0; ///< Starting state.
  TimingEdgeState to = TimingEdgeState::k0; ///< Ending state.
  std::string raw; ///< Raw pattern text.
};

/// Min/typ/max limit triple.
struct TimingCheckLimit {
  std::unique_ptr<Expr> min; ///< Minimum limit.
  std::unique_ptr<Expr> typ; ///< Typical limit.
  std::unique_ptr<Expr> max; ///< Maximum limit.
};

/// Timing check event expression and qualifier.
struct TimingCheckEvent {
  EventEdgeKind edge = EventEdgeKind::kAny; ///< Edge qualifier.
  bool has_edge_list = false; ///< True if edge_list is present.
  std::vector<TimingEdgePattern> edge_list; ///< Edge transition list.
  std::unique_ptr<Expr> expr; ///< Event expression.
  std::unique_ptr<Expr> cond; ///< Optional condition.
  std::string raw_expr; ///< Raw event expression text.
  std::string raw_cond; ///< Raw condition text.
};

/// Single event control item.
struct EventItem {
  EventEdgeKind edge = EventEdgeKind::kAny; ///< Edge qualifier.
  std::unique_ptr<Expr> expr; ///< Event expression.
};

struct Statement;

/// Case item and body.
struct CaseItem {
  std::vector<std::unique_ptr<Expr>> labels; ///< Case item labels.
  std::vector<Statement> body; ///< Case item statements.
};

/// Statement node.
struct Statement {
  StatementKind kind = StatementKind::kAssign; ///< Statement kind.
  CaseKind case_kind = CaseKind::kCase; ///< Case variant.
  SequentialAssign assign; ///< Assignment payload.
  bool is_procedural = false; ///< True for procedural assignments.
  std::string for_init_lhs; ///< LHS for for-loop init.
  std::unique_ptr<Expr> for_init_rhs; ///< RHS for for-loop init.
  std::unique_ptr<Expr> for_condition; ///< For-loop condition.
  std::string for_step_lhs; ///< LHS for for-loop step.
  std::unique_ptr<Expr> for_step_rhs; ///< RHS for for-loop step.
  std::vector<Statement> for_body; ///< For-loop body.
  std::unique_ptr<Expr> while_condition; ///< While-loop condition.
  std::vector<Statement> while_body; ///< While-loop body.
  std::unique_ptr<Expr> repeat_count; ///< Repeat count expression.
  std::vector<Statement> repeat_body; ///< Repeat-loop body.
  std::unique_ptr<Expr> delay; ///< Delay expression.
  std::vector<Statement> delay_body; ///< Body after delay.
  EventEdgeKind event_edge = EventEdgeKind::kAny; ///< Event edge qualifier.
  std::unique_ptr<Expr> event_expr; ///< Single event expression.
  std::vector<EventItem> event_items; ///< Event control list.
  std::vector<Statement> event_body; ///< Body after event control.
  std::unique_ptr<Expr> wait_condition; ///< Wait condition.
  std::vector<Statement> wait_body; ///< Wait body.
  std::vector<Statement> forever_body; ///< Forever-loop body.
  std::vector<Statement> fork_branches; ///< Forked branches.
  std::string disable_target; ///< Disable target name.
  std::string task_name; ///< Task name for calls.
  std::vector<std::unique_ptr<Expr>> task_args; ///< Task call arguments.
  std::string trigger_target; ///< Event trigger target.
  std::string force_target; ///< Force target name.
  std::string release_target; ///< Release target name.
  std::unique_ptr<Expr> condition; ///< If/ternary condition.
  std::vector<Statement> then_branch; ///< Then-branch statements.
  std::vector<Statement> else_branch; ///< Else-branch statements.
  std::vector<Statement> block; ///< Block statements.
  std::string block_label; ///< Optional block label.
  std::unique_ptr<Expr> case_expr; ///< Case expression.
  std::vector<CaseItem> case_items; ///< Case item list.
  std::vector<Statement> default_branch; ///< Default case branch.
};

/// Edge sensitivity for always blocks.
enum class EdgeKind {
  kPosedge,       ///< Positive edge.
  kNegedge,       ///< Negative edge.
  kCombinational, ///< Combinational sensitivity.
  kInitial,       ///< Initial block.
};

/// Always or initial block.
struct AlwaysBlock {
  EdgeKind edge = EdgeKind::kPosedge; ///< Sensitivity kind.
  std::string clock; ///< Clock signal name.
  std::string sensitivity; ///< Sensitivity list text.
  bool is_synthesized = false; ///< True if inferred by synthesis.
  bool is_decl_init = false; ///< True if derived from declaration init.
  int origin_depth = 0; ///< Origin depth for derived blocks.
  std::vector<Statement> statements; ///< Block statements.
};

/// Task argument direction.
enum class TaskArgDir {
  kInput,  ///< Input argument.
  kOutput, ///< Output argument.
  kInout,  ///< Inout argument.
};

/// Task argument declaration.
struct TaskArg {
  TaskArgDir dir = TaskArgDir::kInput; ///< Argument direction.
  std::string name; ///< Argument name.
  int width = 1; ///< Argument width in bits.
  bool is_signed = false; ///< Signedness flag.
  bool is_real = false; ///< True for real-typed args.
  std::shared_ptr<Expr> msb_expr; ///< MSB expression for parametric width.
  std::shared_ptr<Expr> lsb_expr; ///< LSB expression for parametric width.
};

/// Task declaration and body.
struct Task {
  std::string name; ///< Task name.
  std::vector<TaskArg> args; ///< Task arguments.
  std::vector<Statement> body; ///< Task body statements.
};

/// Event declaration.
struct EventDecl {
  std::string name; ///< Event name.
};

/// Instance port connection.
struct Connection {
  std::string port; ///< Port name.
  std::unique_ptr<Expr> expr; ///< Connected expression.
};

/// Parameter override in an instance.
struct ParamOverride {
  std::string name; ///< Parameter name.
  std::unique_ptr<Expr> expr; ///< Override expression.
};

/// Module instance.
struct Instance {
  std::string module_name; ///< Instantiated module name.
  std::string name; ///< Instance name.
  bool has_array = false; ///< True for arrayed instances.
  std::unique_ptr<Expr> array_msb; ///< MSB expression for instance array.
  std::unique_ptr<Expr> array_lsb; ///< LSB expression for instance array.
  std::vector<ParamOverride> param_overrides; ///< Parameter overrides.
  std::vector<Connection> connections; ///< Port connections.
};

/// defparam override.
struct DefParam {
  std::string instance; ///< Target instance path.
  std::string param; ///< Parameter name.
  std::unique_ptr<Expr> expr; ///< Override expression.
  int line = 0; ///< Source line.
  int column = 0; ///< Source column.
};

/// Timing check specification.
struct TimingCheck {
  std::string name; ///< Check name.
  std::string edge; ///< Edge spec string.
  std::string signal; ///< Signal name.
  std::string condition; ///< Condition text.
  TimingCheckKind kind = TimingCheckKind::kSetup; ///< Check kind.
  TimingCheckEvent data_event; ///< Data event.
  TimingCheckEvent ref_event; ///< Reference event.
  TimingCheckLimit limit; ///< Primary limits.
  TimingCheckLimit limit2; ///< Secondary limits.
  std::unique_ptr<Expr> threshold; ///< Threshold expression.
  std::unique_ptr<Expr> check_cond; ///< Check condition expression.
  std::unique_ptr<Expr> event_based_flag; ///< Event-based flag expression.
  std::unique_ptr<Expr> remain_active_flag; ///< Remain-active flag expression.
  std::string notifier; ///< Notifier name.
  std::string delayed_ref; ///< Delayed reference signal name.
  std::string delayed_data; ///< Delayed data signal name.
  int line = 0; ///< Source line.
  int column = 0; ///< Source column.
};

/// Pulse specification for a path.
struct PathPulseSpec {
  std::string name; ///< Path name.
  std::string input; ///< Input signal name.
  std::string output; ///< Output signal name.
  TimingCheckLimit reject; ///< Reject limit.
  TimingCheckLimit error; ///< Error limit.
  bool has_error = false; ///< True if error limit is present.
};

/// Specify path declaration.
struct SpecifyPath {
  SpecifyPathKind kind = SpecifyPathKind::kParallel; ///< Path kind.
  SpecifyPathPolarity polarity = SpecifyPathPolarity::kNone; ///< Path polarity.
  TimingCheckEvent input_event; ///< Input event.
  std::unique_ptr<Expr> data_expr; ///< Data expression.
  SequentialAssign target; ///< Target assignment.
  std::vector<TimingCheckLimit> delays; ///< Path delays.
  std::unique_ptr<Expr> condition; ///< Condition expression.
  bool is_conditional = false; ///< True when conditional.
  bool is_ifnone = false; ///< True when ifnone applies.
  bool showcancelled = false; ///< Showcancelled flag.
  std::string pulse_input; ///< Pulse input signal name.
  TimingCheckLimit pulse_reject; ///< Pulse reject limit.
  TimingCheckLimit pulse_error; ///< Pulse error limit.
  bool has_pulse = false; ///< True if pulse limits are present.
  bool has_pulse_error = false; ///< True if pulse error limit is present.
  int line = 0; ///< Source line.
  int column = 0; ///< Source column.
};

/// Module declaration and contents.
struct Module {
  std::string name; ///< Module name.
  std::string timescale; ///< Timescale directive.
  std::vector<Port> ports; ///< Module ports.
  std::vector<Net> nets; ///< Net declarations.
  std::vector<Assign> assigns; ///< Continuous assignments.
  std::vector<Switch> switches; ///< Switch primitives.
  std::vector<Instance> instances; ///< Module instances.
  std::vector<AlwaysBlock> always_blocks; ///< Always/initial blocks.
  std::vector<Parameter> parameters; ///< Parameter declarations.
  std::vector<Function> functions; ///< Function declarations.
  std::vector<Task> tasks; ///< Task declarations.
  std::vector<EventDecl> events; ///< Event declarations.
  std::vector<DefParam> defparams; ///< defparam overrides.
  std::vector<TimingCheck> timing_checks; ///< Timing checks.
  std::vector<SpecifyPath> specify_paths; ///< Specify paths.
  std::unordered_map<std::string, PathPulseSpec> path_pulses; ///< Path pulse specs.
  std::unordered_set<std::string> generate_labels; ///< Generate block labels.
  UnconnectedDrive unconnected_drive = UnconnectedDrive::kNone; ///< Unconnected drive mode.
};

/// Parsed program with module list.
struct Program {
  std::vector<Module> modules; ///< Modules in the program.
};

/// Packed 4-state value used for constant folding.
struct FourStateValue {
  uint64_t value_bits = 0; ///< Known value bits.
  uint64_t x_bits = 0; ///< X bits.
  uint64_t z_bits = 0; ///< Z bits.
  int width = 0; ///< Bit width.

  /// True if any X or Z bits are present.
  bool HasXorZ() const { return x_bits != 0 || z_bits != 0; }
};

/**
 * @brief Clone an expression tree.
 *
 * @param expr Source expression.
 * @return Cloned expression tree.
 */
std::unique_ptr<Expr> CloneExpr(const Expr& expr);
/**
 * @brief Evaluate a constant expression to a signed integer.
 *
 * @param expr Expression to evaluate.
 * @param params Parameter map for identifiers.
 * @param out_value Output value on success.
 * @param error Optional error message.
 * @return True when evaluation succeeds.
 */
bool EvalConstExpr(const Expr& expr,
                   const std::unordered_map<std::string, int64_t>& params,
                   int64_t* out_value, std::string* error);
/**
 * @brief Evaluate a constant expression to a 4-state value.
 *
 * @param expr Expression to evaluate.
 * @param params Parameter map for identifiers.
 * @param out_value Output 4-state value on success.
 * @param error Optional error message.
 * @return True when evaluation succeeds.
 */
bool EvalConstExpr4State(const Expr& expr,
                         const std::unordered_map<std::string, int64_t>& params,
                         FourStateValue* out_value, std::string* error);

}  // namespace gpga
