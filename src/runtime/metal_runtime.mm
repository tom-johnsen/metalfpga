#include "runtime/metal_runtime.hh"

#include <algorithm>
#include <cstring>
#include <dispatch/dispatch.h>
#include <filesystem>
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
  id<MTLDynamicLibrary> real_lib = nil;
  std::string last_source;
  bool prefer_source_bindings = false;
  ~Impl() {
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

std::string FormatNSError(NSError* error);
std::string ExpandIncludes(const std::string& source,
                           const std::vector<std::string>& include_paths,
                           std::string* error);

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
  max_buffer_bindings_ = 0;
  thread_execution_width_ = 0;
  max_threads_per_threadgroup_ = 0;
}

MetalKernel::MetalKernel(MetalKernel&& other) noexcept
    : pipeline_(other.pipeline_),
      argument_table_(other.argument_table_),
      buffer_indices_(std::move(other.buffer_indices_)),
      max_buffer_bindings_(other.max_buffer_bindings_),
      thread_execution_width_(other.thread_execution_width_),
      max_threads_per_threadgroup_(other.max_threads_per_threadgroup_) {
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
  buffer_indices_ = std::move(other.buffer_indices_);
  max_buffer_bindings_ = other.max_buffer_bindings_;
  thread_execution_width_ = other.thread_execution_width_;
  max_threads_per_threadgroup_ = other.max_threads_per_threadgroup_;
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
  if (impl_->prefer_source_bindings) {
    pipe_opts.shaderReflection = static_cast<MTL4ShaderReflection>(0);
  } else {
    pipe_opts.shaderReflection = MTL4ShaderReflectionBindingInfo;
  }
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
    const uint32_t max_bindings = kernel.MaxBufferBindings();
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
  ParseUintConst(source, "GPGA_SCHED_SERVICE_WIDE_WORDS",
                 &info.service_wide_words);
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
  specs->reserve(indices.size());
  for (const auto& entry : indices) {
    BufferSpec spec;
    spec.name = entry.first;
    const std::string& name = spec.name;
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
