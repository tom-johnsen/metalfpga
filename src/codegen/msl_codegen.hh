#pragma once

#include <string>

#include "core/scheduler_vm.hh"
#include "frontend/ast.hh"

namespace gpga {

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

}  // namespace gpga
