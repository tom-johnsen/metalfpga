#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace gpga {

enum class ServiceArgKind : uint32_t {
  kValue = 0u,
  kIdent = 1u,
  kString = 2u,
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
};

struct ServiceStringTable {
  std::vector<std::string> entries;
};

struct ServiceArgView {
  ServiceArgKind kind = ServiceArgKind::kValue;
  uint32_t width = 0;
  uint64_t value = 0;
  uint64_t xz = 0;
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

size_t ServiceRecordStride(uint32_t max_args, bool has_xz);

ServiceDrainResult DrainSchedulerServices(
    const void* records, uint32_t record_count, uint32_t max_args,
    bool has_xz, const ServiceStringTable& strings, std::ostream& out);

}  // namespace gpga
