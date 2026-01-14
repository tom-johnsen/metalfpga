/**
 * @file scheduler_vm.hh
 * @brief Scheduler VM bytecode definitions and helpers.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gpga {

/// Scheduler VM opcode.
enum class SchedulerVmOp : uint32_t {
  kDone = 0u,            ///< End of bytecode for a process.
  kCallGroup = 1u,       ///< Call a procedure group.
  kNoop = 2u,            ///< No-op.
  kJump = 3u,            ///< Unconditional jump.
  kJumpIf = 4u,          ///< Conditional jump.
  kCase = 5u,            ///< Case dispatch.
  kRepeat = 6u,          ///< Repeat loop.
  kAssign = 7u,          ///< Blocking assignment.
  kAssignNb = 8u,        ///< Nonblocking assignment.
  kAssignDelay = 9u,     ///< Delayed assignment.
  kForce = 10u,          ///< Force assignment.
  kRelease = 11u,        ///< Release force.
  kWaitTime = 12u,       ///< Wait for time.
  kWaitDelta = 13u,      ///< Wait for delta cycle.
  kWaitEvent = 14u,      ///< Wait for event.
  kWaitEdge = 15u,       ///< Wait for edge.
  kWaitCond = 16u,       ///< Wait for condition.
  kWaitJoin = 17u,       ///< Wait for fork join.
  kWaitService = 18u,    ///< Wait for service result.
  kEventTrigger = 19u,   ///< Trigger an event.
  kFork = 20u,           ///< Fork execution.
  kDisable = 21u,        ///< Disable block/process.
  kServiceCall = 22u,    ///< Invoke runtime service.
  kServiceRetAssign = 23u, ///< Assign service return.
  kServiceRetBranch = 24u, ///< Branch on service return.
  kTaskCall = 25u,       ///< Task call.
  kRet = 26u,            ///< Return from task.
  kHaltSim = 27u,        ///< Halt simulation.
};

/// Fork/join behavior.
enum class SchedulerVmJoinKind : uint32_t {
  kAll = 0u,  ///< Wait for all branches.
  kAny = 1u,  ///< Wait for any branch.
  kNone = 2u, ///< Do not wait (join_none).
};

/// Disable target kind.
enum class SchedulerVmDisableKind : uint32_t {
  kBlock = 0u,     ///< Disable named block.
  kChildProc = 1u, ///< Disable child process.
  kCrossProc = 2u, ///< Disable cross-module process.
};

/// Condition entry kind.
enum class SchedulerVmCondKind : uint32_t {
  kDynamic = 0u, ///< Condition computed at runtime.
  kConst = 1u,   ///< Constant condition.
  kExpr = 2u,    ///< Expression-based condition.
};

/// Expression bytecode opcode.
enum class SchedulerVmExprOp : uint32_t {
  kDone = 0u,        ///< End of expression.
  kPushConst = 1u,   ///< Push constant.
  kPushSignal = 2u,  ///< Push signal value.
  kPushImm = 3u,     ///< Push immediate literal.
  kUnary = 4u,       ///< Unary operation.
  kBinary = 5u,      ///< Binary operation.
  kTernary = 6u,     ///< Ternary operation.
  kSelect = 7u,      ///< Part/bit select.
  kIndex = 8u,       ///< Array index.
  kConcat = 9u,      ///< Concatenation.
  kCall = 10u,       ///< Function call.
  kPushConstXz = 11u, ///< Push constant with X/Z mask.
};

/// Unary expression operation.
enum class SchedulerVmExprUnaryOp : uint32_t {
  kPlus = 0u,   ///< Unary plus.
  kMinus = 1u,  ///< Unary minus.
  kBitNot = 2u, ///< Bitwise not.
  kLogNot = 3u, ///< Logical not.
  kRedAnd = 4u, ///< Reduction AND.
  kRedNand = 5u, ///< Reduction NAND.
  kRedOr = 6u,  ///< Reduction OR.
  kRedNor = 7u, ///< Reduction NOR.
  kRedXor = 8u, ///< Reduction XOR.
  kRedXnor = 9u, ///< Reduction XNOR.
};

/// Binary expression operation.
enum class SchedulerVmExprBinaryOp : uint32_t {
  kAdd = 0u,    ///< Addition.
  kSub = 1u,    ///< Subtraction.
  kMul = 2u,    ///< Multiplication.
  kDiv = 3u,    ///< Division.
  kMod = 4u,    ///< Modulo.
  kPow = 5u,    ///< Power.
  kShl = 6u,    ///< Shift left logical.
  kShr = 7u,    ///< Shift right logical.
  kAshr = 8u,   ///< Arithmetic shift right.
  kAnd = 9u,    ///< Bitwise AND.
  kOr = 10u,    ///< Bitwise OR.
  kXor = 11u,   ///< Bitwise XOR.
  kXnor = 12u,  ///< Bitwise XNOR.
  kLogAnd = 13u, ///< Logical AND.
  kLogOr = 14u, ///< Logical OR.
  kEq = 15u,    ///< Equality.
  kNeq = 16u,   ///< Inequality.
  kCaseEq = 17u, ///< Case equality.
  kCaseNeq = 18u, ///< Case inequality.
  kLt = 19u,    ///< Less than.
  kLe = 20u,    ///< Less or equal.
  kGt = 21u,    ///< Greater than.
  kGe = 22u,    ///< Greater or equal.
  kCaseZ = 23u, ///< casez compare.
  kCaseX = 24u, ///< casex compare.
};

/// Builtin call operation.
enum class SchedulerVmExprCallOp : uint32_t {
  kTime = 0u,       ///< $time
  kStime = 1u,      ///< $stime
  kRealtime = 2u,   ///< $realtime
  kIToR = 3u,       ///< $itor
  kBitsToReal = 4u, ///< $bitstoreal
  kRealToBits = 5u, ///< $realtobits
  kRToI = 6u,       ///< $rtoi
  kLog10 = 7u,      ///< log10
  kLn = 8u,         ///< ln
  kExp = 9u,        ///< exp
  kSqrt = 10u,      ///< sqrt
  kFloor = 11u,     ///< floor
  kCeil = 12u,      ///< ceil
  kSin = 13u,       ///< sin
  kCos = 14u,       ///< cos
  kTan = 15u,       ///< tan
  kAsin = 16u,      ///< asin
  kAcos = 17u,      ///< acos
  kAtan = 18u,      ///< atan
  kSinh = 19u,      ///< sinh
  kCosh = 20u,      ///< cosh
  kTanh = 21u,      ///< tanh
  kAsinh = 22u,     ///< asinh
  kAcosh = 23u,     ///< acosh
  kAtanh = 24u,     ///< atanh
  kPow = 25u,       ///< pow
  kAtan2 = 26u,     ///< atan2
  kHypot = 27u,     ///< hypot
};

/// Case opcode kind.
enum class SchedulerVmCaseKind : uint32_t {
  kCase = 0u,  ///< case
  kCaseX = 1u, ///< casex
  kCaseZ = 2u, ///< casez
};

/// Case dispatch strategy.
enum class SchedulerVmCaseStrategy : uint32_t {
  kLinear = 0u, ///< Linear scan.
  kBucket = 1u, ///< Bucketed compare.
  kLut = 2u,    ///< Lookup table.
};

constexpr uint32_t kSchedulerVmWordsPerProc = 2u; ///< Minimum words per proc.
constexpr uint32_t kSchedulerVmCallFrameWords = 4u; ///< Call frame size in words.
constexpr uint32_t kSchedulerVmCallFrameDepth = 1u; ///< Call frame stack depth.
constexpr uint32_t kSchedulerVmOpMask = 0xFFu; ///< Opcode bit mask.
constexpr uint32_t kSchedulerVmOpShift = 8u; ///< Opcode argument shift.
constexpr uint32_t kSchedulerVmForkJoinShift = 24u; ///< Fork join shift.
constexpr uint32_t kSchedulerVmForkCountMask = 0x00FFFFFFu; ///< Fork count mask.
constexpr uint32_t kSchedulerVmExprNoExtra = 0xFFFFFFFFu; ///< Sentinel for no extra word.
constexpr uint32_t kSchedulerVmExprSignedFlag = 1u << 8u; ///< Expression signed flag.
constexpr uint32_t kSchedulerVmAssignFlagNonblocking = 1u << 0u; ///< Nonblocking assign.
constexpr uint32_t kSchedulerVmAssignFlagFallback = 1u << 1u; ///< Use fallback path.
constexpr uint32_t kSchedulerVmAssignFlagIsArray = 1u << 2u; ///< Array assignment.
constexpr uint32_t kSchedulerVmAssignFlagIsBitSelect = 1u << 3u; ///< Bit-select assign.
constexpr uint32_t kSchedulerVmAssignFlagIsRange = 1u << 4u; ///< Range assign.
constexpr uint32_t kSchedulerVmAssignFlagIsIndexedRange = 1u << 5u; ///< Indexed range assign.
constexpr uint32_t kSchedulerVmAssignFlagWideConst = 1u << 6u; ///< Wide const RHS.
constexpr uint32_t kSchedulerVmAssignFlagRhsCond = 1u << 7u; ///< RHS conditional.
constexpr uint32_t kSchedulerVmAssignFlagRhsSigned = 1u << 8u; ///< RHS signed.
constexpr uint32_t kSchedulerVmForceFlagProcedural = 1u << 0u; ///< Procedural force.
constexpr uint32_t kSchedulerVmForceFlagFallback = 1u << 1u; ///< Use fallback path.
constexpr uint32_t kSchedulerVmForceFlagOverrideReg = 1u << 2u; ///< Override reg storage.
constexpr uint32_t kSchedulerVmDelayAssignFlagNonblocking = 1u << 0u; ///< Delayed nonblocking.
constexpr uint32_t kSchedulerVmDelayAssignFlagInertial = 1u << 1u; ///< Inertial delay.
constexpr uint32_t kSchedulerVmDelayAssignFlagShowcancelled = 1u << 2u; ///< Showcancelled.
constexpr uint32_t kSchedulerVmDelayAssignFlagHasPulse = 1u << 3u; ///< Has pulse reject.
constexpr uint32_t kSchedulerVmDelayAssignFlagHasPulseError = 1u << 4u; ///< Has pulse error.
constexpr uint32_t kSchedulerVmDelayAssignFlagIsArray = 1u << 5u; ///< Array assignment.
constexpr uint32_t kSchedulerVmDelayAssignFlagIsBitSelect = 1u << 6u; ///< Bit-select assignment.
constexpr uint32_t kSchedulerVmDelayAssignFlagIsRange = 1u << 7u; ///< Range assignment.
constexpr uint32_t kSchedulerVmDelayAssignFlagIsIndexedRange = 1u << 8u; ///< Indexed range assignment.
constexpr uint32_t kSchedulerVmDelayAssignFlagIsReal = 1u << 9u; ///< Real assignment.
constexpr uint32_t kSchedulerVmDelayAssignFlagFallback = 1u << 10u; ///< Use fallback path.
constexpr uint32_t kSchedulerVmServiceFlagFallback = 1u << 0u; ///< Use fallback path.
constexpr uint32_t kSchedulerVmServiceFlagGlobalOnly = 1u << 1u; ///< Global-only service.
constexpr uint32_t kSchedulerVmServiceFlagGuardFd = 1u << 2u; ///< Guard file descriptor.
constexpr uint32_t kSchedulerVmServiceFlagMonitor = 1u << 3u; ///< Monitor service.
constexpr uint32_t kSchedulerVmServiceFlagMonitorOn = 1u << 4u; ///< Enable monitor.
constexpr uint32_t kSchedulerVmServiceFlagMonitorOff = 1u << 5u; ///< Disable monitor.
constexpr uint32_t kSchedulerVmServiceFlagStrobe = 1u << 6u; ///< Strobe service.
constexpr uint32_t kSchedulerVmServiceFlagFinish = 1u << 7u; ///< Finish request.
constexpr uint32_t kSchedulerVmServiceFlagStop = 1u << 8u; ///< Stop request.
constexpr uint32_t kSchedulerVmServiceArgFlagExpr = 1u << 0u; ///< Argument is expr.
constexpr uint32_t kSchedulerVmServiceArgFlagTime = 1u << 1u; ///< Argument is time.
constexpr uint32_t kSchedulerVmServiceArgFlagStime = 1u << 2u; ///< Argument is signed time.
constexpr uint32_t kSchedulerVmServiceRetAssignFlagFallback = 1u << 0u; ///< Use fallback path.

/**
 * @brief Pack a scheduler VM instruction word.
 *
 * @param op Opcode.
 * @param arg Opcode argument.
 * @return Packed instruction word.
 */
constexpr uint32_t MakeSchedulerVmInstr(SchedulerVmOp op,
                                        uint32_t arg = 0u) {
  return (arg << kSchedulerVmOpShift) | static_cast<uint32_t>(op);
}

/**
 * @brief Pack a scheduler VM expression instruction word.
 *
 * @param op Expression opcode.
 * @param arg Opcode argument.
 * @return Packed instruction word.
 */
constexpr uint32_t MakeSchedulerVmExprInstr(SchedulerVmExprOp op,
                                            uint32_t arg = 0u) {
  return (arg << kSchedulerVmOpShift) | static_cast<uint32_t>(op);
}

/**
 * @brief Decode the opcode from an instruction word.
 *
 * @param instr Packed instruction.
 * @return Decoded opcode.
 */
constexpr SchedulerVmOp DecodeSchedulerVmOp(uint32_t instr) {
  return static_cast<SchedulerVmOp>(instr & kSchedulerVmOpMask);
}

/**
 * @brief Decode the argument from an instruction word.
 *
 * @param instr Packed instruction.
 * @return Decoded argument.
 */
constexpr uint32_t DecodeSchedulerVmArg(uint32_t instr) {
  return instr >> kSchedulerVmOpShift;
}

/**
 * @brief Pack fork count and join kind into an argument word.
 *
 * @param count Forked process count.
 * @param kind Join kind.
 * @return Packed argument.
 */
constexpr uint32_t PackSchedulerVmForkArg(uint32_t count,
                                          SchedulerVmJoinKind kind) {
  return (static_cast<uint32_t>(kind) << kSchedulerVmForkJoinShift) |
         (count & kSchedulerVmForkCountMask);
}

/**
 * @brief Decode fork count from a packed argument.
 *
 * @param arg Packed argument.
 * @return Forked process count.
 */
constexpr uint32_t DecodeSchedulerVmForkCount(uint32_t arg) {
  return arg & kSchedulerVmForkCountMask;
}

/**
 * @brief Decode join kind from a packed argument.
 *
 * @param arg Packed argument.
 * @return Join kind.
 */
constexpr SchedulerVmJoinKind DecodeSchedulerVmForkKind(uint32_t arg) {
  return static_cast<SchedulerVmJoinKind>(
      (arg >> kSchedulerVmForkJoinShift) & 0xFFu);
}

/// Expression bytecode and literal table.
struct SchedulerVmExprTable {
  std::vector<uint32_t> words; ///< Expression bytecode stream.
  std::vector<uint32_t> imm_words; ///< Literal pool storage.
};

/// Condition table entry.
struct SchedulerVmCondEntry {
  uint32_t kind = 0u; ///< Condition kind.
  uint32_t val = 0u; ///< Constant value bits.
  uint32_t xz = 1u; ///< X/Z mask bits.
  uint32_t expr_offset = 0u; ///< Expression offset.
};

/// Packed signal storage slot.
struct SchedulerVmPackedSlot {
  uint32_t word_size = 0u; ///< Word count for this slot.
  uint32_t array_size = 1u; ///< Array size for this slot.
};

/// Signal table entry.
struct SchedulerVmSignalEntry {
  uint32_t val_slot = 0u; ///< Value slot index.
  uint32_t xz_slot = 0u; ///< X/Z slot index.
  uint32_t width = 0u; ///< Signal width in bits.
  uint32_t array_size = 1u; ///< Array size.
  uint32_t flags = 0u; ///< Signal flags.
};

constexpr uint32_t kSchedulerVmSignalFlagReal = 1u << 0u; ///< Signal is real.
constexpr uint32_t kSchedulerVmExprStackMax = 32u; ///< Max expression stack depth.

/// Case table header.
struct SchedulerVmCaseHeader {
  uint32_t kind = 0u; ///< Case kind.
  uint32_t strategy = 0u; ///< Dispatch strategy.
  uint32_t width = 0u; ///< Expression width.
  uint32_t entry_count = 0u; ///< Entry count.
  uint32_t entry_offset = 0u; ///< Entry offset.
  uint32_t expr_offset = kSchedulerVmExprNoExtra; ///< Expression offset.
  uint32_t default_target = 0u; ///< Default target label.
};

/// Case entry record.
struct SchedulerVmCaseEntry {
  uint32_t want_offset = 0u; ///< Wanted value offset.
  uint32_t care_offset = 0u; ///< Care mask offset.
  uint32_t target = 0u; ///< Target label.
};

/// Assignment entry record.
struct SchedulerVmAssignEntry {
  uint32_t flags = 0u; ///< Assignment flags.
  uint32_t signal_id = 0u; ///< Target signal id.
  uint32_t rhs_expr = kSchedulerVmExprNoExtra; ///< RHS expression offset.
  uint32_t idx_expr = kSchedulerVmExprNoExtra; ///< Index expression offset.
  uint32_t width = 0u; ///< Assignment width.
  uint32_t base_width = 0u; ///< Base signal width.
  uint32_t range_lsb = 0u; ///< Range LSB.
  uint32_t array_size = 0u; ///< Array size.
  uint32_t force_slot = 0xFFFFFFFFu; ///< Force slot.
  uint32_t passign_slot = 0xFFFFFFFFu; ///< Procedural assign slot.
};

/// Delayed assignment entry record.
struct SchedulerVmDelayAssignEntry {
  uint32_t flags = 0u; ///< Assignment flags.
  uint32_t signal_id = 0u; ///< Target signal id.
  uint32_t rhs_expr = kSchedulerVmExprNoExtra; ///< RHS expression offset.
  uint32_t delay_expr = kSchedulerVmExprNoExtra; ///< Delay expression offset.
  uint32_t idx_expr = kSchedulerVmExprNoExtra; ///< Index expression offset.
  uint32_t width = 0u; ///< Assignment width.
  uint32_t base_width = 0u; ///< Base signal width.
  uint32_t range_lsb = 0u; ///< Range LSB.
  uint32_t array_size = 0u; ///< Array size.
  uint32_t pulse_reject_expr = kSchedulerVmExprNoExtra; ///< Pulse reject expr.
  uint32_t pulse_error_expr = kSchedulerVmExprNoExtra; ///< Pulse error expr.
};

/// Force entry record.
struct SchedulerVmForceEntry {
  uint32_t flags = 0u; ///< Force flags.
  uint32_t signal_id = 0u; ///< Target signal id.
  uint32_t rhs_expr = kSchedulerVmExprNoExtra; ///< RHS expression offset.
  uint32_t force_id = 0u; ///< Force id.
  uint32_t force_slot = 0xFFFFFFFFu; ///< Force slot index.
  uint32_t passign_slot = 0xFFFFFFFFu; ///< Procedural assign slot.
};

/// Release entry record.
struct SchedulerVmReleaseEntry {
  uint32_t flags = 0u; ///< Release flags.
  uint32_t signal_id = 0u; ///< Target signal id.
  uint32_t force_slot = 0xFFFFFFFFu; ///< Force slot index.
  uint32_t passign_slot = 0xFFFFFFFFu; ///< Procedural assign slot.
};

/// Service call entry record.
struct SchedulerVmServiceEntry {
  uint32_t kind = 0u; ///< Service kind.
  uint32_t format_id = 0u; ///< Format string id.
  uint32_t arg_offset = 0u; ///< Argument list offset.
  uint32_t arg_count = 0u; ///< Argument count.
  uint32_t flags = 0u; ///< Service flags.
  uint32_t aux = 0u; ///< Auxiliary data.
};

/// Service argument record.
struct SchedulerVmServiceArg {
  uint32_t kind = 0u; ///< Argument kind.
  uint32_t width = 0u; ///< Argument width.
  uint32_t payload = 0u; ///< Payload value or offset.
  uint32_t flags = 0u; ///< Argument flags.
};

/// Service return assignment entry record.
struct SchedulerVmServiceRetAssignEntry {
  uint32_t flags = 0u; ///< Return flags.
  uint32_t signal_id = 0u; ///< Target signal id.
  uint32_t width = 0u; ///< Assignment width.
  uint32_t force_slot = 0xFFFFFFFFu; ///< Force slot index.
  uint32_t passign_slot = 0xFFFFFFFFu; ///< Procedural assign slot.
  uint32_t reserved = 0u; ///< Reserved field.
};

/// Scheduler VM layout and tables.
struct SchedulerVmLayout {
  uint32_t proc_count = 0u; ///< Process count.
  uint32_t words_per_proc = 0u; ///< Words per process.
  std::vector<uint32_t> bytecode; ///< Bytecode words.
  std::vector<uint32_t> proc_offsets; ///< Per-proc bytecode offsets.
  std::vector<uint32_t> proc_lengths; ///< Per-proc bytecode lengths.
  std::vector<SchedulerVmPackedSlot> packed_slots; ///< Packed storage slots.
  std::vector<SchedulerVmSignalEntry> signal_entries; ///< Signal table.
  std::vector<SchedulerVmCondEntry> cond_entries; ///< Condition table.
  std::vector<SchedulerVmCaseHeader> case_headers; ///< Case headers.
  std::vector<SchedulerVmCaseEntry> case_entries; ///< Case entries.
  std::vector<uint64_t> case_words; ///< Case literal words.
  std::vector<SchedulerVmAssignEntry> assign_entries; ///< Assign entries.
  std::vector<SchedulerVmDelayAssignEntry> delay_assign_entries; ///< Delay assigns.
  std::vector<SchedulerVmForceEntry> force_entries; ///< Force entries.
  std::vector<SchedulerVmReleaseEntry> release_entries; ///< Release entries.
  std::vector<SchedulerVmServiceEntry> service_entries; ///< Service entries.
  std::vector<SchedulerVmServiceArg> service_args; ///< Service arguments.
  std::vector<SchedulerVmServiceRetAssignEntry> service_ret_entries; ///< Service returns.
  SchedulerVmExprTable expr_table; ///< Expression table.
  std::vector<uint32_t> edge_item_expr_offsets; ///< Edge item expr offsets.
  std::vector<uint32_t> edge_star_expr_offsets; ///< Edge-star expr offsets.
  std::vector<uint32_t> repeat_expr_offsets; ///< Repeat expr offsets.
};

/// Builds a scheduler VM instruction stream.
class SchedulerVmBuilder {
 public:
  /**
   * @brief Emit an instruction into the current stream.
   *
   * @param op Opcode to emit.
   * @param arg Opcode argument.
   */
  void Emit(SchedulerVmOp op, uint32_t arg = 0u) {
    words_.push_back(MakeSchedulerVmInstr(op, arg));
  }

  /// Emit a call-group opcode.
  void EmitCallGroup() { Emit(SchedulerVmOp::kCallGroup); }
  /// Emit a done opcode.
  void EmitDone() { Emit(SchedulerVmOp::kDone); }

  /**
   * @brief Return the built instruction stream.
   *
   * @return Instruction words.
   */
  const std::vector<uint32_t>& words() const { return words_; }

 private:
  std::vector<uint32_t> words_; ///< Instruction stream words.
};

/// Builds scheduler VM expression bytecode and literal pools.
class SchedulerVmExprBuilder {
 public:
  /**
   * @brief Emit an expression opcode.
   *
   * @param op Expression opcode.
   * @param arg Opcode argument.
   * @param extra Optional extra word for op-specific payload.
   * @return Offset of the emitted instruction.
   */
  uint32_t EmitOp(SchedulerVmExprOp op, uint32_t arg = 0u,
                  uint32_t extra = kSchedulerVmExprNoExtra) {
    const uint32_t offset = static_cast<uint32_t>(words_.size());
    words_.push_back(MakeSchedulerVmExprInstr(op, arg));
    if (extra != kSchedulerVmExprNoExtra) {
      words_.push_back(extra);
    }
    return offset;
  }

  /**
   * @brief Append words to the literal pool.
   *
   * @param words Literal words to append.
   * @return Base offset of the appended literal data.
   */
  uint32_t EmitImmTable(const std::vector<uint32_t>& words) {
    const uint32_t base = static_cast<uint32_t>(imm_words_.size());
    imm_words_.insert(imm_words_.end(), words.begin(), words.end());
    return base;
  }

  /**
   * @brief Return the expression bytecode stream.
   *
   * @return Expression bytecode words.
   */
  const std::vector<uint32_t>& words() const { return words_; }
  /**
   * @brief Return the literal pool words.
   *
   * @return Literal pool words.
   */
  const std::vector<uint32_t>& imm_words() const { return imm_words_; }
  /**
   * @brief Truncate expression and literal tables to the given sizes.
   *
   * @param word_size Considered size of the expression bytecode stream.
   * @param imm_size Considered size of the literal pool.
   */
  void Truncate(size_t word_size, size_t imm_size) {
    words_.resize(word_size);
    imm_words_.resize(imm_size);
  }

 private:
  std::vector<uint32_t> words_; ///< Expression bytecode stream.
  std::vector<uint32_t> imm_words_; ///< Literal pool.
};

/**
 * @brief Build a scheduler VM layout from per-process bytecode.
 *
 * @param procs Per-process bytecode streams.
 * @param out Output layout to populate.
 * @param error Optional error string destination.
 * @return True on success.
 */
inline bool BuildSchedulerVmLayout(
    const std::vector<std::vector<uint32_t>>& procs,
    SchedulerVmLayout* out, std::string* error) {
  if (!out) {
    if (error) {
      *error = "missing scheduler VM layout output";
    }
    return false;
  }
  out->proc_count = 0u;
  out->words_per_proc = 0u;
  out->bytecode.clear();
  out->proc_offsets.clear();
  out->proc_lengths.clear();
  out->packed_slots.clear();
  out->signal_entries.clear();
  out->cond_entries.clear();
  out->case_headers.clear();
  out->case_entries.clear();
  out->case_words.clear();
  out->assign_entries.clear();
  out->delay_assign_entries.clear();
  out->force_entries.clear();
  out->release_entries.clear();
  out->service_entries.clear();
  out->service_args.clear();
  out->service_ret_entries.clear();
  out->expr_table.words.clear();
  out->expr_table.imm_words.clear();
  out->edge_item_expr_offsets.clear();
  out->edge_star_expr_offsets.clear();
  const uint32_t proc_count = static_cast<uint32_t>(procs.size());
  if (proc_count == 0u) {
    out->proc_count = 0u;
    out->words_per_proc = kSchedulerVmWordsPerProc;
    return true;
  }
  uint32_t max_len = 0u;
  for (const auto& proc : procs) {
    max_len = std::max<uint32_t>(max_len,
                                 static_cast<uint32_t>(proc.size()));
  }
  const uint32_t words_per_proc =
      std::max<uint32_t>(max_len, kSchedulerVmWordsPerProc);
  out->proc_count = proc_count;
  out->words_per_proc = words_per_proc;
  out->bytecode.assign(static_cast<size_t>(proc_count) * words_per_proc, 0u);
  out->proc_offsets.resize(proc_count);
  out->proc_lengths.resize(proc_count);
  for (uint32_t pid = 0u; pid < proc_count; ++pid) {
    const uint32_t offset = pid * words_per_proc;
    out->proc_offsets[pid] = offset;
    out->proc_lengths[pid] =
        static_cast<uint32_t>(procs[pid].size());
    if (procs[pid].empty()) {
      continue;
    }
    std::copy(procs[pid].begin(), procs[pid].end(),
              out->bytecode.begin() + offset);
  }
  return true;
}

/**
 * @brief Build a seed scheduler VM layout with minimal bytecode.
 *
 * @param proc_count Process count to seed.
 * @param out Output layout to populate.
 * @param error Optional error string destination.
 * @return True on success.
 */
inline bool BuildSchedulerVmSeedLayout(uint32_t proc_count,
                                       SchedulerVmLayout* out,
                                       std::string* error) {
  std::vector<std::vector<uint32_t>> procs(proc_count);
  for (uint32_t pid = 0u; pid < proc_count; ++pid) {
    SchedulerVmBuilder builder;
    builder.EmitCallGroup();
    builder.EmitDone();
    procs[pid] = builder.words();
  }
  return BuildSchedulerVmLayout(procs, out, error);
}

}  // namespace gpga
