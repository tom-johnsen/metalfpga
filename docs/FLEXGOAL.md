# FLEXGOAL.md — The Ultimate Demonstration

## Vision

Once metalfpga reaches GPU execution maturity, the ultimate flex demonstration will be:

**Running a complete NES (Nintendo Entertainment System) emulator compiled from Verilog RTL to Metal shaders, executing on Apple GPU hardware.**

Not a software emulator. Not an approximation. **Actual hardware description language compiled to GPU compute kernels.**

---

## The Chain

```
NES Hardware Specification
    ↓
Verilog RTL Implementation (6502 CPU + PPU + APU)
    ↓
metalfpga Compiler (parse → elaborate → flatten)
    ↓
Metal Shading Language Compute Kernels
    ↓
Apple GPU Execution (M-series or AMD)
    ↓
Playable NES Games (Super Mario Bros, Zelda, Metroid, etc.)
```

---

## Why This Matters

### Technical Validation

- **Proves completeness**: NES cores use real-world RTL patterns metalfpga must handle
- **Proves performance**: GPU parallelism must handle ~10K lines of Verilog at interactive framerates
- **Proves correctness**: Cycle-accurate timing, edge case handling, multi-module hierarchy

### Marketing Impact

> "We compiled Super Mario Bros to Metal shaders. Your move, Nvidia."

This isn't a toy demo. It's a statement:
- metalfpga handles production-grade RTL
- GPU-based FPGA simulation is viable
- Verilog-to-Metal compilation is real

---

## Requirements

### Verilog Features Needed

All of these are already implemented or planned:

- ✅ Module hierarchy (NES has 10+ modules)
- ✅ Sequential logic (`always @(posedge clk)` for CPU/PPU state machines)
- ✅ Memory arrays (`reg [7:0] vram [0:2047]` for video RAM)
- ✅ Case statements (CPU instruction decode)
- ✅ Signed arithmetic (APU waveform generation)
- ✅ Bit-slicing and concatenation (register packing/unpacking)
- ✅ Parameters (clock dividers, timing constants)
- 🔜 Real number arithmetic (for audio sample rate conversion - nice to have)

### System Integration Needed

- **ROM loading**: `$readmemh` to load game ROMs (already supported)
- **Video output**: PPU frame buffer → Metal texture → display
- **Audio output**: APU samples → CoreAudio
- **Controller input**: Keyboard/gamepad → Verilog input ports
- **Timing**: ~60 Hz frame rate, ~1.79 MHz CPU clock (scaled or real-time)

### metalfpga Milestones Required

1. ✅ Verilog-2005 completeness (in progress)
2. ✅ Host emitter (next milestone)
3. ⏳ GPU kernel execution (after host emitter)
4. ⏳ Service record infrastructure (`$readmemh`, `$display`)
5. ⏳ Performance profiling and optimization
6. 🎯 NES RTL compilation and execution

---

## Existing NES Cores (Open Source)

Several high-quality NES Verilog implementations exist:

- **fpgaNES** (https://github.com/strigeus/fpgaNES)
- **NesMini** (https://github.com/Redherring32/NesMini)
- **NES_MiSTer** (https://github.com/MiSTer-devel/NES_MiSTer)

These are ~5K-15K lines of Verilog, well-documented, and proven on FPGA hardware.

**Legal note**: All are open-source (GPL/MIT). Game ROMs require legal ownership.

---

## Technical Challenges

### CPU (6502)

- **Instruction decode**: Large case statement (56 opcodes)
- **Address modes**: Indexed, indirect, zero-page
- **Cycle accuracy**: Some instructions take 2-7 cycles
- **Interrupts**: NMI (vblank), IRQ (mapper/APU)

**metalfpga readiness**: ✅ All features supported

### PPU (Picture Processing Unit)

- **Dual playfields**: Background + sprites
- **Sprite 0 hit detection**: Classic timing edge case
- **Mid-scanline register writes**: Requires fine-grained timing
- **Scrolling**: 2D nametable stitching

**metalfpga readiness**: ✅ Sequential logic, memory arrays, bit manipulation all work

### APU (Audio Processing Unit)

- **5 channels**: Pulse x2, triangle, noise, DMC
- **Waveform generation**: Duty cycle, linear feedback shift register
- **Sample output**: 44.1 kHz or 48 kHz reconstruction

**metalfpga readiness**: ⚠️ May need real number arithmetic for sample rate math (optional, can be precomputed)

### Mappers

- **Memory bank switching**: MMC1, MMC3, UxROM, etc.
- **Expansion hardware**: Extra RAM, IRQ counters

**metalfpga readiness**: ✅ Parameters and conditional generate blocks handle this

---

## Demo Scenarios

### Milestone 1: CPU Only

- Load a simple test ROM
- Execute instructions
- `$display` register state each cycle
- Verify against known-good trace

**Visual output**: Terminal log
**Validation**: Instruction trace matches reference

### Milestone 2: CPU + PPU (No Rendering)

- Full NES core running
- PPU generates vblank interrupts
- CPU executes game logic
- `$dumpvars` VCD waveform output

**Visual output**: VCD timing diagram
**Validation**: Frame timing matches spec (60 Hz vblank)

### Milestone 3: Full Emulation

- PPU frame buffer extracted each frame
- Metal texture updated at 60 Hz
- Controller input mapped to Verilog ports
- Audio samples sent to CoreAudio

**Visual output**: Playable Super Mario Bros on macOS
**Validation**: Human plays the game, it works

---

## Performance Targets

### Real-time Execution

- **NES CPU clock**: 1.79 MHz
- **PPU pixel clock**: 5.37 MHz
- **Frame rate**: 60 Hz (16.67 ms per frame)

**GPU advantage**: Combinational logic can be massively parallel. Sequential blocks are serialized by design, but GPU can handle thousands of signal updates simultaneously.

**Expectation**: Modern Apple GPUs (M1/M2/M3) should handle this comfortably, possibly at **faster than real-time**.

### Profiling Points

- Kernel dispatch overhead per cycle
- Memory bandwidth (VRAM/PRG-ROM reads)
- Service record frequency (`$display` spam)
- VCD dump I/O (if enabled)

---

## Debugging Arsenal

When (not if) bugs appear:

1. **VCD waveforms**: `$dumpvars` → GTKWave visualization
2. **Metal Frame Capture**: GPU timeline, buffer inspection
3. **Instruction trace**: `$display` CPU state each cycle
4. **Reference comparison**: Known-good emulator (Mesen, FCEUX) trace diff
5. **nestest ROM**: Standard CPU validation suite

---

## Success Criteria

### Technical

- [ ] NES core compiles without errors
- [ ] CPU executes all 56 opcodes correctly
- [ ] PPU renders frames at 60 Hz
- [ ] Audio plays without glitches
- [ ] Controller input responsive
- [ ] Passes nestest CPU validation ROM
- [ ] Runs at real-time or faster

### Demonstration

- [ ] Video recording: Super Mario Bros running via metalfpga
- [ ] Blog post: "How I Compiled NES Hardware to GPU Shaders"
- [ ] Conference talk: "GPU-Based FPGA Simulation at Scale"
- [ ] GitHub release: `examples/nes/` with ROM loader and display

---

## Marketing Angle

### Headline

**"Super Mario Bros, Compiled to Metal: A GPU-Based NES Emulator from Verilog RTL"**

### Key Points

- Not a software emulator — actual hardware description compiled to shaders
- Proves Verilog-to-GPU compilation is viable for real designs
- Demonstrates metalfpga handles production-grade RTL (10K+ lines)
- Shows GPU parallelism advantage over CPU-based simulators
- Open-source, reproducible, and educational

### Target Audience

- FPGA engineers (looking for faster prototyping)
- Retro gaming enthusiasts (novel emulation approach)
- GPU compute researchers (unusual use case for Metal)
- Compiler developers (Verilog → MSL is underexplored)

---

## Timeline (Post-GPU Execution)

1. **Month 1**: Compile simple NES core, verify CPU instruction execution
2. **Month 2**: Integrate PPU, render test patterns
3. **Month 3**: Full system integration, controller input, audio output
4. **Month 4**: Performance profiling, optimization, polish
5. **Month 5**: Public demo, blog post, video release

**Contingency**: If too complex, fall back to simpler targets:
- Chip-8 interpreter (256 bytes RAM, 35 opcodes)
- Game Boy (similar complexity, better documentation)
- Custom RISC-V core (modern, well-specified)

---

## Fallback Flex Targets

If NES proves too ambitious initially:

### Chip-8 (Easiest)

- **Complexity**: ~500 lines of Verilog
- **Features**: 35 instructions, 64x32 display, 16 keys
- **Demo**: Pong, Space Invaders, Tetris clones
- **Validation**: Chip-8 test ROMs widely available

### Game Boy (Medium)

- **Complexity**: ~8K lines of Verilog
- **Features**: Modified Z80 CPU, tile-based PPU, 4-channel audio
- **Demo**: Tetris, Pokemon Red (if ambitious)
- **Validation**: Blargg test ROMs, Mooneye test suite

### RISC-V Core (Educational)

- **Complexity**: ~3K lines for RV32I
- **Features**: Simple ISA, no legacy quirks
- **Demo**: Bare-metal "Hello World" UART output
- **Validation**: RISC-V compliance tests

---

## Closing Thought

This isn't just a flex. It's a **proof of concept** that GPU-based hardware simulation is:

- **Viable** for real designs
- **Fast** enough for interactive use
- **Accessible** via high-level compilation (Verilog → Metal)

And it's **fun** — because who doesn't want to see Super Mario running on compute shaders?

---

**Status**: 🎯 Vision defined, waiting for GPU execution milestone
**ETA**: Post-v1.0 (Verilog-2005 complete + host emitter + kernel validation)
**Difficulty**: High, but achievable
**Flex Level**: Maximum
