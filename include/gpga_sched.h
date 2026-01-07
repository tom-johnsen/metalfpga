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

struct GpgaSchedParams { uint count; uint max_steps; uint max_proc_steps; uint service_capacity; };

constant constexpr uint GPGA_SCHED_NO_PARENT = 0xFFFFFFFFu;
constant constexpr uint GPGA_SCHED_WAIT_NONE = 0u;
constant constexpr uint GPGA_SCHED_WAIT_TIME = 1u;
constant constexpr uint GPGA_SCHED_WAIT_EVENT = 2u;
constant constexpr uint GPGA_SCHED_WAIT_COND = 3u;
constant constexpr uint GPGA_SCHED_WAIT_JOIN = 4u;
constant constexpr uint GPGA_SCHED_WAIT_DELTA = 5u;
constant constexpr uint GPGA_SCHED_WAIT_EDGE = 6u;
constant constexpr uint GPGA_SCHED_WAIT_SERVICE = 7u;
constant constexpr uint GPGA_SCHED_EDGE_ANY = 0u;
constant constexpr uint GPGA_SCHED_EDGE_POSEDGE = 1u;
constant constexpr uint GPGA_SCHED_EDGE_NEGEDGE = 2u;
constant constexpr uint GPGA_SCHED_EDGE_LIST = 3u;
constant constexpr uint GPGA_SCHED_PROC_READY = 0u;
constant constexpr uint GPGA_SCHED_PROC_BLOCKED = 1u;
constant constexpr uint GPGA_SCHED_PROC_DONE = 2u;
constant constexpr uint GPGA_SCHED_PHASE_ACTIVE = 0u;
constant constexpr uint GPGA_SCHED_PHASE_NBA = 1u;
constant constexpr uint GPGA_SCHED_STATUS_RUNNING = 0u;
constant constexpr uint GPGA_SCHED_STATUS_IDLE = 1u;
constant constexpr uint GPGA_SCHED_STATUS_FINISHED = 2u;
constant constexpr uint GPGA_SCHED_STATUS_ERROR = 3u;
constant constexpr uint GPGA_SCHED_STATUS_STOPPED = 4u;
constant constexpr uint GPGA_SCHED_FLAG_INITIALIZED = 1u;
constant constexpr uint GPGA_SCHED_FLAG_ACTIVE_INIT = 2u;
constant constexpr uint GPGA_SCHED_HALT_FINISH = 0u;
constant constexpr uint GPGA_SCHED_HALT_STOP = 1u;
constant constexpr uint GPGA_SCHED_HALT_ERROR = 2u;
constant constexpr uint GPGA_SCHED_HALT_NONE = 0xFFFFFFFFu;

constant constexpr uint GPGA_SERVICE_INVALID_ID = 0xFFFFFFFFu;
constant constexpr uint GPGA_SERVICE_ARG_VALUE = 0u;
constant constexpr uint GPGA_SERVICE_ARG_IDENT = 1u;
constant constexpr uint GPGA_SERVICE_ARG_STRING = 2u;
constant constexpr uint GPGA_SERVICE_ARG_REAL = 3u;
constant constexpr uint GPGA_SERVICE_ARG_WIDE = 4u;
constant constexpr uint GPGA_SERVICE_KIND_DISPLAY = 0u;
constant constexpr uint GPGA_SERVICE_KIND_MONITOR = 1u;
constant constexpr uint GPGA_SERVICE_KIND_FINISH = 2u;
constant constexpr uint GPGA_SERVICE_KIND_DUMPFILE = 3u;
constant constexpr uint GPGA_SERVICE_KIND_DUMPVARS = 4u;
constant constexpr uint GPGA_SERVICE_KIND_READMEMH = 5u;
constant constexpr uint GPGA_SERVICE_KIND_READMEMB = 6u;
constant constexpr uint GPGA_SERVICE_KIND_STOP = 7u;
constant constexpr uint GPGA_SERVICE_KIND_STROBE = 8u;
constant constexpr uint GPGA_SERVICE_KIND_DUMPOFF = 9u;
constant constexpr uint GPGA_SERVICE_KIND_DUMPON = 10u;
constant constexpr uint GPGA_SERVICE_KIND_DUMPFLUSH = 11u;
constant constexpr uint GPGA_SERVICE_KIND_DUMPALL = 12u;
constant constexpr uint GPGA_SERVICE_KIND_DUMPLIMIT = 13u;
constant constexpr uint GPGA_SERVICE_KIND_FWRITE = 14u;
constant constexpr uint GPGA_SERVICE_KIND_FDISPLAY = 15u;
constant constexpr uint GPGA_SERVICE_KIND_FOPEN = 16u;
constant constexpr uint GPGA_SERVICE_KIND_FCLOSE = 17u;
constant constexpr uint GPGA_SERVICE_KIND_FGETC = 18u;
constant constexpr uint GPGA_SERVICE_KIND_FGETS = 19u;
constant constexpr uint GPGA_SERVICE_KIND_FEOF = 20u;
constant constexpr uint GPGA_SERVICE_KIND_FSCANF = 21u;
constant constexpr uint GPGA_SERVICE_KIND_SSCANF = 22u;
constant constexpr uint GPGA_SERVICE_KIND_FTELL = 23u;
constant constexpr uint GPGA_SERVICE_KIND_REWIND = 24u;
constant constexpr uint GPGA_SERVICE_KIND_WRITEMEMH = 25u;
constant constexpr uint GPGA_SERVICE_KIND_WRITEMEMB = 26u;
constant constexpr uint GPGA_SERVICE_KIND_FSEEK = 27u;
constant constexpr uint GPGA_SERVICE_KIND_FFLUSH = 28u;
constant constexpr uint GPGA_SERVICE_KIND_FERROR = 29u;
constant constexpr uint GPGA_SERVICE_KIND_FUNGETC = 30u;
constant constexpr uint GPGA_SERVICE_KIND_FREAD = 31u;
constant constexpr uint GPGA_SERVICE_KIND_WRITE = 32u;
constant constexpr uint GPGA_SERVICE_KIND_SFORMAT = 33u;
constant constexpr uint GPGA_SERVICE_KIND_TIMEFORMAT = 34u;
constant constexpr uint GPGA_SERVICE_KIND_PRINTTIMESCALE = 35u;
constant constexpr uint GPGA_SERVICE_KIND_TESTPLUSARGS = 36u;
constant constexpr uint GPGA_SERVICE_KIND_VALUEPLUSARGS = 37u;
constant constexpr uint GPGA_SERVICE_KIND_ASYNC_AND_ARRAY = 38u;
constant constexpr uint GPGA_SERVICE_KIND_SYNC_OR_PLANE = 39u;
constant constexpr uint GPGA_SERVICE_KIND_ASYNC_NOR_PLANE = 40u;
constant constexpr uint GPGA_SERVICE_KIND_SYNC_NAND_PLANE = 41u;
constant constexpr uint GPGA_SERVICE_KIND_SHOWCANCELLED = 42u;

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

#define GPGA_SCHED_DEFINE_INDEX() \
inline uint gpga_sched_index(uint gid, uint pid) { \
  return (gid * GPGA_SCHED_PROC_COUNT) + pid; \
}

#define GPGA_SCHED_DEFINE_SERVICE_RECORD_SIMPLE() \
struct GpgaServiceRecord { \
  uint kind; \
  uint pid; \
  uint format_id; \
  uint arg_count; \
  uint arg_kind[GPGA_SCHED_SERVICE_MAX_ARGS]; \
  uint arg_width[GPGA_SCHED_SERVICE_MAX_ARGS]; \
  ulong arg_val[GPGA_SCHED_SERVICE_MAX_ARGS]; \
  ulong arg_xz[GPGA_SCHED_SERVICE_MAX_ARGS]; \
};

#define GPGA_SCHED_DEFINE_SERVICE_RECORD_WIDE() \
struct GpgaServiceRecord { \
  uint kind; \
  uint pid; \
  uint format_id; \
  uint arg_count; \
  uint arg_kind[GPGA_SCHED_SERVICE_MAX_ARGS]; \
  uint arg_width[GPGA_SCHED_SERVICE_MAX_ARGS]; \
  ulong arg_val[GPGA_SCHED_SERVICE_MAX_ARGS]; \
  ulong arg_xz[GPGA_SCHED_SERVICE_MAX_ARGS]; \
  ulong arg_wide_val[GPGA_SCHED_SERVICE_MAX_ARGS * GPGA_SCHED_SERVICE_WIDE_WORDS]; \
  ulong arg_wide_xz[GPGA_SCHED_SERVICE_MAX_ARGS * GPGA_SCHED_SERVICE_WIDE_WORDS]; \
};

struct GpgaSchedVmCaseHeader {
  uint kind;
  uint strategy;
  uint width;
  uint entry_count;
  uint entry_offset;
  uint expr_offset;
  uint default_target;
};
struct GpgaSchedVmCondEntry {
  uint kind;
  uint val;
  uint xz;
  uint expr_offset;
};
struct GpgaSchedVmSignalEntry {
  uint val_offset;
  uint xz_offset;
  uint width;
  uint array_size;
  uint flags;
};
struct GpgaSchedVmCaseEntry {
  uint want_offset;
  uint care_offset;
  uint target;
};
struct GpgaSchedVmAssignEntry {
  uint flags;
  uint signal_id;
  uint rhs_expr;
};
struct GpgaSchedVmDelayAssignEntry {
  uint flags;
  uint signal_id;
  uint rhs_expr;
  uint delay_expr;
  uint idx_expr;
  uint width;
  uint base_width;
  uint range_lsb;
  uint array_size;
  uint pulse_reject_expr;
  uint pulse_error_expr;
};
struct GpgaSchedVmForceEntry {
  uint flags;
  uint signal_id;
  uint rhs_expr;
  uint force_id;
  uint force_slot;
  uint passign_slot;
};
struct GpgaSchedVmReleaseEntry {
  uint flags;
  uint signal_id;
  uint force_slot;
  uint passign_slot;
};
struct GpgaSchedVmServiceEntry {
  uint kind;
  uint format_id;
  uint arg_offset;
  uint arg_count;
  uint flags;
  uint aux;
};
struct GpgaSchedVmServiceArg {
  uint kind;
  uint width;
  uint payload;
  uint flags;
};
struct GpgaSchedVmServiceRetAssignEntry {
  uint flags;
  uint signal_id;
  uint width;
  uint force_slot;
  uint passign_slot;
  uint reserved;
};

constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_NONBLOCKING = 1u << 0u;
constant constexpr uint GPGA_SCHED_VM_ASSIGN_FLAG_FALLBACK = 1u << 1u;
constant constexpr uint GPGA_SCHED_VM_FORCE_FLAG_PROCEDURAL = 1u << 0u;
constant constexpr uint GPGA_SCHED_VM_FORCE_FLAG_FALLBACK = 1u << 1u;
constant constexpr uint GPGA_SCHED_VM_FORCE_FLAG_OVERRIDE_REG = 1u << 2u;

constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_NONBLOCKING = 1u << 0u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_INERTIAL = 1u << 1u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_SHOWCANCELLED = 1u << 2u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_HAS_PULSE = 1u << 3u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_HAS_PULSE_ERROR = 1u << 4u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_ARRAY = 1u << 5u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_BIT_SELECT = 1u << 6u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_RANGE = 1u << 7u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_INDEXED_RANGE = 1u << 8u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_IS_REAL = 1u << 9u;
constant constexpr uint GPGA_SCHED_VM_DELAY_ASSIGN_FLAG_FALLBACK = 1u << 10u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_FALLBACK = 1u << 0u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_GLOBAL_ONLY = 1u << 1u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_GUARD_FD = 1u << 2u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_MONITOR = 1u << 3u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_MONITOR_ON = 1u << 4u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_MONITOR_OFF = 1u << 5u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_STROBE = 1u << 6u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_FINISH = 1u << 7u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_FLAG_STOP = 1u << 8u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_FLAG_EXPR = 1u << 0u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_FLAG_TIME = 1u << 1u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_FLAG_STIME = 1u << 2u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_VALUE = 0u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_IDENT = 1u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_STRING = 2u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_REAL = 3u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_ARG_KIND_WIDE = 4u;
constant constexpr uint GPGA_SCHED_VM_SERVICE_RET_ASSIGN_FLAG_FALLBACK = 1u << 0u;

#define GPGA_SCHED_DEFINE_PROC_PARENT(...) \
constant uint gpga_proc_parent[(GPGA_SCHED_PROC_COUNT > 0u) ? \
    GPGA_SCHED_PROC_COUNT : 1u] = { __VA_ARGS__ };

#define GPGA_SCHED_DEFINE_PROC_JOIN_TAG(...) \
constant uint gpga_proc_join_tag[(GPGA_SCHED_PROC_COUNT > 0u) ? \
    GPGA_SCHED_PROC_COUNT : 1u] = { __VA_ARGS__ };

#endif  // GPGA_SCHED_H
