#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "codegen/host_codegen.hh"
#include "codegen/msl_codegen.hh"
#include "core/elaboration.hh"
#include "frontend/verilog_parser.hh"
#include "runtime/metal_runtime.hh"
#include "utils/diagnostics.hh"

namespace {

void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " <input.v> [<more.v> ...] [--emit-msl <path>] [--emit-host <path>]"
            << " [--dump-flat] [--top <module>] [--4state] [--auto]"
            << " [--run] [--count N] [--service-capacity N]"
            << " [--max-steps N] [--max-proc-steps N]"
            << " [--vcd-dir <path>] [--vcd-steps N]\n";
}

bool WriteFile(const std::string& path, const std::string& content,
               gpga::Diagnostics* diagnostics) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to open output file",
                     gpga::SourceLocation{path});
    return false;
  }
  out << content;
  if (!out) {
    diagnostics->Add(gpga::Severity::kError,
                     "failed to write output file",
                     gpga::SourceLocation{path});
    return false;
  }
  return true;
}

std::string DirLabel(gpga::PortDir dir) {
  switch (dir) {
    case gpga::PortDir::kInput:
      return "input";
    case gpga::PortDir::kOutput:
      return "output";
    case gpga::PortDir::kInout:
      return "inout";
  }
  return "unknown";
}

struct SysTaskInfo {
  bool has_tasks = false;
  bool has_dumpvars = false;
  size_t max_args = 0;
  std::vector<std::string> string_table;
  std::unordered_map<std::string, size_t> string_ids;
};

bool IsSystemTask(const std::string& name) {
  return !name.empty() && name[0] == '$';
}

bool IdentAsString(const std::string& name) {
  return name == "$dumpvars" || name == "$readmemh" || name == "$readmemb";
}

void AddString(SysTaskInfo* info, const std::string& value) {
  if (!info) {
    return;
  }
  auto it = info->string_ids.find(value);
  if (it != info->string_ids.end()) {
    return;
  }
  info->string_ids[value] = info->string_table.size();
  info->string_table.push_back(value);
}

void CollectTasks(const gpga::Statement& stmt, SysTaskInfo* info) {
  if (!info) {
    return;
  }
  if (stmt.kind == gpga::StatementKind::kTaskCall &&
      IsSystemTask(stmt.task_name)) {
    info->has_tasks = true;
    if (stmt.task_name == "$dumpvars") {
      info->has_dumpvars = true;
    }
    info->max_args = std::max(info->max_args, stmt.task_args.size());
    bool treat_ident = IdentAsString(stmt.task_name);
    for (const auto& arg : stmt.task_args) {
      if (!arg) {
        continue;
      }
      if (arg->kind == gpga::ExprKind::kString) {
        AddString(info, arg->string_value);
      } else if (treat_ident && arg->kind == gpga::ExprKind::kIdentifier) {
        AddString(info, arg->ident);
      }
    }
  }
  if (stmt.kind == gpga::StatementKind::kIf) {
    for (const auto& inner : stmt.then_branch) {
      CollectTasks(inner, info);
    }
    for (const auto& inner : stmt.else_branch) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kBlock) {
    for (const auto& inner : stmt.block) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kCase) {
    for (const auto& item : stmt.case_items) {
      for (const auto& inner : item.body) {
        CollectTasks(inner, info);
      }
    }
    for (const auto& inner : stmt.default_branch) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFor) {
    for (const auto& inner : stmt.for_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWhile) {
    for (const auto& inner : stmt.while_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kRepeat) {
    for (const auto& inner : stmt.repeat_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kDelay) {
    for (const auto& inner : stmt.delay_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kEventControl) {
    for (const auto& inner : stmt.event_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWait) {
    for (const auto& inner : stmt.wait_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kForever) {
    for (const auto& inner : stmt.forever_body) {
      CollectTasks(inner, info);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFork) {
    for (const auto& inner : stmt.fork_branches) {
      CollectTasks(inner, info);
    }
  }
}

gpga::ServiceStringTable BuildStringTable(const gpga::Module& module) {
  SysTaskInfo info;
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectTasks(stmt, &info);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      CollectTasks(stmt, &info);
    }
  }
  gpga::ServiceStringTable table;
  table.entries = std::move(info.string_table);
  return table;
}

bool ModuleUsesDumpvars(const gpga::Module& module) {
  SysTaskInfo info;
  for (const auto& block : module.always_blocks) {
    for (const auto& stmt : block.statements) {
      CollectTasks(stmt, &info);
    }
  }
  for (const auto& task : module.tasks) {
    for (const auto& stmt : task.body) {
      CollectTasks(stmt, &info);
    }
  }
  return info.has_dumpvars;
}

gpga::ModuleInfo BuildModuleInfo(const gpga::Module& module,
                                 bool four_state) {
  gpga::ModuleInfo info;
  info.name = module.name;
  info.four_state = four_state;
  std::unordered_map<std::string, gpga::SignalInfo> signals;
  for (const auto& port : module.ports) {
    gpga::SignalInfo sig;
    sig.name = port.name;
    sig.width = port.width;
    sig.is_real = port.is_real;
    signals[port.name] = sig;
  }
  for (const auto& net : module.nets) {
    auto it = signals.find(net.name);
    if (it == signals.end()) {
      gpga::SignalInfo sig;
      sig.name = net.name;
      sig.width = net.width;
      sig.array_size = net.array_size;
      sig.is_real = net.is_real;
      sig.is_trireg = net.type == gpga::NetType::kTrireg;
      signals[net.name] = sig;
    } else {
      it->second.width = std::max(it->second.width,
                                  static_cast<uint32_t>(net.width));
      if (net.array_size > 0) {
        it->second.array_size = net.array_size;
      }
      if (net.is_real) {
        it->second.is_real = true;
      }
      if (net.type == gpga::NetType::kTrireg) {
        it->second.is_trireg = true;
      }
    }
  }
  info.signals.reserve(signals.size());
  for (auto& entry : signals) {
    info.signals.push_back(std::move(entry.second));
  }
  return info;
}

uint32_t ReadU32(const uint8_t* base, size_t offset) {
  uint32_t value = 0;
  std::memcpy(&value, base + offset, sizeof(value));
  return value;
}

uint64_t ReadU64(const uint8_t* base, size_t offset) {
  uint64_t value = 0;
  std::memcpy(&value, base + offset, sizeof(value));
  return value;
}

std::string ResolveString(const gpga::ServiceStringTable& strings,
                          uint32_t id) {
  if (id >= strings.entries.size()) {
    return "<invalid_string_id>";
  }
  return strings.entries[id];
}

struct DecodedServiceRecord {
  gpga::ServiceKind kind = gpga::ServiceKind::kDisplay;
  uint32_t pid = 0;
  uint32_t format_id = 0xFFFFFFFFu;
  std::vector<gpga::ServiceArgView> args;
};

void DecodeServiceRecords(const void* records, uint32_t record_count,
                          uint32_t max_args, bool has_xz,
                          std::vector<DecodedServiceRecord>* out) {
  if (!out) {
    return;
  }
  out->clear();
  if (!records || record_count == 0 || max_args == 0) {
    return;
  }
  const auto* base = static_cast<const uint8_t*>(records);
  const size_t stride = gpga::ServiceRecordStride(max_args, has_xz);
  out->reserve(record_count);
  for (uint32_t i = 0; i < record_count; ++i) {
    const uint8_t* rec = base + (stride * i);
    DecodedServiceRecord record;
    uint32_t kind_raw = ReadU32(rec, 0);
    record.kind = static_cast<gpga::ServiceKind>(kind_raw);
    record.pid = ReadU32(rec, sizeof(uint32_t));
    record.format_id = ReadU32(rec, sizeof(uint32_t) * 2u);
    uint32_t arg_count = ReadU32(rec, sizeof(uint32_t) * 3u);
    if (arg_count > max_args) {
      arg_count = max_args;
    }

    size_t offset = sizeof(uint32_t) * 4u;
    const size_t arg_kind_offset = offset;
    const size_t arg_width_offset =
        arg_kind_offset + sizeof(uint32_t) * max_args;
    const size_t arg_val_offset =
        arg_width_offset + sizeof(uint32_t) * max_args;
    const size_t arg_xz_offset = arg_val_offset + sizeof(uint64_t) * max_args;

    record.args.reserve(arg_count);
    for (uint32_t a = 0; a < arg_count; ++a) {
      gpga::ServiceArgView arg;
      arg.kind = static_cast<gpga::ServiceArgKind>(
          ReadU32(rec, arg_kind_offset + sizeof(uint32_t) * a));
      arg.width = ReadU32(rec, arg_width_offset + sizeof(uint32_t) * a);
      arg.value = ReadU64(rec, arg_val_offset + sizeof(uint64_t) * a);
      if (has_xz) {
        arg.xz = ReadU64(rec, arg_xz_offset + sizeof(uint64_t) * a);
      }
      record.args.push_back(arg);
    }
    out->push_back(std::move(record));
  }
}

std::string VcdIdForIndex(size_t index) {
  const int base = 94;
  const int first = 33;
  std::string id;
  size_t value = index;
  do {
    int digit = static_cast<int>(value % base);
    id.push_back(static_cast<char>(first + digit));
    value /= base;
  } while (value > 0);
  return id;
}

struct VcdSignal {
  std::string name;
  std::string id;
  std::string base_name;
  uint32_t width = 1;
  uint32_t array_size = 1;
  uint32_t array_index = 0;
  uint32_t instance_index = 0;
  bool is_real = false;
  uint64_t last_val = 0;
  uint64_t last_xz = 0;
  bool has_value = false;
};

class VcdWriter {
 public:
  bool Start(const std::string& filename, const std::string& output_dir,
             const gpga::ModuleInfo& module,
             const std::vector<std::string>& filter, uint32_t depth,
             bool four_state,
             const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
             const std::string& timescale,
             uint32_t instance_count,
             const std::unordered_map<std::string, std::string>* flat_to_hier,
             std::string* error) {
    if (active_) {
      return true;
    }
    std::string path = filename.empty() ? "dump.vcd" : filename;
    if (!output_dir.empty()) {
      std::filesystem::path base(output_dir);
      std::filesystem::path out_path(path);
      if (!out_path.is_absolute()) {
        out_path = base / out_path;
      }
      std::error_code ec;
      std::filesystem::create_directories(out_path.parent_path(), ec);
      if (ec) {
        if (error) {
          *error = "failed to create VCD directory: " +
                   out_path.parent_path().string();
        }
        return false;
      }
      path = out_path.string();
    }
    out_.open(path, std::ios::out | std::ios::trunc);
    if (!out_) {
      if (error) {
        *error = "failed to open VCD file: " + path;
      }
      return false;
    }
    four_state_ = four_state;
    timescale_ = timescale.empty() ? "1ns" : timescale;
    dumping_ = true;
    bool dump_all = filter.empty();
    for (const auto& name : filter) {
      if (name == module.name) {
        dump_all = true;
        break;
      }
    }
    BuildSignals(module, filter, dump_all, depth, instance_count, flat_to_hier);
    WriteHeader(module.name);
    EmitInitialValues(buffers);
    active_ = true;
    return true;
  }

  void Update(uint64_t time,
              const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
    if (!dumping_) {
      return;
    }
    EmitSnapshot(time, buffers, false);
  }

  void FinalSnapshot(
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
    if (!active_) {
      return;
    }
    if (!dumping_) {
      return;
    }
    uint64_t time = has_time_ ? last_time_ : 0;
    if (!last_time_had_values_) {
      EmitSnapshot(time, buffers, true);
    }
  }

  void ForceSnapshot(
      uint64_t time,
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
    if (!active_ || !dumping_) {
      return;
    }
    EmitSnapshot(time, buffers, true);
  }

  void SetDumping(bool enabled) { dumping_ = enabled; }

  void SetDumpLimit(uint64_t limit) {
    dump_limit_ = limit;
    CheckDumpLimit();
  }

  void Flush() {
    if (out_) {
      out_.flush();
    }
  }

  void Close() {
    if (out_) {
      out_.flush();
      out_.close();
    }
    active_ = false;
  }

  bool active() const { return active_; }

 private:
  void BuildSignals(const gpga::ModuleInfo& module,
                    const std::vector<std::string>& filter, bool dump_all,
                    uint32_t depth_limit, uint32_t instance_count,
                    const std::unordered_map<std::string, std::string>* flat_to_hier) {
    std::unordered_set<std::string> wanted(filter.begin(), filter.end());
    signals_.clear();
    size_t index = 0;
    for (const auto& sig : module.signals) {
      std::string display_base = sig.name;
      if (flat_to_hier) {
        auto it = flat_to_hier->find(sig.name);
        if (it != flat_to_hier->end() && !it->second.empty()) {
          display_base = it->second;
        }
      }
      std::string display_rel = StripModulePrefix(display_base, module.name);
      if (display_rel.empty()) {
        display_rel = display_base;
      }
      uint32_t array_size = sig.array_size > 0 ? sig.array_size : 1u;
      uint32_t inst_count = std::max<uint32_t>(1u, instance_count);
      for (uint32_t inst = 0; inst < inst_count; ++inst) {
        std::string display_name = display_rel;
        if (inst_count > 1u) {
          display_name =
              "inst" + std::to_string(inst) + "." + display_rel;
        }
        for (uint32_t i = 0; i < array_size; ++i) {
          bool include = dump_all || wanted.empty();
          if (!include) {
            include =
                MatchesFilterName(wanted, module.name, display_name, i) ||
                MatchesFilterName(wanted, module.name, display_rel, i) ||
                MatchesFilterName(wanted, module.name, sig.name, i);
          }
          if (!include) {
            continue;
          }
          if (depth_limit > 0 && !PassesDepth(display_rel, depth_limit)) {
            continue;
          }
          VcdSignal entry;
          entry.base_name = sig.name;
          entry.array_size = array_size;
          entry.array_index = i;
          entry.instance_index = inst;
          entry.is_real = sig.is_real;
          entry.width = sig.is_real ? 64u : std::max<uint32_t>(1u, sig.width);
          entry.id = VcdIdForIndex(index++);
          if (array_size > 1u) {
            entry.name = display_name + "[" + std::to_string(i) + "]";
          } else {
            entry.name = display_name;
          }
          signals_.push_back(std::move(entry));
        }
      }
    }
  }

  bool PassesDepth(const std::string& name, uint32_t depth_limit) const {
    if (depth_limit == 0u) {
      return true;
    }
    std::vector<std::string> scope;
    std::string leaf;
    SplitHierName(name, &scope, &leaf);
    uint32_t depth = scope.empty() ? 1u
                                   : static_cast<uint32_t>(scope.size() + 1u);
    return depth <= depth_limit;
  }

  bool MatchesFilterName(const std::unordered_set<std::string>& wanted,
                         const std::string& module_name,
                         const std::string& name, uint32_t index) const {
    if (wanted.empty()) {
      return true;
    }
    const std::string indexed = name + "[" + std::to_string(index) + "]";
    for (const auto& raw : wanted) {
      std::string filter = raw;
      if (!module_name.empty() && StartsWith(filter, module_name + ".")) {
        filter = filter.substr(module_name.size() + 1);
      } else if (!module_name.empty() &&
                 StartsWith(filter, module_name + "__")) {
        filter = filter.substr(module_name.size() + 2);
      }
      if (filter == name || filter == indexed) {
        return true;
      }
      if (StartsWith(name, filter)) {
        if (name.size() == filter.size()) {
          return true;
        }
        char next = name[filter.size()];
        if (next == '.') {
          return true;
        }
        if (next == '_' && filter.size() + 1 < name.size() &&
            name[filter.size() + 1] == '_') {
          return true;
        }
      }
    }
    return false;
  }

  bool StartsWith(const std::string& value, const std::string& prefix) const {
    return value.size() >= prefix.size() &&
           value.compare(0, prefix.size(), prefix) == 0;
  }

  std::string StripModulePrefix(const std::string& name,
                                const std::string& module_name) const {
    if (module_name.empty()) {
      return name;
    }
    const std::string dot_prefix = module_name + ".";
    const std::string flat_prefix = module_name + "__";
    if (StartsWith(name, dot_prefix)) {
      return name.substr(dot_prefix.size());
    }
    if (StartsWith(name, flat_prefix)) {
      return name.substr(flat_prefix.size());
    }
    return name;
  }

  void SplitHierName(const std::string& name, std::vector<std::string>* scope,
                     std::string* leaf) const {
    if (scope) {
      scope->clear();
    }
    if (leaf) {
      *leaf = name;
    }
    if (!scope || !leaf) {
      return;
    }
    std::vector<std::string> parts;
    std::string current;
    int bracket_depth = 0;
    for (size_t i = 0; i < name.size(); ++i) {
      char c = name[i];
      if (c == '[') {
        bracket_depth++;
        current.push_back(c);
        continue;
      }
      if (c == ']') {
        if (bracket_depth > 0) {
          bracket_depth--;
        }
        current.push_back(c);
        continue;
      }
      if (bracket_depth == 0 && c == '.') {
        if (!current.empty()) {
          parts.push_back(current);
          current.clear();
        }
        continue;
      }
      if (bracket_depth == 0 && c == '_' && i + 1 < name.size() &&
          name[i + 1] == '_') {
        if (!current.empty()) {
          parts.push_back(current);
          current.clear();
        }
        i++;
        continue;
      }
      current.push_back(c);
    }
    if (!current.empty()) {
      parts.push_back(current);
    }
    if (parts.empty()) {
      return;
    }
    if (parts.size() == 1) {
      *leaf = parts[0];
      return;
    }
    scope->assign(parts.begin(), parts.end() - 1);
    *leaf = parts.back();
  }

  void WriteHeader(const std::string& module_name) {
    out_ << "$date\n  today\n$end\n";
    out_ << "$version\n  metalfpga\n$end\n";
    out_ << "$timescale " << timescale_ << " $end\n";
    out_ << "$scope module " << module_name << " $end\n";
    struct ScopedSignal {
      std::vector<std::string> scope;
      std::string leaf;
      const VcdSignal* signal = nullptr;
    };
    std::vector<ScopedSignal> ordered;
    ordered.reserve(signals_.size());
    for (const auto& sig : signals_) {
      ScopedSignal entry;
      entry.signal = &sig;
      SplitHierName(sig.name, &entry.scope, &entry.leaf);
      if (entry.leaf.empty()) {
        entry.leaf = sig.name;
      }
      ordered.push_back(std::move(entry));
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const ScopedSignal& a, const ScopedSignal& b) {
                if (a.scope != b.scope) {
                  return std::lexicographical_compare(
                      a.scope.begin(), a.scope.end(), b.scope.begin(),
                      b.scope.end());
                }
                return a.leaf < b.leaf;
              });
    std::vector<std::string> current;
    for (const auto& entry : ordered) {
      size_t common = 0;
      while (common < current.size() && common < entry.scope.size() &&
             current[common] == entry.scope[common]) {
        common++;
      }
      for (size_t i = current.size(); i > common; --i) {
        out_ << "$upscope $end\n";
      }
      for (size_t i = common; i < entry.scope.size(); ++i) {
        out_ << "$scope module " << entry.scope[i] << " $end\n";
      }
      current = entry.scope;
      const auto& sig = *entry.signal;
      if (sig.is_real) {
        out_ << "$var real 64 " << sig.id << " " << entry.leaf << " $end\n";
      } else {
        out_ << "$var wire " << sig.width << " " << sig.id << " "
             << entry.leaf << " $end\n";
      }
    }
    for (size_t i = current.size(); i > 0; --i) {
      out_ << "$upscope $end\n";
    }
    out_ << "$upscope $end\n";
    out_ << "$enddefinitions $end\n";
    CheckDumpLimit();
  }

  void EmitInitialValues(
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers) {
    out_ << "#0\n";
    for (auto& sig : signals_) {
      uint64_t val = 0;
      uint64_t xz = 0;
      if (!ReadSignal(sig, buffers, &val, &xz)) {
        continue;
      }
      sig.last_val = val;
      sig.last_xz = xz;
      sig.has_value = true;
      EmitValue(sig, val, xz);
    }
    last_time_ = 0;
    has_time_ = true;
    last_time_had_values_ = true;
    CheckDumpLimit();
  }

  bool ReadSignal(const VcdSignal& sig,
                  const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
                  uint64_t* val, uint64_t* xz) const {
    if (!val || !xz) {
      return false;
    }
    const gpga::MetalBuffer* val_buf = FindBuffer(buffers, sig.base_name, "_val");
    const gpga::MetalBuffer* xz_buf = nullptr;
    if (four_state_) {
      xz_buf = FindBuffer(buffers, sig.base_name, "_xz");
    }
    if (!val_buf) {
      return false;
    }
    uint32_t width = sig.is_real ? 64u : sig.width;
    size_t elem_size = (width > 32u) ? sizeof(uint64_t) : sizeof(uint32_t);
    size_t offset = elem_size * (static_cast<size_t>(sig.instance_index) *
                                     sig.array_size +
                                 sig.array_index);
    if (!val_buf->contents() || val_buf->length() < offset + elem_size) {
      return false;
    }
    std::memcpy(val, static_cast<const uint8_t*>(val_buf->contents()) + offset,
                elem_size);
    if (four_state_ && xz_buf && xz_buf->contents() &&
        xz_buf->length() >= offset + elem_size) {
      std::memcpy(xz, static_cast<const uint8_t*>(xz_buf->contents()) + offset,
                  elem_size);
    } else {
      *xz = 0;
    }
    return true;
  }

  const gpga::MetalBuffer* FindBuffer(
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
      const std::string& base, const char* suffix) const {
    auto it = buffers.find(base + suffix);
    if (it != buffers.end()) {
      return &it->second;
    }
    it = buffers.find(base);
    if (it != buffers.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void EmitValue(const VcdSignal& sig, uint64_t val, uint64_t xz) {
    if (sig.is_real) {
      double real_val = 0.0;
      std::memcpy(&real_val, &val, sizeof(real_val));
      out_ << "r" << real_val << " " << sig.id << "\n";
      return;
    }
    std::string bits;
    bits.reserve(sig.width);
    for (int i = static_cast<int>(sig.width) - 1; i >= 0; --i) {
      uint64_t mask = 1ull << static_cast<uint32_t>(i);
      bool bit_xz = (xz & mask) != 0ull;
      bool bit_val = (val & mask) != 0ull;
      if (bit_xz) {
        bits.push_back(bit_val ? 'x' : 'z');
      } else {
        bits.push_back(bit_val ? '1' : '0');
      }
    }
    if (sig.width == 1u) {
      out_ << bits[0] << sig.id << "\n";
    } else {
      out_ << "b" << bits << " " << sig.id << "\n";
    }
  }

  void EmitSnapshot(
      uint64_t time,
      const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
      bool force_values) {
    if (!active_) {
      return;
    }
    if (!has_time_ || time != last_time_) {
      out_ << "#" << time << "\n";
      last_time_ = time;
      has_time_ = true;
      last_time_had_values_ = false;
    }
    for (auto& sig : signals_) {
      uint64_t val = 0;
      uint64_t xz = 0;
      if (!ReadSignal(sig, buffers, &val, &xz)) {
        continue;
      }
      if (!force_values && sig.has_value && sig.last_val == val &&
          sig.last_xz == xz) {
        continue;
      }
      sig.last_val = val;
      sig.last_xz = xz;
      sig.has_value = true;
      EmitValue(sig, val, xz);
      last_time_had_values_ = true;
    }
    CheckDumpLimit();
  }

  void CheckDumpLimit() {
    if (!dumping_ || dump_limit_ == 0u || !out_) {
      return;
    }
    std::streampos pos = out_.tellp();
    if (pos < 0) {
      return;
    }
    uint64_t written = static_cast<uint64_t>(pos);
    if (written >= dump_limit_) {
      dumping_ = false;
    }
  }

  bool active_ = false;
  bool four_state_ = false;
  bool has_time_ = false;
  bool last_time_had_values_ = false;
  bool dumping_ = true;
  uint64_t last_time_ = 0;
  uint64_t dump_limit_ = 0;
  std::string timescale_ = "1ns";
  std::ofstream out_;
  std::vector<VcdSignal> signals_;
};

std::string StripComments(const std::string& line) {
  size_t pos = line.find("//");
  std::string out = (pos == std::string::npos) ? line : line.substr(0, pos);
  pos = out.find('#');
  if (pos != std::string::npos) {
    out = out.substr(0, pos);
  }
  return out;
}

std::string NormalizeToken(const std::string& token) {
  std::string out;
  out.reserve(token.size());
  for (char c : token) {
    if (c != '_') {
      out.push_back(c);
    }
  }
  return out;
}

bool ParseUnsigned(const std::string& token, int base, uint64_t* out) {
  if (!out) {
    return false;
  }
  try {
    size_t pos = 0;
    uint64_t value = std::stoull(token, &pos, base);
    if (pos != token.size()) {
      return false;
    }
    *out = value;
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseMemValue(const std::string& token_in, bool is_hex, uint32_t width,
                   uint64_t* val, uint64_t* xz) {
  if (!val || !xz) {
    return false;
  }
  std::string token = NormalizeToken(token_in);
  if (token.size() >= 2 && token[0] == '0' &&
      (token[1] == 'x' || token[1] == 'X')) {
    is_hex = true;
    token = token.substr(2);
  } else if (token.size() >= 2 && token[0] == '0' &&
             (token[1] == 'b' || token[1] == 'B')) {
    is_hex = false;
    token = token.substr(2);
  }
  uint64_t out_val = 0;
  uint64_t out_xz = 0;
  uint32_t max_bits = std::min<uint32_t>(width, 64u);
  if (is_hex) {
    int bit_pos = 0;
    for (int i = static_cast<int>(token.size()) - 1; i >= 0 && bit_pos < static_cast<int>(max_bits); --i) {
      char c = token[static_cast<size_t>(i)];
      uint8_t nibble = 0;
      bool is_x = false;
      bool is_z = false;
      if (c >= '0' && c <= '9') {
        nibble = static_cast<uint8_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        nibble = static_cast<uint8_t>(10 + (c - 'a'));
      } else if (c >= 'A' && c <= 'F') {
        nibble = static_cast<uint8_t>(10 + (c - 'A'));
      } else if (c == 'x' || c == 'X') {
        is_x = true;
      } else if (c == 'z' || c == 'Z') {
        is_z = true;
      } else {
        return false;
      }
      for (int b = 0; b < 4 && bit_pos < static_cast<int>(max_bits); ++b, ++bit_pos) {
        uint64_t mask = 1ull << static_cast<uint32_t>(bit_pos);
        if (is_x || is_z) {
          out_xz |= mask;
          if (is_x) {
            out_val |= mask;
          }
        } else if (nibble & (1u << b)) {
          out_val |= mask;
        }
      }
    }
  } else {
    int bit_pos = 0;
    for (int i = static_cast<int>(token.size()) - 1; i >= 0 && bit_pos < static_cast<int>(max_bits); --i, ++bit_pos) {
      char c = token[static_cast<size_t>(i)];
      uint64_t mask = 1ull << static_cast<uint32_t>(bit_pos);
      if (c == '1') {
        out_val |= mask;
      } else if (c == '0') {
        continue;
      } else if (c == 'x' || c == 'X') {
        out_xz |= mask;
        out_val |= mask;
      } else if (c == 'z' || c == 'Z') {
        out_xz |= mask;
      } else {
        return false;
      }
    }
  }
  *val = out_val;
  *xz = out_xz;
  return true;
}

bool ApplyReadmem(const std::string& filename, bool is_hex,
                  const gpga::SignalInfo& signal, bool four_state,
                  std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
                  uint32_t instance_count, uint64_t start, uint64_t end,
                  std::string* error) {
  if (!buffers) {
    return false;
  }
  std::ifstream in(filename);
  if (!in) {
    if (error) {
      *error = "failed to open readmem file: " + filename;
    }
    return false;
  }
  const std::string base = signal.name;
  auto it_val = buffers->find(base + "_val");
  if (it_val == buffers->end()) {
    it_val = buffers->find(base);
  }
  if (it_val == buffers->end()) {
    if (error) {
      *error = "readmem target buffer not found: " + base;
    }
    return false;
  }
  gpga::MetalBuffer* val_buf = &it_val->second;
  gpga::MetalBuffer* xz_buf = nullptr;
  if (four_state) {
    auto it_xz = buffers->find(base + "_xz");
    if (it_xz != buffers->end()) {
      xz_buf = &it_xz->second;
    }
  }
  uint32_t width = signal.is_real ? 64u : signal.width;
  if (width == 0u) {
    width = 1u;
  }
  size_t elem_size = (width > 32u) ? sizeof(uint64_t) : sizeof(uint32_t);
  uint32_t array_size = signal.array_size > 0 ? signal.array_size : 1u;
  if (end == std::numeric_limits<uint64_t>::max()) {
    end = (array_size > 0) ? static_cast<uint64_t>(array_size - 1u) : 0u;
  }
  if (start > end) {
    std::swap(start, end);
  }
  uint64_t address = start;
  std::string line;
  while (std::getline(in, line)) {
    line = StripComments(line);
    std::stringstream ss(line);
    std::string token;
    while (ss >> token) {
      token = NormalizeToken(token);
      if (token.empty()) {
        continue;
      }
      if (token[0] == '@') {
        uint64_t addr = 0;
        std::string addr_token = token.substr(1);
        if (!ParseUnsigned(addr_token, is_hex ? 16 : 2, &addr)) {
          if (error) {
            *error = "invalid readmem address: " + token;
          }
          return false;
        }
        address = addr;
        continue;
      }
      if (address > end) {
        return true;
      }
      uint64_t val = 0;
      uint64_t xz = 0;
      if (!ParseMemValue(token, is_hex, width, &val, &xz)) {
        if (error) {
          *error = "invalid readmem value: " + token;
        }
        return false;
      }
      if (address >= array_size) {
        address++;
        continue;
      }
      for (uint32_t gid = 0; gid < instance_count; ++gid) {
        size_t offset = (static_cast<size_t>(gid) * array_size + address) *
                        elem_size;
        if (val_buf->length() >= offset + elem_size) {
          std::memcpy(static_cast<uint8_t*>(val_buf->contents()) + offset,
                      &val, elem_size);
        }
        if (four_state && xz_buf && xz_buf->length() >= offset + elem_size) {
          std::memcpy(static_cast<uint8_t*>(xz_buf->contents()) + offset,
                      &xz, elem_size);
        }
      }
      address++;
    }
  }
  return true;
}

bool HandleServiceRecords(
    const std::vector<DecodedServiceRecord>& records,
    const gpga::ServiceStringTable& strings, const gpga::ModuleInfo& module,
    const std::string& vcd_dir,
    const std::unordered_map<std::string, std::string>* flat_to_hier,
    const std::string& timescale,
    bool four_state, uint32_t instance_count,
    std::unordered_map<std::string, gpga::MetalBuffer>* buffers,
    VcdWriter* vcd, std::string* dumpfile, std::string* error) {
  if (!buffers || !vcd || !dumpfile) {
    return false;
  }
  auto current_time = [&]() -> uint64_t {
    auto time_it = buffers->find("sched_time");
    if (time_it != buffers->end() && time_it->second.contents()) {
      uint64_t time = 0;
      std::memcpy(&time, time_it->second.contents(), sizeof(time));
      return time;
    }
    return 0;
  };
  for (const auto& rec : records) {
    switch (rec.kind) {
      case gpga::ServiceKind::kDumpfile: {
        *dumpfile = ResolveString(strings, rec.format_id);
        break;
      }
      case gpga::ServiceKind::kDumpvars: {
        std::vector<std::string> targets;
        size_t start = 0;
        uint32_t depth = 0;
        if (!rec.args.empty() &&
            rec.args[0].kind == gpga::ServiceArgKind::kValue) {
          depth = static_cast<uint32_t>(rec.args[0].value);
          start = 1;
        }
        for (size_t i = start; i < rec.args.size(); ++i) {
          const auto& arg = rec.args[i];
          if (arg.kind == gpga::ServiceArgKind::kString ||
              arg.kind == gpga::ServiceArgKind::kIdent) {
            targets.push_back(ResolveString(strings,
                                            static_cast<uint32_t>(arg.value)));
          }
        }
        if (!vcd->Start(*dumpfile, vcd_dir, module, targets, depth, four_state,
                        *buffers, timescale, instance_count, flat_to_hier,
                        error)) {
          return false;
        }
        break;
      }
      case gpga::ServiceKind::kDumpoff: {
        vcd->SetDumping(false);
        break;
      }
      case gpga::ServiceKind::kDumpon: {
        vcd->SetDumping(true);
        vcd->ForceSnapshot(current_time(), *buffers);
        break;
      }
      case gpga::ServiceKind::kDumpflush: {
        vcd->Flush();
        break;
      }
      case gpga::ServiceKind::kDumpall: {
        vcd->ForceSnapshot(current_time(), *buffers);
        break;
      }
      case gpga::ServiceKind::kDumplimit: {
        uint64_t limit = 0;
        if (!rec.args.empty() &&
            rec.args[0].kind == gpga::ServiceArgKind::kValue) {
          limit = rec.args[0].value;
        }
        vcd->SetDumpLimit(limit);
        break;
      }
      case gpga::ServiceKind::kReadmemh:
      case gpga::ServiceKind::kReadmemb: {
        std::string filename = ResolveString(strings, rec.format_id);
        std::string target;
        uint64_t start = 0;
        uint64_t end = std::numeric_limits<uint64_t>::max();
        bool seen_target = false;
        bool seen_start = false;
        for (const auto& arg : rec.args) {
          if (arg.kind == gpga::ServiceArgKind::kIdent ||
              arg.kind == gpga::ServiceArgKind::kString) {
            if (!seen_target &&
                arg.kind == gpga::ServiceArgKind::kString &&
                arg.value == static_cast<uint64_t>(rec.format_id)) {
              continue;
            }
            if (!seen_target) {
              target =
                  ResolveString(strings, static_cast<uint32_t>(arg.value));
              seen_target = true;
              continue;
            }
          }
          if (seen_target && arg.kind == gpga::ServiceArgKind::kValue) {
            if (!seen_start) {
              start = arg.value;
              seen_start = true;
            } else if (end == std::numeric_limits<uint64_t>::max()) {
              end = arg.value;
            }
          }
        }
        if (target.empty()) {
          continue;
        }
        auto it = std::find_if(module.signals.begin(), module.signals.end(),
                               [&](const gpga::SignalInfo& sig) {
                                 return sig.name == target;
                               });
        if (it == module.signals.end()) {
          if (error) {
            *error = "readmem target not found: " + target;
          }
          return false;
        }
        bool is_hex = rec.kind == gpga::ServiceKind::kReadmemh;
        if (!ApplyReadmem(filename, is_hex, *it, four_state, buffers,
                          instance_count, start, end, error)) {
          return false;
        }
        break;
      }
      default:
        break;
    }
  }
  return true;
}

bool HasKernel(const std::string& msl, const std::string& name) {
  return msl.find("kernel void " + name) != std::string::npos;
}

void MergeSpecs(std::unordered_map<std::string, size_t>* lengths,
                const std::vector<gpga::BufferSpec>& specs) {
  if (!lengths) {
    return;
  }
  for (const auto& spec : specs) {
    size_t& current = (*lengths)[spec.name];
    current = std::max(current, spec.length);
  }
}

bool BuildBindings(const gpga::MetalKernel& kernel,
                   const std::unordered_map<std::string, gpga::MetalBuffer>& buffers,
                   std::vector<gpga::MetalBufferBinding>* bindings,
                   std::string* error) {
  if (!bindings) {
    return false;
  }
  bindings->clear();
  for (const auto& entry : kernel.BufferIndices()) {
    auto it = buffers.find(entry.first);
    if (it == buffers.end()) {
      if (error) {
        *error = "missing buffer for " + entry.first;
      }
      return false;
    }
    bindings->push_back({entry.second, &it->second, 0});
  }
  return true;
}

void SwapNextBuffers(std::unordered_map<std::string, gpga::MetalBuffer>* buffers) {
  if (!buffers) {
    return;
  }
  std::vector<std::pair<std::string, std::string>> swaps;
  for (const auto& entry : *buffers) {
    const std::string& name = entry.first;
    if (name.size() > 9 && name.compare(name.size() - 9, 9, "_next_val") == 0) {
      swaps.emplace_back(name.substr(0, name.size() - 9) + "_val", name);
    } else if (name.size() > 8 &&
               name.compare(name.size() - 8, 8, "_next_xz") == 0) {
      swaps.emplace_back(name.substr(0, name.size() - 8) + "_xz", name);
    } else if (name.size() > 5 &&
               name.compare(name.size() - 5, 5, "_next") == 0) {
      swaps.emplace_back(name.substr(0, name.size() - 5), name);
    }
  }
  for (const auto& pair : swaps) {
    auto it_a = buffers->find(pair.first);
    auto it_b = buffers->find(pair.second);
    if (it_a != buffers->end() && it_b != buffers->end()) {
      std::swap(it_a->second, it_b->second);
    }
  }
}

bool RunMetal(const gpga::Module& module, const std::string& msl,
              const std::unordered_map<std::string, std::string>& flat_to_hier,
              bool enable_4state, uint32_t count, uint32_t service_capacity,
              uint32_t max_steps, uint32_t max_proc_steps,
              const std::string& vcd_dir, uint32_t vcd_steps,
              std::string* error) {
  gpga::MetalRuntime runtime;
  if (!runtime.CompileSource(msl, {"include"}, error)) {
    return false;
  }

  gpga::SchedulerConstants sched;
  gpga::ParseSchedulerConstants(msl, &sched, error);

  gpga::ModuleInfo info = BuildModuleInfo(module, enable_4state);
  const std::string base = "gpga_" + module.name;
  const bool has_sched = HasKernel(msl, base + "_sched_step");
  const bool has_init = HasKernel(msl, base + "_init");
  const bool has_tick = HasKernel(msl, base + "_tick");

  gpga::MetalKernel comb_kernel;
  gpga::MetalKernel init_kernel;
  gpga::MetalKernel tick_kernel;
  gpga::MetalKernel sched_kernel;

  if (has_sched) {
    if (!runtime.CreateKernel(base + "_sched_step", &sched_kernel, error)) {
      return false;
    }
  } else {
    if (!runtime.CreateKernel(base, &comb_kernel, error)) {
      return false;
    }
    if (has_init) {
      if (!runtime.CreateKernel(base + "_init", &init_kernel, error)) {
        return false;
      }
    }
    if (has_tick) {
      if (!runtime.CreateKernel(base + "_tick", &tick_kernel, error)) {
        return false;
      }
    }
  }

  std::unordered_map<std::string, size_t> buffer_lengths;
  std::vector<gpga::BufferSpec> specs;
  if (has_sched) {
    if (!gpga::BuildBufferSpecs(info, sched_kernel, sched, count,
                                service_capacity, &specs, error)) {
      return false;
    }
    MergeSpecs(&buffer_lengths, specs);
  } else {
    if (!gpga::BuildBufferSpecs(info, comb_kernel, sched, count,
                                service_capacity, &specs, error)) {
      return false;
    }
    MergeSpecs(&buffer_lengths, specs);
    if (has_init) {
      if (!gpga::BuildBufferSpecs(info, init_kernel, sched, count,
                                  service_capacity, &specs, error)) {
        return false;
      }
      MergeSpecs(&buffer_lengths, specs);
    }
    if (has_tick) {
      if (!gpga::BuildBufferSpecs(info, tick_kernel, sched, count,
                                  service_capacity, &specs, error)) {
        return false;
      }
      MergeSpecs(&buffer_lengths, specs);
    }
  }

  std::unordered_map<std::string, gpga::MetalBuffer> buffers;
  for (const auto& entry : buffer_lengths) {
    gpga::MetalBuffer buffer = runtime.CreateBuffer(entry.second, nullptr);
    if (buffer.contents()) {
      std::memset(buffer.contents(), 0, buffer.length());
    }
    buffers.emplace(entry.first, std::move(buffer));
  }

  const bool has_dumpvars = ModuleUsesDumpvars(module);
  const uint32_t vcd_step_budget = (vcd_steps > 0u) ? vcd_steps : 1u;
  uint32_t effective_max_steps = max_steps;
  if (has_dumpvars) {
    effective_max_steps = 1u;
  }

  auto params_it = buffers.find("params");
  if (params_it != buffers.end() && params_it->second.contents()) {
    auto* params =
        static_cast<gpga::GpgaParams*>(params_it->second.contents());
    params->count = count;
  }
  auto sched_it = buffers.find("sched");
  gpga::GpgaSchedParams* sched_params = nullptr;
  if (sched_it != buffers.end() && sched_it->second.contents()) {
    sched_params =
        static_cast<gpga::GpgaSchedParams*>(sched_it->second.contents());
    sched_params->max_steps = effective_max_steps;
    sched_params->max_proc_steps = max_proc_steps;
    sched_params->service_capacity = service_capacity;
  }

  gpga::ServiceStringTable strings = BuildStringTable(module);
  VcdWriter vcd;
  std::string dumpfile;

  if (has_sched) {
    std::vector<gpga::MetalBufferBinding> bindings;
    if (!BuildBindings(sched_kernel, buffers, &bindings, error)) {
      return false;
    }
    const uint32_t kStatusFinished = 2u;
    const uint32_t kStatusError = 3u;
    const uint32_t kStatusStopped = 4u;
    const uint32_t max_iters = 100000u;
    for (uint32_t iter = 0; iter < max_iters; ++iter) {
      if (sched_params && has_dumpvars) {
        sched_params->max_steps = vcd.active() ? vcd_step_budget : 1u;
      }
      if (!runtime.Dispatch(sched_kernel, bindings, count, error)) {
        return false;
      }
      bool saw_finish = false;
      if (sched_kernel.HasBuffer("sched_service") &&
          sched_kernel.HasBuffer("sched_service_count")) {
        auto* counts =
            static_cast<uint32_t*>(buffers["sched_service_count"].contents());
        auto* records =
            static_cast<uint8_t*>(buffers["sched_service"].contents());
        if (counts && records) {
          size_t stride =
              gpga::ServiceRecordStride(
                  std::max<uint32_t>(1u, sched.service_max_args),
                  enable_4state);
          for (uint32_t gid = 0; gid < count; ++gid) {
            uint32_t used = counts[gid];
            if (used > service_capacity) {
              used = service_capacity;
            }
            if (used == 0u) {
              continue;
            }
            const uint8_t* rec_base =
                records + (gid * service_capacity * stride);
            gpga::ServiceDrainResult result = gpga::DrainSchedulerServices(
                rec_base, used, std::max<uint32_t>(1u, sched.service_max_args),
                enable_4state, strings, std::cout);
            if (result.saw_finish || result.saw_stop || result.saw_error) {
              saw_finish = true;
            }
            std::vector<DecodedServiceRecord> decoded;
            DecodeServiceRecords(rec_base, used,
                                 std::max<uint32_t>(1u,
                                                    sched.service_max_args),
                                 enable_4state, &decoded);
            if (!HandleServiceRecords(decoded, strings, info, vcd_dir,
                                      &flat_to_hier, module.timescale,
                                      enable_4state, count, &buffers, &vcd,
                                      &dumpfile, error)) {
              return false;
            }
            counts[gid] = 0u;
          }
        }
      }
      if (vcd.active()) {
        auto time_it = buffers.find("sched_time");
        if (time_it != buffers.end() && time_it->second.contents()) {
          uint64_t time = 0;
          std::memcpy(&time, time_it->second.contents(), sizeof(time));
          vcd.Update(time, buffers);
        } else {
          vcd.Update(static_cast<uint64_t>(iter), buffers);
        }
      }
      auto* status =
          static_cast<uint32_t*>(buffers["sched_status"].contents());
      if (!status) {
        break;
      }
      if (status[0] == kStatusFinished || status[0] == kStatusStopped ||
          status[0] == kStatusError || saw_finish) {
        vcd.FinalSnapshot(buffers);
        vcd.Close();
        break;
      }
    }
  } else {
    std::vector<gpga::MetalBufferBinding> bindings;
    if (!BuildBindings(comb_kernel, buffers, &bindings, error)) {
      return false;
    }
    if (has_init) {
      std::vector<gpga::MetalBufferBinding> init_bindings;
      if (!BuildBindings(init_kernel, buffers, &init_bindings, error)) {
        return false;
      }
      if (!runtime.Dispatch(init_kernel, init_bindings, count, error)) {
        return false;
      }
    }
    if (!runtime.Dispatch(comb_kernel, bindings, count, error)) {
      return false;
    }
    if (has_tick) {
      std::vector<gpga::MetalBufferBinding> tick_bindings;
      if (!BuildBindings(tick_kernel, buffers, &tick_bindings, error)) {
        return false;
      }
      if (!runtime.Dispatch(tick_kernel, tick_bindings, count, error)) {
        return false;
      }
      SwapNextBuffers(&buffers);
    }
  }
  return true;
}

int SignalWidth(const gpga::Module& module, const std::string& name) {
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return port.width;
    }
  }
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return net.width;
    }
  }
  return 32;
}

bool SignalSigned(const gpga::Module& module, const std::string& name) {
  for (const auto& port : module.ports) {
    if (port.name == name) {
      return port.is_signed;
    }
  }
  for (const auto& net : module.nets) {
    if (net.name == name) {
      return net.is_signed;
    }
  }
  return false;
}

bool IsArrayNet(const gpga::Module& module, const std::string& name,
                int* element_width) {
  for (const auto& net : module.nets) {
    if (net.name == name &&
        (net.array_size > 0 || !net.array_dims.empty())) {
      if (element_width) {
        *element_width = net.width;
      }
      return true;
    }
  }
  return false;
}

int MinimalWidth(uint64_t value) {
  if (value == 0) {
    return 1;
  }
  int width = 0;
  while (value > 0) {
    value >>= 1;
    ++width;
  }
  return width;
}

int ExprWidth(const gpga::Expr& expr, const gpga::Module& module) {
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      return SignalWidth(module, expr.ident);
    case gpga::ExprKind::kNumber:
      if (expr.has_width && expr.number_width > 0) {
        return expr.number_width;
      }
      return MinimalWidth(expr.number);
    case gpga::ExprKind::kString:
      return 0;
    case gpga::ExprKind::kUnary:
      if (expr.unary_op == '!' || expr.unary_op == '&' ||
          expr.unary_op == '|' || expr.unary_op == '^') {
        return 1;
      }
      if (expr.unary_op == 'C') {
        return 32;
      }
      return expr.operand ? ExprWidth(*expr.operand, module) : 32;
    case gpga::ExprKind::kBinary:
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G' ||
          expr.op == 'A' || expr.op == 'O') {
        return 1;
      }
      if (expr.op == 'l' || expr.op == 'r' || expr.op == 'R') {
        return expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      }
      if (expr.op == 'p') {
        return expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
      }
      return std::max(expr.lhs ? ExprWidth(*expr.lhs, module) : 32,
                      expr.rhs ? ExprWidth(*expr.rhs, module) : 32);
    case gpga::ExprKind::kTernary:
      return std::max(expr.then_expr ? ExprWidth(*expr.then_expr, module) : 32,
                      expr.else_expr ? ExprWidth(*expr.else_expr, module) : 32);
    case gpga::ExprKind::kSelect: {
      if (expr.indexed_range && expr.indexed_width > 0) {
        return expr.indexed_width;
      }
      int lo = std::min(expr.msb, expr.lsb);
      int hi = std::max(expr.msb, expr.lsb);
      return hi - lo + 1;
    }
    case gpga::ExprKind::kIndex: {
      if (expr.base && expr.base->kind == gpga::ExprKind::kIdentifier) {
        int element_width = 0;
        if (IsArrayNet(module, expr.base->ident, &element_width)) {
          return element_width;
        }
      }
      return 1;
    }
    case gpga::ExprKind::kCall:
      if (expr.ident == "$time") {
        return 64;
      }
      if (expr.ident == "$realtobits") {
        return 64;
      }
      return 32;
    case gpga::ExprKind::kConcat: {
      int total = 0;
      for (const auto& element : expr.elements) {
        total += ExprWidth(*element, module);
      }
      return total * std::max(1, expr.repeat);
    }
  }
  return 32;
}

bool ExprSigned(const gpga::Expr& expr, const gpga::Module& module) {
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      return SignalSigned(module, expr.ident);
    case gpga::ExprKind::kNumber:
      return expr.is_signed || !expr.has_base;
    case gpga::ExprKind::kString:
      return false;
    case gpga::ExprKind::kUnary:
      if (expr.unary_op == 'S') {
        return true;
      }
      if (expr.unary_op == 'U') {
        return false;
      }
      if (expr.unary_op == 'C') {
        return false;
      }
      if (expr.unary_op == '!' || expr.unary_op == '&' ||
          expr.unary_op == '|' || expr.unary_op == '^') {
        return false;
      }
      if (expr.unary_op == '-' && expr.operand &&
          expr.operand->kind == gpga::ExprKind::kNumber) {
        return true;
      }
      return expr.operand ? ExprSigned(*expr.operand, module) : false;
    case gpga::ExprKind::kBinary: {
      if (expr.op == 'E' || expr.op == 'N' || expr.op == '<' ||
          expr.op == '>' || expr.op == 'L' || expr.op == 'G' ||
          expr.op == 'A' || expr.op == 'O') {
        return false;
      }
      if (expr.op == 'l' || expr.op == 'r' || expr.op == 'R') {
        return expr.lhs ? ExprSigned(*expr.lhs, module) : false;
      }
      bool lhs_signed = expr.lhs ? ExprSigned(*expr.lhs, module) : false;
      bool rhs_signed = expr.rhs ? ExprSigned(*expr.rhs, module) : false;
      return lhs_signed && rhs_signed;
    }
    case gpga::ExprKind::kTernary: {
      bool t_signed =
          expr.then_expr ? ExprSigned(*expr.then_expr, module) : false;
      bool e_signed =
          expr.else_expr ? ExprSigned(*expr.else_expr, module) : false;
      return t_signed && e_signed;
    }
    case gpga::ExprKind::kSelect:
    case gpga::ExprKind::kIndex:
    case gpga::ExprKind::kConcat:
      return false;
    case gpga::ExprKind::kCall:
      return false;
  }
  return false;
}

bool IsAllOnesExpr(const gpga::Expr& expr, const gpga::Module& module,
                   int* width_out) {
  switch (expr.kind) {
    case gpga::ExprKind::kNumber: {
      if (expr.x_bits != 0 || expr.z_bits != 0) {
        return false;
      }
      int width = expr.has_width && expr.number_width > 0
                      ? expr.number_width
                      : MinimalWidth(expr.number);
      if (width_out) {
        *width_out = width;
      }
      if (width <= 0) {
        return false;
      }
      if (width > 64) {
        return false;
      }
      uint64_t mask = (width == 64)
                          ? std::numeric_limits<uint64_t>::max()
                          : ((1ULL << width) - 1ULL);
      return expr.number == mask;
    }
    case gpga::ExprKind::kString:
      return false;
    case gpga::ExprKind::kConcat: {
      int base_width = 0;
      for (const auto& element : expr.elements) {
        int element_width = 0;
        if (!IsAllOnesExpr(*element, module, &element_width)) {
          return false;
        }
        base_width += element_width;
      }
      if (base_width <= 0) {
        return false;
      }
      int total = base_width * std::max(1, expr.repeat);
      if (width_out) {
        *width_out = total;
      }
      return true;
    }
    default:
      return false;
  }
}

const gpga::Expr* SimplifyAssignMask(const gpga::Expr& expr,
                                     const gpga::Module& module,
                                     int lhs_width) {
  if (expr.kind != gpga::ExprKind::kBinary || expr.op != '&') {
    return nullptr;
  }
  if (!expr.lhs || !expr.rhs) {
    return nullptr;
  }
  int lhs_expr_width = ExprWidth(*expr.lhs, module);
  int rhs_expr_width = ExprWidth(*expr.rhs, module);
  int target = std::max(lhs_expr_width, rhs_expr_width);
  if (lhs_expr_width == lhs_width &&
      IsAllOnesExpr(*expr.rhs, module, nullptr) &&
      ExprWidth(*expr.rhs, module) == target) {
    return expr.lhs.get();
  }
  if (rhs_expr_width == lhs_width &&
      IsAllOnesExpr(*expr.lhs, module, nullptr) &&
      ExprWidth(*expr.lhs, module) == target) {
    return expr.rhs.get();
  }
  return nullptr;
}

bool IsCompareOp(char op) {
  return op == 'E' || op == 'N' || op == '<' || op == '>' || op == 'L' ||
         op == 'G';
}

bool IsShiftOp(char op) { return op == 'l' || op == 'r' || op == 'R'; }

bool IsLogicalOp(char op) { return op == 'A' || op == 'O'; }

std::string ExprToString(const gpga::Expr& expr, const gpga::Module& module) {
  switch (expr.kind) {
    case gpga::ExprKind::kIdentifier:
      return expr.ident;
    case gpga::ExprKind::kNumber: {
      const char* digits = "0123456789ABCDEF";
      if (expr.has_base) {
        if (expr.x_bits != 0 || expr.z_bits != 0) {
          int width = expr.has_width && expr.number_width > 0
                          ? expr.number_width
                          : MinimalWidth(expr.number);
          int bits_per_digit = 1;
          switch (expr.base_char) {
            case 'b':
              bits_per_digit = 1;
              break;
            case 'o':
              bits_per_digit = 3;
              break;
            case 'h':
              bits_per_digit = 4;
              break;
            default:
              bits_per_digit = 1;
              break;
          }
          int digit_count =
              std::max(1, (width + bits_per_digit - 1) / bits_per_digit);
          std::string repr;
          repr.reserve(static_cast<size_t>(digit_count));
          for (int i = 0; i < digit_count; ++i) {
            int shift = (digit_count - 1 - i) * bits_per_digit;
            uint64_t mask_bits =
                (bits_per_digit >= 64) ? 0xFFFFFFFFFFFFFFFFull
                                       : ((1ull << bits_per_digit) - 1ull);
            uint64_t x =
                (shift >= 64) ? 0 : (expr.x_bits >> shift) & mask_bits;
            uint64_t z =
                (shift >= 64) ? 0 : (expr.z_bits >> shift) & mask_bits;
            if (x != 0) {
              repr.push_back('x');
              continue;
            }
            if (z != 0) {
              repr.push_back('z');
              continue;
            }
            uint64_t val = (shift >= 64)
                               ? 0
                               : (expr.value_bits >> shift) & mask_bits;
            repr.push_back(digits[static_cast<int>(val)]);
          }
          std::string prefix;
          if (expr.has_width && expr.number_width > 0) {
            prefix = std::to_string(expr.number_width);
          }
          std::string sign = expr.is_signed ? "s" : "";
          return prefix + "'" + sign + std::string(1, expr.base_char) + repr;
        }
        uint64_t value = expr.number;
        int base = 10;
        switch (expr.base_char) {
          case 'b':
            base = 2;
            break;
          case 'o':
            base = 8;
            break;
          case 'd':
            base = 10;
            break;
          case 'h':
            base = 16;
            break;
          default:
            base = 10;
            break;
        }
        std::string repr;
        if (value == 0) {
          repr = "0";
        } else {
          while (value > 0) {
            int digit = static_cast<int>(value % base);
            repr.insert(repr.begin(), digits[digit]);
            value /= static_cast<uint64_t>(base);
          }
        }
        std::string prefix;
        if (expr.has_width && expr.number_width > 0) {
          prefix = std::to_string(expr.number_width);
        }
        std::string sign = expr.is_signed ? "s" : "";
        return prefix + "'" + sign + std::string(1, expr.base_char) + repr;
      }
      if (expr.has_width && expr.number_width > 0) {
        return std::to_string(expr.number_width) + "'d" +
               std::to_string(expr.number);
      }
      return std::to_string(expr.number);
    }
    case gpga::ExprKind::kString:
      return "\"" + expr.string_value + "\"";
    case gpga::ExprKind::kUnary: {
      std::string operand =
          expr.operand ? ExprToString(*expr.operand, module) : "0";
      if (expr.unary_op == 'S') {
        return "$signed(" + operand + ")";
      }
      if (expr.unary_op == 'U') {
        return "$unsigned(" + operand + ")";
      }
      if (expr.unary_op == 'C') {
        return "$clog2(" + operand + ")";
      }
      return std::string(1, expr.unary_op) + operand;
    }
    case gpga::ExprKind::kBinary:
      {
        int lhs_width = expr.lhs ? ExprWidth(*expr.lhs, module) : 32;
        int rhs_width = expr.rhs ? ExprWidth(*expr.rhs, module) : 32;
        int target = std::max(lhs_width, rhs_width);
        bool signed_op = expr.lhs && expr.rhs &&
                         ExprSigned(*expr.lhs, module) &&
                         ExprSigned(*expr.rhs, module);
        std::string lhs = expr.lhs ? ExprToString(*expr.lhs, module) : "0";
        std::string rhs = expr.rhs ? ExprToString(*expr.rhs, module) : "0";
        if (!IsShiftOp(expr.op) && !IsLogicalOp(expr.op)) {
          if (lhs_width < target) {
            lhs = (signed_op ? "sext(" : "zext(") + lhs + ", " +
                  std::to_string(target) + ")";
          }
          if (rhs_width < target) {
            rhs = (signed_op ? "sext(" : "zext(") + rhs + ", " +
                  std::to_string(target) + ")";
          }
        }
        if (expr.op == 'E') {
          return "(" + lhs + " == " + rhs + ")";
        }
        if (expr.op == 'N') {
          return "(" + lhs + " != " + rhs + ")";
        }
        if (expr.op == 'C') {
          return "(" + lhs + " === " + rhs + ")";
        }
        if (expr.op == 'c') {
          return "(" + lhs + " !== " + rhs + ")";
        }
        if (expr.op == 'W') {
          return "(" + lhs + " ==? " + rhs + ")";
        }
        if (expr.op == 'w') {
          return "(" + lhs + " !=? " + rhs + ")";
        }
        if (expr.op == 'L') {
          return "(" + lhs + " <= " + rhs + ")";
        }
        if (expr.op == 'G') {
          return "(" + lhs + " >= " + rhs + ")";
        }
        if (expr.op == 'l') {
          return "(" + lhs + " << " + rhs + ")";
        }
        if (expr.op == 'r') {
          return "(" + lhs + " >> " + rhs + ")";
        }
        if (expr.op == 'R') {
          return "(" + lhs + " >>> " + rhs + ")";
        }
        if (expr.op == 'A') {
          return "(" + lhs + " && " + rhs + ")";
        }
        if (expr.op == 'O') {
          return "(" + lhs + " || " + rhs + ")";
        }
        return "(" + lhs + " " + expr.op + " " + rhs + ")";
      }
    case gpga::ExprKind::kTernary: {
      std::string cond =
          expr.condition ? ExprToString(*expr.condition, module) : "0";
      std::string then_expr =
          expr.then_expr ? ExprToString(*expr.then_expr, module) : "0";
      std::string else_expr =
          expr.else_expr ? ExprToString(*expr.else_expr, module) : "0";
      return "(" + cond + " ? " + then_expr + " : " + else_expr + ")";
    }
    case gpga::ExprKind::kSelect: {
      std::string base = ExprToString(*expr.base, module);
      if (expr.indexed_range && expr.indexed_width > 0) {
        const gpga::Expr* start =
            expr.indexed_desc ? expr.msb_expr.get() : expr.lsb_expr.get();
        std::string start_expr =
            start ? ExprToString(*start, module)
                  : std::to_string(expr.indexed_desc ? expr.msb : expr.lsb);
        std::string op = expr.indexed_desc ? "-:" : "+:";
        return base + "[" + start_expr + " " + op + " " +
               std::to_string(expr.indexed_width) + "]";
      }
      if (expr.has_range) {
        return base + "[" + std::to_string(expr.msb) + ":" +
               std::to_string(expr.lsb) + "]";
      }
      return base + "[" + std::to_string(expr.msb) + "]";
    }
    case gpga::ExprKind::kIndex: {
      std::string base = ExprToString(*expr.base, module);
      std::string index = expr.index ? ExprToString(*expr.index, module) : "0";
      return base + "[" + index + "]";
    }
    case gpga::ExprKind::kCall: {
      std::string out = expr.ident + "(";
      for (size_t i = 0; i < expr.call_args.size(); ++i) {
        if (i > 0) {
          out += ", ";
        }
        out += ExprToString(*expr.call_args[i], module);
      }
      out += ")";
      return out;
    }
    case gpga::ExprKind::kConcat: {
      std::string inner;
      for (size_t i = 0; i < expr.elements.size(); ++i) {
        if (i > 0) {
          inner += ", ";
        }
        inner += ExprToString(*expr.elements[i], module);
      }
      if (expr.repeat > 1) {
        return "{" + std::to_string(expr.repeat) + "{" + inner + "}}";
      }
      return "{" + inner + "}";
    }
  }
  return "<expr>";
}

void DumpStatement(const gpga::Statement& stmt, const gpga::Module& module,
                   int indent, std::ostream& os) {
  std::string pad(static_cast<size_t>(indent), ' ');
  if (stmt.kind == gpga::StatementKind::kAssign) {
    if (stmt.assign.rhs) {
      std::string lhs = stmt.assign.lhs;
      if (stmt.assign.lhs_index) {
        lhs += "[" + ExprToString(*stmt.assign.lhs_index, module) + "]";
      }
      os << pad << lhs
         << (stmt.assign.nonblocking ? " <= " : " = ")
         << (stmt.assign.delay ? "#" + ExprToString(*stmt.assign.delay, module) + " "
                               : "")
         << ExprToString(*stmt.assign.rhs, module) << ";\n";
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kIf) {
    const gpga::Statement* current = &stmt;
    bool first = true;
    while (current) {
      std::string cond = current->condition
                             ? ExprToString(*current->condition, module)
                             : "0";
      if (first) {
        os << pad << "if (" << cond << ") {\n";
        first = false;
      } else {
        os << pad << "} else if (" << cond << ") {\n";
      }
      for (const auto& inner : current->then_branch) {
        DumpStatement(inner, module, indent + 2, os);
      }
      if (current->else_branch.empty()) {
        os << pad << "}\n";
        break;
      }
      if (current->else_branch.size() == 1 &&
          current->else_branch[0].kind == gpga::StatementKind::kIf) {
        current = &current->else_branch[0];
        continue;
      }
      os << pad << "} else {\n";
      for (const auto& inner : current->else_branch) {
        DumpStatement(inner, module, indent + 2, os);
      }
      os << pad << "}\n";
      break;
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kCase) {
    std::string expr = stmt.case_expr ? ExprToString(*stmt.case_expr, module)
                                      : "0";
    const char* case_name = "case";
    if (stmt.case_kind == gpga::CaseKind::kCaseZ) {
      case_name = "casez";
    } else if (stmt.case_kind == gpga::CaseKind::kCaseX) {
      case_name = "casex";
    }
    os << pad << case_name << " (" << expr << ")\n";
    for (const auto& item : stmt.case_items) {
      std::string labels;
      for (size_t i = 0; i < item.labels.size(); ++i) {
        if (i > 0) {
          labels += ", ";
        }
        labels += ExprToString(*item.labels[i], module);
      }
      os << pad << "  " << labels << ":\n";
      for (const auto& inner : item.body) {
        DumpStatement(inner, module, indent + 4, os);
      }
    }
    if (!stmt.default_branch.empty()) {
      os << pad << "  default:\n";
      for (const auto& inner : stmt.default_branch) {
        DumpStatement(inner, module, indent + 4, os);
      }
    }
    os << pad << "endcase\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kBlock) {
    os << pad << "begin";
    if (!stmt.block_label.empty()) {
      os << " : " << stmt.block_label;
    }
    os << "\n";
    for (const auto& inner : stmt.block) {
      DumpStatement(inner, module, indent + 2, os);
    }
    os << pad << "end";
    if (!stmt.block_label.empty()) {
      os << " : " << stmt.block_label;
    }
    os << "\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kDelay) {
    std::string delay =
        stmt.delay ? ExprToString(*stmt.delay, module) : "0";
    os << pad << "#" << delay;
    if (stmt.delay_body.empty()) {
      os << ";\n";
      return;
    }
    os << "\n";
    for (const auto& inner : stmt.delay_body) {
      DumpStatement(inner, module, indent + 2, os);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kEventControl) {
    if (!stmt.event_items.empty()) {
      os << pad << "@(";
      for (size_t i = 0; i < stmt.event_items.size(); ++i) {
        if (i > 0) {
          os << ", ";
        }
        if (stmt.event_items[i].edge == gpga::EventEdgeKind::kPosedge) {
          os << "posedge ";
        } else if (stmt.event_items[i].edge == gpga::EventEdgeKind::kNegedge) {
          os << "negedge ";
        }
        if (stmt.event_items[i].expr) {
          os << ExprToString(*stmt.event_items[i].expr, module);
        } else {
          os << "/*missing*/";
        }
      }
      os << ")";
    } else if (!stmt.event_expr) {
      os << pad << "@*";
    } else {
      std::string expr = ExprToString(*stmt.event_expr, module);
      os << pad << "@(";
      if (stmt.event_edge == gpga::EventEdgeKind::kPosedge) {
        os << "posedge ";
      } else if (stmt.event_edge == gpga::EventEdgeKind::kNegedge) {
        os << "negedge ";
      }
      os << expr << ")";
    }
    if (stmt.event_body.empty()) {
      os << ";\n";
      return;
    }
    os << "\n";
    for (const auto& inner : stmt.event_body) {
      DumpStatement(inner, module, indent + 2, os);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kEventTrigger) {
    os << pad << "-> " << stmt.trigger_target << ";\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kWait) {
    std::string expr =
        stmt.wait_condition ? ExprToString(*stmt.wait_condition, module) : "0";
    os << pad << "wait (" << expr << ")";
    if (stmt.wait_body.empty()) {
      os << ";\n";
      return;
    }
    os << "\n";
    for (const auto& inner : stmt.wait_body) {
      DumpStatement(inner, module, indent + 2, os);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kForever) {
    os << pad << "forever\n";
    for (const auto& inner : stmt.forever_body) {
      DumpStatement(inner, module, indent + 2, os);
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kFork) {
    os << pad << "fork";
    if (!stmt.block_label.empty()) {
      os << " : " << stmt.block_label;
    }
    os << "\n";
    for (const auto& inner : stmt.fork_branches) {
      DumpStatement(inner, module, indent + 2, os);
    }
    os << pad << "join\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kDisable) {
    os << pad << "disable " << stmt.disable_target << ";\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kTaskCall) {
    os << pad << stmt.task_name << "(";
    for (size_t i = 0; i < stmt.task_args.size(); ++i) {
      if (i > 0) {
        os << ", ";
      }
      os << ExprToString(*stmt.task_args[i], module);
    }
    os << ");\n";
    return;
  }
  if (stmt.kind == gpga::StatementKind::kForce) {
    if (stmt.assign.rhs) {
      std::string lhs =
          stmt.force_target.empty() ? stmt.assign.lhs : stmt.force_target;
      if (stmt.assign.lhs_index) {
        lhs += "[" + ExprToString(*stmt.assign.lhs_index, module) + "]";
      }
      os << pad << "force " << lhs << " = "
         << ExprToString(*stmt.assign.rhs, module) << ";\n";
    }
    return;
  }
  if (stmt.kind == gpga::StatementKind::kRelease) {
    std::string lhs =
        stmt.release_target.empty() ? stmt.assign.lhs : stmt.release_target;
    if (stmt.assign.lhs_index) {
      lhs += "[" + ExprToString(*stmt.assign.lhs_index, module) + "]";
    }
    os << pad << "release " << lhs << ";\n";
    return;
  }
}

void DumpFlat(const gpga::ElaboratedDesign& design, std::ostream& os) {
  const auto& top = design.top;
  os << "Top: " << top.name << "\n";
  if (!top.parameters.empty()) {
    os << "Parameters:\n";
    for (const auto& param : top.parameters) {
      if (param.value) {
        os << "  - " << param.name << " = "
           << ExprToString(*param.value, top) << "\n";
      } else {
        os << "  - " << param.name << "\n";
      }
    }
  }
  os << "Ports:\n";
  for (const auto& port : top.ports) {
    os << "  - " << DirLabel(port.dir);
    if (port.is_signed) {
      os << " signed";
    }
    os << " " << port.name << " [" << port.width << "]\n";
  }
  os << "Nets:\n";
  for (const auto& net : top.nets) {
    const char* type = "wire";
    switch (net.type) {
      case gpga::NetType::kWire:
        type = "wire";
        break;
      case gpga::NetType::kReg:
        type = "reg";
        break;
      case gpga::NetType::kWand:
        type = "wand";
        break;
      case gpga::NetType::kWor:
        type = "wor";
        break;
      case gpga::NetType::kTri0:
        type = "tri0";
        break;
      case gpga::NetType::kTri1:
        type = "tri1";
        break;
      case gpga::NetType::kTriand:
        type = "triand";
        break;
      case gpga::NetType::kTrior:
        type = "trior";
        break;
      case gpga::NetType::kTrireg:
        type = "trireg";
        break;
      case gpga::NetType::kSupply0:
        type = "supply0";
        break;
      case gpga::NetType::kSupply1:
        type = "supply1";
        break;
    }
    os << "  - " << type;
    if (net.is_signed) {
      os << " signed";
    }
    os << " " << net.name << " [" << net.width << "]";
    if (net.array_size > 0) {
      os << " [" << net.array_size << "]";
    }
    os << "\n";
  }
  if (!top.events.empty()) {
    os << "Events:\n";
    for (const auto& evt : top.events) {
      os << "  - event " << evt.name << "\n";
    }
  }
  os << "Assigns:\n";
  for (const auto& assign : top.assigns) {
    if (assign.rhs) {
      int lhs_width = SignalWidth(top, assign.lhs);
      std::string lhs = assign.lhs;
      if (assign.lhs_has_range) {
        int lo = std::min(assign.lhs_msb, assign.lhs_lsb);
        int hi = std::max(assign.lhs_msb, assign.lhs_lsb);
        lhs_width = hi - lo + 1;
        if (assign.lhs_msb == assign.lhs_lsb) {
          lhs += "[" + std::to_string(assign.lhs_msb) + "]";
        } else {
          lhs += "[" + std::to_string(assign.lhs_msb) + ":" +
                 std::to_string(assign.lhs_lsb) + "]";
        }
      }
      const gpga::Expr* rhs_expr = assign.rhs.get();
      if (const gpga::Expr* simplified =
              SimplifyAssignMask(*assign.rhs, top, lhs_width)) {
        rhs_expr = simplified;
      }
      int rhs_width = ExprWidth(*rhs_expr, top);
      std::string rhs = ExprToString(*rhs_expr, top);
      if (rhs_width < lhs_width) {
        rhs = (ExprSigned(*rhs_expr, top) ? "sext(" : "zext(") + rhs + ", " +
              std::to_string(lhs_width) + ")";
      } else if (rhs_width > lhs_width) {
        rhs = "trunc(" + rhs + ", " + std::to_string(lhs_width) + ")";
      }
      os << "  - " << lhs << " = " << rhs << "\n";
    }
  }
  os << "Switches:\n";
  for (const auto& sw : top.switches) {
    const char* kind = "tran";
    switch (sw.kind) {
      case gpga::SwitchKind::kTran:
        kind = "tran";
        break;
      case gpga::SwitchKind::kTranif1:
        kind = "tranif1";
        break;
      case gpga::SwitchKind::kTranif0:
        kind = "tranif0";
        break;
      case gpga::SwitchKind::kCmos:
        kind = "cmos";
        break;
    }
    os << "  - " << kind << " " << sw.a << " <-> " << sw.b;
    if (sw.control) {
      os << " (" << ExprToString(*sw.control, top) << ")";
    }
    if (sw.control_n) {
      os << " / (" << ExprToString(*sw.control_n, top) << ")";
    }
    os << "\n";
  }
  os << "Always blocks:\n";
  for (const auto& block : top.always_blocks) {
    if (block.edge == gpga::EdgeKind::kCombinational) {
      if (!block.sensitivity.empty() && block.sensitivity != "*") {
        os << "  - always @(" << block.sensitivity << ")\n";
      } else {
        os << "  - always @*\n";
      }
    } else if (block.edge == gpga::EdgeKind::kInitial) {
      os << "  - initial\n";
    } else {
      if (!block.sensitivity.empty()) {
        os << "  - always @(" << block.sensitivity << ")\n";
      } else {
        os << "  - always @("
           << (block.edge == gpga::EdgeKind::kPosedge ? "posedge " : "negedge ")
           << block.clock << ")\n";
      }
    }
    for (const auto& stmt : block.statements) {
      DumpStatement(stmt, top, 4, os);
    }
  }
  os << "Flat name map:\n";
  for (const auto& entry : design.flat_to_hier) {
    os << "  - " << entry.first << " -> " << entry.second << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 2;
  }

  std::vector<std::string> input_paths;
  std::string msl_out;
  std::string host_out;
  std::string top_name;
  bool dump_flat = false;
  bool enable_4state = false;
  bool auto_discover = false;
  bool run = false;
  uint32_t run_count = 1u;
  uint32_t run_service_capacity = 32u;
  uint32_t run_max_steps = 1024u;
  uint32_t run_max_proc_steps = 64u;
  std::string vcd_dir;
  uint32_t vcd_steps = 0u;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--emit-msl") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      msl_out = argv[++i];
    } else if (arg == "--dump-flat") {
      dump_flat = true;
    } else if (arg == "--top") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      top_name = argv[++i];
    } else if (arg == "--emit-host") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      host_out = argv[++i];
    } else if (arg == "--4state") {
      enable_4state = true;
    } else if (arg == "--auto") {
      auto_discover = true;
    } else if (arg == "--run") {
      run = true;
    } else if (arg == "--count") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_count = static_cast<uint32_t>(std::stoul(argv[++i]));
      if (run_count == 0u) {
        run_count = 1u;
      }
    } else if (arg == "--service-capacity") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_service_capacity = static_cast<uint32_t>(std::stoul(argv[++i]));
      if (run_service_capacity == 0u) {
        run_service_capacity = 1u;
      }
    } else if (arg == "--max-steps") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_max_steps = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--max-proc-steps") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      run_max_proc_steps = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--vcd-dir" || arg == "--vcr-dir") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      vcd_dir = argv[++i];
    } else if (arg == "--vcd-steps" || arg == "--vcr-steps") {
      if (i + 1 >= argc) {
        PrintUsage(argv[0]);
        return 2;
      }
      vcd_steps = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (!arg.empty() && arg[0] == '-') {
      PrintUsage(argv[0]);
      return 2;
    } else {
      input_paths.push_back(arg);
    }
  }

  if (input_paths.empty()) {
    PrintUsage(argv[0]);
    return 2;
  }

  gpga::Diagnostics diagnostics;
  gpga::Program program;
  program.modules.clear();
  gpga::ParseOptions parse_options;
  parse_options.enable_4state = enable_4state;

  struct ParseItem {
    std::string path;
    bool explicit_input = false;
  };

  std::vector<ParseItem> parse_queue;
  parse_queue.reserve(input_paths.size());
  std::unordered_map<std::string, size_t> seen_paths;
  auto add_path = [&](const std::string& path, bool explicit_input) {
    std::error_code ec;
    std::filesystem::path fs_path(path);
    std::filesystem::path normalized =
        std::filesystem::weakly_canonical(fs_path, ec);
    std::string key = ec ? fs_path.lexically_normal().string()
                         : normalized.string();
    auto it = seen_paths.find(key);
    if (it == seen_paths.end()) {
      seen_paths[key] = parse_queue.size();
      parse_queue.push_back(ParseItem{path, explicit_input});
      return;
    }
    if (explicit_input) {
      parse_queue[it->second].explicit_input = true;
    }
  };
  for (const auto& path : input_paths) {
    add_path(path, true);
  }
  if (auto_discover) {
    for (const auto& path : input_paths) {
      std::error_code ec;
      std::filesystem::path root = std::filesystem::path(path).parent_path();
      if (root.empty()) {
        root = ".";
      }
      std::vector<std::string> discovered;
      for (auto it =
               std::filesystem::recursive_directory_iterator(root, ec);
           it != std::filesystem::recursive_directory_iterator();
           it.increment(ec)) {
        if (ec) {
          break;
        }
        if (!it->is_regular_file()) {
          continue;
        }
        if (it->path().extension() == ".v") {
          discovered.push_back(it->path().string());
        }
      }
      std::sort(discovered.begin(), discovered.end());
      for (const auto& candidate : discovered) {
        add_path(candidate, false);
      }
    }
  }

  for (const auto& item : parse_queue) {
    if (item.explicit_input) {
      if (!gpga::ParseVerilogFile(item.path, &program, &diagnostics,
                                  parse_options)) {
        diagnostics.RenderTo(std::cerr);
        return 1;
      }
      if (diagnostics.HasErrors()) {
        diagnostics.RenderTo(std::cerr);
        return 1;
      }
      continue;
    }
    gpga::Program temp_program;
    gpga::Diagnostics temp_diag;
    if (!gpga::ParseVerilogFile(item.path, &temp_program, &temp_diag,
                                parse_options)) {
      continue;
    }
    if (temp_diag.HasErrors()) {
      continue;
    }
    for (auto& module : temp_program.modules) {
      program.modules.push_back(std::move(module));
    }
  }

  gpga::ElaboratedDesign design;
  bool elaborated = false;
  if (!top_name.empty()) {
    elaborated =
        gpga::Elaborate(program, top_name, &design, &diagnostics, enable_4state);
  } else {
    elaborated = gpga::Elaborate(program, &design, &diagnostics, enable_4state);
  }
  if (!elaborated || diagnostics.HasErrors()) {
    diagnostics.RenderTo(std::cerr);
    return 1;
  }
  if (!diagnostics.Items().empty()) {
    diagnostics.RenderTo(std::cerr);
  }

  std::string msl;
  if (!msl_out.empty() || run) {
    msl = gpga::EmitMSLStub(design.top, enable_4state);
    if (!msl_out.empty()) {
      if (!WriteFile(msl_out, msl, &diagnostics)) {
        diagnostics.RenderTo(std::cerr);
        return 1;
      }
    }
  }

  if (!host_out.empty()) {
    std::string host = gpga::EmitHostStub(design.top);
    if (!WriteFile(host_out, host, &diagnostics)) {
      diagnostics.RenderTo(std::cerr);
      return 1;
    }
  }

  if (run) {
    std::string error;
    if (!RunMetal(design.top, msl, design.flat_to_hier, enable_4state,
                  run_count,
                  run_service_capacity, run_max_steps, run_max_proc_steps,
                  vcd_dir, vcd_steps, &error)) {
      std::cerr << "Run failed: " << error << "\n";
      return 1;
    }
  }

  if (msl_out.empty() && host_out.empty() && !run) {
    std::cout << "Elaborated top module '" << design.top.name
              << "'. Use --emit-msl/--emit-host to write stubs.\n";
  }
  if (dump_flat) {
    DumpFlat(design, std::cout);
  }

  return 0;
}
