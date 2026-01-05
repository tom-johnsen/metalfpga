# Runtime Profile Post-REV40 - test_clock_big_vcd

Command:
`artifacts/profile/test_clock_big_vcd_host artifacts/profile/test_clock_big_vcd.msl --profile`

Runs captured: 9

Summary (ms):
| Metric | Mean | Median | Min | Max |
| --- | --- | --- | --- | --- |
| total | 207.329 | 209.324 | 196.605 | 211.287 |
| read_msl | 8.703 | 7.217 | 6.894 | 13.343 |
| runtime_init | 42.034 | 43.682 | 35.847 | 45.882 |
| compile_source | 53.083 | 52.781 | 51.694 | 54.954 |
| parse_sched | 68.205 | 67.763 | 67.280 | 69.550 |
| build_module_info | 1.892 | 1.884 | 1.809 | 2.062 |
| create_kernels | 1.405 | 1.390 | 1.350 | 1.480 |
| sim_loop | 31.526 | 31.871 | 29.200 | 33.186 |

Notes:
- Much more consistent performance compared to pre-REV40 baseline
- No significant outliers observed
- All metrics show tight clustering around median values
- **47.8% improvement in median total time** (401.128ms → 209.324ms)

Per-run highlights (ms):
| Run | total | runtime_init | compile_source | parse_sched | build_module_info | sim_loop |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 211.287 | 45.555 | 54.954 | 67.280 | 1.878 | 33.186 |
| 2 | 210.663 | 45.284 | 52.547 | 69.523 | 1.950 | 32.651 |
| 3 | 204.557 | 37.434 | 52.510 | 69.550 | 1.815 | 31.872 |
| 4 | 196.605 | 35.847 | 53.448 | 67.451 | 2.062 | 29.302 |
| 5 | 204.434 | 39.660 | 53.682 | 68.023 | 1.820 | 29.200 |
| 6 | 207.063 | 45.882 | 51.694 | 67.408 | 1.817 | 31.543 |
| 7 | 209.324 | 44.398 | 53.713 | 67.623 | 1.809 | 33.060 |
| 8 | 210.626 | 40.888 | 52.442 | 67.763 | 1.974 | 32.611 |
| 9 | 210.405 | 43.682 | 52.782 | 69.192 | 1.884 | 31.270 |

Raw output (verbatim):
```text
[profile] args step=4.1e-05ms total=4.1e-05ms
[profile] read_msl step=6.89367ms total=6.89371ms
[profile] runtime_init step=45.5549ms total=52.4486ms
[profile] compile_source step=54.9538ms total=107.402ms
[profile] parse_sched step=67.2798ms total=174.682ms
[profile] build_module_info step=1.87825ms total=176.56ms
[profile] create_kernels step=1.39008ms total=177.951ms
[profile] buffer_specs step=0.056208ms total=178.007ms
[profile] buffer_alloc step=0.080958ms total=178.088ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=33.1863
[profile] done step=33.1998ms total=211.287ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=7.19221ms total=7.19225ms
[profile] runtime_init step=45.2844ms total=52.4767ms
[profile] compile_source step=52.5466ms total=105.023ms
[profile] parse_sched step=69.5228ms total=174.546ms
[profile] build_module_info step=1.95025ms total=176.496ms
[profile] create_kernels step=1.36171ms total=177.858ms
[profile] buffer_specs step=0.059333ms total=177.917ms
[profile] buffer_alloc step=0.077209ms total=177.995ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=32.6512
[profile] done step=32.6689ms total=210.663ms

[profile] args step=0ms total=0ms
[profile] read_msl step=9.77333ms total=9.77333ms
[profile] runtime_init step=37.4335ms total=47.2068ms
[profile] compile_source step=52.5097ms total=99.7165ms
[profile] parse_sched step=69.5502ms total=169.267ms
[profile] build_module_info step=1.81462ms total=171.081ms
[profile] create_kernels step=1.45608ms total=172.537ms
[profile] buffer_specs step=0.059666ms total=172.597ms
[profile] buffer_alloc step=0.0745ms total=172.672ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=31.8716
[profile] done step=31.8855ms total=204.557ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=6.99621ms total=6.99625ms
[profile] runtime_init step=35.8468ms total=42.843ms
[profile] compile_source step=53.4477ms total=96.2907ms
[profile] parse_sched step=67.4513ms total=163.742ms
[profile] build_module_info step=2.06175ms total=165.804ms
[profile] create_kernels step=1.35017ms total=167.154ms
[profile] buffer_specs step=0.05725ms total=167.211ms
[profile] buffer_alloc step=0.075875ms total=167.287ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=29.3018
[profile] done step=29.3177ms total=196.605ms

[profile] args step=0ms total=0ms
[profile] read_msl step=10.5216ms total=10.5216ms
[profile] runtime_init step=39.6595ms total=50.1811ms
[profile] compile_source step=53.6821ms total=103.863ms
[profile] parse_sched step=68.023ms total=171.886ms
[profile] build_module_info step=1.82033ms total=173.706ms
[profile] create_kernels step=1.37029ms total=175.077ms
[profile] buffer_specs step=0.057083ms total=175.134ms
[profile] buffer_alloc step=0.077667ms total=175.212ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=29.2004
[profile] done step=29.2223ms total=204.434ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=7.21725ms total=7.21729ms
[profile] runtime_init step=45.882ms total=53.0992ms
[profile] compile_source step=51.6935ms total=104.793ms
[profile] parse_sched step=67.4079ms total=172.201ms
[profile] build_module_info step=1.81737ms total=174.018ms
[profile] create_kernels step=1.35775ms total=175.376ms
[profile] buffer_specs step=0.055584ms total=175.431ms
[profile] buffer_alloc step=0.075458ms total=175.507ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=31.5425
[profile] done step=31.5557ms total=207.063ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=7.19767ms total=7.19771ms
[profile] runtime_init step=44.3975ms total=51.5952ms
[profile] compile_source step=53.7127ms total=105.308ms
[profile] parse_sched step=67.6228ms total=172.931ms
[profile] build_module_info step=1.809ms total=174.74ms
[profile] create_kernels step=1.37271ms total=176.112ms
[profile] buffer_specs step=0.056458ms total=176.169ms
[profile] buffer_alloc step=0.081209ms total=176.25ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=33.0597
[profile] done step=33.0735ms total=209.324ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=13.3427ms total=13.3427ms
[profile] runtime_init step=40.888ms total=54.2307ms
[profile] compile_source step=52.4415ms total=106.672ms
[profile] parse_sched step=67.7628ms total=174.435ms
[profile] build_module_info step=1.97388ms total=176.409ms
[profile] create_kernels step=1.44833ms total=177.857ms
[profile] buffer_specs step=0.061541ms total=177.919ms
[profile] buffer_alloc step=0.082042ms total=178.001ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=32.6114
[profile] done step=32.6251ms total=210.626ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=9.99854ms total=9.99858ms
[profile] runtime_init step=43.6821ms total=53.6807ms
[profile] compile_source step=52.7815ms total=106.462ms
[profile] parse_sched step=69.1924ms total=175.655ms
[profile] build_module_info step=1.88408ms total=177.539ms
[profile] create_kernels step=1.448ms total=178.987ms
[profile] buffer_specs step=0.057ms total=179.044ms
[profile] buffer_alloc step=0.077208ms total=179.121ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=31.27
[profile] done step=31.284ms total=210.405ms
```
