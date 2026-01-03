# Metal 4 Runtime Flags (MetalFPGA)

Runtime and profiling knobs used by the Metal 4 runtime and host harnesses.

## Pipeline archives
| Variable | Purpose | Default |
| --- | --- | --- |
| `METALFPGA_PIPELINE_ARCHIVE` | Override pipeline archive path. | Debug: `artifacts/pipeline_cache/metal4_pipelines.mtl4archive`<br>Release: `~/Library/Caches/metalfpga/metal4_pipelines.mtl4archive` |
| `METALFPGA_PIPELINE_ARCHIVE_LOAD` | Load archive if present. | off |
| `METALFPGA_PIPELINE_ARCHIVE_SAVE` | Save archive on shutdown. | off |
| `METALFPGA_PIPELINE_HARVEST` | Alias for `..._ARCHIVE_SAVE`. | off |
| `METALFPGA_PIPELINE_LOG` | Log archive load/save status. | off |
| `METALFPGA_PIPELINE_ASYNC` | Enable async pipeline precompile tasks. | on |
| `METALFPGA_PIPELINE_PRECOMPILE` | Alias for `..._PIPELINE_ASYNC`. | on |

## Dispatch and residency
| Variable | Purpose | Default |
| --- | --- | --- |
| `METALFPGA_THREADGROUP_SIZE` | Override threadgroup size. | auto |
| `METALFPGA_RESIDENCY_SET` | Enable residency set grouping. | off |

## GPU profiling
| Variable | Purpose | Default |
| --- | --- | --- |
| `METALFPGA_GPU_TIMESTAMPS` | Enable GPU timestamp logging. | off |
| `METALFPGA_GPU_PROFILE` | Alias for `..._GPU_TIMESTAMPS`. | off |
| `METALFPGA_GPU_TIMESTAMPS_PRECISE` | Use precise timestamp granularity. | off |
| `METALFPGA_GPU_TIMESTAMPS_EVERY` | Sample every Nth dispatch. | `1` |

## Host profiling
Use `--profile` on generated host binaries (or the CLI flag) to emit CPU-side
timings with the `[profile]` prefix. The harness script aggregates these logs.

Notes:
- Set `METALFPGA_PIPELINE_ASYNC=0` (or `METALFPGA_PIPELINE_PRECOMPILE=0`) to
  force synchronous compilation.
