/**
 * @file metal_runtime.hh
 * @brief Metal runtime interfaces for kernel compilation and dispatch.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @namespace gpga
 * @brief Runtime, codegen, and frontend support types for metalfpga.
 */
namespace gpga {

/// Argument payload kind for runtime services.
enum class ServiceArgKind : uint32_t {
  kValue = 0u,  ///< Value payload.
  kIdent = 1u,  ///< Identifier payload.
  kString = 2u, ///< String payload.
  kReal = 3u,   ///< Real payload.
  kWide = 4u,   ///< Wide payload.
};

/// Runtime service request kind.
enum class ServiceKind : uint32_t {
  kDisplay = 0u,        ///< $display service.
  kMonitor = 1u,        ///< $monitor service.
  kFinish = 2u,         ///< $finish service.
  kDumpfile = 3u,       ///< $dumpfile service.
  kDumpvars = 4u,       ///< $dumpvars service.
  kReadmemh = 5u,       ///< $readmemh service.
  kReadmemb = 6u,       ///< $readmemb service.
  kStop = 7u,           ///< $stop service.
  kStrobe = 8u,         ///< $strobe service.
  kDumpoff = 9u,        ///< $dumpoff service.
  kDumpon = 10u,        ///< $dumpon service.
  kDumpflush = 11u,     ///< $dumpflush service.
  kDumpall = 12u,       ///< $dumpall service.
  kDumplimit = 13u,     ///< $dumplimit service.
  kFwrite = 14u,        ///< $fwrite service.
  kFdisplay = 15u,      ///< $fdisplay service.
  kFopen = 16u,         ///< $fopen service.
  kFclose = 17u,        ///< $fclose service.
  kFgetc = 18u,         ///< $fgetc service.
  kFgets = 19u,         ///< $fgets service.
  kFeof = 20u,          ///< $feof service.
  kFscanf = 21u,        ///< $fscanf service.
  kSscanf = 22u,        ///< $sscanf service.
  kFtell = 23u,         ///< $ftell service.
  kRewind = 24u,        ///< $rewind service.
  kWritememh = 25u,     ///< $writememh service.
  kWritememb = 26u,     ///< $writememb service.
  kFseek = 27u,         ///< $fseek service.
  kFflush = 28u,        ///< $fflush service.
  kFerror = 29u,        ///< $ferror service.
  kFungetc = 30u,       ///< $fungetc service.
  kFread = 31u,         ///< $fread service.
  kWrite = 32u,         ///< $write service.
  kSformat = 33u,       ///< $sformat service.
  kTimeformat = 34u,    ///< $timeformat service.
  kPrinttimescale = 35u, ///< $printtimescale service.
  kTestPlusargs = 36u,  ///< $test$plusargs service.
  kValuePlusargs = 37u, ///< $value$plusargs service.
  kAsyncAndArray = 38u, ///< Asynchronous AND array primitive.
  kSyncOrPlane = 39u,   ///< Synchronous OR plane primitive.
  kAsyncNorPlane = 40u, ///< Asynchronous NOR plane primitive.
  kSyncNandPlane = 41u, ///< Synchronous NAND plane primitive.
  kShowcancelled = 42u, ///< $showcancelled service.
};

/// String table for runtime service formatting.
struct ServiceStringTable {
  std::vector<std::string> entries; ///< Format string entries.
};

/// Decoded service argument payload.
struct ServiceArgView {
  ServiceArgKind kind = ServiceArgKind::kValue; ///< Argument kind.
  uint32_t width = 0; ///< Argument bit width.
  uint64_t value = 0; ///< Scalar value payload.
  uint64_t xz = 0; ///< X/Z mask for scalar payload.
  std::vector<uint64_t> wide_value; ///< Wide value payload.
  std::vector<uint64_t> wide_xz; ///< Wide X/Z mask.
};

/// Decoded service record from the scheduler buffer.
struct ServiceRecordView {
  ServiceKind kind = ServiceKind::kDisplay; ///< Service kind.
  uint32_t pid = 0; ///< Process id.
  uint32_t format_id = 0xFFFFFFFFu; ///< Format string id.
  std::vector<ServiceArgView> args; ///< Argument list.
};

/// Summary of service drain results.
struct ServiceDrainResult {
  bool saw_finish = false; ///< True if $finish was seen.
  bool saw_stop = false; ///< True if $stop was seen.
  bool saw_error = false; ///< True if an error was reported.
};

/**
 * @brief Compute the byte stride for a service record buffer.
 *
 * @param max_args Maximum number of arguments per record.
 * @param wide_words Wide words per argument.
 * @param has_xz True if X/Z payloads are present.
 * @return Record stride in bytes.
 */
size_t ServiceRecordStride(uint32_t max_args, uint32_t wide_words, bool has_xz);

/**
 * @brief Decode and print scheduler service records.
 *
 * @param records Pointer to the raw record buffer.
 * @param record_count Number of records in the buffer.
 * @param max_args Maximum number of arguments per record.
 * @param wide_words Wide words per argument.
 * @param has_xz True if X/Z payloads are present.
 * @param strings Format string table.
 * @param out Output stream for formatted services.
 * @return Summary of observed service outcomes.
 */
ServiceDrainResult DrainSchedulerServices(
    const void* records, uint32_t record_count, uint32_t max_args,
    uint32_t wide_words, bool has_xz, const ServiceStringTable& strings,
    std::ostream& out);

/// Simulation parameters common to runtime entry points.
struct GpgaParams {
  uint32_t count = 0; ///< Instance count.
};

/// Scheduler parameter block.
struct GpgaSchedParams {
  uint32_t count = 0; ///< Instance count.
  uint32_t max_steps = 0; ///< Max total scheduler steps per tick.
  uint32_t max_proc_steps = 0; ///< Max steps per process per tick.
  uint32_t service_capacity = 0; ///< Service record capacity.
};

/// Signal metadata extracted from compiled kernels.
struct SignalInfo {
  std::string name; ///< Signal name.
  uint32_t width = 1; ///< Signal width in bits.
  uint32_t array_size = 0; ///< Array size (if any).
  bool is_real = false; ///< True if signal is real-typed.
  bool is_trireg = false; ///< True if signal is trireg.
};

/// Module metadata for runtime introspection.
struct ModuleInfo {
  std::string name; ///< Module name.
  bool four_state = false; ///< True if module uses 4-state signals.
  std::vector<SignalInfo> signals; ///< Signal list.
};

/// Scheduler constants extracted from compiled sources.
struct SchedulerConstants {
  bool has_scheduler = false; ///< True if scheduler symbols are present.
  uint32_t proc_count = 0; ///< Process count.
  uint32_t event_count = 0; ///< Event count.
  uint32_t edge_count = 0; ///< Edge trigger count.
  uint32_t edge_star_count = 0; ///< Edge-* trigger count.
  uint32_t repeat_count = 0; ///< Repeat entry count.
  uint32_t delay_count = 0; ///< Delay entry count.
  uint32_t max_dnba = 0; ///< Max delayed NBA count.
  uint32_t monitor_count = 0; ///< Monitor count.
  uint32_t monitor_max_args = 0; ///< Max monitor args.
  uint32_t strobe_count = 0; ///< Strobe count.
  uint32_t service_max_args = 0; ///< Max service args.
  uint32_t service_wide_words = 0; ///< Wide words per service arg.
  uint32_t string_count = 0; ///< String literal count.
  uint32_t force_count = 0; ///< Force slot count.
  uint32_t pcont_count = 0; ///< Passign/continuous count.
  uint32_t timing_check_count = 0; ///< Timing check count.
  bool has_services = false; ///< True if service support is present.
  bool vm_enabled = false; ///< True if VM bytecode is present.
  uint32_t vm_bytecode_words = 0; ///< VM bytecode word count.
  uint32_t vm_cond_count = 0; ///< VM condition entry count.
  uint32_t vm_assign_count = 0; ///< VM assign entry count.
  uint32_t vm_force_count = 0; ///< VM force entry count.
  uint32_t vm_release_count = 0; ///< VM release entry count.
  uint32_t vm_service_call_count = 0; ///< VM service call count.
  uint32_t vm_service_assign_count = 0; ///< VM service assign count.
  uint32_t vm_service_arg_count = 0; ///< VM service arg count.
  uint32_t vm_call_frame_words = 0; ///< VM call frame words.
  uint32_t vm_call_frame_depth = 0; ///< VM call frame depth.
  uint32_t vm_case_header_count = 0; ///< VM case header count.
  uint32_t vm_case_entry_count = 0; ///< VM case entry count.
  uint32_t vm_case_word_count = 0; ///< VM case word count.
  uint32_t vm_expr_word_count = 0; ///< VM expression word count.
  uint32_t vm_expr_imm_word_count = 0; ///< VM immediate expression word count.
  uint32_t vm_signal_count = 0; ///< VM signal count.
};

/// Buffer specification for runtime allocation.
struct BufferSpec {
  std::string name; ///< Buffer name.
  size_t length = 0; ///< Buffer size in bytes.
};

/// RAII wrapper for a Metal buffer.
class MetalBuffer {
 public:
  /// Construct an empty buffer handle.
  MetalBuffer() = default;
  /// Release the underlying Metal buffer.
  ~MetalBuffer();
  /// Move-construct a buffer handle.
  MetalBuffer(MetalBuffer&& other) noexcept;
  /// Move-assign a buffer handle.
  MetalBuffer& operator=(MetalBuffer&& other) noexcept;
  MetalBuffer(const MetalBuffer&) = delete;
  MetalBuffer& operator=(const MetalBuffer&) = delete;

  /// @return CPU-visible buffer contents (if mapped).
  void* contents() const { return contents_; }
  /// @return Buffer length in bytes.
  size_t length() const { return length_; }

 private:
  friend class MetalRuntime;
  void* handle_ = nullptr;
  void* contents_ = nullptr;
  size_t length_ = 0;
};

/// Buffer binding for kernel dispatch.
struct MetalBufferBinding {
  uint32_t index = 0; ///< Binding index.
  const MetalBuffer* buffer = nullptr; ///< Buffer handle.
  size_t offset = 0; ///< Byte offset into the buffer.
};

class MetalKernel;

/// Kernel dispatch descriptor.
struct MetalDispatch {
  const MetalKernel* kernel = nullptr; ///< Kernel to dispatch.
  const std::vector<MetalBufferBinding>* bindings = nullptr; ///< Buffer bindings.
  uint32_t grid_size = 0; ///< Thread grid size.
  const MetalBuffer* indirect_buffer = nullptr; ///< Indirect dispatch buffer.
  size_t indirect_offset = 0; ///< Byte offset for indirect args.
};

/// Compiled Metal compute kernel metadata.
class MetalKernel {
 public:
  /// Construct an empty kernel handle.
  MetalKernel() = default;
  /// Release kernel resources.
  ~MetalKernel();
  /// Move-construct a kernel handle.
  MetalKernel(MetalKernel&& other) noexcept;
  /// Move-assign a kernel handle.
  MetalKernel& operator=(MetalKernel&& other) noexcept;
  MetalKernel(const MetalKernel&) = delete;
  MetalKernel& operator=(const MetalKernel&) = delete;

  /// @param name Buffer name. @return Binding index for the buffer.
  uint32_t BufferIndex(const std::string& name) const;
  /// @param name Buffer name. @return True if the buffer exists.
  bool HasBuffer(const std::string& name) const;
  /// @return Kernel name.
  const std::string& Name() const { return name_; }
  /// @return Map of buffer names to binding indices.
  const std::unordered_map<std::string, uint32_t>& BufferIndices() const {
    return buffer_indices_;
  }
  /// @return Thread execution width reported by Metal.
  uint32_t ThreadExecutionWidth() const { return thread_execution_width_; }
  /// @return Max threads per threadgroup.
  uint32_t MaxThreadsPerThreadgroup() const {
    return max_threads_per_threadgroup_;
  }
  /// @return Required threads per threadgroup (if any).
  uint32_t RequiredThreadsPerThreadgroup() const {
    return required_threads_per_threadgroup_;
  }
  /// @return Maximum number of buffer bindings.
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

/// Metal runtime for compiling and dispatching kernels.
class MetalRuntime {
 public:
  /// Construct a runtime instance.
  MetalRuntime();
  /// Destroy the runtime instance.
  ~MetalRuntime();
  MetalRuntime(const MetalRuntime&) = delete;
  MetalRuntime& operator=(const MetalRuntime&) = delete;

  /**
   * @brief Initialize the Metal runtime.
   *
   * @param error Optional error message output.
   * @return True on success.
   */
  bool Initialize(std::string* error);
  /**
   * @brief Prefer source-based buffer bindings when available.
   *
   * @param value True to prefer source bindings.
   */
  void SetPreferSourceBindings(bool value);
  /**
   * @brief Compile Metal source code into a library.
   *
   * @param source Metal source code.
   * @param include_paths Include search paths.
   * @param error Optional error message output.
   * @return True on success.
   */
  bool CompileSource(const std::string& source,
                     const std::vector<std::string>& include_paths,
                     std::string* error);
  /**
   * @brief Get the last compiled source.
   *
   * @param out Output string for the source.
   * @return True if a source is available.
   */
  bool GetLastSource(std::string* out) const;
  /**
   * @brief Create a kernel handle by name.
   *
   * @param name Kernel function name.
   * @param kernel Output kernel handle.
   * @param error Optional error message output.
   * @return True on success.
   */
  bool CreateKernel(const std::string& name, MetalKernel* kernel,
                    std::string* error);
  /**
   * @brief Precompile a list of kernel functions.
   *
   * @param names Kernel function names.
   * @param error Optional error message output.
   * @return True on success.
   */
  bool PrecompileKernels(const std::vector<std::string>& names,
                         std::string* error);
  /**
   * @brief Create a Metal buffer.
   *
   * @param length Buffer size in bytes.
   * @param initial_data Optional initial data pointer.
   * @return Created buffer handle.
   */
  MetalBuffer CreateBuffer(size_t length, const void* initial_data);
  /**
   * @brief Encode an argument buffer for indirect bindings.
   *
   * @param kernel Kernel whose argument buffer is encoded.
   * @param buffer_index Argument buffer binding index.
   * @param bindings Buffer bindings to encode.
   * @param out Output buffer handle.
   * @param error Optional error message output.
   * @return True on success.
   */
  bool EncodeArgumentBuffer(const MetalKernel& kernel, uint32_t buffer_index,
                            const std::vector<MetalBufferBinding>& bindings,
                            MetalBuffer* out, std::string* error);
  /**
   * @brief Dispatch a kernel with direct bindings.
   *
   * @param kernel Kernel to dispatch.
   * @param bindings Buffer bindings.
   * @param grid_size Thread grid size.
   * @param error Optional error message output.
   * @param timeout_ms Optional dispatch timeout in ms (0 for none).
   * @return True on success.
   */
  bool Dispatch(const MetalKernel& kernel,
                const std::vector<MetalBufferBinding>& bindings,
                uint32_t grid_size, std::string* error,
                uint32_t timeout_ms = 0u);
  /**
   * @brief Dispatch a kernel using indirect threadgroup arguments.
   *
   * @param kernel Kernel to dispatch.
   * @param bindings Buffer bindings.
   * @param indirect_buffer Buffer containing dispatch args.
   * @param indirect_offset Byte offset into the indirect buffer.
   * @param error Optional error message output.
   * @param timeout_ms Optional dispatch timeout in ms (0 for none).
   * @return True on success.
   */
  bool DispatchIndirectThreads(const MetalKernel& kernel,
                               const std::vector<MetalBufferBinding>& bindings,
                               const MetalBuffer& indirect_buffer,
                               size_t indirect_offset, std::string* error,
                               uint32_t timeout_ms = 0u);
  /**
   * @brief Dispatch a batch of kernels.
   *
   * @param dispatches Dispatch list.
   * @param grid_size Thread grid size.
   * @param error Optional error message output.
   * @param timeout_ms Optional dispatch timeout in ms (0 for none).
   * @return True on success.
   */
  bool DispatchBatch(const std::vector<MetalDispatch>& dispatches,
                     uint32_t grid_size, std::string* error,
                     uint32_t timeout_ms = 0u);
  /**
   * @brief Compute a default threadgroup size for a kernel.
   *
   * @param kernel Kernel to query.
   * @return Recommended threadgroup size.
   */
  uint32_t ComputeThreadgroupSize(const MetalKernel& kernel) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Parse scheduler constants from generated source.
 *
 * @param source Source text to scan.
 * @param out Output constants.
 * @param error Optional error message output.
 * @return True on success.
 */
bool ParseSchedulerConstants(const std::string& source,
                             SchedulerConstants* out,
                             std::string* error);

/**
 * @brief Build buffer specs for a compiled module and scheduler.
 *
 * @param module Module metadata.
 * @param kernel Compiled kernel metadata.
 * @param sched Scheduler constants.
 * @param instance_count Instance count.
 * @param service_capacity Service buffer capacity.
 * @param specs Output buffer specs.
 * @param error Optional error message output.
 * @return True on success.
 */
bool BuildBufferSpecs(const ModuleInfo& module, const MetalKernel& kernel,
                      const SchedulerConstants& sched,
                      uint32_t instance_count, uint32_t service_capacity,
                      std::vector<BufferSpec>* specs, std::string* error);

}  // namespace gpga
