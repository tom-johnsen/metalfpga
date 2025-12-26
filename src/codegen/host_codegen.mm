#include "codegen/host_codegen.hh"

#include <functional>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace gpga {

std::string EmitHostStub(const Module& module) {
  std::ostringstream out;
  out << "// Placeholder host stub emitted by GPGA.\n";
  out << "// Module: " << module.name << "\n";
  struct SysTaskInfo {
    bool has_tasks = false;
    size_t max_args = 0;
    std::vector<std::string> string_table;
    std::unordered_map<std::string, size_t> string_ids;
  };
  auto is_system_task = [](const std::string& name) {
    return !name.empty() && name[0] == '$';
  };
  auto ident_as_string = [](const std::string& name) {
    return name == "$dumpvars" || name == "$readmemh" || name == "$readmemb";
  };
  auto add_string = [](SysTaskInfo& info, const std::string& value) {
    auto it = info.string_ids.find(value);
    if (it != info.string_ids.end()) {
      return;
    }
    info.string_ids[value] = info.string_table.size();
    info.string_table.push_back(value);
  };
  auto escape_string = [](const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
      switch (c) {
        case '\\':
          out += "\\\\";
          break;
        case '"':
          out += "\\\"";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\t':
          out += "\\t";
          break;
        case '\r':
          out += "\\r";
          break;
        default:
          out.push_back(c);
          break;
      }
    }
    return out;
  };
  SysTaskInfo sys_info;
  std::function<void(const Statement&)> collect_tasks;
  collect_tasks = [&](const Statement& stmt) {
    if (stmt.kind == StatementKind::kTaskCall &&
        is_system_task(stmt.task_name)) {
      sys_info.has_tasks = true;
      sys_info.max_args = std::max(sys_info.max_args, stmt.task_args.size());
      bool treat_ident = ident_as_string(stmt.task_name);
      for (const auto& arg : stmt.task_args) {
        if (!arg) {
          continue;
        }
        if (arg->kind == ExprKind::kString) {
          add_string(sys_info, arg->string_value);
        } else if (treat_ident && arg->kind == ExprKind::kIdentifier) {
          add_string(sys_info, arg->ident);
        }
      }
    }
    if (stmt.kind == StatementKind::kIf) {
      for (const auto& inner : stmt.then_branch) {
        collect_tasks(inner);
      }
      for (const auto& inner : stmt.else_branch) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kBlock) {
      for (const auto& inner : stmt.block) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kCase) {
      for (const auto& item : stmt.case_items) {
        for (const auto& inner : item.body) {
          collect_tasks(inner);
        }
      }
      for (const auto& inner : stmt.default_branch) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kFor) {
      for (const auto& inner : stmt.for_body) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kWhile) {
      for (const auto& inner : stmt.while_body) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kRepeat) {
      for (const auto& inner : stmt.repeat_body) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kDelay) {
      for (const auto& inner : stmt.delay_body) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kEventControl) {
      for (const auto& inner : stmt.event_body) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kWait) {
      for (const auto& inner : stmt.wait_body) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kForever) {
      for (const auto& inner : stmt.forever_body) {
        collect_tasks(inner);
      }
      return;
    }
    if (stmt.kind == StatementKind::kFork) {
      for (const auto& inner : stmt.fork_branches) {
        collect_tasks(inner);
      }
    }
  };
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      collect_tasks(stmt);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      collect_tasks(stmt);
    }
  }
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
    out << "//                      sched_wait_edge_kind/sched_edge_prev_*\n";
    out << "//                      sched_edge_star_prev_*\n";
    out << "//                      sched_error/sched_status + GpgaSchedParams\n";
    if (sys_info.has_tasks) {
      out << "//   service buffers: sched_service_count + sched_service\n";
      out << "//   sched.service_capacity = per-gid service record capacity\n";
      if (!sys_info.string_table.empty()) {
        out << "//   service string table:\n";
        for (size_t i = 0; i < sys_info.string_table.size(); ++i) {
          out << "//     [" << i << "] \""
              << escape_string(sys_info.string_table[i]) << "\"\n";
        }
      }
    }
    out << "//   polling hint:\n";
    out << "//     do { dispatch sched_step; } while (sched_status[gid] == RUNNING)\n";
    out << "//     stop when sched_status == FINISHED, STOPPED, or ERROR\n";
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
