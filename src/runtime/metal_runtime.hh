#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace gpga {

enum class ServiceArgKind : uint32_t {
  kValue = 0u,
  kIdent = 1u,
  kString = 2u,
  kReal = 3u,
  kWide = 4u,
};

enum class ServiceKind : uint32_t {
  kDisplay = 0u,
  kMonitor = 1u,
  kFinish = 2u,
  kDumpfile = 3u,
  kDumpvars = 4u,
  kReadmemh = 5u,
  kReadmemb = 6u,
  kStop = 7u,
  kStrobe = 8u,
  kDumpoff = 9u,
  kDumpon = 10u,
  kDumpflush = 11u,
  kDumpall = 12u,
  kDumplimit = 13u,
  kFwrite = 14u,
  kFdisplay = 15u,
  kFopen = 16u,
  kFclose = 17u,
  kFgetc = 18u,
  kFgets = 19u,
  kFeof = 20u,
  kFscanf = 21u,
  kSscanf = 22u,
  kFtell = 23u,
  kRewind = 24u,
  kWritememh = 25u,
  kWritememb = 26u,
  kFseek = 27u,
  kFflush = 28u,
  kFerror = 29u,
  kFungetc = 30u,
  kFread = 31u,
  kWrite = 32u,
  kSformat = 33u,
  kTimeformat = 34u,
  kPrinttimescale = 35u,
  kTestPlusargs = 36u,
  kValuePlusargs = 37u,
  kAsyncAndArray = 38u,
  kSyncOrPlane = 39u,
  kAsyncNorPlane = 40u,
  kSyncNandPlane = 41u,
  kShowcancelled = 42u,
};

struct ServiceStringTable {
  std::vector<std::string> entries;
};

struct ServiceArgView {
  ServiceArgKind kind = ServiceArgKind::kValue;
  uint32_t width = 0;
  uint64_t value = 0;
  uint64_t xz = 0;
  std::vector<uint64_t> wide_value;
  std::vector<uint64_t> wide_xz;
};

struct ServiceRecordView {
  ServiceKind kind = ServiceKind::kDisplay;
  uint32_t pid = 0;
  uint32_t format_id = 0xFFFFFFFFu;
  std::vector<ServiceArgView> args;
};

struct ServiceDrainResult {
  bool saw_finish = false;
  bool saw_stop = false;
  bool saw_error = false;
};

size_t ServiceRecordStride(uint32_t max_args, uint32_t wide_words, bool has_xz);

ServiceDrainResult DrainSchedulerServices(
    const void* records, uint32_t record_count, uint32_t max_args,
    uint32_t wide_words, bool has_xz, const ServiceStringTable& strings,
    std::ostream& out);

struct GpgaParams {
  uint32_t count = 0;
};

struct GpgaSchedParams {
  uint32_t count = 0;
  uint32_t max_steps = 0;
  uint32_t max_proc_steps = 0;
  uint32_t service_capacity = 0;
};

struct SignalInfo {
  std::string name;
  uint32_t width = 1;
  uint32_t array_size = 0;
  bool is_real = false;
  bool is_trireg = false;
};

struct ModuleInfo {
  std::string name;
  bool four_state = false;
  std::vector<SignalInfo> signals;
};

struct SchedulerConstants {
  bool has_scheduler = false;
  uint32_t proc_count = 0;
  uint32_t event_count = 0;
  uint32_t edge_count = 0;
  uint32_t edge_star_count = 0;
  uint32_t repeat_count = 0;
  uint32_t delay_count = 0;
  uint32_t max_dnba = 0;
  uint32_t monitor_count = 0;
  uint32_t monitor_max_args = 0;
  uint32_t strobe_count = 0;
  uint32_t service_max_args = 0;
  uint32_t service_wide_words = 0;
  uint32_t string_count = 0;
  uint32_t force_count = 0;
  uint32_t pcont_count = 0;
  uint32_t timing_check_count = 0;
  bool has_services = false;
  bool vm_enabled = false;
  uint32_t vm_bytecode_words = 0;
  uint32_t vm_cond_count = 0;
  uint32_t vm_assign_count = 0;
  uint32_t vm_force_count = 0;
  uint32_t vm_release_count = 0;
  uint32_t vm_service_call_count = 0;
  uint32_t vm_service_assign_count = 0;
  uint32_t vm_service_arg_count = 0;
  uint32_t vm_call_frame_words = 0;
  uint32_t vm_call_frame_depth = 0;
  uint32_t vm_case_header_count = 0;
  uint32_t vm_case_entry_count = 0;
  uint32_t vm_case_word_count = 0;
  uint32_t vm_expr_word_count = 0;
  uint32_t vm_expr_imm_word_count = 0;
  uint32_t vm_signal_count = 0;
};

struct BufferSpec {
  std::string name;
  size_t length = 0;
};

class MetalBuffer {
 public:
  MetalBuffer() = default;
  ~MetalBuffer();
  MetalBuffer(MetalBuffer&& other) noexcept;
  MetalBuffer& operator=(MetalBuffer&& other) noexcept;
  MetalBuffer(const MetalBuffer&) = delete;
  MetalBuffer& operator=(const MetalBuffer&) = delete;

  void* contents() const { return contents_; }
  size_t length() const { return length_; }

 private:
  friend class MetalRuntime;
  void* handle_ = nullptr;
  void* contents_ = nullptr;
  size_t length_ = 0;
};

struct MetalBufferBinding {
  uint32_t index = 0;
  const MetalBuffer* buffer = nullptr;
  size_t offset = 0;
};

class MetalKernel;

struct MetalDispatch {
  const MetalKernel* kernel = nullptr;
  const std::vector<MetalBufferBinding>* bindings = nullptr;
  uint32_t grid_size = 0;
  const MetalBuffer* indirect_buffer = nullptr;
  size_t indirect_offset = 0;
};

class MetalKernel {
 public:
  MetalKernel() = default;
  ~MetalKernel();
  MetalKernel(MetalKernel&& other) noexcept;
  MetalKernel& operator=(MetalKernel&& other) noexcept;
  MetalKernel(const MetalKernel&) = delete;
  MetalKernel& operator=(const MetalKernel&) = delete;

  uint32_t BufferIndex(const std::string& name) const;
  bool HasBuffer(const std::string& name) const;
  const std::string& Name() const { return name_; }
  const std::unordered_map<std::string, uint32_t>& BufferIndices() const {
    return buffer_indices_;
  }
  uint32_t ThreadExecutionWidth() const { return thread_execution_width_; }
  uint32_t MaxThreadsPerThreadgroup() const {
    return max_threads_per_threadgroup_;
  }
  uint32_t RequiredThreadsPerThreadgroup() const {
    return required_threads_per_threadgroup_;
  }
  uint32_t MaxBufferBindings() const { return max_buffer_bindings_; }

 private:
  friend class MetalRuntime;
  void* pipeline_ = nullptr;
  void* argument_table_ = nullptr;
  std::string name_;
  std::unordered_map<std::string, uint32_t> buffer_indices_;
  uint32_t max_buffer_bindings_ = 0;
  uint32_t thread_execution_width_ = 0;
  uint32_t max_threads_per_threadgroup_ = 0;
  uint32_t required_threads_per_threadgroup_ = 0;
  mutable std::vector<uint64_t> last_binding_addresses_;
};

class MetalRuntime {
 public:
  MetalRuntime();
  ~MetalRuntime();
  MetalRuntime(const MetalRuntime&) = delete;
  MetalRuntime& operator=(const MetalRuntime&) = delete;

  bool Initialize(std::string* error);
  void SetPreferSourceBindings(bool value);
  bool CompileSource(const std::string& source,
                     const std::vector<std::string>& include_paths,
                     std::string* error);
  bool GetLastSource(std::string* out) const;
  bool CreateKernel(const std::string& name, MetalKernel* kernel,
                    std::string* error);
  bool PrecompileKernels(const std::vector<std::string>& names,
                         std::string* error);
  MetalBuffer CreateBuffer(size_t length, const void* initial_data);
  bool EncodeArgumentBuffer(const MetalKernel& kernel, uint32_t buffer_index,
                            const std::vector<MetalBufferBinding>& bindings,
                            MetalBuffer* out, std::string* error);
  bool Dispatch(const MetalKernel& kernel,
                const std::vector<MetalBufferBinding>& bindings,
                uint32_t grid_size, std::string* error,
                uint32_t timeout_ms = 0u);
  bool DispatchIndirectThreads(const MetalKernel& kernel,
                               const std::vector<MetalBufferBinding>& bindings,
                               const MetalBuffer& indirect_buffer,
                               size_t indirect_offset, std::string* error,
                               uint32_t timeout_ms = 0u);
  bool DispatchBatch(const std::vector<MetalDispatch>& dispatches,
                     uint32_t grid_size, std::string* error,
                     uint32_t timeout_ms = 0u);
  uint32_t ComputeThreadgroupSize(const MetalKernel& kernel) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

bool ParseSchedulerConstants(const std::string& source,
                             SchedulerConstants* out,
                             std::string* error);

bool BuildBufferSpecs(const ModuleInfo& module, const MetalKernel& kernel,
                      const SchedulerConstants& sched,
                      uint32_t instance_count, uint32_t service_capacity,
                      std::vector<BufferSpec>* specs, std::string* error);

}  // namespace gpga
