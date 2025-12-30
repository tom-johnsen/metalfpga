#include <algorithm>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
#include "thirdparty/crlibm/crlibm.h"
}

#define GPGA_REAL_TRACE 1
#include "gpga_real.h"

namespace fs = std::filesystem;

enum class Mode { kRn, kRd, kRu, kRz };
enum class Domain { kAny, kPositive, kNonNegative, kMinusOneToOne, kGreaterMinusOne };

struct UnarySpec {
  std::string name;
  Domain domain = Domain::kAny;
  double (*ref_rn)(double) = nullptr;
  double (*ref_rd)(double) = nullptr;
  double (*ref_ru)(double) = nullptr;
  double (*ref_rz)(double) = nullptr;
  gpga_double (*gpga_rn)(gpga_double) = nullptr;
  gpga_double (*gpga_rd)(gpga_double) = nullptr;
  gpga_double (*gpga_ru)(gpga_double) = nullptr;
  gpga_double (*gpga_rz)(gpga_double) = nullptr;
};

struct BinarySpec {
  std::string name;
  double (*ref_rn)(double, double) = nullptr;
  gpga_double (*gpga_rn)(gpga_double, gpga_double) = nullptr;
};

struct CompareStats {
  uint64_t total = 0;
  uint64_t pass = 0;
  uint64_t fail = 0;
  uint64_t max_ulp = 0;
  long double sum_ulp = 0.0L;
  uint64_t worst_input0 = 0;
  uint64_t worst_input1 = 0;
  uint64_t worst_ref = 0;
  uint64_t worst_got = 0;
};

static uint64_t DoubleToBits(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static double BitsToDouble(uint64_t bits) {
  double value = 0.0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

static bool IsNaNBits(uint64_t bits) {
  return ((bits >> 52) & 0x7FFu) == 0x7FFu &&
         (bits & 0x000FFFFFFFFFFFFFull) != 0;
}

static bool IsInfBits(uint64_t bits) {
  return ((bits >> 52) & 0x7FFu) == 0x7FFu &&
         (bits & 0x000FFFFFFFFFFFFFull) == 0;
}

static uint64_t OrderedBits(uint64_t bits) {
  const uint64_t sign = bits >> 63;
  const uint64_t mask = sign ? 0xFFFFFFFFFFFFFFFFull : 0x8000000000000000ull;
  return bits ^ mask;
}

static uint64_t ULPDiff(uint64_t a, uint64_t b) {
  uint64_t oa = OrderedBits(a);
  uint64_t ob = OrderedBits(b);
  return (oa > ob) ? (oa - ob) : (ob - oa);
}

static bool DomainAccept(Domain domain, uint64_t bits) {
  if (IsNaNBits(bits) || IsInfBits(bits)) {
    return false;
  }
  double v = BitsToDouble(bits);
  if (!std::isfinite(v)) {
    return false;
  }
  switch (domain) {
    case Domain::kAny:
      return true;
    case Domain::kPositive:
      return v > 0.0;
    case Domain::kNonNegative:
      return v >= 0.0;
    case Domain::kMinusOneToOne:
      return v >= -1.0 && v <= 1.0;
    case Domain::kGreaterMinusOne:
      return v > -1.0;
  }
  return false;
}

static std::vector<uint64_t> EdgeInputs() {
  return {
      0x0000000000000000ull,
      0x8000000000000000ull,
      0x0000000000000001ull,
      0x000FFFFFFFFFFFFFull,
      0x0010000000000000ull,
      0x7FEFFFFFFFFFFFFFull,
      0x7FF0000000000000ull,
      0xFFF0000000000000ull,
      0x7FF8000000000000ull,
      0xFFF8000000000000ull,
      DoubleToBits(1.0),
      DoubleToBits(-1.0),
      DoubleToBits(0.5),
      DoubleToBits(-0.5),
      DoubleToBits(2.0),
      DoubleToBits(-2.0),
      DoubleToBits(10.0),
      DoubleToBits(-10.0),
      DoubleToBits(3.14159265358979323846),
  };
}

static uint64_t RandomFinite(std::mt19937_64& rng) {
  uint64_t sign = (rng() & 1ull) << 63;
  uint64_t exp = rng() % 2047ull;
  if (exp == 2047ull) {
    exp = 2046ull;
  }
  uint64_t mant = rng() & 0x000FFFFFFFFFFFFFull;
  return sign | (exp << 52) | mant;
}

static std::vector<uint64_t> MakeUnaryInputs(Domain domain, size_t count,
                                             std::mt19937_64& rng) {
  std::vector<uint64_t> inputs = EdgeInputs();
  std::vector<uint64_t> filtered;
  filtered.reserve(inputs.size());
  for (uint64_t bits : inputs) {
    if (DomainAccept(domain, bits) || IsNaNBits(bits) || IsInfBits(bits)) {
      filtered.push_back(bits);
    }
  }
  while (filtered.size() < count) {
    uint64_t bits = RandomFinite(rng);
    if (DomainAccept(domain, bits)) {
      filtered.push_back(bits);
    }
  }
  return filtered;
}

static std::vector<std::pair<uint64_t, uint64_t>> MakePowInputs(size_t count,
                                                                std::mt19937_64& rng) {
  std::vector<uint64_t> edges = EdgeInputs();
  std::vector<std::pair<uint64_t, uint64_t>> inputs;
  for (uint64_t a : edges) {
    inputs.emplace_back(a, DoubleToBits(2.0));
    inputs.emplace_back(a, DoubleToBits(-2.0));
    inputs.emplace_back(a, DoubleToBits(0.5));
    inputs.emplace_back(a, DoubleToBits(-0.5));
    inputs.emplace_back(a, DoubleToBits(0.0));
    inputs.emplace_back(a, DoubleToBits(1.0));
  }
  while (inputs.size() < count) {
    uint64_t base = RandomFinite(rng);
    uint64_t exp = RandomFinite(rng);
    inputs.emplace_back(base, exp);
  }
  return inputs;
}

static const char* ModeName(Mode mode) {
  switch (mode) {
    case Mode::kRn:
      return "rn";
    case Mode::kRd:
      return "rd";
    case Mode::kRu:
      return "ru";
    case Mode::kRz:
      return "rz";
  }
  return "rn";
}

static bool ParseMode(const std::string& value, Mode* out) {
  if (value == "rn") {
    *out = Mode::kRn;
    return true;
  }
  if (value == "rd") {
    *out = Mode::kRd;
    return true;
  }
  if (value == "ru") {
    *out = Mode::kRu;
    return true;
  }
  if (value == "rz") {
    *out = Mode::kRz;
    return true;
  }
  return false;
}

static void AppendUnary(std::vector<UnarySpec>* specs, UnarySpec spec) {
  specs->push_back(std::move(spec));
}

static void AppendBinary(std::vector<BinarySpec>* specs, BinarySpec spec) {
  specs->push_back(std::move(spec));
}

static std::vector<UnarySpec> BuildUnarySpecs() {
  std::vector<UnarySpec> specs;
  AppendUnary(&specs, {"exp", Domain::kAny, exp_rn, exp_rd, exp_ru, exp_rz,
                       gpga_exp_rn, gpga_exp_rd, gpga_exp_ru, gpga_exp_rz});
  AppendUnary(&specs, {"log", Domain::kPositive, log_rn, log_rd, log_ru, log_rz,
                       gpga_log_rn, gpga_log_rd, gpga_log_ru, gpga_log_rz});
  AppendUnary(&specs, {"log2", Domain::kPositive, log2_rn, log2_rd, log2_ru, log2_rz,
                       gpga_log2_rn, gpga_log2_rd, gpga_log2_ru, gpga_log2_rz});
  AppendUnary(&specs, {"log10", Domain::kPositive, log10_rn, log10_rd, log10_ru, log10_rz,
                       gpga_log10_rn, gpga_log10_rd, gpga_log10_ru, gpga_log10_rz});
  AppendUnary(&specs, {"log1p", Domain::kGreaterMinusOne, log1p_rn, log1p_rd, log1p_ru, log1p_rz,
                       gpga_log1p_rn, gpga_log1p_rd, gpga_log1p_ru, gpga_log1p_rz});
  AppendUnary(&specs, {"expm1", Domain::kAny, expm1_rn, expm1_rd, expm1_ru, expm1_rz,
                       gpga_expm1_rn, gpga_expm1_rd, gpga_expm1_ru, gpga_expm1_rz});
  AppendUnary(&specs, {"sin", Domain::kAny, sin_rn, sin_rd, sin_ru, sin_rz,
                       gpga_sin_rn, gpga_sin_rd, gpga_sin_ru, gpga_sin_rz});
  AppendUnary(&specs, {"cos", Domain::kAny, cos_rn, cos_rd, cos_ru, cos_rz,
                       gpga_cos_rn, gpga_cos_rd, gpga_cos_ru, gpga_cos_rz});
  AppendUnary(&specs, {"tan", Domain::kAny, tan_rn, tan_rd, tan_ru, tan_rz,
                       gpga_tan_rn, gpga_tan_rd, gpga_tan_ru, gpga_tan_rz});
  AppendUnary(&specs, {"asin", Domain::kMinusOneToOne, asin_rn, asin_rd, asin_ru, asin_rz,
                       gpga_asin_rn, gpga_asin_rd, gpga_asin_ru, gpga_asin_rz});
  AppendUnary(&specs, {"acos", Domain::kMinusOneToOne, acos_rn, acos_rd, acos_ru, acos_rz,
                       gpga_acos_rn, gpga_acos_rd, gpga_acos_ru, gpga_acos_rz});
  AppendUnary(&specs, {"atan", Domain::kAny, atan_rn, atan_rd, atan_ru, atan_rz,
                       gpga_atan_rn, gpga_atan_rd, gpga_atan_ru, gpga_atan_rz});
  AppendUnary(&specs, {"sinh", Domain::kAny, sinh_rn, sinh_rd, sinh_ru, sinh_rz,
                       gpga_sinh_rn, gpga_sinh_rd, gpga_sinh_ru, gpga_sinh_rz});
  AppendUnary(&specs, {"cosh", Domain::kAny, cosh_rn, cosh_rd, cosh_ru, cosh_rz,
                       gpga_cosh_rn, gpga_cosh_rd, gpga_cosh_ru, gpga_cosh_rz});
  AppendUnary(&specs, {"sinpi", Domain::kAny, sinpi_rn, sinpi_rd, sinpi_ru, sinpi_rz,
                       gpga_sinpi_rn, gpga_sinpi_rd, gpga_sinpi_ru, gpga_sinpi_rz});
  AppendUnary(&specs, {"cospi", Domain::kAny, cospi_rn, cospi_rd, cospi_ru, cospi_rz,
                       gpga_cospi_rn, gpga_cospi_rd, gpga_cospi_ru, gpga_cospi_rz});
  AppendUnary(&specs, {"tanpi", Domain::kAny, tanpi_rn, tanpi_rd, tanpi_ru, tanpi_rz,
                       gpga_tanpi_rn, gpga_tanpi_rd, gpga_tanpi_ru, gpga_tanpi_rz});
  AppendUnary(&specs, {"asinpi", Domain::kMinusOneToOne, asinpi_rn, asinpi_rd, asinpi_ru, asinpi_rz,
                       gpga_asinpi_rn, gpga_asinpi_rd, gpga_asinpi_ru, gpga_asinpi_rz});
  AppendUnary(&specs, {"acospi", Domain::kMinusOneToOne, acospi_rn, acospi_rd, acospi_ru, acospi_rz,
                       gpga_acospi_rn, gpga_acospi_rd, gpga_acospi_ru, gpga_acospi_rz});
  AppendUnary(&specs, {"atanpi", Domain::kAny, atanpi_rn, atanpi_rd, atanpi_ru, atanpi_rz,
                       gpga_atanpi_rn, gpga_atanpi_rd, gpga_atanpi_ru, gpga_atanpi_rz});
  return specs;
}

static std::vector<BinarySpec> BuildBinarySpecs() {
  std::vector<BinarySpec> specs;
  AppendBinary(&specs, {"pow", pow_rn, gpga_pow_rn});
  return specs;
}

static double (*SelectRef(const UnarySpec& spec, Mode mode))(double) {
  switch (mode) {
    case Mode::kRn:
      return spec.ref_rn;
    case Mode::kRd:
      return spec.ref_rd;
    case Mode::kRu:
      return spec.ref_ru;
    case Mode::kRz:
      return spec.ref_rz ? spec.ref_rz : spec.ref_rd;
  }
  return nullptr;
}

static gpga_double (*SelectGpga(const UnarySpec& spec, Mode mode))(gpga_double) {
  switch (mode) {
    case Mode::kRn:
      return spec.gpga_rn;
    case Mode::kRd:
      return spec.gpga_rd;
    case Mode::kRu:
      return spec.gpga_ru;
    case Mode::kRz:
      return spec.gpga_rz ? spec.gpga_rz : spec.gpga_rd;
  }
  return nullptr;
}

static std::string HexU64(uint64_t value) {
  std::ostringstream oss;
  oss << "0x" << std::hex << std::setfill('0') << std::setw(16) << value;
  return oss.str();
}

static bool EnsureDir(const fs::path& dir) {
  std::error_code ec;
  if (fs::exists(dir, ec)) {
    return true;
  }
  return fs::create_directories(dir, ec);
}

int main(int argc, char** argv) {
  size_t count = 10000;
  uint64_t seed = 1;
  std::string func_arg = "all";
  std::string mode_arg = "rn";
  std::string out_dir;
  bool trace = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--count" && i + 1 < argc) {
      count = static_cast<size_t>(std::stoull(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--func" && i + 1 < argc) {
      func_arg = argv[++i];
    } else if (arg == "--mode" && i + 1 < argc) {
      mode_arg = argv[++i];
    } else if (arg == "--out-dir" && i + 1 < argc) {
      out_dir = argv[++i];
    } else if (arg == "--trace") {
      trace = true;
    } else if (arg == "--help") {
      std::cout << "Usage: metalfpga_crlibm_compare [--func list|all] "
                   "[--mode rn|rd|ru|rz|all] [--count N] [--seed N] "
                   "[--out-dir path] [--trace]\n";
      return 0;
    }
  }

  if (out_dir.empty()) {
    std::random_device rd;
    uint64_t tag_seed =
        (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
    if (tag_seed == 0) {
      tag_seed = static_cast<uint64_t>(
          std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }
    uint32_t tag = static_cast<uint32_t>(tag_seed ^ (tag_seed >> 32));
    std::ostringstream oss;
    oss << "artifacts/real_ulp/" << std::hex << std::setw(8) << std::setfill('0')
        << tag;
    out_dir = oss.str();
  }

  std::mt19937_64 rng(seed);
  fs::path out_path(out_dir);
  if (!EnsureDir(out_path)) {
    std::cerr << "failed to create output dir: " << out_dir << "\n";
    return 1;
  }

  if (trace) {
    gpga_real_trace_reset();
  }

  std::ofstream csv;
  if (trace) {
    csv.open(out_path / "results.csv");
    if (!csv) {
      std::cerr << "failed to open results.csv\n";
      return 1;
    }
    csv << "func,mode,input0,input1,ref,got,ulp,status\n";
  }

  std::vector<UnarySpec> unary_specs = BuildUnarySpecs();
  std::vector<BinarySpec> binary_specs = BuildBinarySpecs();

  std::vector<Mode> modes;
  if (mode_arg == "all") {
    modes = {Mode::kRn, Mode::kRd, Mode::kRu, Mode::kRz};
  } else {
    Mode m;
    if (!ParseMode(mode_arg, &m)) {
      std::cerr << "unknown mode: " << mode_arg << "\n";
      return 1;
    }
    modes = {m};
  }

  std::unordered_map<std::string, CompareStats> stats;

  auto want_func = [&](const std::string& name) -> bool {
    if (func_arg == "all") {
      return true;
    }
    std::istringstream iss(func_arg);
    std::string token;
    while (std::getline(iss, token, ',')) {
      if (token == name) {
        return true;
      }
    }
    return false;
  };

  const unsigned long long fpu_token = crlibm_init();

  for (const auto& spec : unary_specs) {
    if (!want_func(spec.name)) {
      continue;
    }
    for (Mode mode : modes) {
      auto ref = SelectRef(spec, mode);
      auto gpga = SelectGpga(spec, mode);
      if (!ref || !gpga) {
        continue;
      }
      std::vector<uint64_t> inputs = MakeUnaryInputs(spec.domain, count, rng);
      CompareStats stat;
      for (uint64_t in_bits : inputs) {
        double ref_val = ref(BitsToDouble(in_bits));
        uint64_t ref_bits = DoubleToBits(ref_val);
        gpga_double gpga_bits = gpga(static_cast<gpga_double>(in_bits));
        uint64_t got_bits = static_cast<uint64_t>(gpga_bits);

        uint64_t ulp = 0;
        bool ok = false;
        std::string status = "ok";
        if (IsNaNBits(ref_bits) && IsNaNBits(got_bits)) {
          ok = true;
        } else if (IsInfBits(ref_bits) || IsInfBits(got_bits)) {
          ok = (ref_bits == got_bits);
          if (!ok) {
            status = "inf_mismatch";
            ulp = std::numeric_limits<uint64_t>::max();
          }
        } else {
          ulp = ULPDiff(ref_bits, got_bits);
          ok = (ulp == 0);
          if (!ok) {
            status = "ulp_mismatch";
          }
        }
        stat.total += 1;
        if (ok) {
          stat.pass += 1;
        } else {
          stat.fail += 1;
          if (ulp > stat.max_ulp) {
            stat.max_ulp = ulp;
            stat.worst_input0 = in_bits;
            stat.worst_input1 = 0;
            stat.worst_ref = ref_bits;
            stat.worst_got = got_bits;
          }
        }
        stat.sum_ulp += static_cast<long double>(ulp);
        if (trace) {
          csv << spec.name << "," << ModeName(mode) << ","
              << HexU64(in_bits) << ","
              << "0x0000000000000000"
              << "," << HexU64(ref_bits) << "," << HexU64(got_bits) << ","
              << ulp << "," << status << "\n";
        }
      }
      std::string key = spec.name + ":" + ModeName(mode);
      stats[key] = stat;
    }
  }

  for (const auto& spec : binary_specs) {
    if (!want_func(spec.name)) {
      continue;
    }
    std::vector<std::pair<uint64_t, uint64_t>> inputs =
        MakePowInputs(count, rng);
    CompareStats stat;
    for (const auto& pair : inputs) {
      uint64_t a_bits = pair.first;
      uint64_t b_bits = pair.second;
      double ref_val = spec.ref_rn(BitsToDouble(a_bits), BitsToDouble(b_bits));
      uint64_t ref_bits = DoubleToBits(ref_val);
      gpga_double gpga_bits =
          spec.gpga_rn(static_cast<gpga_double>(a_bits),
                       static_cast<gpga_double>(b_bits));
      uint64_t got_bits = static_cast<uint64_t>(gpga_bits);
      uint64_t ulp = 0;
      bool ok = false;
      std::string status = "ok";
      if (IsNaNBits(ref_bits) && IsNaNBits(got_bits)) {
        ok = true;
      } else if (IsInfBits(ref_bits) || IsInfBits(got_bits)) {
        ok = (ref_bits == got_bits);
        if (!ok) {
          status = "inf_mismatch";
          ulp = std::numeric_limits<uint64_t>::max();
        }
      } else {
        ulp = ULPDiff(ref_bits, got_bits);
        ok = (ulp == 0);
        if (!ok) {
          status = "ulp_mismatch";
        }
      }
      stat.total += 1;
      if (ok) {
        stat.pass += 1;
      } else {
        stat.fail += 1;
        if (ulp > stat.max_ulp) {
          stat.max_ulp = ulp;
          stat.worst_input0 = a_bits;
          stat.worst_input1 = b_bits;
          stat.worst_ref = ref_bits;
          stat.worst_got = got_bits;
        }
      }
      stat.sum_ulp += static_cast<long double>(ulp);
      if (trace) {
        csv << spec.name << ",rn," << HexU64(a_bits) << "," << HexU64(b_bits)
            << "," << HexU64(ref_bits) << "," << HexU64(got_bits) << "," << ulp
            << "," << status << "\n";
      }
    }
    stats[spec.name + ":rn"] = stat;
  }

  crlibm_exit(fpu_token);

  std::ofstream summary(out_path / "summary.json");
  summary << "{\n";
  summary << "  \"seed\": " << seed << ",\n";
  summary << "  \"count\": " << count << ",\n";
  summary << "  \"results\": [\n";
  bool first = true;
  for (const auto& entry : stats) {
    if (!first) {
      summary << ",\n";
    }
    first = false;
    const CompareStats& stat = entry.second;
    long double avg = stat.total ? (stat.sum_ulp / stat.total) : 0.0L;
    summary << "    {\n";
    summary << "      \"id\": \"" << entry.first << "\",\n";
    summary << "      \"total\": " << stat.total << ",\n";
    summary << "      \"pass\": " << stat.pass << ",\n";
    summary << "      \"fail\": " << stat.fail << ",\n";
    summary << "      \"max_ulp\": " << stat.max_ulp << ",\n";
    summary << "      \"avg_ulp\": " << std::fixed << std::setprecision(6)
            << static_cast<double>(avg) << ",\n";
    summary << "      \"worst_input0\": \"" << HexU64(stat.worst_input0)
            << "\",\n";
    summary << "      \"worst_input1\": \"" << HexU64(stat.worst_input1)
            << "\",\n";
    summary << "      \"worst_ref\": \"" << HexU64(stat.worst_ref) << "\",\n";
    summary << "      \"worst_got\": \"" << HexU64(stat.worst_got) << "\"\n";
    summary << "    }";
  }
  summary << "\n  ]";
  if (trace) {
    const auto& counters = gpga_real_trace_counters();
    summary << ",\n  \"trace\": {\n";
    summary << "    \"sin_rn_fallback\": " << counters.sin_rn_fallback << ",\n";
    summary << "    \"sin_ru_fallback\": " << counters.sin_ru_fallback << ",\n";
    summary << "    \"sin_rd_fallback\": " << counters.sin_rd_fallback << ",\n";
    summary << "    \"sin_rz_fallback\": " << counters.sin_rz_fallback << ",\n";
    summary << "    \"cos_rn_fallback\": " << counters.cos_rn_fallback << ",\n";
    summary << "    \"cos_ru_fallback\": " << counters.cos_ru_fallback << ",\n";
    summary << "    \"cos_rd_fallback\": " << counters.cos_rd_fallback << ",\n";
    summary << "    \"cos_rz_fallback\": " << counters.cos_rz_fallback << ",\n";
    summary << "    \"tan_rn_fallback\": " << counters.tan_rn_fallback << ",\n";
    summary << "    \"tan_ru_fallback\": " << counters.tan_ru_fallback << ",\n";
    summary << "    \"tan_rd_fallback\": " << counters.tan_rd_fallback << ",\n";
    summary << "    \"tan_rz_fallback\": " << counters.tan_rz_fallback << "\n";
    summary << "  }";
  }
  summary << "\n";
  summary << "}\n";

  std::cout << "ULP comparison complete. Results in " << out_dir << "\n";
  return 0;
}
