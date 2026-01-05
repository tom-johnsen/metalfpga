#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gpga {

enum class SchedulerVmOp : uint32_t {
  kDone = 0u,
  kCallGroup = 1u,
  kNoop = 2u,
  kJump = 3u,
  kJumpIf = 4u,
  kCase = 5u,
  kRepeat = 6u,
  kAssign = 7u,
  kAssignNb = 8u,
  kAssignDelay = 9u,
  kForce = 10u,
  kRelease = 11u,
  kWaitTime = 12u,
  kWaitDelta = 13u,
  kWaitEvent = 14u,
  kWaitEdge = 15u,
  kWaitCond = 16u,
  kWaitJoin = 17u,
  kWaitService = 18u,
  kEventTrigger = 19u,
  kFork = 20u,
  kDisable = 21u,
  kServiceCall = 22u,
  kServiceRetAssign = 23u,
  kServiceRetBranch = 24u,
  kTaskCall = 25u,
  kRet = 26u,
  kHaltSim = 27u,
};

enum class SchedulerVmJoinKind : uint32_t {
  kAll = 0u,
  kAny = 1u,
  kNone = 2u,
};

enum class SchedulerVmDisableKind : uint32_t {
  kBlock = 0u,
  kChildProc = 1u,
  kCrossProc = 2u,
};

enum class SchedulerVmCondKind : uint32_t {
  kDynamic = 0u,
  kConst = 1u,
  kExpr = 2u,
};

enum class SchedulerVmExprOp : uint32_t {
  kDone = 0u,
  kPushConst = 1u,
  kPushSignal = 2u,
  kPushImm = 3u,
  kUnary = 4u,
  kBinary = 5u,
  kTernary = 6u,
  kSelect = 7u,
  kIndex = 8u,
  kConcat = 9u,
  kCall = 10u,
};

enum class SchedulerVmExprUnaryOp : uint32_t {
  kPlus = 0u,
  kMinus = 1u,
  kBitNot = 2u,
  kLogNot = 3u,
  kRedAnd = 4u,
  kRedNand = 5u,
  kRedOr = 6u,
  kRedNor = 7u,
  kRedXor = 8u,
  kRedXnor = 9u,
};

enum class SchedulerVmExprBinaryOp : uint32_t {
  kAdd = 0u,
  kSub = 1u,
  kMul = 2u,
  kDiv = 3u,
  kMod = 4u,
  kPow = 5u,
  kShl = 6u,
  kShr = 7u,
  kAshr = 8u,
  kAnd = 9u,
  kOr = 10u,
  kXor = 11u,
  kXnor = 12u,
  kLogAnd = 13u,
  kLogOr = 14u,
  kEq = 15u,
  kNeq = 16u,
  kCaseEq = 17u,
  kCaseNeq = 18u,
  kLt = 19u,
  kLe = 20u,
  kGt = 21u,
  kGe = 22u,
};

enum class SchedulerVmCaseKind : uint32_t {
  kCase = 0u,
  kCaseX = 1u,
  kCaseZ = 2u,
};

enum class SchedulerVmCaseStrategy : uint32_t {
  kLinear = 0u,
  kBucket = 1u,
  kLut = 2u,
};

constexpr uint32_t kSchedulerVmWordsPerProc = 2u;
constexpr uint32_t kSchedulerVmCallFrameWords = 4u;
constexpr uint32_t kSchedulerVmCallFrameDepth = 1u;
constexpr uint32_t kSchedulerVmOpMask = 0xFFu;
constexpr uint32_t kSchedulerVmOpShift = 8u;
constexpr uint32_t kSchedulerVmForkJoinShift = 24u;
constexpr uint32_t kSchedulerVmForkCountMask = 0x00FFFFFFu;
constexpr uint32_t kSchedulerVmExprNoExtra = 0xFFFFFFFFu;
constexpr uint32_t kSchedulerVmExprSignedFlag = 1u << 8u;

constexpr uint32_t MakeSchedulerVmInstr(SchedulerVmOp op,
                                        uint32_t arg = 0u) {
  return (arg << kSchedulerVmOpShift) | static_cast<uint32_t>(op);
}

constexpr uint32_t MakeSchedulerVmExprInstr(SchedulerVmExprOp op,
                                            uint32_t arg = 0u) {
  return (arg << kSchedulerVmOpShift) | static_cast<uint32_t>(op);
}

constexpr SchedulerVmOp DecodeSchedulerVmOp(uint32_t instr) {
  return static_cast<SchedulerVmOp>(instr & kSchedulerVmOpMask);
}

constexpr uint32_t DecodeSchedulerVmArg(uint32_t instr) {
  return instr >> kSchedulerVmOpShift;
}

constexpr uint32_t PackSchedulerVmForkArg(uint32_t count,
                                          SchedulerVmJoinKind kind) {
  return (static_cast<uint32_t>(kind) << kSchedulerVmForkJoinShift) |
         (count & kSchedulerVmForkCountMask);
}

constexpr uint32_t DecodeSchedulerVmForkCount(uint32_t arg) {
  return arg & kSchedulerVmForkCountMask;
}

constexpr SchedulerVmJoinKind DecodeSchedulerVmForkKind(uint32_t arg) {
  return static_cast<SchedulerVmJoinKind>(
      (arg >> kSchedulerVmForkJoinShift) & 0xFFu);
}

struct SchedulerVmExprTable {
  // Expression bytecode stream (stack-based ops, optional immediate words).
  std::vector<uint32_t> words;
  // Literal pool storage (implementation-defined layout per op/width).
  std::vector<uint32_t> imm_words;
};

struct SchedulerVmCondEntry {
  uint32_t kind = 0u;
  uint32_t val = 0u;
  uint32_t xz = 1u;
  uint32_t expr_offset = 0u;
};

struct SchedulerVmPackedSlot {
  uint32_t word_size = 0u;
  uint32_t array_size = 1u;
};

struct SchedulerVmSignalEntry {
  uint32_t val_slot = 0u;
  uint32_t xz_slot = 0u;
  uint32_t width = 0u;
  uint32_t array_size = 1u;
  uint32_t flags = 0u;
};

constexpr uint32_t kSchedulerVmSignalFlagReal = 1u << 0u;
constexpr uint32_t kSchedulerVmExprStackMax = 32u;

struct SchedulerVmCaseHeader {
  uint32_t kind = 0u;
  uint32_t strategy = 0u;
  uint32_t width = 0u;
  uint32_t entry_count = 0u;
  uint32_t entry_offset = 0u;
  uint32_t default_target = 0u;
};

struct SchedulerVmCaseEntry {
  uint32_t want_offset = 0u;
  uint32_t care_offset = 0u;
  uint32_t target = 0u;
};

struct SchedulerVmLayout {
  uint32_t proc_count = 0u;
  uint32_t words_per_proc = 0u;
  std::vector<uint32_t> bytecode;
  std::vector<uint32_t> proc_offsets;
  std::vector<uint32_t> proc_lengths;
  std::vector<SchedulerVmPackedSlot> packed_slots;
  std::vector<SchedulerVmSignalEntry> signal_entries;
  std::vector<SchedulerVmCondEntry> cond_entries;
  std::vector<SchedulerVmCaseHeader> case_headers;
  std::vector<SchedulerVmCaseEntry> case_entries;
  std::vector<uint64_t> case_words;
  SchedulerVmExprTable expr_table;
};

class SchedulerVmBuilder {
 public:
  void Emit(SchedulerVmOp op, uint32_t arg = 0u) {
    words_.push_back(MakeSchedulerVmInstr(op, arg));
  }

  void EmitCallGroup() { Emit(SchedulerVmOp::kCallGroup); }
  void EmitDone() { Emit(SchedulerVmOp::kDone); }

  const std::vector<uint32_t>& words() const { return words_; }

 private:
  std::vector<uint32_t> words_;
};

class SchedulerVmExprBuilder {
 public:
  uint32_t EmitOp(SchedulerVmExprOp op, uint32_t arg = 0u,
                  uint32_t extra = kSchedulerVmExprNoExtra) {
    const uint32_t offset = static_cast<uint32_t>(words_.size());
    words_.push_back(MakeSchedulerVmExprInstr(op, arg));
    if (extra != kSchedulerVmExprNoExtra) {
      words_.push_back(extra);
    }
    return offset;
  }

  uint32_t EmitImmTable(const std::vector<uint32_t>& words) {
    const uint32_t base = static_cast<uint32_t>(imm_words_.size());
    imm_words_.insert(imm_words_.end(), words.begin(), words.end());
    return base;
  }

  const std::vector<uint32_t>& words() const { return words_; }
  const std::vector<uint32_t>& imm_words() const { return imm_words_; }
  void Truncate(size_t word_size, size_t imm_size) {
    words_.resize(word_size);
    imm_words_.resize(imm_size);
  }

 private:
  std::vector<uint32_t> words_;
  std::vector<uint32_t> imm_words_;
};

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
  out->expr_table.words.clear();
  out->expr_table.imm_words.clear();
  const uint32_t proc_count = static_cast<uint32_t>(procs.size());
  if (proc_count == 0u) {
    if (error) {
      *error = "scheduler VM layout requires at least one proc";
    }
    return false;
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

inline bool BuildSchedulerVmSeedLayout(uint32_t proc_count,
                                       SchedulerVmLayout* out,
                                       std::string* error) {
  if (proc_count == 0u) {
    if (error) {
      *error = "scheduler VM enabled without proc count";
    }
    return false;
  }
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
