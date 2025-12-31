#include "codegen/host_codegen.hh"

#include <functional>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace gpga {

std::string EmitHostStub(const Module& module) {
  std::ostringstream out;
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
  bool has_tick = false;
  for (const auto& block : module.always_blocks) {
    if (block.edge == EdgeKind::kInitial) {
      has_initial = true;
    } else if (block.edge != EdgeKind::kCombinational) {
      has_tick = true;
    }
    for (const auto& stmt : block.statements) {
      if (stmt_needs_scheduler(stmt)) {
        needs_scheduler = true;
      }
    }
  }

  struct HostSignal {
    std::string name;
    int width = 1;
    int array_size = 0;
    bool is_real = false;
    bool is_trireg = false;
  };
  std::unordered_map<std::string, HostSignal> signals;
  for (const auto& port : module.ports) {
    HostSignal sig;
    sig.name = port.name;
    sig.width = port.width;
    sig.is_real = port.is_real;
    signals[port.name] = sig;
  }
  for (const auto& net : module.nets) {
    auto it = signals.find(net.name);
    if (it == signals.end()) {
      HostSignal sig;
      sig.name = net.name;
      sig.width = net.width;
      sig.array_size = net.array_size;
      sig.is_real = net.is_real;
      sig.is_trireg = net.type == NetType::kTrireg;
      signals[net.name] = sig;
    } else {
      if (net.array_size > 0) {
        it->second.array_size = net.array_size;
      }
      if (net.is_real) {
        it->second.is_real = true;
      }
      if (net.type == NetType::kTrireg) {
        it->second.is_trireg = true;
      }
    }
  }

  out << "#include \"runtime/metal_runtime.hh\"\n";
  out << "#include <algorithm>\n";
  out << "#include <cstdlib>\n";
  out << "#include <cstring>\n";
  out << "#include <fstream>\n";
  out << "#include <iostream>\n";
  out << "#include <sstream>\n";
  out << "#include <string>\n";
  out << "#include <unordered_map>\n";
  out << "#include <vector>\n\n";
  out << "namespace {\n";
  out << "bool ReadFile(const std::string& path, std::string* out) {\n";
  out << "  std::ifstream file(path);\n";
  out << "  if (!file) {\n";
  out << "    return false;\n";
  out << "  }\n";
  out << "  std::ostringstream ss;\n";
  out << "  ss << file.rdbuf();\n";
  out << "  *out = ss.str();\n";
  out << "  return true;\n";
  out << "}\n\n";
  out << "bool StartsWith(const std::string& value, const std::string& prefix) {\n";
  out << "  return value.size() >= prefix.size() &&\n";
  out << "         value.compare(0, prefix.size(), prefix) == 0;\n";
  out << "}\n\n";
  out << "bool EndsWith(const std::string& value, const std::string& suffix) {\n";
  out << "  return value.size() >= suffix.size() &&\n";
  out << "         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;\n";
  out << "}\n\n";
  out << "gpga::ModuleInfo BuildModuleInfo() {\n";
  out << "  gpga::ModuleInfo info;\n";
  out << "  info.name = \"" << module.name << "\";\n";
  out << "  info.signals.reserve(" << signals.size() << ");\n";
  for (const auto& entry : signals) {
    const HostSignal& sig = entry.second;
    out << "  info.signals.push_back({\""
        << sig.name << "\", " << sig.width << "u, " << sig.array_size
        << "u, " << (sig.is_real ? "true" : "false") << ", "
        << (sig.is_trireg ? "true" : "false") << "});\n";
  }
  out << "  return info;\n";
  out << "}\n\n";
  if (sys_info.has_tasks) {
    out << "gpga::ServiceStringTable BuildStringTable() {\n";
    out << "  gpga::ServiceStringTable table;\n";
    for (const auto& entry : sys_info.string_table) {
      out << "  table.entries.push_back(\"" << escape_string(entry) << "\");\n";
    }
    out << "  return table;\n";
    out << "}\n\n";
  }
  out << "uint32_t ParseU32(const char* text, uint32_t fallback) {\n";
  out << "  if (!text || !*text) {\n";
  out << "    return fallback;\n";
  out << "  }\n";
  out << "  char* end = nullptr;\n";
  out << "  unsigned long value = std::strtoul(text, &end, 10);\n";
  out << "  if (!end || *end != '\\0') {\n";
  out << "    return fallback;\n";
  out << "  }\n";
  out << "  return static_cast<uint32_t>(value);\n";
  out << "}\n\n";
  out << "void MergeSpecs(std::unordered_map<std::string, size_t>* lengths,\n";
  out << "                const std::vector<gpga::BufferSpec>& specs) {\n";
  out << "  for (const auto& spec : specs) {\n";
  out << "    size_t& current = (*lengths)[spec.name];\n";
  out << "    current = std::max(current, spec.length);\n";
  out << "  }\n";
  out << "}\n\n";
  out << "bool BuildBindings(const gpga::MetalKernel& kernel,\n";
  out << "                   const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,\n";
  out << "                   std::vector<gpga::MetalBufferBinding>* bindings,\n";
  out << "                   std::string* error) {\n";
  out << "  bindings->clear();\n";
  out << "  for (const auto& entry : kernel.BufferIndices()) {\n";
  out << "    auto it = buffers.find(entry.first);\n";
  out << "    if (it == buffers.end()) {\n";
  out << "      if (error) {\n";
  out << "        *error = \"Missing buffer for \" + entry.first;\n";
  out << "      }\n";
  out << "      return false;\n";
  out << "    }\n";
  out << "    bindings->push_back({entry.second, &it->second, 0});\n";
  out << "  }\n";
  out << "  return true;\n";
  out << "}\n\n";
  out << "void SwapNextBuffers(std::unordered_map<std::string, gpga::MetalBuffer>* buffers) {\n";
  out << "  std::vector<std::pair<std::string, std::string>> swaps;\n";
  out << "  for (const auto& entry : *buffers) {\n";
  out << "    const std::string& name = entry.first;\n";
  out << "    if (EndsWith(name, \"_next_val\")) {\n";
  out << "      std::string base = name.substr(0, name.size() - 9) + \"_val\";\n";
  out << "      swaps.emplace_back(base, name);\n";
  out << "    } else if (EndsWith(name, \"_next_xz\")) {\n";
  out << "      std::string base = name.substr(0, name.size() - 8) + \"_xz\";\n";
  out << "      swaps.emplace_back(base, name);\n";
  out << "    } else if (EndsWith(name, \"_next\")) {\n";
  out << "      std::string base = name.substr(0, name.size() - 5);\n";
  out << "      swaps.emplace_back(base, name);\n";
  out << "    }\n";
  out << "  }\n";
  out << "  for (const auto& pair : swaps) {\n";
  out << "    auto it_a = buffers->find(pair.first);\n";
  out << "    auto it_b = buffers->find(pair.second);\n";
  out << "    if (it_a != buffers->end() && it_b != buffers->end()) {\n";
  out << "      std::swap(it_a->second, it_b->second);\n";
  out << "    }\n";
  out << "  }\n";
  out << "}\n";
  out << "}  // namespace\n\n";

  out << "int main(int argc, char** argv) {\n";
  out << "  if (argc < 2) {\n";
  out << "    std::cerr << \"Usage: \" << argv[0]\n";
  out << "              << \" <msl_file> [count] [--service-capacity N]\"\n";
  out << "              << \" [--max-steps N] [--max-proc-steps N]\\n\";\n";
  out << "    return 2;\n";
  out << "  }\n";
  out << "  std::string msl_path = argv[1];\n";
  out << "  uint32_t count = (argc >= 3 && argv[2][0] != '-') ? ParseU32(argv[2], 1) : 1u;\n";
  out << "  uint32_t service_capacity = 32u;\n";
  out << "  uint32_t max_steps = 1024u;\n";
  out << "  uint32_t max_proc_steps = 64u;\n";
  out << "  for (int i = 2; i < argc; ++i) {\n";
  out << "    std::string arg = argv[i];\n";
  out << "    if (arg == \"--service-capacity\" && i + 1 < argc) {\n";
  out << "      service_capacity = ParseU32(argv[++i], service_capacity);\n";
  out << "    } else if (arg == \"--max-steps\" && i + 1 < argc) {\n";
  out << "      max_steps = ParseU32(argv[++i], max_steps);\n";
  out << "    } else if (arg == \"--max-proc-steps\" && i + 1 < argc) {\n";
  out << "      max_proc_steps = ParseU32(argv[++i], max_proc_steps);\n";
  out << "    }\n";
  out << "  }\n\n";
  out << "  std::string msl_source;\n";
  out << "  if (!ReadFile(msl_path, &msl_source)) {\n";
  out << "    std::cerr << \"Failed to read MSL: \" << msl_path << \"\\n\";\n";
  out << "    return 1;\n";
  out << "  }\n\n";
  out << "  gpga::MetalRuntime runtime;\n";
  out << "  std::string error;\n";
  out << "  if (!runtime.CompileSource(msl_source, {\"include\"}, &error)) {\n";
  out << "    std::cerr << \"Metal compile failed: \" << error << \"\\n\";\n";
  out << "    return 1;\n";
  out << "  }\n\n";
  out << "  gpga::SchedulerConstants sched;\n";
  out << "  gpga::ParseSchedulerConstants(msl_source, &sched, &error);\n";
  out << "  gpga::ModuleInfo module = BuildModuleInfo();\n";
  out << "  module.four_state = msl_source.find(\"gpga_4state.h\") != std::string::npos;\n\n";

  out << "  const bool needs_scheduler = " << (needs_scheduler ? "true" : "false") << ";\n";
  out << "  const bool has_init = " << (has_initial ? "true" : "false") << ";\n";
  out << "  const bool has_tick = " << (has_tick ? "true" : "false") << ";\n\n";

  out << "  gpga::MetalKernel comb_kernel;\n";
  out << "  gpga::MetalKernel init_kernel;\n";
  out << "  gpga::MetalKernel tick_kernel;\n";
  out << "  gpga::MetalKernel sched_kernel;\n";
  out << "  if (needs_scheduler) {\n";
  if (sys_info.has_tasks) {
    out << "    gpga::ServiceStringTable strings = BuildStringTable();\n";
  } else {
    out << "    gpga::ServiceStringTable strings;\n";
  }
  out << "    if (!runtime.CreateKernel(\"gpga_" << module.name << "_sched_step\", &sched_kernel, &error)) {\n";
  out << "      std::cerr << \"Failed to create scheduler kernel: \" << error << \"\\n\";\n";
  out << "      return 1;\n";
  out << "    }\n";
  out << "  } else {\n";
  out << "    if (!runtime.CreateKernel(\"gpga_" << module.name << "\", &comb_kernel, &error)) {\n";
  out << "      std::cerr << \"Failed to create comb kernel: \" << error << \"\\n\";\n";
  out << "      return 1;\n";
  out << "    }\n";
  out << "    if (has_init) {\n";
  out << "      if (!runtime.CreateKernel(\"gpga_" << module.name << "_init\", &init_kernel, &error)) {\n";
  out << "        std::cerr << \"Failed to create init kernel: \" << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "    }\n";
  out << "    if (has_tick) {\n";
  out << "      if (!runtime.CreateKernel(\"gpga_" << module.name << "_tick\", &tick_kernel, &error)) {\n";
  out << "        std::cerr << \"Failed to create tick kernel: \" << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "    }\n";
  out << "  }\n\n";

  out << "  std::unordered_map<std::string, size_t> buffer_lengths;\n";
  out << "  std::vector<gpga::BufferSpec> specs;\n";
  out << "  if (needs_scheduler) {\n";
  out << "    if (!gpga::BuildBufferSpecs(module, sched_kernel, sched, count, service_capacity, &specs, &error)) {\n";
  out << "      std::cerr << \"Buffer plan failed: \" << error << \"\\n\";\n";
  out << "      return 1;\n";
  out << "    }\n";
  out << "    MergeSpecs(&buffer_lengths, specs);\n";
  out << "  } else {\n";
  out << "    if (!gpga::BuildBufferSpecs(module, comb_kernel, sched, count, service_capacity, &specs, &error)) {\n";
  out << "      std::cerr << \"Buffer plan failed: \" << error << \"\\n\";\n";
  out << "      return 1;\n";
  out << "    }\n";
  out << "    MergeSpecs(&buffer_lengths, specs);\n";
  out << "    if (has_init) {\n";
  out << "      if (!gpga::BuildBufferSpecs(module, init_kernel, sched, count, service_capacity, &specs, &error)) {\n";
  out << "        std::cerr << \"Buffer plan failed: \" << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "      MergeSpecs(&buffer_lengths, specs);\n";
  out << "    }\n";
  out << "    if (has_tick) {\n";
  out << "      if (!gpga::BuildBufferSpecs(module, tick_kernel, sched, count, service_capacity, &specs, &error)) {\n";
  out << "        std::cerr << \"Buffer plan failed: \" << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "      MergeSpecs(&buffer_lengths, specs);\n";
  out << "    }\n";
  out << "  }\n\n";

  out << "  std::unordered_map<std::string, gpga::MetalBuffer> buffers;\n";
  out << "  for (const auto& entry : buffer_lengths) {\n";
  out << "    gpga::MetalBuffer buffer = runtime.CreateBuffer(entry.second, nullptr);\n";
  out << "    if (buffer.contents()) {\n";
  out << "      std::memset(buffer.contents(), 0, buffer.length());\n";
  out << "    }\n";
  out << "    buffers.emplace(entry.first, std::move(buffer));\n";
  out << "  }\n\n";

  out << "  auto params_it = buffers.find(\"params\");\n";
  out << "  if (params_it != buffers.end() && params_it->second.contents()) {\n";
  out << "    auto* params = static_cast<gpga::GpgaParams*>(params_it->second.contents());\n";
  out << "    params->count = count;\n";
  out << "  }\n";
  out << "  auto sched_it = buffers.find(\"sched\");\n";
  out << "  if (sched_it != buffers.end() && sched_it->second.contents()) {\n";
  out << "    auto* sched_params = static_cast<gpga::GpgaSchedParams*>(sched_it->second.contents());\n";
  out << "    sched_params->count = count;\n";
  out << "    sched_params->max_steps = max_steps;\n";
  out << "    sched_params->max_proc_steps = max_proc_steps;\n";
  out << "    sched_params->service_capacity = service_capacity;\n";
  out << "  }\n\n";

  out << "  if (needs_scheduler) {\n";
  out << "    std::vector<gpga::MetalBufferBinding> bindings;\n";
  out << "    if (!BuildBindings(sched_kernel, buffers, &bindings, &error)) {\n";
  out << "      std::cerr << error << \"\\n\";\n";
  out << "      return 1;\n";
  out << "    }\n";
  out << "    const uint32_t kStatusFinished = 2u;\n";
  out << "    const uint32_t kStatusError = 3u;\n";
  out << "    const uint32_t kStatusStopped = 4u;\n";
  out << "    bool running = true;\n";
  out << "    while (running) {\n";
  out << "      if (!runtime.Dispatch(sched_kernel, bindings, count, &error)) {\n";
  out << "        std::cerr << \"Dispatch failed: \" << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "      bool saw_finish = false;\n";
  out << "      if (sched_kernel.HasBuffer(\"sched_service\") &&\n";
  out << "          sched_kernel.HasBuffer(\"sched_service_count\")) {\n";
  out << "        auto* counts = static_cast<uint32_t*>(buffers[\"sched_service_count\"].contents());\n";
  out << "        auto* records = static_cast<uint8_t*>(buffers[\"sched_service\"].contents());\n";
  out << "        if (counts && records) {\n";
  out << "          size_t stride = gpga::ServiceRecordStride(std::max<uint32_t>(1, sched.service_max_args), sched.service_wide_words, module.four_state);\n";
  out << "          for (uint32_t gid = 0; gid < count; ++gid) {\n";
  out << "            uint32_t used = counts[gid];\n";
  out << "            if (used > service_capacity) {\n";
  out << "              used = service_capacity;\n";
  out << "            }\n";
  out << "            if (used == 0) {\n";
  out << "              continue;\n";
  out << "            }\n";
  out << "            const uint8_t* rec_base = records + (gid * service_capacity * stride);\n";
  out << "            gpga::ServiceDrainResult result = gpga::DrainSchedulerServices(\n";
  out << "                rec_base, used, std::max<uint32_t>(1, sched.service_max_args),\n";
  out << "                sched.service_wide_words, module.four_state, strings, std::cout);\n";
  out << "            if (result.saw_finish || result.saw_stop || result.saw_error) {\n";
  out << "              saw_finish = true;\n";
  out << "            }\n";
  out << "            counts[gid] = 0u;\n";
  out << "          }\n";
  out << "        }\n";
  out << "      }\n";
  out << "      auto* status = static_cast<uint32_t*>(buffers[\"sched_status\"].contents());\n";
  out << "      if (!status) {\n";
  out << "        break;\n";
  out << "      }\n";
  out << "      if (status[0] == kStatusFinished || status[0] == kStatusStopped ||\n";
  out << "          status[0] == kStatusError || saw_finish) {\n";
  out << "        running = false;\n";
  out << "      }\n";
  out << "    }\n";
  out << "  } else {\n";
  out << "    std::vector<gpga::MetalBufferBinding> bindings;\n";
  out << "    if (!BuildBindings(comb_kernel, buffers, &bindings, &error)) {\n";
  out << "      std::cerr << error << \"\\n\";\n";
  out << "      return 1;\n";
  out << "    }\n";
  out << "    if (has_init) {\n";
  out << "      std::vector<gpga::MetalBufferBinding> init_bindings;\n";
  out << "      if (!BuildBindings(init_kernel, buffers, &init_bindings, &error)) {\n";
  out << "        std::cerr << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "      if (!runtime.Dispatch(init_kernel, init_bindings, count, &error)) {\n";
  out << "        std::cerr << \"Init dispatch failed: \" << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "    }\n";
  out << "    if (!runtime.Dispatch(comb_kernel, bindings, count, &error)) {\n";
  out << "      std::cerr << \"Comb dispatch failed: \" << error << \"\\n\";\n";
  out << "      return 1;\n";
  out << "    }\n";
  out << "    if (has_tick) {\n";
  out << "      std::vector<gpga::MetalBufferBinding> tick_bindings;\n";
  out << "      if (!BuildBindings(tick_kernel, buffers, &tick_bindings, &error)) {\n";
  out << "        std::cerr << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "      if (!runtime.Dispatch(tick_kernel, tick_bindings, count, &error)) {\n";
  out << "        std::cerr << \"Tick dispatch failed: \" << error << \"\\n\";\n";
  out << "        return 1;\n";
  out << "      }\n";
  out << "      SwapNextBuffers(&buffers);\n";
  out << "    }\n";
  out << "  }\n";
  out << "  return 0;\n";
  out << "}\n";
  return out.str();
}

}  // namespace gpga
