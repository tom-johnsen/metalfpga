#include "runtime/metal_runtime.hh"

#include <algorithm>
#include <cstring>
#include <dispatch/dispatch.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>
#include <unordered_map>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <Metal/MTL4ArgumentTable.h>
#import <Metal/MTL4CommandAllocator.h>
#import <Metal/MTL4CommandBuffer.h>
#import <Metal/MTL4CommandQueue.h>
#import <Metal/MTL4Compiler.h>
#import <Metal/MTL4ComputeCommandEncoder.h>
#import <Metal/MTL4ComputePipeline.h>
#import <Metal/MTL4LibraryDescriptor.h>
#import <Metal/MTL4LibraryFunctionDescriptor.h>

namespace gpga {

struct MetalRuntime::Impl {
  id<MTLDevice> device = nil;
  id<MTL4CommandQueue> queue = nil;
  id<MTL4CommandAllocator> allocator = nil;
  id<MTL4Compiler> compiler = nil;
  id<MTLLibrary> library = nil;
  ~Impl() {
    if (library) {
      [library release];
      library = nil;
    }
    if (compiler) {
      [compiler release];
      compiler = nil;
    }
    if (allocator) {
      [allocator release];
      allocator = nil;
    }
    if (queue) {
      [queue release];
      queue = nil;
    }
    if (device) {
      [device release];
      device = nil;
    }
  }
};

namespace {

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

std::string ResolveString(const ServiceStringTable& strings, uint32_t id) {
  if (id >= strings.entries.size()) {
    return "<invalid_string_id>";
  }
  return strings.entries[id];
}

uint64_t MaskForWidth(uint32_t width) {
  if (width >= 64u) {
    return 0xFFFFFFFFFFFFFFFFull;
  }
  if (width == 0u) {
    return 0ull;
  }
  return (1ull << width) - 1ull;
}

int64_t SignExtend(uint64_t value, uint32_t width) {
  if (width == 0u || width >= 64u) {
    return static_cast<int64_t>(value);
  }
  uint64_t mask = MaskForWidth(width);
  uint64_t sign_bit = 1ull << (width - 1u);
  uint64_t masked = value & mask;
  if ((masked & sign_bit) != 0ull) {
    return static_cast<int64_t>(masked | ~mask);
  }
  return static_cast<int64_t>(masked);
}

std::string FormatBits(uint64_t value, uint64_t xz, uint32_t width, int base,
                       bool has_xz) {
  if (width == 0u) {
    width = 1u;
  }
  if (width > 64u) {
    width = 64u;
  }
  uint64_t mask = MaskForWidth(width);
  value &= mask;
  xz &= mask;

  int group = 1;
  if (base == 16) {
    group = 4;
  } else if (base == 8) {
    group = 3;
  }
  int digits = static_cast<int>((width + group - 1u) / group);
  std::string out;
  out.reserve(static_cast<size_t>(digits));
  for (int i = digits - 1; i >= 0; --i) {
    int shift = i * group;
    uint64_t group_mask = ((1ull << group) - 1ull) << shift;
    if (has_xz && (xz & group_mask) != 0ull) {
      out.push_back('x');
      continue;
    }
    uint64_t digit = (value >> shift) & ((1ull << group) - 1ull);
    if (base == 16) {
      out.push_back("0123456789abcdef"[digit & 0xF]);
    } else if (base == 8) {
      out.push_back("01234567"[digit & 0x7]);
    } else {
      out.push_back((digit & 1ull) ? '1' : '0');
    }
  }
  return out;
}

std::string ApplyPadding(std::string text, int width, bool zero_pad) {
  if (width <= 0 || static_cast<int>(text.size()) >= width) {
    return text;
  }
  char pad_char = zero_pad ? '0' : ' ';
  int pad_len = width - static_cast<int>(text.size());
  if (zero_pad && !text.empty() && text[0] == '-') {
    return "-" + std::string(pad_len, pad_char) + text.substr(1);
  }
  return std::string(pad_len, pad_char) + text;
}

std::string FormatNumeric(const ServiceArgView& arg, char spec, bool has_xz) {
  if (has_xz && arg.xz != 0u &&
      (spec == 'd' || spec == 'u' || spec == 't')) {
    return "x";
  }
  uint32_t width = arg.width;
  if (spec == 'b') {
    return FormatBits(arg.value, arg.xz, width, 2, has_xz);
  }
  if (spec == 'o') {
    return FormatBits(arg.value, arg.xz, width, 8, has_xz);
  }
  if (spec == 'h' || spec == 'x') {
    return FormatBits(arg.value, arg.xz, width, 16, has_xz);
  }
  if (spec == 't') {
    return std::to_string(arg.value);
  }
  if (spec == 'u') {
    uint64_t mask = MaskForWidth(width);
    return std::to_string(arg.value & mask);
  }
  int64_t signed_value = SignExtend(arg.value, width);
  return std::to_string(signed_value);
}

std::string FormatArg(const ServiceArgView& arg, char spec,
                      const ServiceStringTable& strings, bool has_xz) {
  if (arg.kind == ServiceArgKind::kString ||
      arg.kind == ServiceArgKind::kIdent) {
    return ResolveString(strings, static_cast<uint32_t>(arg.value));
  }
  if (spec == 's') {
    return FormatNumeric(arg, 'd', has_xz);
  }
  return FormatNumeric(arg, spec, has_xz);
}

std::string FormatWithSpec(const std::string& fmt,
                           const std::vector<ServiceArgView>& args,
                           size_t start_index,
                           const ServiceStringTable& strings, bool has_xz) {
  std::ostringstream oss;
  size_t arg_index = start_index;
  for (size_t i = 0; i < fmt.size(); ++i) {
    char c = fmt[i];
    if (c != '%') {
      oss << c;
      continue;
    }
    if (i + 1 < fmt.size() && fmt[i + 1] == '%') {
      oss << '%';
      ++i;
      continue;
    }
    bool zero_pad = false;
    int width = 0;
    size_t j = i + 1;
    if (j < fmt.size() && fmt[j] == '0') {
      zero_pad = true;
      ++j;
    }
    while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') {
      width = (width * 10) + (fmt[j] - '0');
      ++j;
    }
    if (j >= fmt.size()) {
      break;
    }
    char spec = fmt[j];
    if (spec >= 'A' && spec <= 'Z') {
      spec = static_cast<char>(spec - 'A' + 'a');
    }
    i = j;
    if (arg_index >= args.size()) {
      oss << ApplyPadding("<missing>", width, false);
      continue;
    }
    std::string text = FormatArg(args[arg_index], spec, strings, has_xz);
    ++arg_index;
    oss << ApplyPadding(std::move(text), width, zero_pad);
  }
  return oss.str();
}

std::string FormatDefaultArgs(const std::vector<ServiceArgView>& args,
                              const ServiceStringTable& strings, bool has_xz) {
  std::ostringstream oss;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      oss << " ";
    }
    const auto& arg = args[i];
    if (arg.kind == ServiceArgKind::kString ||
        arg.kind == ServiceArgKind::kIdent) {
      oss << ResolveString(strings, static_cast<uint32_t>(arg.value));
    } else {
      oss << FormatNumeric(arg, 'd', has_xz);
    }
  }
  return oss.str();
}

std::string ReadFileContents(const std::string& path, std::string* error) {
  std::ifstream file(path);
  if (!file) {
    if (error) {
      *error = "failed to open include file: " + path;
    }
    return {};
  }
  std::ostringstream oss;
  oss << file.rdbuf();
  return oss.str();
}

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

std::string FormatNSError(NSError* error) {
  if (!error) {
    return "unknown Metal error";
  }
  NSString* desc = [error localizedDescription];
  if (!desc) {
    return "unknown Metal error";
  }
  return std::string([desc UTF8String]);
}

std::string ExpandIncludes(const std::string& source,
                           const std::vector<std::string>& include_paths,
                           std::string* error) {
  std::ostringstream out;
  std::istringstream in(source);
  std::string line;
  while (std::getline(in, line)) {
    std::string trimmed = line;
    trimmed.erase(trimmed.begin(),
                  std::find_if(trimmed.begin(), trimmed.end(),
                               [](unsigned char c) { return c != ' '; }));
    if (StartsWith(trimmed, "#include")) {
      size_t first_quote = trimmed.find('"');
      size_t last_quote = trimmed.rfind('"');
      if (first_quote != std::string::npos &&
          last_quote != std::string::npos && last_quote > first_quote) {
        std::string name =
            trimmed.substr(first_quote + 1, last_quote - first_quote - 1);
        std::string contents;
        for (const auto& dir : include_paths) {
          std::string path = dir;
          if (!path.empty() && path.back() != '/') {
            path += '/';
          }
          path += name;
          contents = ReadFileContents(path, error);
          if (!contents.empty()) {
            break;
          }
        }
        if (!contents.empty()) {
          out << contents << "\n";
          continue;
        }
      }
    }
    out << line << "\n";
  }
  return out.str();
}

bool ParseUintConst(const std::string& source, const std::string& name,
                    uint32_t* value_out) {
  std::regex pattern("(?:constant\\s+)?constexpr\\s+uint\\s+" + name +
                     "\\s*=\\s*([0-9]+)u;");
  std::smatch match;
  if (!std::regex_search(source, match, pattern)) {
    return false;
  }
  if (match.size() < 2) {
    return false;
  }
  try {
    *value_out = static_cast<uint32_t>(std::stoul(match[1].str()));
  } catch (...) {
    return false;
  }
  return true;
}

}  // namespace

MetalBuffer::~MetalBuffer() {
  if (handle_) {
    id<MTLBuffer> buffer = (id<MTLBuffer>)handle_;
    [buffer release];
    handle_ = nullptr;
    contents_ = nullptr;
    length_ = 0;
  }
}

MetalBuffer::MetalBuffer(MetalBuffer&& other) noexcept {
  handle_ = other.handle_;
  contents_ = other.contents_;
  length_ = other.length_;
  other.handle_ = nullptr;
  other.contents_ = nullptr;
  other.length_ = 0;
}

MetalBuffer& MetalBuffer::operator=(MetalBuffer&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (handle_) {
    id<MTLBuffer> buffer = (id<MTLBuffer>)handle_;
    [buffer release];
  }
  handle_ = other.handle_;
  contents_ = other.contents_;
  length_ = other.length_;
  other.handle_ = nullptr;
  other.contents_ = nullptr;
  other.length_ = 0;
  return *this;
}

MetalKernel::~MetalKernel() {
  if (pipeline_) {
    id<MTLComputePipelineState> pipeline =
        (id<MTLComputePipelineState>)pipeline_;
    [pipeline release];
    pipeline_ = nullptr;
  }
  if (argument_table_) {
    id<MTL4ArgumentTable> table = (id<MTL4ArgumentTable>)argument_table_;
    [table release];
    argument_table_ = nullptr;
  }
  buffer_indices_.clear();
  thread_execution_width_ = 0;
  max_threads_per_threadgroup_ = 0;
}

MetalKernel::MetalKernel(MetalKernel&& other) noexcept
    : pipeline_(other.pipeline_),
      argument_table_(other.argument_table_),
      buffer_indices_(std::move(other.buffer_indices_)),
      thread_execution_width_(other.thread_execution_width_),
      max_threads_per_threadgroup_(other.max_threads_per_threadgroup_) {
  other.pipeline_ = nullptr;
  other.argument_table_ = nullptr;
  other.thread_execution_width_ = 0;
  other.max_threads_per_threadgroup_ = 0;
}

MetalKernel& MetalKernel::operator=(MetalKernel&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (pipeline_) {
    id<MTLComputePipelineState> pipeline =
        (id<MTLComputePipelineState>)pipeline_;
    [pipeline release];
  }
  if (argument_table_) {
    id<MTL4ArgumentTable> table = (id<MTL4ArgumentTable>)argument_table_;
    [table release];
  }
  pipeline_ = other.pipeline_;
  argument_table_ = other.argument_table_;
  buffer_indices_ = std::move(other.buffer_indices_);
  thread_execution_width_ = other.thread_execution_width_;
  max_threads_per_threadgroup_ = other.max_threads_per_threadgroup_;
  other.pipeline_ = nullptr;
  other.argument_table_ = nullptr;
  other.thread_execution_width_ = 0;
  other.max_threads_per_threadgroup_ = 0;
  return *this;
}

uint32_t MetalKernel::BufferIndex(const std::string& name) const {
  auto it = buffer_indices_.find(name);
  if (it == buffer_indices_.end()) {
    return std::numeric_limits<uint32_t>::max();
  }
  return it->second;
}

bool MetalKernel::HasBuffer(const std::string& name) const {
  return buffer_indices_.find(name) != buffer_indices_.end();
}

MetalRuntime::MetalRuntime() : impl_(std::make_unique<Impl>()) {}

MetalRuntime::~MetalRuntime() = default;

bool MetalRuntime::Initialize(std::string* error) {
  if (!impl_) {
    impl_ = std::make_unique<Impl>();
  }
  if (!impl_->device) {
    impl_->device = MTLCreateSystemDefaultDevice();
  }
  if (!impl_->device) {
    if (error) {
      *error = "Metal device unavailable";
    }
    return false;
  }
  if (!impl_->queue) {
    impl_->queue = [impl_->device newMTL4CommandQueue];
  }
  if (!impl_->queue) {
    if (error) {
      *error = "Metal command queue unavailable";
    }
    return false;
  }
  if (!impl_->allocator) {
    impl_->allocator = [impl_->device newCommandAllocator];
  }
  if (!impl_->allocator) {
    if (error) {
      *error = "Metal command allocator unavailable";
    }
    return false;
  }
  if (!impl_->compiler) {
    MTL4CompilerDescriptor* desc = [[MTL4CompilerDescriptor alloc] init];
    NSError* err = nil;
    impl_->compiler = [impl_->device newCompilerWithDescriptor:desc
                                                         error:&err];
    [desc release];
    if (!impl_->compiler) {
      if (error) {
        *error = FormatNSError(err);
      }
      return false;
    }
  }
  return true;
}

bool MetalRuntime::CompileSource(const std::string& source,
                                 const std::vector<std::string>& include_paths,
                                 std::string* error) {
  if (!Initialize(error)) {
    return false;
  }
  if (!impl_->compiler) {
    if (error) {
      *error = "Metal 4 compiler unavailable";
    }
    return false;
  }
  std::string expanded = ExpandIncludes(source, include_paths, error);
  if (expanded.empty()) {
    expanded = source;
  }
  NSString* ns_source =
      [[NSString alloc] initWithBytes:expanded.data()
                               length:expanded.size()
                             encoding:NSUTF8StringEncoding];
  MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
  MTL4LibraryDescriptor* lib_desc = [[MTL4LibraryDescriptor alloc] init];
  lib_desc.source = ns_source;
  lib_desc.options = options;
  NSError* err = nil;
  id<MTLLibrary> library =
      [impl_->compiler newLibraryWithDescriptor:lib_desc error:&err];
  [lib_desc release];
  [options release];
  [ns_source release];
  if (!library) {
    if (error) {
      *error = FormatNSError(err);
    }
    return false;
  }
  if (impl_->library) {
    [impl_->library release];
  }
  impl_->library = library;
  return true;
}

bool MetalRuntime::CreateKernel(const std::string& name, MetalKernel* kernel,
                                std::string* error) {
  if (!kernel) {
    if (error) {
      *error = "kernel output pointer is null";
    }
    return false;
  }
  if (!impl_ || !impl_->library) {
    if (error) {
      *error = "Metal library not initialized";
    }
    return false;
  }
  if (!impl_->compiler) {
    if (error) {
      *error = "Metal 4 compiler unavailable";
    }
    return false;
  }
  NSString* fn_name = [NSString stringWithUTF8String:name.c_str()];
  MTL4LibraryFunctionDescriptor* fn_desc =
      [[MTL4LibraryFunctionDescriptor alloc] init];
  fn_desc.name = fn_name;
  fn_desc.library = impl_->library;

  MTL4ComputePipelineDescriptor* desc =
      [[MTL4ComputePipelineDescriptor alloc] init];
  desc.computeFunctionDescriptor = fn_desc;
  MTL4PipelineOptions* pipe_opts = [[MTL4PipelineOptions alloc] init];
  pipe_opts.shaderReflection = MTL4ShaderReflectionBindingInfo;
  desc.options = pipe_opts;

  NSError* err = nil;
  id<MTLComputePipelineState> pipeline =
      [impl_->compiler newComputePipelineStateWithDescriptor:desc
                                           compilerTaskOptions:nil
                                                        error:&err];
  [pipe_opts release];
  [desc release];
  [fn_desc release];
  if (!pipeline) {
    if (error) {
      *error = FormatNSError(err);
    }
    return false;
  }
  MTLComputePipelineReflection* reflection = pipeline.reflection;
  if (!reflection) {
    if (error) {
      *error = "Metal pipeline reflection unavailable";
    }
    [pipeline release];
    return false;
  }
  MetalKernel temp;
  temp.pipeline_ = pipeline;
  temp.thread_execution_width_ =
      static_cast<uint32_t>(pipeline.threadExecutionWidth);
  temp.max_threads_per_threadgroup_ =
      static_cast<uint32_t>(pipeline.maxTotalThreadsPerThreadgroup);
  uint32_t max_index = 0;
  bool has_index = false;
  for (id<MTLBinding> binding in reflection.bindings) {
    if (binding.type != MTLBindingTypeBuffer) {
      continue;
    }
    uint32_t index = static_cast<uint32_t>(binding.index);
    temp.buffer_indices_[std::string([binding.name UTF8String])] = index;
    if (!has_index || index > max_index) {
      max_index = index;
      has_index = true;
    }
  }
  if (has_index) {
    if (max_index >= 31u) {
      if (error) {
        *error = "Metal 4 argument tables support up to 31 buffer bindings";
      }
      [pipeline release];
      temp.pipeline_ = nullptr;
      return false;
    }
    MTL4ArgumentTableDescriptor* table_desc =
        [[MTL4ArgumentTableDescriptor alloc] init];
    table_desc.maxBufferBindCount = static_cast<NSUInteger>(max_index + 1u);
    table_desc.initializeBindings = YES;
    NSError* table_err = nil;
    id<MTL4ArgumentTable> table =
        [impl_->device newArgumentTableWithDescriptor:table_desc
                                                error:&table_err];
    [table_desc release];
    if (!table) {
      if (error) {
        *error = FormatNSError(table_err);
      }
      [pipeline release];
      temp.pipeline_ = nullptr;
      return false;
    }
    temp.argument_table_ = table;
  }
  *kernel = std::move(temp);
  return true;
}

MetalBuffer MetalRuntime::CreateBuffer(size_t length,
                                       const void* initial_data) {
  MetalBuffer buffer;
  if (!impl_ || !impl_->device || length == 0) {
    return buffer;
  }
  id<MTLBuffer> mtl_buffer =
      [impl_->device newBufferWithLength:length
                                  options:MTLResourceStorageModeShared];
  if (!mtl_buffer) {
    return buffer;
  }
  buffer.handle_ = (void*)mtl_buffer;
  buffer.contents_ = [mtl_buffer contents];
  buffer.length_ = length;
  if (initial_data && buffer.contents_) {
    std::memcpy(buffer.contents_, initial_data, length);
  }
  return buffer;
}

bool MetalRuntime::Dispatch(const MetalKernel& kernel,
                            const std::vector<MetalBufferBinding>& bindings,
                            uint32_t grid_size, std::string* error) {
  if (!impl_ || !impl_->queue || !impl_->allocator || !kernel.pipeline_) {
    if (error) {
      *error = "Metal runtime not initialized";
    }
    return false;
  }
  id<MTL4CommandBuffer> cmd = [impl_->device newCommandBuffer];
  if (!cmd) {
    if (error) {
      *error = "Failed to create Metal 4 command buffer";
    }
    return false;
  }
  [cmd beginCommandBufferWithAllocator:impl_->allocator];
  id<MTL4ComputeCommandEncoder> encoder = [cmd computeCommandEncoder];
  if (!encoder) {
    if (error) {
      *error = "Failed to create Metal 4 compute encoder";
    }
    [cmd endCommandBuffer];
    [cmd release];
    return false;
  }
  id<MTLComputePipelineState> pipeline =
      (id<MTLComputePipelineState>)kernel.pipeline_;
  [encoder setComputePipelineState:pipeline];
  id<MTL4ArgumentTable> table = (id<MTL4ArgumentTable>)kernel.argument_table_;
  if (!bindings.empty() && !table) {
    if (error) {
      *error = "Metal 4 argument table unavailable for bindings";
    }
    [encoder endEncoding];
    [cmd endCommandBuffer];
    [cmd release];
    return false;
  }
  if (table) {
    for (const auto& binding : bindings) {
      if (!binding.buffer || !binding.buffer->handle_) {
        continue;
      }
      id<MTLBuffer> buffer = (id<MTLBuffer>)binding.buffer->handle_;
      MTLGPUAddress address =
          buffer.gpuAddress + static_cast<MTLGPUAddress>(binding.offset);
      [table setAddress:address atIndex:binding.index];
    }
    [encoder setArgumentTable:table];
  }
  uint32_t threadgroup = kernel.ThreadExecutionWidth();
  if (threadgroup == 0 || threadgroup > kernel.MaxThreadsPerThreadgroup()) {
    threadgroup = kernel.MaxThreadsPerThreadgroup();
  }
  if (threadgroup == 0) {
    threadgroup = 1;
  }
  MTLSize threads_per_group = MTLSizeMake(threadgroup, 1, 1);
  MTLSize grid = MTLSizeMake(grid_size, 1, 1);
  [encoder dispatchThreads:grid threadsPerThreadgroup:threads_per_group];
  [encoder endEncoding];
  [cmd endCommandBuffer];
  __block NSError* commit_error = nil;
  dispatch_semaphore_t done = dispatch_semaphore_create(0);
  MTL4CommitOptions* options = [[MTL4CommitOptions alloc] init];
  [options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback) {
    if (feedback.error) {
      commit_error = [feedback.error retain];
    }
    dispatch_semaphore_signal(done);
  }];
  const id<MTL4CommandBuffer> buffers[] = {cmd};
  [impl_->queue commit:buffers count:1 options:options];
  dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
  [options release];
#if !OS_OBJECT_USE_OBJC
  dispatch_release(done);
#endif
  if (commit_error) {
    if (error) {
      *error = FormatNSError(commit_error);
    }
    [commit_error release];
    [cmd release];
    [impl_->allocator reset];
    return false;
  }
  [cmd release];
  [impl_->allocator reset];
  return true;
}

bool ParseSchedulerConstants(const std::string& source,
                             SchedulerConstants* out,
                             std::string* error) {
  if (!out) {
    if (error) {
      *error = "scheduler output pointer is null";
    }
    return false;
  }
  SchedulerConstants info;
  ParseUintConst(source, "GPGA_SCHED_PROC_COUNT", &info.proc_count);
  ParseUintConst(source, "GPGA_SCHED_EVENT_COUNT", &info.event_count);
  ParseUintConst(source, "GPGA_SCHED_EDGE_COUNT", &info.edge_count);
  ParseUintConst(source, "GPGA_SCHED_EDGE_STAR_COUNT", &info.edge_star_count);
  ParseUintConst(source, "GPGA_SCHED_REPEAT_COUNT", &info.repeat_count);
  if (ParseUintConst(source, "GPGA_SCHED_DELAY_COUNT", &info.delay_count)) {
    info.has_scheduler = true;
  }
  if (ParseUintConst(source, "GPGA_SCHED_MAX_DNBA", &info.max_dnba)) {
    info.has_scheduler = true;
  }
  ParseUintConst(source, "GPGA_SCHED_MONITOR_COUNT", &info.monitor_count);
  ParseUintConst(source, "GPGA_SCHED_MONITOR_MAX_ARGS", &info.monitor_max_args);
  ParseUintConst(source, "GPGA_SCHED_STROBE_COUNT", &info.strobe_count);
  ParseUintConst(source, "GPGA_SCHED_SERVICE_MAX_ARGS", &info.service_max_args);
  ParseUintConst(source, "GPGA_SCHED_STRING_COUNT", &info.string_count);
  info.has_scheduler = info.proc_count > 0;
  info.has_services = info.service_max_args > 0;
  *out = info;
  return true;
}

bool BuildBufferSpecs(const ModuleInfo& module, const MetalKernel& kernel,
                      const SchedulerConstants& sched,
                      uint32_t instance_count, uint32_t service_capacity,
                      std::vector<BufferSpec>* specs, std::string* error) {
  if (!specs) {
    if (error) {
      *error = "buffer spec output is null";
    }
    return false;
  }
  std::unordered_map<std::string, SignalInfo> signals;
  for (const auto& signal : module.signals) {
    signals[signal.name] = signal;
  }
  auto signal_bytes = [&](const SignalInfo& signal) -> size_t {
    uint32_t width = signal.width;
    if (signal.is_real && width < 64u) {
      width = 64u;
    }
    return (width > 32u) ? sizeof(uint64_t) : sizeof(uint32_t);
  };
  auto signal_elements = [&](const SignalInfo& signal) -> size_t {
    uint32_t array_size = signal.array_size > 0 ? signal.array_size : 1u;
    return static_cast<size_t>(instance_count) *
           static_cast<size_t>(array_size);
  };
  specs->clear();
  const auto& indices = kernel.BufferIndices();
  specs->reserve(indices.size());
  for (const auto& entry : indices) {
    BufferSpec spec;
    spec.name = entry.first;
    const std::string& name = spec.name;
    if (name == "params") {
      spec.length = sizeof(GpgaParams);
      specs->push_back(spec);
      continue;
    }
    if (name == "sched") {
      spec.length = sizeof(GpgaSchedParams);
      specs->push_back(spec);
      continue;
    }
    if (StartsWith(name, "sched_")) {
      if (!sched.has_scheduler) {
        if (error) {
          *error = "scheduler buffers requested but no scheduler constants";
        }
        return false;
      }
      if (name == "sched_pc" || name == "sched_state" ||
          name == "sched_wait_kind" || name == "sched_wait_edge_kind" ||
          name == "sched_wait_id" || name == "sched_wait_event" ||
          name == "sched_join_count" || name == "sched_parent" ||
          name == "sched_join_tag") {
        spec.length = sizeof(uint32_t) * instance_count * sched.proc_count;
      } else if (name == "sched_wait_time") {
        spec.length = sizeof(uint64_t) * instance_count * sched.proc_count;
      } else if (name == "sched_time") {
        spec.length = sizeof(uint64_t) * instance_count;
      } else if (name == "sched_phase" || name == "sched_flags" ||
                 name == "sched_error" || name == "sched_status") {
        spec.length = sizeof(uint32_t) * instance_count;
      } else if (name == "sched_repeat_left" ||
                 name == "sched_repeat_active") {
        spec.length = sizeof(uint32_t) * instance_count * sched.repeat_count;
      } else if (name == "sched_edge_prev_val" ||
                 name == "sched_edge_prev_xz") {
        spec.length = sizeof(uint64_t) * instance_count * sched.edge_count;
      } else if (name == "sched_edge_star_prev_val" ||
                 name == "sched_edge_star_prev_xz") {
        spec.length = sizeof(uint64_t) * instance_count *
                      sched.edge_star_count;
      } else if (name == "sched_event_pending") {
        spec.length = sizeof(uint32_t) * instance_count * sched.event_count;
      } else if (name == "sched_delay_val" || name == "sched_delay_xz") {
        spec.length = sizeof(uint64_t) * instance_count * sched.delay_count;
      } else if (name == "sched_delay_index_val" ||
                 name == "sched_delay_index_xz") {
        spec.length = sizeof(uint32_t) * instance_count * sched.delay_count;
      } else if (name == "sched_dnba_count") {
        spec.length = sizeof(uint32_t) * instance_count;
      } else if (name == "sched_dnba_time" || name == "sched_dnba_val" ||
                 name == "sched_dnba_xz") {
        spec.length = sizeof(uint64_t) * instance_count * sched.max_dnba;
      } else if (name == "sched_dnba_id" ||
                 name == "sched_dnba_index_val" ||
                 name == "sched_dnba_index_xz") {
        spec.length = sizeof(uint32_t) * instance_count * sched.max_dnba;
      } else if (name == "sched_monitor_active") {
        spec.length = sizeof(uint32_t) * instance_count * sched.monitor_count;
      } else if (name == "sched_monitor_enable") {
        spec.length = sizeof(uint32_t) * instance_count;
      } else if (name == "sched_monitor_val" ||
                 name == "sched_monitor_xz") {
        spec.length = sizeof(uint64_t) * instance_count * sched.monitor_count *
                      sched.monitor_max_args;
      } else if (name == "sched_strobe_pending") {
        spec.length = sizeof(uint32_t) * instance_count * sched.strobe_count;
      } else if (name == "sched_service_count") {
        spec.length = sizeof(uint32_t) * instance_count;
      } else if (name == "sched_service") {
        size_t stride =
            ServiceRecordStride(std::max<uint32_t>(1, sched.service_max_args),
                                module.four_state);
        spec.length = stride * instance_count * service_capacity;
      } else {
        if (error) {
          *error = "unknown scheduler buffer: " + name;
        }
        return false;
      }
      specs->push_back(spec);
      continue;
    }

    std::string base = name;
    if (StartsWith(base, "nb_")) {
      base = base.substr(3);
    }
    if (EndsWith(base, "_val")) {
      base = base.substr(0, base.size() - 4);
    } else if (EndsWith(base, "_xz")) {
      base = base.substr(0, base.size() - 3);
    }
    if (EndsWith(base, "_next")) {
      base = base.substr(0, base.size() - 5);
    }
    auto it = signals.find(base);
    if (it == signals.end()) {
      if (error) {
        *error = "unknown signal buffer: " + name;
      }
      return false;
    }
    const SignalInfo& signal = it->second;
    spec.length = signal_bytes(signal) * signal_elements(signal);
    specs->push_back(spec);
  }
  return true;
}

size_t ServiceRecordStride(uint32_t max_args, bool has_xz) {
  size_t header = sizeof(uint32_t) * 4u;
  size_t arg_kind = sizeof(uint32_t) * max_args;
  size_t arg_width = sizeof(uint32_t) * max_args;
  size_t arg_val = sizeof(uint64_t) * max_args;
  size_t arg_xz = has_xz ? sizeof(uint64_t) * max_args : 0u;
  return header + arg_kind + arg_width + arg_val + arg_xz;
}

ServiceDrainResult DrainSchedulerServices(
    const void* records, uint32_t record_count, uint32_t max_args,
    bool has_xz, const ServiceStringTable& strings, std::ostream& out) {
  ServiceDrainResult result;
  if (!records || record_count == 0 || max_args == 0) {
    return result;
  }
  const auto* base = static_cast<const uint8_t*>(records);
  const size_t stride = ServiceRecordStride(max_args, has_xz);
  for (uint32_t i = 0; i < record_count; ++i) {
    const uint8_t* rec = base + (stride * i);
    uint32_t kind_raw = ReadU32(rec, 0);
    uint32_t pid = ReadU32(rec, sizeof(uint32_t));
    uint32_t format_id = ReadU32(rec, sizeof(uint32_t) * 2u);
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

    auto read_arg = [&](uint32_t index) -> ServiceArgView {
      ServiceArgView arg;
      arg.kind =
          static_cast<ServiceArgKind>(ReadU32(rec, arg_kind_offset +
                                                        sizeof(uint32_t) * index));
      arg.width = ReadU32(rec, arg_width_offset + sizeof(uint32_t) * index);
      arg.value = ReadU64(rec, arg_val_offset + sizeof(uint64_t) * index);
      if (has_xz) {
        arg.xz = ReadU64(rec, arg_xz_offset + sizeof(uint64_t) * index);
      }
      return arg;
    };

    std::vector<ServiceArgView> args;
    args.reserve(arg_count);
    for (uint32_t a = 0; a < arg_count; ++a) {
      args.push_back(read_arg(a));
    }

    ServiceKind kind = static_cast<ServiceKind>(kind_raw);
    switch (kind) {
      case ServiceKind::kFinish:
        result.saw_finish = true;
        out << "$finish (pid=" << pid << ")\n";
        break;
      case ServiceKind::kStop:
        result.saw_stop = true;
        out << "$stop (pid=" << pid << ")\n";
        break;
      case ServiceKind::kDisplay:
      case ServiceKind::kMonitor:
      case ServiceKind::kStrobe: {
        std::string fmt = (format_id != 0xFFFFFFFFu)
                              ? ResolveString(strings, format_id)
                              : "";
        size_t start_index = 0;
        if (!fmt.empty() && !args.empty() &&
            args.front().kind == ServiceArgKind::kString &&
            args.front().value == static_cast<uint64_t>(format_id)) {
          start_index = 1;
        }
        std::string line =
            fmt.empty() ? FormatDefaultArgs(args, strings, has_xz)
                        : FormatWithSpec(fmt, args, start_index, strings,
                                         has_xz);
        out << line << "\n";
        break;
      }
      case ServiceKind::kDumpfile: {
        std::string filename = ResolveString(strings, format_id);
        out << "$dumpfile \"" << filename << "\" (pid=" << pid << ")\n";
        break;
      }
      case ServiceKind::kDumpvars: {
        out << "$dumpvars (pid=" << pid << ")";
        for (uint32_t a = 0; a < arg_count; ++a) {
          const ServiceArgView& arg = args[a];
          out << " ";
          if (arg.kind == ServiceArgKind::kString ||
              arg.kind == ServiceArgKind::kIdent) {
            out << ResolveString(strings,
                                 static_cast<uint32_t>(arg.value));
          } else {
            out << FormatNumeric(arg, 'h', has_xz);
          }
        }
        out << "\n";
        break;
      }
      case ServiceKind::kDumpoff:
        out << "$dumpoff (pid=" << pid << ")\n";
        break;
      case ServiceKind::kDumpon:
        out << "$dumpon (pid=" << pid << ")\n";
        break;
      case ServiceKind::kDumpflush:
        out << "$dumpflush (pid=" << pid << ")\n";
        break;
      case ServiceKind::kDumpall:
        out << "$dumpall (pid=" << pid << ")\n";
        break;
      case ServiceKind::kDumplimit: {
        out << "$dumplimit (pid=" << pid << ")";
        if (!args.empty()) {
          out << " " << FormatNumeric(args.front(), 'h', has_xz);
        }
        out << "\n";
        break;
      }
      case ServiceKind::kReadmemh:
      case ServiceKind::kReadmemb: {
        std::string label =
            (kind == ServiceKind::kReadmemh) ? "$readmemh" : "$readmemb";
        std::string filename = ResolveString(strings, format_id);
        out << label << " \"" << filename << "\" (pid=" << pid << ")";
        for (uint32_t a = 0; a < arg_count; ++a) {
          const ServiceArgView& arg = args[a];
          out << " ";
          if (arg.kind == ServiceArgKind::kString ||
              arg.kind == ServiceArgKind::kIdent) {
            out << ResolveString(strings,
                                 static_cast<uint32_t>(arg.value));
          } else {
            out << FormatNumeric(arg, 'h', has_xz);
          }
        }
        out << "\n";
        break;
      }
      default:
        result.saw_error = true;
        out << "unknown service kind " << kind_raw << " (pid=" << pid << ")\n";
        break;
    }
  }
  return result;
}

}  // namespace gpga
