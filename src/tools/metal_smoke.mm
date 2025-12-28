#include "runtime/metal_runtime.hh"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char* kSmokeSource = R"msl(
#include <metal_stdlib>
using namespace metal;

struct GpgaParams { uint count; };

kernel void gpga_smoke(device uint* data [[buffer(0)]],
                       constant GpgaParams& params [[buffer(1)]],
                       uint gid [[thread_position_in_grid]]) {
  if (gid >= params.count) {
    return;
  }
  data[gid] = data[gid] + 1u;
}
)msl";

}  // namespace

int main(int argc, char** argv) {
  uint32_t count = 16u;
  if (argc >= 2) {
    count = static_cast<uint32_t>(std::stoul(argv[1]));
    if (count == 0u) {
      count = 16u;
    }
  }

  gpga::MetalRuntime runtime;
  std::string error;
  if (!runtime.CompileSource(kSmokeSource, {}, &error)) {
    std::cerr << "Metal compile failed: " << error << "\n";
    return 1;
  }

  gpga::MetalKernel kernel;
  if (!runtime.CreateKernel("gpga_smoke", &kernel, &error)) {
    std::cerr << "Kernel creation failed: " << error << "\n";
    return 1;
  }

  std::vector<uint32_t> data(count);
  for (uint32_t i = 0; i < count; ++i) {
    data[i] = i;
  }

  gpga::MetalBuffer data_buf =
      runtime.CreateBuffer(data.size() * sizeof(uint32_t), data.data());
  gpga::GpgaParams params{};
  params.count = count;
  gpga::MetalBuffer params_buf =
      runtime.CreateBuffer(sizeof(params), &params);

  std::vector<gpga::MetalBufferBinding> bindings;
  bindings.push_back({kernel.BufferIndex("data"), &data_buf, 0});
  bindings.push_back({kernel.BufferIndex("params"), &params_buf, 0});

  if (!runtime.Dispatch(kernel, bindings, count, &error)) {
    std::cerr << "Dispatch failed: " << error << "\n";
    return 1;
  }

  auto* out = static_cast<uint32_t*>(data_buf.contents());
  if (!out) {
    std::cerr << "Failed to map output buffer\n";
    return 1;
  }

  std::cout << "Smoke output:";
  for (uint32_t i = 0; i < std::min<uint32_t>(count, 8u); ++i) {
    std::cout << " " << out[i];
  }
  std::cout << "\n";
  return 0;
}
