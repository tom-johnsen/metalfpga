#include "codegen/host_codegen.hh"

#include <functional>
#include <sstream>

namespace gpga {

std::string EmitHostStub(const Module& module) {
  std::ostringstream out;
  out << "// Placeholder host stub emitted by GPGA.\n";
  out << "// Module: " << module.name << "\n";
  auto is_scheduler_stmt_kind = [](StatementKind kind) {
    switch (kind) {
      case StatementKind::kDelay:
      case StatementKind::kEventControl:
      case StatementKind::kEventTrigger:
      case StatementKind::kWait:
      case StatementKind::kForever:
      case StatementKind::kFork:
      case StatementKind::kDisable:
      case StatementKind::kTaskCall:
        return true;
      default:
        return false;
    }
  };
  std::function<bool(const Statement&)> stmt_needs_scheduler;
  stmt_needs_scheduler = [&](const Statement& stmt) -> bool {
    if (stmt.kind == StatementKind::kAssign && stmt.assign.delay) {
      return true;
    }
    if (is_scheduler_stmt_kind(stmt.kind)) {
      return true;
    }
    if (stmt.kind == StatementKind::kIf) {
      for (const auto& inner : stmt.then_branch) {
        if (stmt_needs_scheduler(inner)) {
          return true;
        }
      }
      for (const auto& inner : stmt.else_branch) {
        if (stmt_needs_scheduler(inner)) {
          return true;
        }
      }
    }
    if (stmt.kind == StatementKind::kBlock) {
      for (const auto& inner : stmt.block) {
        if (stmt_needs_scheduler(inner)) {
          return true;
        }
      }
    }
    if (stmt.kind == StatementKind::kCase) {
      for (const auto& item : stmt.case_items) {
        for (const auto& inner : item.body) {
          if (stmt_needs_scheduler(inner)) {
            return true;
          }
        }
      }
      for (const auto& inner : stmt.default_branch) {
        if (stmt_needs_scheduler(inner)) {
          return true;
        }
      }
    }
    if (stmt.kind == StatementKind::kFor) {
      for (const auto& inner : stmt.for_body) {
        if (stmt_needs_scheduler(inner)) {
          return true;
        }
      }
    }
    if (stmt.kind == StatementKind::kWhile) {
      for (const auto& inner : stmt.while_body) {
        if (stmt_needs_scheduler(inner)) {
          return true;
        }
      }
    }
    if (stmt.kind == StatementKind::kRepeat) {
      for (const auto& inner : stmt.repeat_body) {
        if (stmt_needs_scheduler(inner)) {
          return true;
        }
      }
    }
    return false;
  };

  bool has_initial = false;
  bool needs_scheduler = false;
  for (const auto& block : module.always_blocks) {
    if (block.edge == EdgeKind::kInitial) {
      has_initial = true;
    }
    for (const auto& stmt : block.statements) {
      if (stmt_needs_scheduler(stmt)) {
        needs_scheduler = true;
      }
    }
  }
  if (has_initial && !needs_scheduler) {
    out << "//   init kernel: gpga_" << module.name << "_init (run once)\n";
    out << "//   dispatch order: init -> comb -> tick (if present)\n";
  }
  if (needs_scheduler) {
    out << "//   scheduler kernel: gpga_" << module.name << "_sched_step\n";
    out << "//   dispatch order: init -> sched_step (loop until $finish)\n";
    out << "//   scheduler buffers: sched_pc/sched_state/sched_wait_*"
           "/sched_join_count\n";
    out << "//                      sched_parent/sched_join_tag/sched_time\n";
    out << "//                      sched_phase/sched_initialized/sched_event_pending\n";
    out << "//                      sched_error/sched_status + GpgaSchedParams\n";
    out << "//   polling hint:\n";
    out << "//     do { dispatch sched_step; } while (sched_status[gid] == RUNNING)\n";
    out << "//     stop when sched_status == FINISHED or ERROR\n";
  }
  for (const auto& port : module.ports) {
    out << "//   port " << port.name << " (width " << port.width << ")\n";
  }
  for (const auto& net : module.nets) {
    if (net.array_size <= 0) {
      continue;
    }
    out << "//   array " << net.name << " (element width " << net.width
        << ", length " << net.array_size << ")\n";
    out << "//     buffer size hint: params.count * " << net.array_size
        << " elements\n";
    out << "//     layout: index = gid * " << net.array_size
        << " + element_index\n";
    out << "//     double buffer: " << net.name << " + " << net.name
        << "_next (swap after tick)\n";
  }
  out << "\n";
  out << "// TODO: wire Metal pipeline state and buffer bindings.\n";
  return out.str();
}

}  // namespace gpga
