/**
 * @file msl_codegen.hh
 * @brief MSL codegen entry points and scheduler VM diagnostics.
 */
#pragma once

#include <string>
#include <vector>

#include "core/scheduler_vm.hh"
#include "frontend/ast.hh"

namespace gpga {

/// Features detected while encoding an expression for the scheduler VM.
struct SchedulerVmExprFeatures {
  int width = 0; ///< Expression bit width.
  bool is_real = false; ///< Expression evaluates to real.
  bool has_call = false; ///< Contains a function call.
  bool has_select = false; ///< Contains bit/part select.
  bool has_index = false; ///< Contains array indexing.
  bool has_concat = false; ///< Contains concatenation.
  bool has_repeat = false; ///< Contains replication.
  bool has_ternary = false; ///< Contains ternary operator.
  bool has_xz = false; ///< Requires X/Z tracking.
  bool has_real_literal = false; ///< Contains a real literal.
};

/// Details for an assignment that fell back to a slower path.
struct SchedulerVmAssignFallbackInfo {
  size_t index = 0; ///< Index in the fallback list.
  std::string stmt_text; ///< Source text for the statement.
  std::string lhs; ///< LHS signal name.
  std::string lhs_text; ///< LHS expression text.
  std::string rhs_text; ///< RHS expression text.
  bool nonblocking = false; ///< Assignment is nonblocking.
  bool lhs_has_index = false; ///< LHS includes array indexing.
  bool lhs_has_range = false; ///< LHS includes a range select.
  size_t lhs_index_count = 0; ///< Index count on LHS.
  bool override_target = false; ///< Target is overridden.
  bool lhs_real = false; ///< LHS is real-typed.
  int lhs_width = 0; ///< LHS bit width.
  bool missing_signal = false; ///< LHS signal could not be resolved.
  bool rhs_missing = false; ///< RHS expression missing.
  bool rhs_unencodable = false; ///< RHS could not be encoded.
  SchedulerVmExprFeatures rhs_features; ///< Detected RHS features.
  std::vector<std::string> reasons; ///< Human-readable reasons.
};

/// Details for a service call that fell back to a slower path.
struct SchedulerVmServiceFallbackInfo {
  size_t index = 0; ///< Index in the fallback list.
  bool is_syscall = false; ///< Service is a system task/function.
  std::string name; ///< Service name.
  std::string call_text; ///< Source text for the call.
  size_t arg_count = 0; ///< Argument count.
  std::vector<std::string> reasons; ///< Human-readable reasons.
};

/// Aggregated fallback diagnostics for scheduler VM lowering.
struct SchedulerVmFallbackDiagnostics {
  std::vector<SchedulerVmAssignFallbackInfo> assign_fallbacks; ///< Assignments.
  std::vector<SchedulerVmServiceFallbackInfo> service_fallbacks; ///< Services.
};

/// Options for emitting MSL stubs.
struct MslEmitOptions {
  bool four_state = false; ///< Enable 4-state support.
  bool sched_vm = false; ///< Emit scheduler VM support.
};

/**
 * @brief Emit a Metal Shading Language stub for a module.
 *
 * @param module Input module.
 * @param options Emission options.
 * @return Generated MSL source.
 */
std::string EmitMSLStub(const Module& module,
                        const MslEmitOptions& options = {});

/**
 * @brief Build a scheduler VM layout for a module.
 *
 * @param module Input module.
 * @param out Output scheduler VM layout.
 * @param error Optional error string destination.
 * @param four_state Enable 4-state semantics.
 * @param extra_signal_names Optional extra signals to record.
 * @return True on success.
 */
bool BuildSchedulerVmLayoutFromModule(const Module& module,
                                      SchedulerVmLayout* out,
                                      std::string* error,
                                      bool four_state,
                                      std::vector<std::string>* extra_signal_names = nullptr);
/**
 * @brief Build a scheduler VM layout with fallback diagnostics.
 *
 * @param module Input module.
 * @param out Output scheduler VM layout.
 * @param error Optional error string destination.
 * @param four_state Enable 4-state semantics.
 * @param diag Output fallback diagnostics.
 * @param extra_signal_names Optional extra signals to record.
 * @return True on success.
 */
bool BuildSchedulerVmLayoutFromModuleWithDiag(
    const Module& module,
    SchedulerVmLayout* out,
    std::string* error,
    bool four_state,
    SchedulerVmFallbackDiagnostics* diag,
    std::vector<std::string>* extra_signal_names = nullptr);

}  // namespace gpga
