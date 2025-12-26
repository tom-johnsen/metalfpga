#include "runtime/metal_runtime.hh"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace gpga {

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

}  // namespace

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
