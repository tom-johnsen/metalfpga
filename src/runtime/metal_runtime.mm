#include "runtime/metal_runtime.hh"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dispatch/dispatch.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_map>

#include "utils/msl_naming.hh"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <Metal/MTL4ArgumentTable.h>
#import <Metal/MTL4CommandAllocator.h>
#import <Metal/MTL4CommandBuffer.h>
#import <Metal/MTL4CommandQueue.h>
#import <Metal/MTL4Compiler.h>
#import <Metal/MTL4ComputeCommandEncoder.h>
#import <Metal/MTL4ComputePipeline.h>
#import <Metal/MTL4Counters.h>
#import <Metal/MTL4LibraryDescriptor.h>
#import <Metal/MTL4LibraryFunctionDescriptor.h>
#import <Metal/MTL4PipelineDataSetSerializer.h>
#import <Metal/MTL4Archive.h>
#import <Metal/MTLResidencySet.h>

namespace gpga {

std::string FormatNSError(NSError* error);
void LogPipeline(bool enabled, const std::string& message);
std::string FormatTimestamp();

struct MetalRuntime::Impl {
  struct PipelineTask {
    id<MTL4CompilerTask> task = nil;
    id<MTLComputePipelineState> pipeline = nil;
    NSError* error = nil;
    dispatch_semaphore_t done = nullptr;
    std::chrono::steady_clock::time_point start =
        std::chrono::steady_clock::time_point{};
  };
  id<MTLDevice> device = nil;
  id<MTL4CommandQueue> queue = nil;
  id<MTL4CommandAllocator> allocator = nil;
  id<MTL4Compiler> compiler = nil;
  id<MTL4PipelineDataSetSerializer> pipeline_serializer = nil;
  id<MTL4Archive> pipeline_archive = nil;
  id<MTLResidencySet> residency_set = nil;
  id<MTL4CounterHeap> timestamp_heap = nil;
  id<MTLLibrary> library = nil;
  id<MTLDynamicLibrary> real_lib = nil;
  std::unordered_map<std::string, id<MTLComputePipelineState>> pipeline_cache;
  std::unordered_map<std::string, PipelineTask> pipeline_tasks;
  std::mutex pipeline_mutex;
  std::filesystem::path pipeline_archive_path;
  bool pipeline_archive_load = false;
  bool pipeline_archive_save = false;
  bool pipeline_async = false;
  bool pipeline_log = false;
  bool pipeline_progress = true;
  bool pipeline_trace = true;
  size_t precompile_total = 0;
  size_t precompile_done = 0;
  uint32_t threadgroup_override = 0;
  bool use_residency_set = false;
  bool gpu_timestamps = false;
  bool gpu_timestamps_precise = false;
  uint32_t gpu_timestamp_every = 1;
  uint64_t gpu_timestamp_seq = 0;
  uint64_t timestamp_frequency = 0;
  NSUInteger timestamp_heap_count = 0;
  std::string last_source;
  bool prefer_source_bindings = false;
  bool ShouldSampleGpuTimestamps() {
    if (!gpu_timestamps) {
      return false;
    }
    if (gpu_timestamp_every == 0u) {
      return false;
    }
    uint64_t seq = gpu_timestamp_seq++;
    return (seq % gpu_timestamp_every) == 0u;
  }
  bool EnsureTimestampHeap(NSUInteger count, std::string* error) {
    if (!device) {
      if (error) {
        *error = "Metal device unavailable for timestamp heap";
      }
      return false;
    }
    if (timestamp_heap && timestamp_heap_count >= count) {
      return true;
    }
    if (timestamp_heap) {
      [timestamp_heap release];
      timestamp_heap = nil;
      timestamp_heap_count = 0;
    }
    MTL4CounterHeapDescriptor* desc = [[MTL4CounterHeapDescriptor alloc] init];
    desc.type = MTL4CounterHeapTypeTimestamp;
    desc.count = count;
    NSError* err = nil;
    id<MTL4CounterHeap> heap =
        [device newCounterHeapWithDescriptor:desc error:&err];
    [desc release];
    if (!heap) {
      if (error) {
        *error = err ? FormatNSError(err)
                     : "Failed to create Metal timestamp heap";
      }
      return false;
    }
    timestamp_heap = heap;
    timestamp_heap_count = count;
    if (timestamp_frequency == 0) {
      timestamp_frequency = [device queryTimestampFrequency];
    }
    return true;
  }
  MTL4TimestampGranularity TimestampGranularity() const {
    return gpu_timestamps_precise ? MTL4TimestampGranularityPrecise
                                  : MTL4TimestampGranularityRelaxed;
  }
  void LogGpuTimestampResult(const char* label, uint32_t grid_size,
                             size_t dispatch_count) {
    if (!timestamp_heap) {
      return;
    }
    @autoreleasepool {
      NSData* data =
          [timestamp_heap resolveCounterRange:NSMakeRange(0, 2)];
      if (!data) {
        std::cerr << "[gpu_profile] " << label
                  << " resolveCounterRange failed\n";
        return;
      }
      const size_t length = [data length];
      size_t entry_size =
          static_cast<size_t>([device sizeOfCounterHeapEntry:
                                       MTL4CounterHeapTypeTimestamp]);
      if (entry_size == 0) {
        entry_size = sizeof(uint64_t);
      }
      if (length < entry_size * 2) {
        std::cerr << "[gpu_profile] " << label
                  << " timestamp data too small\n";
        return;
      }
      const uint8_t* bytes =
          static_cast<const uint8_t*>([data bytes]);
      uint64_t start = 0;
      uint64_t end = 0;
      std::memcpy(&start, bytes, sizeof(uint64_t));
      std::memcpy(&end, bytes + entry_size, sizeof(uint64_t));
      if (timestamp_frequency > 0 && end >= start) {
        double ms =
            (static_cast<double>(end - start) * 1000.0) /
            static_cast<double>(timestamp_frequency);
        std::cerr << "[gpu_profile] " << label << " ms=" << ms
                  << " grid=" << grid_size
                  << " dispatches=" << dispatch_count << "\n";
      } else {
        std::cerr << "[gpu_profile] " << label << " ticks=" << (end - start)
                  << " grid=" << grid_size
                  << " dispatches=" << dispatch_count << "\n";
      }
    }
  }
  ~Impl() {
    std::vector<dispatch_semaphore_t> pending;
    {
      std::lock_guard<std::mutex> lock(pipeline_mutex);
      pending.reserve(pipeline_tasks.size());
      for (auto& entry : pipeline_tasks) {
        if (entry.second.done) {
          pending.push_back(entry.second.done);
        }
      }
    }
    for (dispatch_semaphore_t sema : pending) {
      dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    }
    {
      std::lock_guard<std::mutex> lock(pipeline_mutex);
    for (auto& entry : pipeline_tasks) {
      PipelineTask& task = entry.second;
        if (task.task) {
          [task.task release];
          task.task = nil;
        }
        if (task.pipeline) {
          [task.pipeline release];
          task.pipeline = nil;
        }
        if (task.error) {
          [task.error release];
          task.error = nil;
        }
        if (task.done) {
#if !OS_OBJECT_USE_OBJC
          dispatch_release(task.done);
#endif
          task.done = nullptr;
        }
      }
      pipeline_tasks.clear();
    }
    if (pipeline_serializer && pipeline_archive_save &&
        !pipeline_archive_path.empty()) {
      std::error_code ec;
      std::filesystem::path parent = pipeline_archive_path.parent_path();
      if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
      }
      std::string archive_path = pipeline_archive_path.string();
      NSString* ns_path = [NSString stringWithUTF8String:archive_path.c_str()];
      NSURL* url = [NSURL fileURLWithPath:ns_path];
      NSError* archive_err = nil;
      BOOL ok =
          [pipeline_serializer serializeAsArchiveAndFlushToURL:url
                                                         error:&archive_err];
      if (ok) {
        LogPipeline(pipeline_log,
                    "saved pipeline archive: " + archive_path);
      } else {
        std::string message =
            "failed to save pipeline archive: " + archive_path;
        if (archive_err) {
          message += " (" + FormatNSError(archive_err) + ")";
        }
        LogPipeline(pipeline_log, message);
      }
    }
    if (timestamp_heap) {
      [timestamp_heap release];
      timestamp_heap = nil;
      timestamp_heap_count = 0;
    }
    if (residency_set) {
      [residency_set release];
      residency_set = nil;
    }
    for (auto& entry : pipeline_cache) {
      if (entry.second) {
        [entry.second release];
      }
    }
    pipeline_cache.clear();
    if (pipeline_archive) {
      [pipeline_archive release];
      pipeline_archive = nil;
    }
    if (pipeline_serializer) {
      [pipeline_serializer release];
      pipeline_serializer = nil;
    }
    if (real_lib) {
      [real_lib release];
      real_lib = nil;
    }
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

std::string ExpandIncludes(const std::string& source,
                           const std::vector<std::string>& include_paths,
                           std::string* error);

bool EnvEnabled(const char* key) {
  const char* value = std::getenv(key);
  if (!value || *value == '\0') {
    return false;
  }
  std::string lowered;
  lowered.reserve(std::strlen(value));
  for (const char* c = value; *c != '\0'; ++c) {
    lowered.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(*c))));
  }
  if (lowered == "0" || lowered == "false" || lowered == "no" ||
      lowered == "off") {
    return false;
  }
  return true;
}

std::optional<bool> EnvTriState(const char* key) {
  const char* value = std::getenv(key);
  if (!value || *value == '\0') {
    return std::nullopt;
  }
  std::string lowered;
  lowered.reserve(std::strlen(value));
  for (const char* c = value; *c != '\0'; ++c) {
    lowered.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(*c))));
  }
  if (lowered == "0" || lowered == "false" || lowered == "no" ||
      lowered == "off") {
    return false;
  }
  return true;
}

std::optional<std::filesystem::path> EnvPath(const char* key) {
  const char* value = std::getenv(key);
  if (!value || *value == '\0') {
    return std::nullopt;
  }
  return std::filesystem::path(value);
}

std::optional<uint32_t> EnvU32(const char* key) {
  const char* value = std::getenv(key);
  if (!value || *value == '\0') {
    return std::nullopt;
  }
  char* end = nullptr;
  unsigned long parsed = std::strtoul(value, &end, 10);
  if (!end || end == value) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(parsed);
}

uint32_t ChooseThreadgroupSize(uint32_t override_size,
                               uint32_t execution_width,
                               uint32_t max_threads) {
  uint32_t threadgroup = execution_width;
  if (override_size > 0u) {
    threadgroup = override_size;
  }
  if (threadgroup == 0u ||
      (max_threads != 0u && threadgroup > max_threads)) {
    threadgroup = max_threads;
  }
  if (threadgroup == 0u) {
    threadgroup = 1u;
  }
  return threadgroup;
}

std::filesystem::path DefaultPipelineArchivePath() {
#ifdef NDEBUG
  const char* home = std::getenv("HOME");
  std::filesystem::path base =
      home ? std::filesystem::path(home)
           : std::filesystem::temp_directory_path();
  base /= "Library";
  base /= "Caches";
  base /= "metalfpga";
#else
  std::filesystem::path base =
      std::filesystem::current_path() / "artifacts" / "pipeline_cache";
#endif
  return base / "metal4_pipelines.mtl4archive";
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

bool WideHasXz(const std::vector<uint64_t>& words) {
  for (uint64_t word : words) {
    if (word != 0u) {
      return true;
    }
  }
  return false;
}

bool WideBit(const std::vector<uint64_t>& words, uint32_t bit) {
  size_t word_index = bit / 64u;
  if (word_index >= words.size()) {
    return false;
  }
  uint32_t shift = bit % 64u;
  return ((words[word_index] >> shift) & 1ull) != 0ull;
}

std::vector<uint64_t> MaskWideWords(std::vector<uint64_t> words,
                                    uint32_t width) {
  if (width == 0u || words.empty()) {
    return words;
  }
  uint32_t word_count = (width + 63u) / 64u;
  if (words.size() > word_count) {
    words.resize(word_count);
  }
  uint32_t rem = width % 64u;
  if (rem != 0u && !words.empty()) {
    uint64_t mask = (1ull << rem) - 1ull;
    words.back() &= mask;
  }
  return words;
}

std::string FormatWideBits(const std::vector<uint64_t>& value_words,
                           const std::vector<uint64_t>& xz_words,
                           uint32_t width, int base, bool has_xz) {
  if (width == 0u) {
    width = 1u;
  }
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
    bool group_xz = false;
    uint64_t digit = 0;
    for (int bit = 0; bit < group; ++bit) {
      int bit_index = shift + bit;
      if (bit_index >= static_cast<int>(width)) {
        continue;
      }
      if (has_xz && WideBit(xz_words, static_cast<uint32_t>(bit_index))) {
        group_xz = true;
      }
      if (WideBit(value_words, static_cast<uint32_t>(bit_index))) {
        digit |= (1ull << bit);
      }
    }
    if (has_xz && group_xz) {
      out.push_back('x');
      continue;
    }
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

std::string FormatWideUnsigned(std::vector<uint64_t> words, uint32_t width) {
  words = MaskWideWords(std::move(words), width);
  while (!words.empty() && words.back() == 0u) {
    words.pop_back();
  }
  if (words.empty()) {
    return "0";
  }
  std::string out;
  while (!words.empty()) {
    unsigned __int128 rem = 0;
    for (size_t i = words.size(); i-- > 0;) {
      unsigned __int128 cur = (rem << 64) | words[i];
      words[i] = static_cast<uint64_t>(cur / 10u);
      rem = cur % 10u;
    }
    out.push_back(static_cast<char>('0' + static_cast<uint32_t>(rem)));
    while (!words.empty() && words.back() == 0u) {
      words.pop_back();
    }
  }
  std::reverse(out.begin(), out.end());
  return out;
}

std::string FormatWideSigned(std::vector<uint64_t> words, uint32_t width) {
  words = MaskWideWords(std::move(words), width);
  if (width == 0u || words.empty()) {
    return "0";
  }
  bool sign = WideBit(words, width - 1u);
  if (!sign) {
    return FormatWideUnsigned(words, width);
  }
  for (auto& word : words) {
    word = ~word;
  }
  words = MaskWideWords(std::move(words), width);
  uint64_t carry = 1u;
  for (auto& word : words) {
    uint64_t prev = word;
    word += carry;
    carry = (word < prev) ? 1u : 0u;
  }
  return "-" + FormatWideUnsigned(words, width);
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
  if (arg.kind == ServiceArgKind::kWide && !arg.wide_value.empty()) {
    const std::vector<uint64_t>& val = arg.wide_value;
    const std::vector<uint64_t>& xz = arg.wide_xz;
    if (has_xz && WideHasXz(xz) &&
        (spec == 'd' || spec == 'u' || spec == 't')) {
      return "x";
    }
    if (spec == 'b') {
      return FormatWideBits(val, xz, arg.width, 2, has_xz);
    }
    if (spec == 'o') {
      return FormatWideBits(val, xz, arg.width, 8, has_xz);
    }
    if (spec == 'h' || spec == 'x') {
      return FormatWideBits(val, xz, arg.width, 16, has_xz);
    }
    if (spec == 'u' || spec == 't') {
      return FormatWideUnsigned(val, arg.width);
    }
    return FormatWideSigned(val, arg.width);
  }
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

std::string FormatReal(const ServiceArgView& arg, char spec, int precision,
                       bool has_xz) {
  if (has_xz && arg.xz != 0u) {
    return "x";
  }
  double value = 0.0;
  if (arg.kind == ServiceArgKind::kReal) {
    uint64_t bits = arg.value;
    std::memcpy(&value, &bits, sizeof(value));
  } else {
    int64_t signed_value = SignExtend(arg.value, arg.width);
    value = static_cast<double>(signed_value);
  }
  std::ostringstream oss;
  if (spec == 'f') {
    oss << std::fixed;
  } else if (spec == 'e') {
    oss << std::scientific;
  }
  if (precision >= 0) {
    oss << std::setprecision(precision);
  }
  oss << value;
  return oss.str();
}

std::string FormatArg(const ServiceArgView& arg, char spec, int precision,
                      const ServiceStringTable& strings, bool has_xz) {
  if (arg.kind == ServiceArgKind::kString ||
      arg.kind == ServiceArgKind::kIdent) {
    return ResolveString(strings, static_cast<uint32_t>(arg.value));
  }
  if (spec == 's') {
    return FormatNumeric(arg, 'd', has_xz);
  }
  if (spec == 'f' || spec == 'e' || spec == 'g') {
    return FormatReal(arg, spec, precision, has_xz);
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
    int precision = -1;
    size_t j = i + 1;
    if (j < fmt.size() && fmt[j] == '0') {
      zero_pad = true;
      ++j;
    }
    while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') {
      width = (width * 10) + (fmt[j] - '0');
      ++j;
    }
    if (j < fmt.size() && fmt[j] == '.') {
      ++j;
      precision = 0;
      while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') {
        precision = (precision * 10) + (fmt[j] - '0');
        ++j;
      }
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
    std::string text =
        FormatArg(args[arg_index], spec, precision, strings, has_xz);
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
    } else if (arg.kind == ServiceArgKind::kReal) {
      oss << FormatReal(arg, 'g', -1, has_xz);
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

bool NeedsDynamicLibrary(const std::string& source,
                         const std::string& include_name) {
  return source.find(include_name) != std::string::npos;
}

bool NeedsRebuild(const std::filesystem::path& output_path,
                  const std::vector<std::filesystem::path>& inputs) {
  std::error_code ec;
  if (!std::filesystem::exists(output_path, ec)) {
    return true;
  }
  auto output_time = std::filesystem::last_write_time(output_path, ec);
  if (ec) {
    return true;
  }
  for (const auto& input : inputs) {
    if (input.empty()) {
      continue;
    }
    auto input_time = std::filesystem::last_write_time(input, ec);
    if (ec || input_time > output_time) {
      return true;
    }
  }
  return false;
}

bool EnsureDynamicLibrary(id<MTL4Compiler> compiler, const std::string& name,
                          const std::string& source_path,
                          const std::vector<std::string>& include_paths,
                          const std::vector<std::string>& dependencies,
                          id<MTLDynamicLibrary>* out, std::string* error) {
  if (!compiler || !out) {
    if (error) {
      *error = "Metal compiler unavailable for dynamic library";
    }
    return false;
  }
  if (*out) {
    return true;
  }
  std::filesystem::path cache_dir =
      std::filesystem::current_path() / "artifacts" / "metal_libs";
  std::error_code ec;
  std::filesystem::create_directories(cache_dir, ec);
  std::filesystem::path cache_path = cache_dir / (name + ".metallib");

  std::filesystem::path source_file = std::filesystem::path(source_path);
  if (!source_file.is_absolute()) {
    source_file = std::filesystem::current_path() / source_file;
  }
  std::vector<std::filesystem::path> inputs;
  inputs.reserve(1 + dependencies.size());
  inputs.push_back(source_file);
  for (const auto& dep : dependencies) {
    if (dep.empty()) {
      continue;
    }
    std::filesystem::path dep_path = std::filesystem::path(dep);
    if (!dep_path.is_absolute()) {
      dep_path = std::filesystem::current_path() / dep_path;
    }
    inputs.push_back(dep_path);
  }

  bool rebuild = NeedsRebuild(cache_path, inputs);
  if (!rebuild) {
    NSString* ns_path =
        [NSString stringWithUTF8String:cache_path.string().c_str()];
    NSURL* url = [NSURL fileURLWithPath:ns_path];
    NSError* load_err = nil;
    id<MTLDynamicLibrary> cached =
        [compiler newDynamicLibraryWithURL:url error:&load_err];
    if (cached) {
      *out = cached;
      return true;
    }
  }

  std::string source = ReadFileContents(source_file.string(), error);
  if (source.empty()) {
    if (error && error->empty()) {
      *error = "dynamic library source missing: " + source_file.string();
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
  options.libraryType = MTLLibraryTypeDynamic;
  options.installName =
      [NSString stringWithUTF8String:cache_path.string().c_str()];
  MTL4LibraryDescriptor* lib_desc = [[MTL4LibraryDescriptor alloc] init];
  lib_desc.source = ns_source;
  lib_desc.options = options;
  NSError* err = nil;
  id<MTLLibrary> library =
      [compiler newLibraryWithDescriptor:lib_desc error:&err];
  [lib_desc release];
  [options release];
  [ns_source release];
  if (!library) {
    if (error) {
      *error = FormatNSError(err);
    }
    return false;
  }
  id<MTLDynamicLibrary> dynamic =
      [compiler newDynamicLibrary:library error:&err];
  [library release];
  if (!dynamic) {
    if (error) {
      *error = FormatNSError(err);
    }
    return false;
  }
  NSString* ns_path =
      [NSString stringWithUTF8String:cache_path.string().c_str()];
  NSURL* url = [NSURL fileURLWithPath:ns_path];
  NSError* save_err = nil;
  if (![dynamic serializeToURL:url error:&save_err]) {
    if (error) {
      *error = FormatNSError(save_err);
    }
    [dynamic release];
    return false;
  }
  *out = dynamic;
  return true;
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

bool ParseKernelBindingsFromSource(
    const std::string& source, const std::string& kernel_name,
    std::unordered_map<std::string, uint32_t>* out, std::string* error) {
  if (!out) {
    if (error) {
      *error = "buffer binding output map is null";
    }
    return false;
  }
  const std::string needle = "kernel void " + kernel_name;
  size_t pos = source.find(needle);
  if (pos == std::string::npos) {
    if (error) {
      *error = "kernel signature not found for " + kernel_name;
    }
    return false;
  }
  size_t open = source.find('(', pos);
  if (open == std::string::npos) {
    if (error) {
      *error = "kernel signature missing '(' for " + kernel_name;
    }
    return false;
  }
  size_t close = std::string::npos;
  int depth = 0;
  for (size_t i = open; i < source.size(); ++i) {
    if (source[i] == '(') {
      depth += 1;
    } else if (source[i] == ')') {
      depth -= 1;
      if (depth == 0) {
        close = i;
        break;
      }
    }
  }
  if (close == std::string::npos || close <= open) {
    if (error) {
      *error = "kernel signature missing ')' for " + kernel_name;
    }
    return false;
  }
  std::string sig = source.substr(open + 1, close - open - 1);
  std::regex pattern(
      R"(([A-Za-z_][A-Za-z0-9_]*)\s*\[\[buffer\((\d+)\)\]\])");
  std::sregex_iterator it(sig.begin(), sig.end(), pattern);
  std::sregex_iterator end;
  out->clear();
  for (; it != end; ++it) {
    const std::smatch& match = *it;
    if (match.size() < 3) {
      continue;
    }
    uint32_t index = 0u;
    try {
      index = static_cast<uint32_t>(std::stoul(match[2].str()));
    } catch (...) {
      continue;
    }
    (*out)[match[1].str()] = index;
  }
  if (out->empty()) {
    if (error) {
      *error = "no buffer bindings parsed for " + kernel_name;
    }
    return false;
  }
  return true;
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

bool ParseSchedDefineConstants(const std::string& source,
                               SchedulerConstants* out) {
  if (!out) {
    return false;
  }
  const std::string token = "GPGA_SCHED_DEFINE_CONSTANTS";
  auto parse_args = [&](const std::string& args,
                        SchedulerConstants* info) -> bool {
    std::vector<uint32_t> values;
    for (size_t i = 0; i < args.size();) {
      while (i < args.size() &&
             !std::isdigit(static_cast<unsigned char>(args[i]))) {
        ++i;
      }
      if (i >= args.size()) {
        break;
      }
      size_t start = i;
      while (i < args.size() &&
             std::isdigit(static_cast<unsigned char>(args[i]))) {
        ++i;
      }
      uint32_t value = 0u;
      try {
        value = static_cast<uint32_t>(
            std::stoul(args.substr(start, i - start)));
      } catch (...) {
        return false;
      }
      values.push_back(value);
      while (i < args.size() &&
             (args[i] == 'u' || args[i] == 'U' || args[i] == 'l' ||
              args[i] == 'L')) {
        ++i;
      }
    }
    if (values.size() < 17u) {
      return false;
    }
    info->proc_count = values[0];
    info->event_count = values[2];
    info->edge_count = values[3];
    info->edge_star_count = values[4];
    info->repeat_count = values[8];
    info->delay_count = values[9];
    info->max_dnba = values[10];
    info->monitor_count = values[11];
    info->monitor_max_args = values[12];
    info->strobe_count = values[13];
    info->service_max_args = values[14];
    info->service_wide_words = values[15];
    info->string_count = values[16];
    if (values.size() > 17u) {
      info->force_count = values[17];
    }
    if (values.size() > 18u) {
      info->pcont_count = values[18];
    }
    info->has_scheduler = info->proc_count > 0u;
    info->has_services = info->service_max_args > 0u;
    return true;
  };

  std::istringstream in(source);
  std::string line;
  while (std::getline(in, line)) {
    std::string trimmed = line;
    trimmed.erase(trimmed.begin(),
                  std::find_if(trimmed.begin(), trimmed.end(),
                               [](unsigned char c) { return c != ' '; }));
    if (StartsWith(trimmed, "#define")) {
      continue;
    }
    size_t pos = trimmed.find(token);
    if (pos == std::string::npos) {
      continue;
    }
    size_t open = trimmed.find('(', pos + token.size());
    size_t close = trimmed.rfind(')');
    if (open == std::string::npos || close == std::string::npos ||
        close <= open + 1) {
      break;
    }
    SchedulerConstants info;
    if (parse_args(trimmed.substr(open + 1, close - open - 1), &info)) {
      *out = info;
      return true;
    }
  }

  size_t pos = 0;
  while (true) {
    pos = source.find(token, pos);
    if (pos == std::string::npos) {
      return false;
    }
    size_t open = source.find('(', pos + token.size());
    if (open == std::string::npos) {
      return false;
    }
    size_t close = std::string::npos;
    int depth = 0;
    for (size_t i = open; i < source.size(); ++i) {
      if (source[i] == '(') {
        depth++;
      } else if (source[i] == ')') {
        depth--;
        if (depth == 0) {
          close = i;
          break;
        }
      }
    }
    if (close == std::string::npos || close <= open + 1) {
      return false;
    }
    SchedulerConstants info;
    if (parse_args(source.substr(open + 1, close - open - 1), &info)) {
      *out = info;
      return true;
    }
    pos = close + 1;
  }
}

}  // namespace

void LogPipeline(bool enabled, const std::string& message) {
  if (!enabled) {
    return;
  }
  std::cerr << FormatTimestamp() << " [metal_runtime] " << message << "\n";
}

std::string FormatProgressBar(size_t done, size_t total, size_t width = 24u) {
  if (width == 0u) {
    width = 1u;
  }
  size_t filled = 0;
  if (total > 0u) {
    filled = (done * width) / total;
    if (filled > width) {
      filled = width;
    }
  }
  std::string bar = "[";
  bar.append(filled, '#');
  bar.append(width - filled, '-');
  bar += "] ";
  bar += std::to_string(done);
  bar += "/";
  bar += std::to_string(total);
  return bar;
}

std::string FormatTimestamp() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  auto elapsed = steady_clock::now() - start;
  auto total_ms = duration_cast<milliseconds>(elapsed).count();
  auto ms = total_ms % 1000;
  auto total_sec = total_ms / 1000;
  auto sec = total_sec % 60;
  auto total_min = total_sec / 60;
  auto min = total_min % 60;
  auto hours = total_min / 60;
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << hours << ":"
      << std::setw(2) << min << ":" << std::setw(2) << sec << ":"
      << std::setw(3) << ms;
  return oss.str();
}

void LogPipelineTrace(bool enabled, const std::string& message) {
  if (!enabled) {
    return;
  }
  std::cerr << FormatTimestamp() << " [metal_runtime][pipeline] " << message
            << "\n";
}

void LogPipelineProgress(bool enabled, size_t done, size_t total,
                         const std::string& name, const std::string& status,
                         double ms) {
  if (!enabled) {
    return;
  }
  std::ostringstream oss;
  oss << FormatTimestamp() << " [metal_runtime][progress] "
      << FormatProgressBar(done, total) << " " << status << " " << name;
  if (ms >= 0.0) {
    oss << " " << std::fixed << std::setprecision(1) << ms << "ms";
  }
  std::cerr << oss.str() << "\n";
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
  max_buffer_bindings_ = 0;
  thread_execution_width_ = 0;
  max_threads_per_threadgroup_ = 0;
}

MetalKernel::MetalKernel(MetalKernel&& other) noexcept
    : pipeline_(other.pipeline_),
      argument_table_(other.argument_table_),
      name_(std::move(other.name_)),
      buffer_indices_(std::move(other.buffer_indices_)),
      max_buffer_bindings_(other.max_buffer_bindings_),
      thread_execution_width_(other.thread_execution_width_),
      max_threads_per_threadgroup_(other.max_threads_per_threadgroup_),
      last_binding_addresses_(std::move(other.last_binding_addresses_)) {
  other.pipeline_ = nullptr;
  other.argument_table_ = nullptr;
  other.max_buffer_bindings_ = 0;
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
  name_ = std::move(other.name_);
  buffer_indices_ = std::move(other.buffer_indices_);
  max_buffer_bindings_ = other.max_buffer_bindings_;
  thread_execution_width_ = other.thread_execution_width_;
  max_threads_per_threadgroup_ = other.max_threads_per_threadgroup_;
  last_binding_addresses_ = std::move(other.last_binding_addresses_);
  other.pipeline_ = nullptr;
  other.argument_table_ = nullptr;
  other.max_buffer_bindings_ = 0;
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

void MetalRuntime::SetPreferSourceBindings(bool value) {
  if (!impl_) {
    impl_ = std::make_unique<Impl>();
  }
  impl_->prefer_source_bindings = value;
}

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
  if (impl_->pipeline_archive_path.empty()) {
    auto override_path = EnvPath("METALFPGA_PIPELINE_ARCHIVE");
    impl_->pipeline_archive_path =
        override_path.value_or(DefaultPipelineArchivePath());
    impl_->pipeline_archive_load =
        EnvEnabled("METALFPGA_PIPELINE_ARCHIVE_LOAD");
    impl_->pipeline_archive_save =
        EnvEnabled("METALFPGA_PIPELINE_ARCHIVE_SAVE") ||
        EnvEnabled("METALFPGA_PIPELINE_HARVEST");
    impl_->pipeline_async = true;
    auto async_pref = EnvTriState("METALFPGA_PIPELINE_ASYNC");
    auto precompile_pref = EnvTriState("METALFPGA_PIPELINE_PRECOMPILE");
    if (async_pref && *async_pref) {
      impl_->pipeline_async = true;
    } else if (precompile_pref && *precompile_pref) {
      impl_->pipeline_async = true;
    } else if (async_pref || precompile_pref) {
      impl_->pipeline_async = false;
    }
    impl_->pipeline_log = EnvEnabled("METALFPGA_PIPELINE_LOG");
    if (auto trace_pref = EnvTriState("METALFPGA_PIPELINE_TRACE")) {
      impl_->pipeline_trace = *trace_pref;
    }
    if (auto progress_pref = EnvTriState("METALFPGA_PIPELINE_PROGRESS")) {
      impl_->pipeline_progress = *progress_pref;
    }
    if (auto override_size = EnvU32("METALFPGA_THREADGROUP_SIZE")) {
      impl_->threadgroup_override = *override_size;
    }
    impl_->use_residency_set = EnvEnabled("METALFPGA_RESIDENCY_SET");
    impl_->gpu_timestamps = EnvEnabled("METALFPGA_GPU_TIMESTAMPS") ||
                            EnvEnabled("METALFPGA_GPU_PROFILE");
    impl_->gpu_timestamps_precise =
        EnvEnabled("METALFPGA_GPU_TIMESTAMPS_PRECISE");
    if (auto every = EnvU32("METALFPGA_GPU_TIMESTAMPS_EVERY")) {
      impl_->gpu_timestamp_every = std::max(1u, *every);
    }
  }
  if (impl_->pipeline_archive_load && !impl_->pipeline_archive) {
    std::string archive_path = impl_->pipeline_archive_path.string();
    NSString* ns_path = [NSString stringWithUTF8String:archive_path.c_str()];
    NSURL* url = [NSURL fileURLWithPath:ns_path];
    id<MTL4Archive> archive =
        [impl_->device newArchiveWithURL:url error:nil];
    if (archive) {
      impl_->pipeline_archive = archive;
      LogPipeline(impl_->pipeline_log,
                  "loaded pipeline archive: " + archive_path);
    } else {
      LogPipeline(impl_->pipeline_log,
                  "pipeline archive not found: " + archive_path);
    }
  }
  if (impl_->pipeline_archive_save && !impl_->pipeline_serializer) {
    MTL4PipelineDataSetSerializerDescriptor* desc =
        [[MTL4PipelineDataSetSerializerDescriptor alloc] init];
    desc.configuration = MTL4PipelineDataSetSerializerConfigurationCaptureDescriptors |
                         MTL4PipelineDataSetSerializerConfigurationCaptureBinaries;
    id<MTL4PipelineDataSetSerializer> serializer =
        [impl_->device newPipelineDataSetSerializerWithDescriptor:desc];
    [desc release];
    if (serializer) {
      impl_->pipeline_serializer = serializer;
    }
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
  if (impl_->use_residency_set && !impl_->residency_set) {
    MTLResidencySetDescriptor* desc = [[MTLResidencySetDescriptor alloc] init];
    desc.initialCapacity = 64;
    desc.label = @"metalfpga";
    NSError* res_err = nil;
    impl_->residency_set =
        [impl_->device newResidencySetWithDescriptor:desc error:&res_err];
    [desc release];
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
    if (impl_->pipeline_serializer) {
      desc.pipelineDataSetSerializer = impl_->pipeline_serializer;
    }
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
  const bool needs_real_lib =
      NeedsDynamicLibrary(source, "gpga_real_decl.h") ||
      NeedsDynamicLibrary(source, "gpga_real.h");
  if (needs_real_lib) {
    if (!EnsureDynamicLibrary(impl_->compiler, "gpga_real",
                              "src/msl/gpga_real_lib.metal", include_paths,
                              {"include/gpga_real.h"}, &impl_->real_lib,
                              error)) {
      return false;
    }
  }
  std::string expanded = ExpandIncludes(source, include_paths, error);
  if (expanded.empty()) {
    expanded = source;
  }
  if (impl_) {
    impl_->last_source = expanded;
  }
  NSString* ns_source =
      [[NSString alloc] initWithBytes:expanded.data()
                               length:expanded.size()
                             encoding:NSUTF8StringEncoding];
  MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
  if (needs_real_lib && impl_->real_lib) {
    NSArray<id<MTLDynamicLibrary>>* libs = @[ impl_->real_lib ];
    options.libraries = libs;
  }
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
  for (auto& entry : impl_->pipeline_cache) {
    if (entry.second) {
      [entry.second release];
    }
  }
  impl_->pipeline_cache.clear();
  {
    std::vector<dispatch_semaphore_t> pending;
    {
      std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
      pending.reserve(impl_->pipeline_tasks.size());
      for (auto& entry : impl_->pipeline_tasks) {
        if (entry.second.done) {
          pending.push_back(entry.second.done);
        }
      }
    }
    for (dispatch_semaphore_t sema : pending) {
      dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    }
    {
      std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
      for (auto& entry : impl_->pipeline_tasks) {
        Impl::PipelineTask& task = entry.second;
        if (task.task) {
          [task.task release];
          task.task = nil;
        }
        if (task.pipeline) {
          [task.pipeline release];
          task.pipeline = nil;
        }
        if (task.error) {
          [task.error release];
          task.error = nil;
        }
        if (task.done) {
#if !OS_OBJECT_USE_OBJC
          dispatch_release(task.done);
#endif
          task.done = nullptr;
        }
      }
      impl_->pipeline_tasks.clear();
      impl_->precompile_total = 0;
      impl_->precompile_done = 0;
    }
  }
  return true;
}

bool MetalRuntime::GetLastSource(std::string* out) const {
  if (!out || !impl_) {
    return false;
  }
  *out = impl_->last_source;
  return !impl_->last_source.empty();
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
  id<MTLComputePipelineState> pipeline = nil;
  auto cached = impl_->pipeline_cache.find(name);
  if (cached != impl_->pipeline_cache.end() && cached->second) {
    pipeline = cached->second;
    [pipeline retain];
  } else {
    dispatch_semaphore_t pending_done = nullptr;
    {
      std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
      auto it = impl_->pipeline_tasks.find(name);
      if (it != impl_->pipeline_tasks.end()) {
        pending_done = it->second.done;
      }
    }
    if (pending_done) {
      if (impl_->pipeline_trace) {
        LogPipelineTrace(true, "awaiting async compile: " + name);
      }
      auto wait_start = std::chrono::steady_clock::now();
      auto last_log = wait_start;
      while (true) {
        long wait_result = dispatch_semaphore_wait(
            pending_done, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
        if (wait_result == 0) {
          break;
        }
        auto now = std::chrono::steady_clock::now();
        if (impl_->pipeline_trace &&
            std::chrono::duration_cast<std::chrono::seconds>(now - last_log)
                    .count() >= 5) {
          auto elapsed_ms =
              std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                    wait_start)
                  .count();
          LogPipelineTrace(true, "still waiting: " + name + " " +
                                    std::to_string(elapsed_ms) + "ms");
          last_log = now;
        }
      }
      NSError* task_error = nil;
      {
        std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
        auto it = impl_->pipeline_tasks.find(name);
        if (it != impl_->pipeline_tasks.end()) {
          pipeline = it->second.pipeline;
          it->second.pipeline = nil;
          task_error = it->second.error;
          it->second.error = nil;
          if (it->second.task) {
            [it->second.task release];
            it->second.task = nil;
          }
          if (it->second.done) {
#if !OS_OBJECT_USE_OBJC
            dispatch_release(it->second.done);
#endif
            it->second.done = nullptr;
          }
          impl_->pipeline_tasks.erase(it);
        }
      }
      if (!pipeline) {
        if (error) {
          *error = task_error ? FormatNSError(task_error)
                              : "Metal pipeline compilation failed";
        }
        if (task_error) {
          [task_error release];
        }
        return false;
      }
      if (task_error) {
        [task_error release];
      }
      impl_->pipeline_cache[name] = [pipeline retain];
    }
  }
  if (!pipeline) {
    NSString* fn_name = [NSString stringWithUTF8String:name.c_str()];
    MTL4LibraryFunctionDescriptor* fn_desc =
        [[MTL4LibraryFunctionDescriptor alloc] init];
    fn_desc.name = fn_name;
    fn_desc.library = impl_->library;

    MTL4ComputePipelineDescriptor* desc =
        [[MTL4ComputePipelineDescriptor alloc] init];
    desc.computeFunctionDescriptor = fn_desc;
    MTL4PipelineOptions* pipe_opts = [[MTL4PipelineOptions alloc] init];
    if (impl_->prefer_source_bindings) {
      pipe_opts.shaderReflection = static_cast<MTL4ShaderReflection>(0);
    } else {
      pipe_opts.shaderReflection = MTL4ShaderReflectionBindingInfo;
    }
    desc.options = pipe_opts;

    NSError* err = nil;
    auto compile_start = std::chrono::steady_clock::now();
    if (impl_->pipeline_trace) {
      LogPipelineTrace(true, "compile sync start: " + name);
    }
    if (impl_->pipeline_archive) {
      pipeline =
          [impl_->pipeline_archive newComputePipelineStateWithDescriptor:desc
                                                                   error:&err];
      if (pipeline && impl_->pipeline_trace) {
        LogPipelineTrace(true, "archive hit: " + name);
      }
    }
    if (!pipeline) {
      err = nil;
      pipeline = [impl_->compiler newComputePipelineStateWithDescriptor:desc
                                                   compilerTaskOptions:nil
                                                                error:&err];
    }
    [pipe_opts release];
    [desc release];
    [fn_desc release];
    if (!pipeline) {
      if (impl_->pipeline_trace) {
        double ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - compile_start)
                        .count();
        LogPipelineTrace(true,
                         "compile sync failed: " + name + " " +
                             std::to_string(static_cast<int64_t>(ms)) +
                             "ms");
      }
      if (error) {
        *error = FormatNSError(err);
      }
      return false;
    }
    if (impl_->pipeline_trace) {
      double ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - compile_start)
                      .count();
      LogPipelineTrace(true, "compile sync done: " + name + " " +
                                std::to_string(static_cast<int64_t>(ms)) +
                                "ms");
    }
    impl_->pipeline_cache[name] = [pipeline retain];
  }
  MetalKernel temp;
  temp.pipeline_ = pipeline;
  temp.thread_execution_width_ =
      static_cast<uint32_t>(pipeline.threadExecutionWidth);
  temp.max_threads_per_threadgroup_ =
      static_cast<uint32_t>(pipeline.maxTotalThreadsPerThreadgroup);
  uint32_t max_index = 0;
  bool has_index = false;
  if (impl_->prefer_source_bindings && !impl_->last_source.empty()) {
    std::unordered_map<std::string, uint32_t> bindings;
    if (!ParseKernelBindingsFromSource(impl_->last_source, name, &bindings,
                                       error)) {
      [pipeline release];
      temp.pipeline_ = nullptr;
      return false;
    }
    temp.buffer_indices_ = std::move(bindings);
    for (const auto& entry : temp.buffer_indices_) {
      uint32_t index = entry.second;
      if (!has_index || index > max_index) {
        max_index = index;
        has_index = true;
      }
    }
  } else {
    MTLComputePipelineReflection* reflection = pipeline.reflection;
    if (!reflection) {
      if (error) {
        *error = "Metal pipeline reflection unavailable";
      }
      [pipeline release];
      return false;
    }
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
  }
  if (has_index) {
    constexpr uint32_t kMaxArgumentTableBindings = 31u;
    if (max_index >= kMaxArgumentTableBindings) {
      if (error) {
        *error =
            "Metal 4 argument tables support up to 31 buffer bindings (0-30)";
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
    temp.max_buffer_bindings_ = max_index + 1u;
    temp.last_binding_addresses_.assign(temp.max_buffer_bindings_,
                                        std::numeric_limits<uint64_t>::max());
  }
  temp.name_ = name;
  *kernel = std::move(temp);
  return true;
}

bool MetalRuntime::PrecompileKernels(const std::vector<std::string>& names,
                                     std::string* error) {
  if (!Initialize(error)) {
    return false;
  }
  if (!impl_ || !impl_->pipeline_async) {
    return true;
  }
  if (!impl_->library) {
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
  for (const auto& name : names) {
    if (name.empty()) {
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
      if (impl_->pipeline_cache.find(name) != impl_->pipeline_cache.end() ||
          impl_->pipeline_tasks.find(name) != impl_->pipeline_tasks.end()) {
        if (impl_->pipeline_trace) {
          LogPipelineTrace(true, "precompile skip (cached/pending): " + name);
        }
        continue;
      }
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
    if (impl_->prefer_source_bindings) {
      pipe_opts.shaderReflection = static_cast<MTL4ShaderReflection>(0);
    } else {
      pipe_opts.shaderReflection = MTL4ShaderReflectionBindingInfo;
    }
    desc.options = pipe_opts;

    NSError* archive_err = nil;
    id<MTLComputePipelineState> archive_pipeline = nil;
    if (impl_->pipeline_archive) {
      archive_pipeline =
          [impl_->pipeline_archive newComputePipelineStateWithDescriptor:desc
                                                                   error:&archive_err];
    }
    if (archive_pipeline) {
      std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
      impl_->pipeline_cache[name] = [archive_pipeline retain];
      [archive_pipeline release];
      if (impl_->pipeline_trace) {
        LogPipelineTrace(true, "archive hit: " + name);
      }
      [pipe_opts release];
      [desc release];
      [fn_desc release];
      continue;
    }

    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    size_t queued_total = 0;
    size_t queued_done = 0;
    {
      std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
      Impl::PipelineTask pending;
      pending.done = done;
      pending.start = std::chrono::steady_clock::now();
      impl_->pipeline_tasks.emplace(name, std::move(pending));
      if (impl_->pipeline_progress) {
        impl_->precompile_total += 1u;
        queued_total = impl_->precompile_total;
        queued_done = impl_->precompile_done;
      }
    }
    if (impl_->pipeline_trace) {
      LogPipelineTrace(true, "compile queued: " + name);
    }
    if (impl_->pipeline_progress) {
      LogPipelineProgress(true, queued_done, queued_total, name, "queued",
                          -1.0);
    }

    Impl* impl_ptr = impl_.get();
    std::string key = name;
    id<MTL4CompilerTask> task =
        [impl_->compiler
            newComputePipelineStateWithDescriptor:desc
                                compilerTaskOptions:nil
                                  completionHandler:^(id<MTLComputePipelineState> pipeline,
                                                      NSError* compile_error) {
                                    size_t done_count = 0;
                                    size_t total_count = 0;
                                    bool progress_enabled = false;
                                    bool trace_enabled = false;
                                    auto start_time =
                                        std::chrono::steady_clock::time_point{};
                                    {
                                      std::lock_guard<std::mutex> lock(
                                          impl_ptr->pipeline_mutex);
                                      auto it =
                                          impl_ptr->pipeline_tasks.find(key);
                                      if (it ==
                                          impl_ptr->pipeline_tasks.end()) {
                                        return;
                                      }
                                      progress_enabled =
                                          impl_ptr->pipeline_progress;
                                      trace_enabled =
                                          impl_ptr->pipeline_trace;
                                      start_time = it->second.start;
                                      if (pipeline) {
                                        it->second.pipeline =
                                            [pipeline retain];
                                      }
                                      if (compile_error) {
                                        it->second.error =
                                            [compile_error retain];
                                      }
                                      if (progress_enabled) {
                                        impl_ptr->precompile_done += 1u;
                                        done_count =
                                            impl_ptr->precompile_done;
                                        total_count =
                                            impl_ptr->precompile_total;
                                      }
                                      dispatch_semaphore_signal(
                                          it->second.done);
                                    }
                                    double elapsed_ms = -1.0;
                                    if (start_time !=
                                        std::chrono::steady_clock::time_point{}) {
                                      elapsed_ms =
                                          std::chrono::duration_cast<
                                              std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now() -
                                              start_time)
                                              .count();
                                    }
                                    if (trace_enabled) {
                                      if (compile_error) {
                                        LogPipelineTrace(
                                            true,
                                            "compile failed: " + key + " " +
                                                std::to_string(
                                                    static_cast<int64_t>(
                                                        elapsed_ms)) +
                                                "ms (" +
                                                FormatNSError(compile_error) +
                                                ")");
                                      } else {
                                        LogPipelineTrace(
                                            true,
                                            "compile done: " + key + " " +
                                                std::to_string(
                                                    static_cast<int64_t>(
                                                        elapsed_ms)) +
                                                "ms");
                                      }
                                    }
                                    if (progress_enabled) {
                                      LogPipelineProgress(
                                          true, done_count, total_count, key,
                                          compile_error ? "failed" : "done",
                                          elapsed_ms);
                                    }
                                  }];

    [pipe_opts release];
    [desc release];
    [fn_desc release];

    if (!task) {
      {
        std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
        auto it = impl_->pipeline_tasks.find(name);
        if (it != impl_->pipeline_tasks.end()) {
#if !OS_OBJECT_USE_OBJC
          dispatch_release(it->second.done);
#endif
          impl_->pipeline_tasks.erase(it);
        }
      }
      if (error) {
        *error = "Failed to create Metal pipeline compilation task";
      }
      return false;
    }

    {
      std::lock_guard<std::mutex> lock(impl_->pipeline_mutex);
      auto it = impl_->pipeline_tasks.find(name);
      if (it != impl_->pipeline_tasks.end()) {
        it->second.task = task;
      }
    }
  }
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
  if (impl_->residency_set) {
    id<MTLAllocation> allocation = (id<MTLAllocation>)mtl_buffer;
    if (![impl_->residency_set containsAllocation:allocation]) {
      [impl_->residency_set addAllocation:allocation];
      [impl_->residency_set commit];
    }
  }
  buffer.handle_ = (void*)mtl_buffer;
  buffer.contents_ = [mtl_buffer contents];
  buffer.length_ = length;
  if (initial_data && buffer.contents_) {
    std::memcpy(buffer.contents_, initial_data, length);
  }
  return buffer;
}

bool MetalRuntime::EncodeArgumentBuffer(
    const MetalKernel& kernel, uint32_t buffer_index,
    const std::vector<MetalBufferBinding>& bindings, MetalBuffer* out,
    std::string* error) {
  if (!out) {
    if (error) {
      *error = "argument buffer output is null";
    }
    return false;
  }
  *out = MetalBuffer{};
  if (!impl_ || !impl_->device || !kernel.pipeline_) {
    if (error) {
      *error = "Metal runtime not initialized for argument buffer";
    }
    return false;
  }
  id<MTLFunction> function = nil;
  if (impl_->library && !kernel.name_.empty()) {
    NSString* fn_name = [NSString stringWithUTF8String:kernel.name_.c_str()];
    function = [impl_->library newFunctionWithName:fn_name];
  }
  if (!function) {
    if (error) {
      *error = "Failed to resolve Metal function for argument buffer";
    }
    return false;
  }
  id<MTLArgumentEncoder> encoder =
      [function newArgumentEncoderWithBufferIndex:buffer_index];
  [function release];
  if (!encoder) {
    if (error) {
      *error = "Failed to create Metal argument encoder";
    }
    return false;
  }
  size_t length = static_cast<size_t>(encoder.encodedLength);
  if (length == 0u) {
    [encoder release];
    if (error) {
      *error = "Metal argument encoder reported zero length";
    }
    return false;
  }
  MetalBuffer buffer = CreateBuffer(length, nullptr);
  if (!buffer.handle_) {
    [encoder release];
    if (error) {
      *error = "Failed to allocate Metal argument buffer";
    }
    return false;
  }
  id<MTLBuffer> arg_buffer = (id<MTLBuffer>)buffer.handle_;
  [encoder setArgumentBuffer:arg_buffer offset:0];
  for (const auto& binding : bindings) {
    if (!binding.buffer || !binding.buffer->handle_) {
      continue;
    }
    if (binding.offset >= binding.buffer->length()) {
      [encoder release];
      if (error) {
        *error =
            "Metal argument buffer binding offset out of range (offset=" +
            std::to_string(binding.offset) + ", length=" +
            std::to_string(binding.buffer->length()) + ")";
      }
      return false;
    }
    id<MTLBuffer> buffer_obj = (id<MTLBuffer>)binding.buffer->handle_;
    [encoder setBuffer:buffer_obj offset:binding.offset atIndex:binding.index];
  }
  [encoder release];
  *out = std::move(buffer);
  return true;
}

bool MetalRuntime::Dispatch(const MetalKernel& kernel,
                            const std::vector<MetalBufferBinding>& bindings,
                            uint32_t grid_size, std::string* error,
                            uint32_t timeout_ms) {
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
  if (impl_->residency_set) {
    [cmd useResidencySet:impl_->residency_set];
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
  bool sample_gpu = impl_->ShouldSampleGpuTimestamps();
  if (sample_gpu) {
    std::string ts_error;
    if (!impl_->EnsureTimestampHeap(2, &ts_error)) {
      if (!ts_error.empty()) {
        std::cerr << "[gpu_profile] " << ts_error << "\n";
      }
      sample_gpu = false;
    }
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
    const uint32_t max_bindings = kernel.MaxBufferBindings();
    if (max_bindings > 0u &&
        kernel.last_binding_addresses_.size() != max_bindings) {
      kernel.last_binding_addresses_.assign(
          max_bindings, std::numeric_limits<uint64_t>::max());
    }
    for (const auto& binding : bindings) {
      if (!binding.buffer || !binding.buffer->handle_) {
        continue;
      }
      if (max_bindings != 0u && binding.index >= max_bindings) {
        if (error) {
          *error = "Metal buffer binding index out of range (index=" +
                   std::to_string(binding.index) + ", max=" +
                   std::to_string(max_bindings - 1u) + ")";
        }
        [encoder endEncoding];
        [cmd endCommandBuffer];
        [cmd release];
        return false;
      }
      if (binding.offset >= binding.buffer->length()) {
        if (error) {
          *error = "Metal buffer binding offset out of range (offset=" +
                   std::to_string(binding.offset) + ", length=" +
                   std::to_string(binding.buffer->length()) + ")";
        }
        [encoder endEncoding];
        [cmd endCommandBuffer];
        [cmd release];
        return false;
      }
      id<MTLBuffer> buffer = (id<MTLBuffer>)binding.buffer->handle_;
      MTLGPUAddress address =
          buffer.gpuAddress + static_cast<MTLGPUAddress>(binding.offset);
      const uint64_t addr_key = static_cast<uint64_t>(address);
      if (max_bindings == 0u ||
          addr_key != kernel.last_binding_addresses_[binding.index]) {
        [table setAddress:address atIndex:binding.index];
        if (max_bindings != 0u) {
          kernel.last_binding_addresses_[binding.index] = addr_key;
        }
      }
    }
    [encoder setArgumentTable:table];
  }
  uint32_t threadgroup = ChooseThreadgroupSize(
      impl_->threadgroup_override, kernel.ThreadExecutionWidth(),
      kernel.MaxThreadsPerThreadgroup());
  MTLSize threads_per_group = MTLSizeMake(threadgroup, 1, 1);
  MTLSize grid = MTLSizeMake(grid_size, 1, 1);
  if (sample_gpu) {
    [encoder writeTimestampWithGranularity:impl_->TimestampGranularity()
                                  intoHeap:impl_->timestamp_heap
                                   atIndex:0];
  }
  [encoder dispatchThreads:grid threadsPerThreadgroup:threads_per_group];
  if (sample_gpu) {
    [encoder writeTimestampWithGranularity:impl_->TimestampGranularity()
                                  intoHeap:impl_->timestamp_heap
                                   atIndex:1];
  }
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
  if (timeout_ms == 0u) {
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
  } else {
    dispatch_time_t timeout =
        dispatch_time(DISPATCH_TIME_NOW,
                      static_cast<int64_t>(timeout_ms) * NSEC_PER_MSEC);
    if (dispatch_semaphore_wait(done, timeout) != 0) {
      if (error) {
        *error = "Metal dispatch timed out";
      }
#if !OS_OBJECT_USE_OBJC
      dispatch_release(done);
#endif
      [options release];
      [cmd release];
      [impl_->allocator reset];
      return false;
    }
  }
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
  if (sample_gpu) {
    impl_->LogGpuTimestampResult("dispatch", grid_size, 1);
  }
  [cmd release];
  [impl_->allocator reset];
  return true;
}

bool MetalRuntime::DispatchBatch(const std::vector<MetalDispatch>& dispatches,
                                 uint32_t grid_size, std::string* error,
                                 uint32_t timeout_ms) {
  if (dispatches.empty()) {
    return true;
  }
  if (!impl_ || !impl_->queue || !impl_->allocator) {
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
  if (impl_->residency_set) {
    [cmd useResidencySet:impl_->residency_set];
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
  bool sample_gpu = impl_->ShouldSampleGpuTimestamps();
  if (sample_gpu) {
    std::string ts_error;
    if (!impl_->EnsureTimestampHeap(2, &ts_error)) {
      if (!ts_error.empty()) {
        std::cerr << "[gpu_profile] " << ts_error << "\n";
      }
      sample_gpu = false;
    }
  }
  if (sample_gpu) {
    [encoder writeTimestampWithGranularity:impl_->TimestampGranularity()
                                  intoHeap:impl_->timestamp_heap
                                   atIndex:0];
  }
  for (const auto& dispatch : dispatches) {
    const MetalKernel* kernel = dispatch.kernel;
    if (!kernel || !kernel->pipeline_) {
      if (error) {
        *error = "Metal kernel unavailable for dispatch";
      }
      [encoder endEncoding];
      [cmd endCommandBuffer];
      [cmd release];
      return false;
    }
    id<MTLComputePipelineState> pipeline =
        (id<MTLComputePipelineState>)kernel->pipeline_;
    [encoder setComputePipelineState:pipeline];
    id<MTL4ArgumentTable> table =
        (id<MTL4ArgumentTable>)kernel->argument_table_;
    const std::vector<MetalBufferBinding>* bindings =
        dispatch.bindings ? dispatch.bindings : nullptr;
    if (bindings && !bindings->empty() && !table) {
      if (error) {
        *error = "Metal 4 argument table unavailable for bindings";
      }
      [encoder endEncoding];
      [cmd endCommandBuffer];
      [cmd release];
      return false;
    }
    if (table) {
      const uint32_t max_bindings = kernel->MaxBufferBindings();
      if (max_bindings > 0u &&
          kernel->last_binding_addresses_.size() != max_bindings) {
        kernel->last_binding_addresses_.assign(
            max_bindings, std::numeric_limits<uint64_t>::max());
      }
      if (bindings) {
        for (const auto& binding : *bindings) {
          if (!binding.buffer || !binding.buffer->handle_) {
            continue;
          }
          if (max_bindings != 0u && binding.index >= max_bindings) {
            if (error) {
              *error = "Metal buffer binding index out of range (index=" +
                       std::to_string(binding.index) + ", max=" +
                       std::to_string(max_bindings - 1u) + ")";
            }
            [encoder endEncoding];
            [cmd endCommandBuffer];
            [cmd release];
            return false;
          }
          if (binding.offset >= binding.buffer->length()) {
            if (error) {
              *error = "Metal buffer binding offset out of range (offset=" +
                       std::to_string(binding.offset) + ", length=" +
                       std::to_string(binding.buffer->length()) + ")";
            }
            [encoder endEncoding];
            [cmd endCommandBuffer];
            [cmd release];
            return false;
          }
          id<MTLBuffer> buffer = (id<MTLBuffer>)binding.buffer->handle_;
          MTLGPUAddress address =
              buffer.gpuAddress + static_cast<MTLGPUAddress>(binding.offset);
          const uint64_t addr_key = static_cast<uint64_t>(address);
          if (max_bindings == 0u ||
              addr_key != kernel->last_binding_addresses_[binding.index]) {
            [table setAddress:address atIndex:binding.index];
            if (max_bindings != 0u) {
              kernel->last_binding_addresses_[binding.index] = addr_key;
            }
          }
        }
      }
      [encoder setArgumentTable:table];
    }
    uint32_t threadgroup = ChooseThreadgroupSize(
        impl_->threadgroup_override, kernel->ThreadExecutionWidth(),
        kernel->MaxThreadsPerThreadgroup());
    MTLSize threads_per_group = MTLSizeMake(threadgroup, 1, 1);
    MTLSize grid = MTLSizeMake(grid_size, 1, 1);
    [encoder dispatchThreads:grid threadsPerThreadgroup:threads_per_group];
  }
  if (sample_gpu) {
    [encoder writeTimestampWithGranularity:impl_->TimestampGranularity()
                                  intoHeap:impl_->timestamp_heap
                                   atIndex:1];
  }
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
  if (timeout_ms == 0u) {
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
  } else {
    dispatch_time_t timeout =
        dispatch_time(DISPATCH_TIME_NOW,
                      static_cast<int64_t>(timeout_ms) * NSEC_PER_MSEC);
    if (dispatch_semaphore_wait(done, timeout) != 0) {
      if (error) {
        *error = "Metal dispatch timed out";
      }
#if !OS_OBJECT_USE_OBJC
      dispatch_release(done);
#endif
      [options release];
      [cmd release];
      [impl_->allocator reset];
      return false;
    }
  }
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
  if (sample_gpu) {
    impl_->LogGpuTimestampResult("dispatch_batch", grid_size,
                                 dispatches.size());
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
  const bool parsed_define = ParseSchedDefineConstants(source, &info);
  if (!parsed_define) {
    ParseUintConst(source, "GPGA_SCHED_PROC_COUNT", &info.proc_count);
    ParseUintConst(source, "GPGA_SCHED_EVENT_COUNT", &info.event_count);
    ParseUintConst(source, "GPGA_SCHED_EDGE_COUNT", &info.edge_count);
    ParseUintConst(source, "GPGA_SCHED_EDGE_STAR_COUNT", &info.edge_star_count);
    ParseUintConst(source, "GPGA_SCHED_REPEAT_COUNT", &info.repeat_count);
    ParseUintConst(source, "GPGA_SCHED_DELAY_COUNT", &info.delay_count);
    ParseUintConst(source, "GPGA_SCHED_MAX_DNBA", &info.max_dnba);
    ParseUintConst(source, "GPGA_SCHED_MONITOR_COUNT", &info.monitor_count);
    ParseUintConst(source, "GPGA_SCHED_MONITOR_MAX_ARGS",
                   &info.monitor_max_args);
    ParseUintConst(source, "GPGA_SCHED_STROBE_COUNT", &info.strobe_count);
    ParseUintConst(source, "GPGA_SCHED_SERVICE_MAX_ARGS",
                   &info.service_max_args);
    ParseUintConst(source, "GPGA_SCHED_SERVICE_WIDE_WORDS",
                   &info.service_wide_words);
    ParseUintConst(source, "GPGA_SCHED_STRING_COUNT", &info.string_count);
    ParseUintConst(source, "GPGA_SCHED_FORCE_COUNT", &info.force_count);
    ParseUintConst(source, "GPGA_SCHED_PCONT_COUNT", &info.pcont_count);
  }
  ParseUintConst(source, "GPGA_SCHED_TIMING_CHECK_COUNT",
                 &info.timing_check_count);
  uint32_t vm_enabled = 0u;
  if (ParseUintConst(source, "GPGA_SCHED_VM_ENABLED", &vm_enabled)) {
    info.vm_enabled = (vm_enabled != 0u);
  }
  ParseUintConst(source, "GPGA_SCHED_VM_BYTECODE_WORDS",
                 &info.vm_bytecode_words);
  ParseUintConst(source, "GPGA_SCHED_VM_COND_COUNT",
                 &info.vm_cond_count);
  ParseUintConst(source, "GPGA_SCHED_VM_CALL_FRAME_WORDS",
                 &info.vm_call_frame_words);
  ParseUintConst(source, "GPGA_SCHED_VM_CALL_FRAME_DEPTH",
                 &info.vm_call_frame_depth);
  ParseUintConst(source, "GPGA_SCHED_VM_CASE_HEADER_COUNT",
                 &info.vm_case_header_count);
  ParseUintConst(source, "GPGA_SCHED_VM_CASE_ENTRY_COUNT",
                 &info.vm_case_entry_count);
  ParseUintConst(source, "GPGA_SCHED_VM_CASE_WORD_COUNT",
                 &info.vm_case_word_count);
  ParseUintConst(source, "GPGA_SCHED_VM_EXPR_WORD_COUNT",
                 &info.vm_expr_word_count);
  ParseUintConst(source, "GPGA_SCHED_VM_EXPR_IMM_WORD_COUNT",
                 &info.vm_expr_imm_word_count);
  ParseUintConst(source, "GPGA_SCHED_VM_SIGNAL_COUNT",
                 &info.vm_signal_count);
  info.has_scheduler = info.proc_count > 0u;
  info.has_services = info.service_max_args > 0u;
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
    signals[MslMangleIdentifier(signal.name)] = signal;
  }
  auto signal_bytes = [&](const SignalInfo& signal) -> size_t {
    uint32_t width = signal.width;
    if (signal.is_real && width < 64u) {
      width = 64u;
    }
    if (width == 0u) {
      width = 1u;
    }
    size_t word_size = (width > 32u) ? sizeof(uint64_t) : sizeof(uint32_t);
    size_t word_count = (width <= 64u) ? 1u : ((width + 63u) / 64u);
    return word_size * word_count;
  };
  auto signal_elements = [&](const SignalInfo& signal) -> size_t {
    uint32_t array_size = signal.array_size > 0 ? signal.array_size : 1u;
    return static_cast<size_t>(instance_count) *
           static_cast<size_t>(array_size);
  };
  auto align8 = [](size_t value) -> size_t {
    return (value + 7u) & ~static_cast<size_t>(7u);
  };
  auto packed_state_bytes = [&]() -> size_t {
    size_t total = 0;
    for (const auto& signal : module.signals) {
      size_t bytes = signal_bytes(signal) * signal_elements(signal);
      total = align8(total);
      total += bytes;
      if (module.four_state) {
        total = align8(total);
        total += bytes;
      }
      if (signal.is_trireg) {
        total = align8(total);
        total += sizeof(uint64_t) * signal_elements(signal);
      }
    }
    if (total == 0) {
      total = 1;
    }
    return total;
  };
  specs->clear();
  const auto& indices = kernel.BufferIndices();
  const bool use_vm_arg_buffer =
      sched.vm_enabled && kernel.HasBuffer("sched_vm_args");
  specs->reserve(indices.size());
  for (const auto& entry : indices) {
    BufferSpec spec;
    spec.name = entry.first;
    const std::string& name = spec.name;
    if (name == "sched_vm_args") {
      continue;
    }
    if (name == "gpga_state") {
      spec.length = packed_state_bytes();
      specs->push_back(spec);
      continue;
    }
    if (name == "nb_state") {
      spec.length = packed_state_bytes();
      specs->push_back(spec);
      continue;
    }
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
      if (StartsWith(name, "sched_vm_") && !sched.vm_enabled) {
        if (error) {
          *error = "scheduler VM buffers requested but VM not enabled";
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
                 name == "sched_error" || name == "sched_status" ||
                 name == "sched_halt_mode") {
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
      } else if (name == "sched_timing_prev_val" ||
                 name == "sched_timing_prev_xz") {
        spec.length = sizeof(uint64_t) * instance_count *
                      sched.timing_check_count * 2u;
      } else if (name == "sched_timing_data_time" ||
                 name == "sched_timing_ref_time" ||
                 name == "sched_timing_window_start" ||
                 name == "sched_timing_window_end") {
        spec.length = sizeof(uint64_t) * instance_count *
                      sched.timing_check_count;
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
      } else if (name == "sched_monitor_wide_val" ||
                 name == "sched_monitor_wide_xz") {
        if (sched.service_wide_words == 0u) {
          if (error) {
            *error = "scheduler wide monitor buffer requested without wide words";
          }
          return false;
        }
        spec.length = sizeof(uint64_t) * instance_count * sched.monitor_count *
                      sched.monitor_max_args * sched.service_wide_words;
      } else if (name == "sched_strobe_pending") {
        spec.length = sizeof(uint32_t) * instance_count * sched.strobe_count;
      } else if (name == "sched_service_count") {
        spec.length = sizeof(uint32_t) * instance_count;
      } else if (name == "sched_service") {
        size_t stride =
            ServiceRecordStride(std::max<uint32_t>(1, sched.service_max_args),
                                sched.service_wide_words, module.four_state);
        spec.length = stride * instance_count * service_capacity;
      } else if (name == "sched_force_id") {
        spec.length = sizeof(uint32_t) * instance_count * sched.force_count;
      } else if (name == "sched_passign_id") {
        spec.length = sizeof(uint32_t) * instance_count * sched.pcont_count;
      } else if (name == "sched_force_state") {
        spec.length = packed_state_bytes();
      } else if (name == "sched_vm_bytecode") {
        if (sched.vm_bytecode_words == 0u) {
          if (error) {
            *error = "sched_vm_bytecode requested without bytecode words";
          }
          return false;
        }
        spec.length =
            sizeof(uint32_t) * instance_count * sched.vm_bytecode_words;
      } else if (name == "sched_vm_cond_val" ||
                 name == "sched_vm_cond_xz") {
        if (sched.vm_cond_count == 0u) {
          if (error) {
            *error = "sched_vm_cond buffers requested without cond sizing";
          }
          return false;
        }
        spec.length = sizeof(uint32_t) * instance_count * sched.proc_count *
                      sched.vm_cond_count;
      } else if (name == "sched_vm_cond_entry") {
        const size_t count =
            (sched.vm_cond_count > 0u) ? sched.vm_cond_count : 1u;
        spec.length = sizeof(uint32_t) * 4u * count;
      } else if (name == "sched_vm_signal_entry") {
        const size_t count =
            (sched.vm_signal_count > 0u) ? sched.vm_signal_count : 1u;
        spec.length = sizeof(uint32_t) * 5u * count;
      } else if (name == "sched_vm_proc_bytecode_offset" ||
                 name == "sched_vm_proc_bytecode_length" ||
                 name == "sched_vm_ip" ||
                 name == "sched_vm_call_sp") {
        spec.length = sizeof(uint32_t) * instance_count * sched.proc_count;
      } else if (name == "sched_vm_call_frame") {
        if (sched.vm_call_frame_words == 0u || sched.vm_call_frame_depth == 0u) {
          if (error) {
            *error = "sched_vm_call_frame requested without frame sizing";
          }
          return false;
        }
        spec.length = sizeof(uint32_t) * instance_count * sched.proc_count *
                      sched.vm_call_frame_words * sched.vm_call_frame_depth;
      } else if (name == "sched_vm_case_header") {
        const size_t count =
            (sched.vm_case_header_count > 0u) ? sched.vm_case_header_count : 1u;
        spec.length = sizeof(uint32_t) * 6u * count;
      } else if (name == "sched_vm_case_entry") {
        const size_t count =
            (sched.vm_case_entry_count > 0u) ? sched.vm_case_entry_count : 1u;
        spec.length = sizeof(uint32_t) * 3u * count;
      } else if (name == "sched_vm_case_words") {
        const size_t count =
            (sched.vm_case_word_count > 0u) ? sched.vm_case_word_count : 1u;
        spec.length = sizeof(uint64_t) * count;
      } else if (name == "sched_vm_expr") {
        const size_t count =
            (sched.vm_expr_word_count > 0u) ? sched.vm_expr_word_count : 1u;
        spec.length = sizeof(uint32_t) * count;
      } else if (name == "sched_vm_expr_imm") {
        const size_t count = (sched.vm_expr_imm_word_count > 0u)
                                 ? sched.vm_expr_imm_word_count
                                 : 1u;
        spec.length = sizeof(uint32_t) * count;
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
  if (use_vm_arg_buffer) {
    auto push_vm = [&](const std::string& name, size_t length) {
      BufferSpec spec;
      spec.name = name;
      spec.length = length;
      specs->push_back(spec);
    };
    if (sched.vm_bytecode_words == 0u) {
      if (error) {
        *error = "sched_vm_bytecode requested without bytecode words";
      }
      return false;
    }
    push_vm("sched_vm_bytecode",
            sizeof(uint32_t) * instance_count * sched.vm_bytecode_words);
    if (sched.vm_cond_count == 0u) {
      if (error) {
        *error = "sched_vm_cond buffers requested without cond sizing";
      }
      return false;
    }
    const size_t cond_len = sizeof(uint32_t) * instance_count *
                            sched.proc_count * sched.vm_cond_count;
    push_vm("sched_vm_cond_val", cond_len);
    push_vm("sched_vm_cond_xz", cond_len);
    {
      const size_t count =
          (sched.vm_cond_count > 0u) ? sched.vm_cond_count : 1u;
      push_vm("sched_vm_cond_entry", sizeof(uint32_t) * 4u * count);
    }
    {
      const size_t count =
          (sched.vm_signal_count > 0u) ? sched.vm_signal_count : 1u;
      push_vm("sched_vm_signal_entry", sizeof(uint32_t) * 5u * count);
    }
    const size_t proc_words =
        sizeof(uint32_t) * instance_count * sched.proc_count;
    push_vm("sched_vm_proc_bytecode_offset", proc_words);
    push_vm("sched_vm_proc_bytecode_length", proc_words);
    push_vm("sched_vm_ip", proc_words);
    push_vm("sched_vm_call_sp", proc_words);
    if (sched.vm_call_frame_words == 0u || sched.vm_call_frame_depth == 0u) {
      if (error) {
        *error = "sched_vm_call_frame requested without frame sizing";
      }
      return false;
    }
    push_vm("sched_vm_call_frame",
            sizeof(uint32_t) * instance_count * sched.proc_count *
                sched.vm_call_frame_words * sched.vm_call_frame_depth);
    {
      const size_t count = (sched.vm_case_header_count > 0u)
                               ? sched.vm_case_header_count
                               : 1u;
      push_vm("sched_vm_case_header", sizeof(uint32_t) * 6u * count);
    }
    {
      const size_t count = (sched.vm_case_entry_count > 0u)
                               ? sched.vm_case_entry_count
                               : 1u;
      push_vm("sched_vm_case_entry", sizeof(uint32_t) * 3u * count);
    }
    {
      const size_t count = (sched.vm_case_word_count > 0u)
                               ? sched.vm_case_word_count
                               : 1u;
      push_vm("sched_vm_case_words", sizeof(uint64_t) * count);
    }
    {
      const size_t count = (sched.vm_expr_word_count > 0u)
                               ? sched.vm_expr_word_count
                               : 1u;
      push_vm("sched_vm_expr", sizeof(uint32_t) * count);
    }
    {
      const size_t count = (sched.vm_expr_imm_word_count > 0u)
                               ? sched.vm_expr_imm_word_count
                               : 1u;
      push_vm("sched_vm_expr_imm", sizeof(uint32_t) * count);
    }
  }
  return true;
}

size_t ServiceRecordStride(uint32_t max_args, uint32_t wide_words,
                           bool has_xz) {
  size_t header = sizeof(uint32_t) * 4u;
  size_t arg_kind = sizeof(uint32_t) * max_args;
  size_t arg_width = sizeof(uint32_t) * max_args;
  size_t arg_val = sizeof(uint64_t) * max_args;
  size_t arg_xz = has_xz ? sizeof(uint64_t) * max_args : 0u;
  size_t arg_wide_val = sizeof(uint64_t) * max_args * wide_words;
  size_t arg_wide_xz = has_xz ? sizeof(uint64_t) * max_args * wide_words : 0u;
  return header + arg_kind + arg_width + arg_val + arg_xz + arg_wide_val +
         arg_wide_xz;
}

ServiceDrainResult DrainSchedulerServices(
    const void* records, uint32_t record_count, uint32_t max_args,
    uint32_t wide_words, bool has_xz, const ServiceStringTable& strings,
    std::ostream& out) {
  ServiceDrainResult result;
  if (!records || record_count == 0 || max_args == 0) {
    return result;
  }
  const auto* base = static_cast<const uint8_t*>(records);
  const size_t stride = ServiceRecordStride(max_args, wide_words, has_xz);
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
    const size_t arg_wide_val_offset =
        arg_xz_offset + (has_xz ? sizeof(uint64_t) * max_args : 0u);
    const size_t arg_wide_xz_offset =
        arg_wide_val_offset +
        sizeof(uint64_t) * max_args * static_cast<size_t>(wide_words);

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
      if (arg.kind == ServiceArgKind::kWide && wide_words > 0u) {
        uint32_t word_count = (arg.width + 63u) / 64u;
        size_t base =
            arg_wide_val_offset +
            sizeof(uint64_t) * (static_cast<size_t>(index) * wide_words);
        arg.wide_value.resize(word_count, 0ull);
        for (uint32_t w = 0; w < word_count; ++w) {
          arg.wide_value[w] =
              ReadU64(rec, base + sizeof(uint64_t) * w);
        }
        if (has_xz) {
          size_t xz_base =
              arg_wide_xz_offset +
              sizeof(uint64_t) * (static_cast<size_t>(index) * wide_words);
          arg.wide_xz.resize(word_count, 0ull);
          for (uint32_t w = 0; w < word_count; ++w) {
            arg.wide_xz[w] =
                ReadU64(rec, xz_base + sizeof(uint64_t) * w);
          }
        }
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
      case ServiceKind::kWrite:
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
        out << line;
        if (kind != ServiceKind::kWrite) {
          out << "\n";
        }
        break;
      }
      case ServiceKind::kSformat:
        out << "$sformat (pid=" << pid << ")\n";
        break;
      case ServiceKind::kTimeformat:
        out << "$timeformat (pid=" << pid << ")\n";
        break;
      case ServiceKind::kPrinttimescale:
        out << "$printtimescale (pid=" << pid << ")\n";
        break;
      case ServiceKind::kTestPlusargs:
        out << "$test$plusargs (pid=" << pid << ")\n";
        break;
      case ServiceKind::kValuePlusargs:
        out << "$value$plusargs (pid=" << pid << ")\n";
        break;
      case ServiceKind::kAsyncAndArray:
      case ServiceKind::kSyncOrPlane:
      case ServiceKind::kAsyncNorPlane:
      case ServiceKind::kSyncNandPlane: {
        const char* label = "$async$and$array";
        switch (kind) {
          case ServiceKind::kSyncOrPlane:
            label = "$sync$or$plane";
            break;
          case ServiceKind::kAsyncNorPlane:
            label = "$async$nor$plane";
            break;
          case ServiceKind::kSyncNandPlane:
            label = "$sync$nand$plane";
            break;
          default:
            break;
        }
        out << label << " (pid=" << pid << ")";
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
      case ServiceKind::kShowcancelled: {
        out << "$showcancelled (pid=" << pid << ")";
        if (arg_count > 0u) {
          out << " delay_id=" << FormatNumeric(args[0], 'h', has_xz);
        }
        if (arg_count > 1u) {
          out << " index=" << FormatNumeric(args[1], 'h', has_xz);
        }
        if (arg_count > 2u) {
          out << " index_xz=" << FormatNumeric(args[2], 'h', has_xz);
        }
        if (arg_count > 3u) {
          out << " time=" << FormatNumeric(args[3], 'd', has_xz);
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
      case ServiceKind::kFtell:
        out << "$ftell (pid=" << pid << ")\n";
        break;
      case ServiceKind::kRewind:
        out << "$rewind (pid=" << pid << ")\n";
        break;
      case ServiceKind::kFseek:
        out << "$fseek (pid=" << pid << ")\n";
        break;
      case ServiceKind::kFflush:
        out << "$fflush (pid=" << pid << ")\n";
        break;
      case ServiceKind::kFerror:
        out << "$ferror (pid=" << pid << ")\n";
        break;
      case ServiceKind::kFungetc:
        out << "$ungetc (pid=" << pid << ")\n";
        break;
      case ServiceKind::kFread:
        out << "$fread (pid=" << pid << ")\n";
        break;
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
      case ServiceKind::kWritememh:
      case ServiceKind::kWritememb: {
        std::string label =
            (kind == ServiceKind::kWritememh) ? "$writememh" : "$writememb";
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
