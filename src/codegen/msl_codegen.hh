#pragma once

#include <string>
#include <vector>

#include "core/scheduler_vm.hh"
#include "frontend/ast.hh"

namespace gpga {

struct SchedulerVmExprFeatures {
  int width = 0;
  bool is_real = false;
  bool has_call = false;
  bool has_select = false;
  bool has_index = false;
  bool has_concat = false;
  bool has_repeat = false;
  bool has_ternary = false;
  bool has_xz = false;
  bool has_real_literal = false;
};

struct SchedulerVmAssignFallbackInfo {
  size_t index = 0;
  std::string stmt_text;
  std::string lhs;
  std::string lhs_text;
  std::string rhs_text;
  bool nonblocking = false;
  bool lhs_has_index = false;
  bool lhs_has_range = false;
  size_t lhs_index_count = 0;
  bool override_target = false;
  bool lhs_real = false;
  int lhs_width = 0;
  bool missing_signal = false;
  bool rhs_missing = false;
  bool rhs_unencodable = false;
  SchedulerVmExprFeatures rhs_features;
  std::vector<std::string> reasons;
};

struct SchedulerVmServiceFallbackInfo {
  size_t index = 0;
  bool is_syscall = false;
  std::string name;
  std::string call_text;
  size_t arg_count = 0;
  std::vector<std::string> reasons;
};

struct SchedulerVmFallbackDiagnostics {
  std::vector<SchedulerVmAssignFallbackInfo> assign_fallbacks;
  std::vector<SchedulerVmServiceFallbackInfo> service_fallbacks;
};

struct MslEmitOptions {
  bool four_state = false;
  bool sched_vm = false;
};

std::string EmitMSLStub(const Module& module,
                        const MslEmitOptions& options = {});

bool BuildSchedulerVmLayoutFromModule(const Module& module,
                                      SchedulerVmLayout* out,
                                      std::string* error,
                                      bool four_state);
bool BuildSchedulerVmLayoutFromModuleWithDiag(
    const Module& module,
    SchedulerVmLayout* out,
    std::string* error,
    bool four_state,
    SchedulerVmFallbackDiagnostics* diag);

}  // namespace gpga
