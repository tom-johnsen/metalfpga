# Metal 4 Pipeline Harvesting (MetalFPGA)

Short guide for capturing and reusing Metal 4 pipeline archives to avoid
first-run stutter.

## Quick start
1) Build or emit the host binary and MSL as usual.
2) Run a workload with harvesting enabled:
   - `METALFPGA_PIPELINE_ARCHIVE_SAVE=1`
   - Optional: `METALFPGA_PIPELINE_ARCHIVE_LOAD=1`
   - Optional: `METALFPGA_PIPELINE_LOG=1`
3) Let the run finish cleanly so the serializer can flush the archive.

## Default archive paths
- Debug: `artifacts/pipeline_cache/metal4_pipelines.mtl4archive`
- Release: `~/Library/Caches/metalfpga/metal4_pipelines.mtl4archive`

Override with `METALFPGA_PIPELINE_ARCHIVE=/path/to/file.mtl4archive`.

## Load vs save
- `METALFPGA_PIPELINE_ARCHIVE_LOAD=1` loads an existing archive (if present).
- `METALFPGA_PIPELINE_ARCHIVE_SAVE=1` (or `METALFPGA_PIPELINE_HARVEST=1`)
  saves a new archive when the process shuts down.

## Async precompile
To front-load compilation work during startup:
- `METALFPGA_PIPELINE_ASYNC=1` or `METALFPGA_PIPELINE_PRECOMPILE=1`

This queues background pipeline compilation tasks so the simulation loop
does not block on JIT work.

To disable async precompile, set `METALFPGA_PIPELINE_ASYNC=0`.
