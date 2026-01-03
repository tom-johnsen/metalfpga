# Runtime Profile Baseline - test_clock_big_vcd

Command:
`artifacts/profile/test_clock_big_vcd_host artifacts/profile/test_clock_big_vcd.msl --profile`

Runs captured: 14

Summary (ms):
| Metric | Mean | Median | Min | Max |
| --- | --- | --- | --- | --- |
| total | 409.697 | 401.128 | 223.086 | 817.150 |
| read_msl | 24.818 | 13.477 | 8.320 | 148.517 |
| runtime_init | 134.296 | 81.276 | 43.142 | 569.959 |
| compile_source | 103.878 | 86.416 | 54.234 | 300.149 |
| parse_sched | 102.082 | 89.362 | 71.852 | 226.593 |
| build_module_info | 7.757 | 2.146 | 1.866 | 75.421 |
| create_kernels | 3.902 | 2.435 | 1.426 | 14.758 |
| sim_loop | 32.641 | 32.504 | 27.833 | 39.459 |

Notes:
- Outliers observed in `read_msl` (148.517ms), `runtime_init` (569.959ms),
  `compile_source` (300.149ms), `parse_sched` (226.593ms),
  and `build_module_info` (75.421ms). Median values are substantially lower.

Per-run highlights (ms):
| Run | total | runtime_init | compile_source | parse_sched | build_module_info | sim_loop |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 605.124 | 352.799 | 109.095 | 72.800 | 1.985 | 39.459 |
| 2 | 817.150 | 569.959 | 91.502 | 74.438 | 1.866 | 31.587 |
| 3 | 390.206 | 145.548 | 83.133 | 116.913 | 2.426 | 30.825 |
| 4 | 475.624 | 107.922 | 77.066 | 153.866 | 75.421 | 34.118 |
| 5 | 305.924 | 57.300 | 89.698 | 103.380 | 2.337 | 35.932 |
| 6 | 531.459 | 72.296 | 300.149 | 103.865 | 2.129 | 35.384 |
| 7 | 223.086 | 43.142 | 57.359 | 73.610 | 2.126 | 28.953 |
| 8 | 266.975 | 54.416 | 67.195 | 96.962 | 2.083 | 33.666 |
| 9 | 423.761 | 51.405 | 101.515 | 226.593 | 2.163 | 27.833 |
| 10 | 239.499 | 65.523 | 58.034 | 71.852 | 2.000 | 30.983 |
| 11 | 311.865 | 97.451 | 77.453 | 84.056 | 4.825 | 32.760 |
| 12 | 438.892 | 90.230 | 196.940 | 94.668 | 5.028 | 32.248 |
| 13 | 294.136 | 72.323 | 90.925 | 80.233 | 2.270 | 33.186 |
| 14 | 412.050 | 99.831 | 54.234 | 75.906 | 1.942 | 30.038 |

Raw output (verbatim):
```text
[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=13.9349ms total=13.935ms
[profile] runtime_init step=352.799ms total=366.734ms
[profile] compile_source step=109.095ms total=475.829ms
[profile] parse_sched step=72.7996ms total=548.629ms
[profile] build_module_info step=1.98533ms total=550.614ms
[profile] create_kernels step=14.7584ms total=565.372ms
[profile] buffer_specs step=0.087709ms total=565.46ms
[profile] buffer_alloc step=0.18875ms total=565.649ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=39.4585
[profile] done step=39.4752ms total=605.124ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=42.0222ms total=42.0223ms
[profile] runtime_init step=569.959ms total=611.982ms
[profile] compile_source step=91.5016ms total=703.483ms
[profile] parse_sched step=74.438ms total=777.921ms
[profile] build_module_info step=1.86592ms total=779.787ms
[profile] create_kernels step=5.58596ms total=785.373ms
[profile] buffer_specs step=0.085ms total=785.458ms
[profile] buffer_alloc step=0.089ms total=785.547ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=31.5873
[profile] done step=31.6027ms total=817.15ms

[profile] args step=0ms total=0ms
[profile] read_msl step=8.31967ms total=8.31967ms
[profile] runtime_init step=145.548ms total=153.868ms
[profile] compile_source step=83.1333ms total=237.001ms
[profile] parse_sched step=116.913ms total=353.915ms
[profile] build_module_info step=2.42646ms total=356.341ms
[profile] create_kernels step=2.29421ms total=358.636ms
[profile] buffer_specs step=0.183416ms total=358.819ms
[profile] buffer_alloc step=0.53775ms total=359.357ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=30.8254
[profile] done step=30.8493ms total=390.206ms

[profile] args step=0ms total=0ms
[profile] read_msl step=21.7245ms total=21.7245ms
[profile] runtime_init step=107.922ms total=129.647ms
[profile] compile_source step=77.0655ms total=206.712ms
[profile] parse_sched step=153.866ms total=360.579ms
[profile] build_module_info step=75.4212ms total=436ms
[profile] create_kernels step=5.15521ms total=441.155ms
[profile] buffer_specs step=0.08675ms total=441.242ms
[profile] buffer_alloc step=0.209875ms total=441.452ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=34.1177
[profile] done step=34.1724ms total=475.624ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=13.6547ms total=13.6548ms
[profile] runtime_init step=57.3004ms total=70.9552ms
[profile] compile_source step=89.6979ms total=160.653ms
[profile] parse_sched step=103.38ms total=264.033ms
[profile] build_module_info step=2.33721ms total=266.37ms
[profile] create_kernels step=3.14ms total=269.51ms
[profile] buffer_specs step=0.208125ms total=269.718ms
[profile] buffer_alloc step=0.240208ms total=269.959ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=35.9319
[profile] done step=35.9653ms total=305.924ms

[profile] args step=0.000208ms total=0.000208ms
[profile] read_msl step=15.7123ms total=15.7125ms
[profile] runtime_init step=72.2958ms total=88.0083ms
[profile] compile_source step=300.149ms total=388.157ms
[profile] parse_sched step=103.865ms total=492.022ms
[profile] build_module_info step=2.12892ms total=494.151ms
[profile] create_kernels step=1.74296ms total=495.894ms
[profile] buffer_specs step=0.069125ms total=495.963ms
[profile] buffer_alloc step=0.098ms total=496.061ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=35.3839
[profile] done step=35.3986ms total=531.459ms

[profile] args step=0ms total=0ms
[profile] read_msl step=16.1244ms total=16.1244ms
[profile] runtime_init step=43.1423ms total=59.2667ms
[profile] compile_source step=57.3591ms total=116.626ms
[profile] parse_sched step=73.61ms total=190.236ms
[profile] build_module_info step=2.12562ms total=192.361ms
[profile] create_kernels step=1.60838ms total=193.97ms
[profile] buffer_specs step=0.064375ms total=194.034ms
[profile] buffer_alloc step=0.084875ms total=194.119ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=28.9527
[profile] done step=28.9671ms total=223.086ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=10.4105ms total=10.4105ms
[profile] runtime_init step=54.4157ms total=64.8262ms
[profile] compile_source step=67.1953ms total=132.022ms
[profile] parse_sched step=96.962ms total=228.983ms
[profile] build_module_info step=2.08321ms total=231.067ms
[profile] create_kernels step=1.71487ms total=232.782ms
[profile] buffer_specs step=0.22975ms total=233.011ms
[profile] buffer_alloc step=0.272625ms total=233.284ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=33.6664
[profile] done step=33.6915ms total=266.975ms

[profile] args step=0.000125ms total=0.000125ms
[profile] read_msl step=11.4269ms total=11.427ms
[profile] runtime_init step=51.4048ms total=62.8318ms
[profile] compile_source step=101.515ms total=164.347ms
[profile] parse_sched step=226.593ms total=390.939ms
[profile] build_module_info step=2.16304ms total=393.102ms
[profile] create_kernels step=2.57583ms total=395.678ms
[profile] buffer_specs step=0.066042ms total=395.744ms
[profile] buffer_alloc step=0.102166ms total=395.846ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=27.833
[profile] done step=27.914ms total=423.761ms

[profile] args step=0ms total=0ms
[profile] read_msl step=9.41821ms total=9.41821ms
[profile] runtime_init step=65.5225ms total=74.9407ms
[profile] compile_source step=58.0336ms total=132.974ms
[profile] parse_sched step=71.8519ms total=204.826ms
[profile] build_module_info step=2.00013ms total=206.826ms
[profile] create_kernels step=1.52283ms total=208.349ms
[profile] buffer_specs step=0.06475ms total=208.414ms
[profile] buffer_alloc step=0.087458ms total=208.501ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=30.983
[profile] done step=30.9978ms total=239.499ms

[profile] args step=4.1e-05ms total=4.1e-05ms
[profile] read_msl step=9.69929ms total=9.69933ms
[profile] runtime_init step=97.4514ms total=107.151ms
[profile] compile_source step=77.4528ms total=184.604ms
[profile] parse_sched step=84.0565ms total=268.66ms
[profile] build_module_info step=4.82475ms total=273.485ms
[profile] create_kernels step=5.28354ms total=278.768ms
[profile] buffer_specs step=0.075209ms total=278.844ms
[profile] buffer_alloc step=0.242ms total=279.086ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=32.7604
[profile] done step=32.7792ms total=311.865ms

[profile] args step=8.4e-05ms total=8.4e-05ms
[profile] read_msl step=13.2988ms total=13.2989ms
[profile] runtime_init step=90.23ms total=103.529ms
[profile] compile_source step=196.94ms total=300.468ms
[profile] parse_sched step=94.6678ms total=395.136ms
[profile] build_module_info step=5.02833ms total=400.164ms
[profile] create_kernels step=5.99338ms total=406.158ms
[profile] buffer_specs step=0.104375ms total=406.262ms
[profile] buffer_alloc step=0.298ms total=406.56ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=32.2475
[profile] done step=32.3317ms total=438.892ms

[profile] args step=0ms total=0ms
[profile] read_msl step=13.1845ms total=13.1845ms
[profile] runtime_init step=72.3227ms total=85.5073ms
[profile] compile_source step=90.9247ms total=176.432ms
[profile] parse_sched step=80.2333ms total=256.665ms
[profile] build_module_info step=2.2695ms total=258.935ms
[profile] create_kernels step=1.82333ms total=260.758ms
[profile] buffer_specs step=0.075041ms total=260.833ms
[profile] buffer_alloc step=0.092292ms total=260.925ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=33.1865
[profile] done step=33.2102ms total=294.136ms

[profile] args step=4.2e-05ms total=4.2e-05ms
[profile] read_msl step=148.517ms total=148.517ms
[profile] runtime_init step=99.8311ms total=248.348ms
[profile] compile_source step=54.2343ms total=302.583ms
[profile] parse_sched step=75.9056ms total=378.488ms
[profile] build_module_info step=1.94196ms total=380.43ms
[profile] create_kernels step=1.42617ms total=381.856ms
[profile] buffer_specs step=0.06225ms total=381.919ms
[profile] buffer_alloc step=0.078875ms total=381.998ms
$dumpfile "test_clock_big_vcd.vcd" (pid=0)
test_clock_big_vcd.vcd
$finish (pid=1)
[profile] sim_loop ms=30.0384
[profile] done step=30.053ms total=412.05ms
```
