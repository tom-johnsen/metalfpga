#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

namespace gpga {

inline bool IsMslIdentStart(char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return std::isalpha(uc) != 0 || c == '_';
}

inline bool IsMslIdentChar(char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return std::isalnum(uc) != 0 || c == '_';
}

inline uint64_t Fnv1aHash64(std::string_view value) {
  uint64_t hash = 14695981039346656037ull;
  for (unsigned char c : value) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

inline std::string Hex64(uint64_t value) {
  static const char kHex[] = "0123456789abcdef";
  std::string out(16, '0');
  for (int i = 15; i >= 0; --i) {
    out[static_cast<size_t>(i)] = kHex[value & 0xfull];
    value >>= 4;
  }
  return out;
}

inline bool IsMslReservedIdentifier(std::string_view name) {
  static const std::unordered_set<std::string_view> kReserved = {
      "alignas", "alignof", "and", "and_eq", "asm", "atomic", "auto", "bitand",
      "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t",
      "class", "compl", "const", "constant", "constexpr", "const_cast",
      "continue",
      "decltype", "default", "delete", "device", "do", "double", "dynamic_cast",
      "else", "enum", "explicit", "export", "extern", "false", "float", "for",
      "friend", "goto", "half", "if", "inline", "int", "kernel", "long",
      "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr",
      "operator", "or", "or_eq", "private", "protected", "public", "register",
      "reinterpret_cast", "return", "sampler", "short", "signed", "sizeof",
      "static", "static_assert", "static_cast", "struct", "switch", "template",
      "texture", "this", "thread", "threadgroup", "threadgroup_imageblock",
      "typedef", "typeid", "typename", "uint", "ulong", "union", "unsigned",
      "using", "vertex", "fragment", "stage_in", "buffer", "virtual", "void",
      "volatile", "wchar_t", "while", "xor", "xor_eq"};
  return kReserved.find(name) != kReserved.end();
}

inline bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

inline std::string MslMangleIdentifier(std::string_view name) {
  bool needs_escape = name.empty();
  if (!needs_escape) {
    if (!IsMslIdentStart(name.front())) {
      needs_escape = true;
    } else {
      for (char c : name) {
        if (!IsMslIdentChar(c)) {
          needs_escape = true;
          break;
        }
      }
    }
  }
  if (!needs_escape) {
    if (IsMslReservedIdentifier(name) || StartsWith(name, "__gpga_") ||
        StartsWith(name, "gpga_")) {
      needs_escape = true;
    }
  }
  if (!needs_escape) {
    return std::string(name);
  }
  std::string sanitized;
  sanitized.reserve(name.size());
  for (char c : name) {
    sanitized.push_back(IsMslIdentChar(c) ? c : '_');
  }
  if (sanitized.empty()) {
    sanitized = "id";
  }
  if (!IsMslIdentStart(sanitized.front())) {
    sanitized.insert(sanitized.begin(), '_');
  }
  uint64_t hash = Fnv1aHash64(name);
  return "__gpga_u_" + sanitized + "_" + Hex64(hash);
}

}  // namespace gpga
