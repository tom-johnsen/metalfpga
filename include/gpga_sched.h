/**
 * @file gpga_sched.h
 * @brief Scheduler constants, flags, and VM metadata for Metal simulation.
 */
#ifndef GPGA_SCHED_H
#define GPGA_SCHED_H

#if defined(__METAL_VERSION__)
#include <metal_stdlib>
using namespace metal;
#else
#include <cstdint>
// Fallback typedefs for editors/non-Metal tooling.
typedef uint32_t uint;
typedef uint64_t ulong;
#ifndef constant
#define constant const
#endif
#endif

/**
 * @brief Runtime scheduler parameters shared with the GPU.
 */
struct GpgaSchedParams {
  uint count;             ///< Number of processes to schedule.
  uint max_steps;         ///< Max total scheduler steps per tick.
  uint max_proc_steps;    ///< Max steps per process per tick.
  uint service_capacity;  ///< Capacity of the service record buffer.
};

constant constexpr uint GPGA_SCHED_NO_PARENT = 0xFFFFFFFFu; ///< Sentinel for no parent process.
constant constexpr uint GPGA_SCHED_WAIT_NONE = 0u; ///< No wait condition.
constant constexpr uint GPGA_SCHED_WAIT_TIME = 1u; ///< Wait for a time delay.
constant constexpr uint GPGA_SCHED_WAIT_EVENT = 2u; ///< Wait for an event trigger.
constant constexpr uint GPGA_SCHED_WAIT_COND = 3u; ///< Wait until a condition is true.
constant constexpr uint GPGA_SCHED_WAIT_JOIN = 4u; ///< Wait for a join to complete.
constant constexpr uint GPGA_SCHED_WAIT_DELTA = 5u; ///< Wait for a delta cycle.
constant constexpr uint GPGA_SCHED_WAIT_EDGE = 6u; ///< Wait for an edge event.
constant constexpr uint GPGA_SCHED_WAIT_SERVICE = 7u; ///< Wait for a service result.
constant constexpr uint GPGA_SCHED_EDGE_ANY = 0u; ///< Any edge qualifies.
constant constexpr uint GPGA_SCHED_EDGE_POSEDGE = 1u; ///< Rising edge qualifier.
constant constexpr uint GPGA_SCHED_EDGE_NEGEDGE = 2u; ///< Falling edge qualifier.
constant constexpr uint GPGA_SCHED_EDGE_LIST = 3u; ///< Edge list qualifier.
constant constexpr uint GPGA_SCHED_PROC_READY = 0u; ///< Process is ready to run.
constant constexpr uint GPGA_SCHED_PROC_BLOCKED = 1u; ///< Process is blocked on a wait.
constant constexpr uint GPGA_SCHED_PROC_DONE = 2u; ///< Process has completed.
constant constexpr uint GPGA_SCHED_PHASE_ACTIVE = 0u; ///< Active event phase.
constant constexpr uint GPGA_SCHED_PHASE_NBA = 1u; ///< Non-blocking assignment phase.
constant constexpr uint GPGA_SCHED_STATUS_RUNNING = 0u; ///< Scheduler is running.
constant constexpr uint GPGA_SCHED_STATUS_IDLE = 1u; ///< Scheduler is idle.
constant constexpr uint GPGA_SCHED_STATUS_FINISHED = 2u; ///< Scheduler finished normally.
constant constexpr uint GPGA_SCHED_STATUS_ERROR = 3u; ///< Scheduler halted on error.
constant constexpr uint GPGA_SCHED_STATUS_STOPPED = 4u; ///< Scheduler stopped by user.
constant constexpr uint GPGA_SCHED_FLAG_INITIALIZED = 1u; ///< Scheduler is initialized.
constant constexpr uint GPGA_SCHED_FLAG_ACTIVE_INIT = 2u; ///< Active region initialized.
constant constexpr uint GPGA_SCHED_FLAG_VM_DEBUG = 4u; ///< VM debug data enabled.
constant constexpr uint GPGA_SCHED_FLAG_EXEC_READY = 8u; ///< Ready to execute processes.
constant constexpr uint GPGA_SCHED_VM_DEBUG_WORDS = 16u; ///< Debug word count per VM record.
constant constexpr uint GPGA_SCHED_HALT_FINISH = 0u; ///< Halt due to $finish.
constant constexpr uint GPGA_SCHED_HALT_STOP = 1u; ///< Halt due to $stop.
constant constexpr uint GPGA_SCHED_HALT_ERROR = 2u; ///< Halt due to error.
constant constexpr uint GPGA_SCHED_HALT_NONE = 0xFFFFFFFFu; ///< No halt requested.

constant constexpr uint GPGA_SERVICE_INVALID_ID = 0xFFFFFFFFu; ///< Sentinel for invalid service id.
constant constexpr uint GPGA_SERVICE_ARG_VALUE = 0u; ///< Service argument is a value.
constant constexpr uint GPGA_SERVICE_ARG_IDENT = 1u; ///< Service argument is an identifier.
constant constexpr uint GPGA_SERVICE_ARG_STRING = 2u; ///< Service argument is a string.
constant constexpr uint GPGA_SERVICE_ARG_REAL = 3u; ///< Service argument is a real.
constant constexpr uint GPGA_SERVICE_ARG_WIDE = 4u; ///< Service argument is a wide value.
constant constexpr uint GPGA_SERVICE_KIND_DISPLAY = 0u; ///< $display service.
constant constexpr uint GPGA_SERVICE_KIND_MONITOR = 1u; ///< $monitor service.
constant constexpr uint GPGA_SERVICE_KIND_FINISH = 2u; ///< $finish service.
constant constexpr uint GPGA_SERVICE_KIND_DUMPFILE = 3u; ///< $dumpfile service.
constant constexpr uint GPGA_SERVICE_KIND_DUMPVARS = 4u; ///< $dumpvars service.
constant constexpr uint GPGA_SERVICE_KIND_READMEMH = 5u; ///< $readmemh service.
constant constexpr uint GPGA_SERVICE_KIND_READMEMB = 6u; ///< $readmemb service.
constant constexpr uint GPGA_SERVICE_KIND_STOP = 7u; ///< $stop service.
constant constexpr uint GPGA_SERVICE_KIND_STROBE = 8u; ///< $strobe service.
constant constexpr uint GPGA_SERVICE_KIND_DUMPOFF = 9u; ///< $dumpoff service.
constant constexpr uint GPGA_SERVICE_KIND_DUMPON = 10u; ///< $dumpon service.
constant constexpr uint GPGA_SERVICE_KIND_DUMPFLUSH = 11u; ///< $dumpflush service.
constant constexpr uint GPGA_SERVICE_KIND_DUMPALL = 12u; ///< $dumpall service.
constant constexpr uint GPGA_SERVICE_KIND_DUMPLIMIT = 13u; ///< $dumplimit service.
constant constexpr uint GPGA_SERVICE_KIND_FWRITE = 14u; ///< $fwrite service.
constant constexpr uint GPGA_SERVICE_KIND_FDISPLAY = 15u; ///< $fdisplay service.
constant constexpr uint GPGA_SERVICE_KIND_FOPEN = 16u; ///< $fopen service.
constant constexpr uint GPGA_SERVICE_KIND_FCLOSE = 17u; ///< $fclose service.
constant constexpr uint GPGA_SERVICE_KIND_FGETC = 18u; ///< $fgetc service.
constant constexpr uint GPGA_SERVICE_KIND_FGETS = 19u; ///< $fgets service.
constant constexpr uint GPGA_SERVICE_KIND_FEOF = 20u; ///< $feof service.
constant constexpr uint GPGA_SERVICE_KIND_FSCANF = 21u; ///< $fscanf service.
constant constexpr uint GPGA_SERVICE_KIND_SSCANF = 22u; ///< $sscanf service.
constant constexpr uint GPGA_SERVICE_KIND_FTELL = 23u; ///< $ftell service.
constant constexpr uint GPGA_SERVICE_KIND_REWIND = 24u; ///< $rewind service.
constant constexpr uint GPGA_SERVICE_KIND_WRITEMEMH = 25u; ///< $writememh service.
constant constexpr uint GPGA_SERVICE_KIND_WRITEMEMB = 26u; ///< $writememb service.
constant constexpr uint GPGA_SERVICE_KIND_FSEEK = 27u; ///< $fseek service.
constant constexpr uint GPGA_SERVICE_KIND_FFLUSH = 28u; ///< $fflush service.
constant constexpr uint GPGA_SERVICE_KIND_FERROR = 29u; ///< $ferror service.
constant constexpr uint GPGA_SERVICE_KIND_FUNGETC = 30u; ///< $fungetc service.
constant constexpr uint GPGA_SERVICE_KIND_FREAD = 31u; ///< $fread service.
constant constexpr uint GPGA_SERVICE_KIND_WRITE = 32u; ///< $write service.
constant constexpr uint GPGA_SERVICE_KIND_SFORMAT = 33u; ///< $sformat service.
constant constexpr uint GPGA_SERVICE_KIND_TIMEFORMAT = 34u; ///< $timeformat service.
constant constexpr uint GPGA_SERVICE_KIND_PRINTTIMESCALE = 35u; ///< $printtimescale service.
constant constexpr uint GPGA_SERVICE_KIND_TESTPLUSARGS = 36u; ///< $test$plusargs service.
constant constexpr uint GPGA_SERVICE_KIND_VALUEPLUSARGS = 37u; ///< $value$plusargs service.
constant constexpr uint GPGA_SERVICE_KIND_ASYNC_AND_ARRAY = 38u; ///< Asynchronous AND array primitive.
constant constexpr uint GPGA_SERVICE_KIND_SYNC_OR_PLANE = 39u; ///< Synchronous OR plane primitive.
constant constexpr uint GPGA_SERVICE_KIND_ASYNC_NOR_PLANE = 40u; ///< Asynchronous NOR plane primitive.
constant constexpr uint GPGA_SERVICE_KIND_SYNC_NAND_PLANE = 41u; ///< Synchronous NAND plane primitive.
constant constexpr uint GPGA_SERVICE_KIND_SHOWCANCELLED = 42u; ///< $showcancelled service.

/**
 * @brief Define compile-time scheduler limits and capacities.
 *
 * @param proc_count Total number of processes.
 * @param root_count Number of root processes.
 * @param event_count Number of event objects.
 * @param edge_count Number of edge triggers.
 * @param edge_star_count Number of edge-* triggers.
 * @param max_ready Maximum size of the ready queue.
 * @param max_time Maximum pending time events.
 * @param max_nba Maximum non-blocking assignments.
 * @param repeat_count Number of repeat entries.
 * @param delay_count Number of delay entries.
 * @param max_dnba Maximum delayed non-blocking assignments.
 * @param monitor_count Number of monitor slots.
 * @param monitor_max_args Max arguments per monitor.
 * @param strobe_count Number of strobe slots.
 * @param service_max_args Max arguments per service call.
 * @param service_wide_words Words per wide service argument.
 * @param string_count Number of string literals.
 * @param force_count Number of force slots.
 * @param pcont_count Number of passign/continuous slots.
 */
#define GPGA_SCHED_DEFINE_CONSTANTS(proc_count, root_count, event_count, edge_count, edge_star_count, max_ready, max_time, max_nba, repeat_count, delay_count, max_dnba, monitor_count, monitor_max_args, strobe_count, service_max_args, service_wide_words, string_count, force_count, pcont_count) \
constant constexpr uint GPGA_SCHED_PROC_COUNT = proc_count; \
constant constexpr uint GPGA_SCHED_ROOT_COUNT = root_count; \
constant constexpr uint GPGA_SCHED_EVENT_COUNT = event_count; \
constant constexpr uint GPGA_SCHED_EDGE_COUNT = edge_count; \
constant constexpr uint GPGA_SCHED_EDGE_STAR_COUNT = edge_star_count; \
constant constexpr uint GPGA_SCHED_MAX_READY = max_ready; \
constant constexpr uint GPGA_SCHED_MAX_TIME = max_time; \
constant constexpr uint GPGA_SCHED_MAX_NBA = max_nba; \
constant constexpr uint GPGA_SCHED_REPEAT_COUNT = repeat_count; \
constant constexpr uint GPGA_SCHED_DELAY_COUNT = delay_count; \
constant constexpr uint GPGA_SCHED_MAX_DNBA = max_dnba; \
constant constexpr uint GPGA_SCHED_MONITOR_COUNT = monitor_count; \
constant constexpr uint GPGA_SCHED_MONITOR_MAX_ARGS = monitor_max_args; \
constant constexpr uint GPGA_SCHED_STROBE_COUNT = strobe_count; \
constant constexpr uint GPGA_SCHED_SERVICE_MAX_ARGS = service_max_args; \
constant constexpr uint GPGA_SCHED_SERVICE_WIDE_WORDS = service_wide_words; \
constant constexpr uint GPGA_SCHED_STRING_COUNT = string_count; \
constant constexpr uint GPGA_SCHED_FORCE_COUNT = force_count; \
constant constexpr uint GPGA_SCHED_PCONT_COUNT = pcont_count;

/**
 * @brief Define the gpga_sched_index helper for gid/pid indexing.
 */
#define GPGA_SCHED_DEFINE_INDEX() \
inline uint gpga_sched_index(uint gid, uint pid) { \
  return (gid * GPGA_SCHED_PROC_COUNT) + pid; \
}

/**
 * @brief Define a service record with fixed-width arguments.
 */
#define GPGA_SCHED_DEFINE_SERVICE_RECORD_SIMPLE() \
struct GpgaServiceRecord { \
  uint kind; /**< Service kind id. */ \
  uint pid; /**< Process id. */ \
  uint format_id; /**< Format string id. */ \
  uint arg_count; /**< Number of arguments. */ \
  uint arg_kind[GPGA_SCHED_SERVICE_MAX_ARGS]; /**< Argument kind codes. */ \
  uint arg_width[GPGA_SCHED_SERVICE_MAX_ARGS]; /**< Argument widths in bits. */ \
  ulong arg_val[GPGA_SCHED_SERVICE_MAX_ARGS]; /**< Argument value payloads. */ \
  ulong arg_xz[GPGA_SCHED_SERVICE_MAX_ARGS]; /**< Argument X/Z payloads. */ \
};

/**
 * @brief Define a service record with optional wide arguments.
 */
#define GPGA_SCHED_DEFINE_SERVICE_RECORD_WIDE() \
struct GpgaServiceRecord { \
  uint kind; /**< Service kind id. */ \
  uint pid; /**< Process id. */ \
  uint format_id; /**< Format string id. */ \
  uint arg_count; /**< Number of arguments. */ \
  uint arg_kind[GPGA_SCHED_SERVICE_MAX_ARGS]; /**< Argument kind codes. */ \
  uint arg_width[GPGA_SCHED_SERVICE_MAX_ARGS]; /**< Argument widths in bits. */ \
  ulong arg_val[GPGA_SCHED_SERVICE_MAX_ARGS]; /**< Argument value payloads. */ \
  ulong arg_xz[GPGA_SCHED_SERVICE_MAX_ARGS]; /**< Argument X/Z payloads. */ \
  ulong arg_wide_val[GPGA_SCHED_SERVICE_MAX_ARGS * GPGA_SCHED_SERVICE_WIDE_WORDS]; /**< Wide argument values. */ \
  ulong arg_wide_xz[GPGA_SCHED_SERVICE_MAX_ARGS * GPGA_SCHED_SERVICE_WIDE_WORDS]; /**< Wide argument X/Z masks. */ \
};

/**
 * @brief Header describing a VM case statement.
 */
struct GpgaSchedVmCaseHeader {
  uint kind;           ///< Case opcode kind.
  uint strategy;       ///< Match strategy identifier.
  uint width;          ///< Expression bit width.
  uint entry_count;    ///< Number of case entries.
  uint entry_offset;   ///< Offset to case entries.
  uint expr_offset;    ///< Offset to case expression.
  uint default_target; ///< Default target label.
};
/**
 * @brief Entry describing a VM conditional test.
 */
struct GpgaSchedVmCondEntry {
  uint kind;        ///< Condition kind.
  uint val;         ///< Condition value bits.
  uint xz;          ///< Condition X/Z mask.
  uint expr_offset; ///< Offset to condition expression.
};
/**
 * @brief Metadata for a signal referenced by the VM.
 */
struct GpgaSchedVmSignalEntry {
  uint val_offset; ///< Offset to value storage.
  uint xz_offset;  ///< Offset to X/Z storage.
  uint width;      ///< Signal width in bits.
  uint array_size; ///< Array size in elements.
  uint flags;      ///< Signal flags.
};
/**
 * @brief Entry describing a VM case match target.
 */
struct GpgaSchedVmCaseEntry {
  uint want_offset; ///< Offset to wanted value.
  uint care_offset; ///< Offset to care mask.
  uint target;      ///< Target label for a match.
};
/**
 * @brief VM assignment entry for a signal write.
 */
struct GpgaSchedVmAssignEntry {
  uint flags;        ///< Assignment flags.
  uint signal_id;    ///< Target signal id.
  uint rhs_expr;     ///< Offset to RHS expression.
  uint idx_expr;     ///< Offset to index expression.
  uint width;        ///< Assignment width in bits.
  uint base_width;   ///< Base signal width.
  uint range_lsb;    ///< Range LSB for part-selects.
  uint array_size;   ///< Array size in elements.
  uint force_slot;   ///< Force slot index.
  uint passign_slot; ///< Procedural assign slot index.
};
/**
 * @brief VM delayed assignment entry for a signal write.
 */
struct GpgaSchedVmDelayAssignEntry {
  uint flags;             ///< Delayed assignment flags.
  uint signal_id;         ///< Target signal id.
  uint rhs_expr;          ///< Offset to RHS expression.
  uint delay_expr;        ///< Offset to delay expression.
  uint idx_expr;          ///< Offset to index expression.
  uint width;             ///< Assignment width in bits.
  uint base_width;        ///< Base signal width.
  uint range_lsb;         ///< Range LSB for part-selects.
  uint array_size;        ///< Array size in elements.
  uint pulse_reject_expr; ///< Offset to pulse reject expression.
  uint pulse_error_expr;  ///< Offset to pulse error expression.
};
/**
 * @brief VM force entry for overriding signal values.
 */
struct GpgaSchedVmForceEntry {
  uint flags;        ///< Force flags.
  uint signal_id;    ///< Target signal id.
  uint rhs_expr;     ///< Offset to RHS expression.
  uint force_id;     ///< Force id.
  uint force_slot;   ///< Force slot index.
  uint passign_slot; ///< Procedural assign slot index.
};
/**
 * @brief VM release entry for clearing a force.
 */
struct GpgaSchedVmReleaseEntry {
  uint flags;        ///< Release flags.
  uint signal_id;    ///< Target signal id.
  uint force_slot;   ///< Force slot index.
  uint passign_slot; ///< Procedural assign slot index.
};
/**
 * @brief VM service call entry.
 */
struct GpgaSchedVmServiceEntry {
  uint kind;       ///< Service kind id.
  uint format_id;  ///< Format string id.
  uint arg_offset; ///< Offset to service arguments.
  uint arg_count;  ///< Number of arguments.
  uint flags;      ///< Service flags.
  uint aux;        ///< Auxiliary data.
};
/**
 * @brief VM service argument descriptor.
 */
struct GpgaSchedVmServiceArg {
  uint kind;    ///< Argument kind.
  uint width;   ///< Argument width in bits.
  uint payload; ///< Payload value or offset.
  uint flags;   ///< Argument flags.
};
/**
 * @brief VM service return assignment entry.
 */
struct GpgaSchedVmServiceRetAssignEntry {
  uint flags;        ///< Return assignment flags.
  uint signal_id;    ///< Target signal id.
  uint width;        ///< Assignment width in bits.
  uint force_slot;   ///< Force slot index.
  uint passign_slot; ///< Procedural assign slot index.
  uint reserved;     ///< Reserved for future use.
};

constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_NONBLOCKING = 1u << 0u; ///< Non-blocking assignment.
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_FALLBACK = 1u << 1u; ///< Use fallback path.
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_IS_ARRAY = 1u << 2u; ///< Target is an array.
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_IS_BIT_SELECT = 1u << 3u; ///< Bit-select assignment.
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_IS_RANGE = 1u << 4u; ///< Range assignment.
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_IS_INDEXED_RANGE = 1u << 5u; ///< Indexed range assignment.
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_WIDE_CONST = 1u << 6u; ///< RHS is a wide constant.
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_RHS_COND = 1u << 7u; ///< RHS uses a conditional.
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_RHS_SIGNED = 1u << 8u; ///< RHS is treated as signed.
constant constexpr uint GPGA_SCHED_VM_FORCE_FLAG_PROCEDURAL = 1u << 0u; ///< Procedural force.
constant constexpr uint GPGA_SCHED_VM_FORCE_FLAG_FALLBACK = 1u << 1u; ///< Use fallback path.
constant constexpr uint GPGA_SCHED_VM_FORCE_FLAG_OVERRIDE_REG = 1u << 2u; ///< Override reg storage.

constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_NONBLOCKING = 1u << 0u; ///< Delayed non-blocking assignment.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_INERTIAL = 1u << 1u; ///< Inertial delay semantics.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_SHOWCANCELLED = 1u << 2u; ///< Track showcancelled events.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_HAS_PULSE = 1u << 3u; ///< Has pulse reject expression.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_HAS_PULSE_ERROR = 1u << 4u; ///< Has pulse error expression.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_ARRAY = 1u << 5u; ///< Target is an array.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_BIT_SELECT = 1u << 6u; ///< Bit-select assignment.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_RANGE = 1u << 7u; ///< Range assignment.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_INDEXED_RANGE = 1u << 8u; ///< Indexed range assignment.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_REAL = 1u << 9u; ///< Assignment targets a real.
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_FALLBACK = 1u << 10u; ///< Use fallback path.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_FALLBACK = 1u << 0u; ///< Use fallback path.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_GLOBAL_ONLY = 1u << 1u; ///< Only allow global scope.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_GUARD_FD = 1u << 2u; ///< Guard file descriptor usage.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_MONITOR = 1u << 3u; ///< Monitor service.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_MONITOR_ON = 1u << 4u; ///< Enable monitor.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_MONITOR_OFF = 1u << 5u; ///< Disable monitor.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_STROBE = 1u << 6u; ///< Strobe service.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_FINISH = 1u << 7u; ///< Finish request.
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_STOP = 1u << 8u; ///< Stop request.
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_FLAG_EXPR = 1u << 0u; ///< Argument is an expression.
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_FLAG_TIME = 1u << 1u; ///< Argument is a time value.
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_FLAG_STIME = 1u << 2u; ///< Argument is a signed time value.
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_VALUE = 0u; ///< Value argument kind.
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_IDENT = 1u; ///< Identifier argument kind.
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_STRING = 2u; ///< String argument kind.
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_REAL = 3u; ///< Real argument kind.
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_WIDE = 4u; ///< Wide argument kind.
constant constexpr uint GPGA_SCHED_VM_SERVICE_RET_ASSIGN_FLAG_FALLBACK = 1u << 0u; ///< Use fallback path.

/**
 * @brief Define the process parent table.
 *
 * Use a list of parent process ids, one per process.
 */
#define GPGA_SCHED_DEFINE_PROC_PARENT(...) \
constant uint gpga_proc_parent[(GPGA_SCHED_PROC_COUNT > 0u) ? \
    GPGA_SCHED_PROC_COUNT : 1u] = { __VA_ARGS__ };

/**
 * @brief Define the process join tag table.
 *
 * Use a list of join tags, one per process.
 */
#define GPGA_SCHED_DEFINE_PROC_JOIN_TAG(...) \
constant uint gpga_proc_join_tag[(GPGA_SCHED_PROC_COUNT > 0u) ? \
    GPGA_SCHED_PROC_COUNT : 1u] = { __VA_ARGS__ };

#endif  // GPGA_SCHED_H
