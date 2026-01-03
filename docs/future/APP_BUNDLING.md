# App Bundling: HDL to Native macOS Applications

## Vision

**metalfpga can compile Verilog/SystemVerilog HDL directly into standalone native macOS applications.**

This transforms HDL cores (NES, Game Boy, arcade machines, custom processors, etc.) into distributable Mac apps that run cycle-accurate RTL simulations on the GPU.

---

## Use Cases

### 1. **Retro Gaming Cores**
Turn FPGA console/arcade cores into native Mac apps:
- NES/SNES/Genesis cores → Standalone emulators
- Arcade machine cores → Preservation-grade implementations
- Handheld console cores → Portable gaming

**Example:**
```bash
metalfpga nes_core.v --bundle NES.app --icon assets/nes_icon.icns
```

### 2. **Educational Tools**
Package custom processors and teaching designs:
- RISC-V cores → Interactive CPU simulators
- Pipeline visualizers → Educational debugging tools
- Custom ISAs → Architecture exploration apps

### 3. **Research Demonstrations**
Turn research HDL into shareable demos:
- Novel architectures → Portable benchmarks
- Accelerator designs → Performance showcases
- Verification testbenches → Validation tools

### 4. **Hardware Prototyping**
Distribute chip simulations before tapeout:
- ASIC validation → Client-ready test harnesses
- IP core demos → Sales engineering tools
- Co-simulation → Multi-team collaboration

---

## Architecture

### macOS App Bundle Structure

```
YourCore.app/
├── Contents/
│   ├── Info.plist                  # App metadata
│   ├── MacOS/
│   │   └── YourCore                # Host executable (C++/Obj-C/Swift)
│   ├── Resources/
│   │   ├── core.msl                # Generated MSL source (optional, for debugging)
│   │   ├── metal4_pipelines.mtl4archive  # PRE-COMPILED Metal pipelines (critical!)
│   │   ├── icon.icns               # App icon
│   │   ├── MainMenu.nib            # GUI resources (optional)
│   │   └── Data/                   # ROMs, save states, config
│   └── Frameworks/                 # Embedded dependencies (optional)
```

### Component Layers

#### 1. **Generated Runtime** (metalfpga output)
The exact code Codex already generates:
- **MSL source** - Generated Metal Shading Language code (core.msl)
- **Pipeline archive** - Pre-compiled Metal pipelines (metal4_pipelines.mtl4archive)
  - **Critical for distribution** - Eliminates 5-10 minute compilation on first launch
  - Like Icarus Verilog's .vvp files, but native GPU code
  - Load instantly (~5 seconds) instead of recompiling every launch
- **Host runtime** - Buffer management, scheduler, service system
- **VCD export** - Optional debugging/waveform capture

#### 2. **App-Specific Glue Code** (metalfpga generates or user-provided)
Bridges HDL signals to macOS APIs:
- **Input mapping** - Map NSEvent/controllers → Verilog input signals
- **Output mapping** - Map Verilog video/audio → Metal textures/CoreAudio
- **File I/O** - Load ROMs, manage save states
- **UI** - AppKit/SwiftUI frontend for controls

#### 3. **macOS Framework Integration**
- **Metal** - GPU execution (already used)
- **AppKit/SwiftUI** - GUI (window, menus, controls)
- **CoreGraphics** - Rendering pipeline
- **CoreAudio** - Audio output
- **GameController** - HID input

---

## Implementation Phases

### Phase 1: Basic App Generation
**Goal:** Generate minimal runnable `.app` bundle

**Inputs:**
```bash
# Build pipeline archive first (one-time, slow)
metalfpga core.v --emit-msl core.msl --emit-host core_host.cc
clang++ core_host.cc -o core_sim
METALFPGA_PIPELINE_HARVEST=1 ./core_sim core.msl  # Creates metal4_pipelines.mtl4archive

# Then bundle into app (fast, includes pre-compiled archive)
metalfpga core.v --bundle MyCore.app --pipeline-archive metal4_pipelines.mtl4archive
```

**Outputs:**
- Standalone macOS app with:
  - **Pre-compiled Metal pipeline archive** (instant load on launch)
  - MSL source (optional, for debugging)
  - Host executable with GUI stub
  - Basic window displaying simulation state

**Generated Components:**
1. **Info.plist** - App metadata (bundle ID, version, icon)
2. **Host executable** - Minimal Cocoa app loading pre-compiled pipelines
3. **Resource packaging** - Bundle MSL source + pipeline archive + assets
4. **Pipeline archive** - metal4_pipelines.mtl4archive (enables instant startup)

### Phase 2: Signal Binding DSL
**Goal:** Declarative mapping of HDL signals to app I/O

**Example Config:**
```yaml
# core_bindings.yaml
inputs:
  reset: key.R
  controller1_a: gamepad.button_a
  controller1_start: key.Return

outputs:
  video:
    signal: vga_rgb[23:0]
    format: RGB888
    resolution: 256x240
    scale: 3x
    refresh: 60Hz

  audio:
    signal: audio_out[15:0]
    format: S16
    sample_rate: 48000
```

**Generated Glue Code:**
- Event loop mapping keyboard/gamepad → Verilog inputs
- Metal texture rendering from Verilog video signals
- CoreAudio buffer feeding from Verilog audio signals

### Phase 3: GUI Templates
**Goal:** Pre-built UI templates for common use cases

**Templates:**
- **Emulator Style** - ROM picker, screen view, controller config
- **Debugger Style** - Waveform viewer, register inspector, breakpoints
- **Benchmark Style** - Performance metrics, throughput graphs
- **Minimal** - Just a rendering surface (for fullscreen apps)

**Example:**
```bash
metalfpga nes.v --bundle NES.app --template emulator --video ppu_rgb --audio apu_out
```

### Phase 4: Advanced Features
- **Multi-instance** - Run N copies of core in parallel (benchmark mode)
- **Save states** - Snapshot/restore entire simulation state
- **Rewind** - Time-travel debugging
- **Network** - Link cable / multiplayer support
- **Plugin API** - User-provided C++/Swift code hooks

---

## Example: NES Core → NES.app

### Input Verilog
```verilog
// nes_top.v - Simplified NES core
module nes_top(
  input wire clk,
  input wire reset,
  input wire [7:0] controller1,
  output wire [23:0] video_rgb,
  output wire hsync, vsync,
  output wire [15:0] audio_out
);
  // ... NES implementation ...
endmodule
```

### Binding Config
```yaml
# nes_bindings.yaml
clock: 21.477272MHz  # NES master clock

inputs:
  reset: key.R
  controller1[7]: gamepad.button_a      # A button
  controller1[6]: gamepad.button_b      # B button
  controller1[4]: gamepad.dpad_up       # D-pad up
  controller1[5]: gamepad.dpad_down
  controller1[6]: gamepad.dpad_left
  controller1[7]: gamepad.dpad_right
  controller1[3]: key.Return            # Start
  controller1[2]: key.RightShift        # Select

outputs:
  video:
    signals: [video_rgb, hsync, vsync]
    format: RGB888
    resolution: 256x240
    scale: 3x

  audio:
    signal: audio_out
    format: S16
    sample_rate: 48000
```

### Build Command
```bash
# Step 1: Generate and harvest pipelines (one-time, ~5-10 min for NES)
metalfpga nes_top.v --emit-msl nes.msl --emit-host nes_host.cc
clang++ nes_host.cc -o nes_sim -framework Metal -framework Foundation
METALFPGA_PIPELINE_HARVEST=1 ./nes_sim nes.msl
# ^ Creates metal4_pipelines.mtl4archive

# Step 2: Bundle into distributable app (fast, <1 min)
metalfpga nes_top.v \
  --bindings nes_bindings.yaml \
  --bundle NES.app \
  --pipeline-archive metal4_pipelines.mtl4archive \
  --template emulator \
  --icon assets/nes_icon.icns \
  --name "NES Emulator" \
  --identifier com.example.nes
```

### Generated App Features
- **ROM loading** - Drag-and-drop `.nes` files
- **Controller support** - Auto-detect USB/Bluetooth gamepads
- **Save states** - F5 = save, F7 = load
- **Fullscreen** - Cmd+F toggles
- **Performance overlay** - FPS, frame time, GPU utilization
- **VCD export** - Debug menu option

---

## Technical Details

### Signal Mapping Implementation

#### Input Path (macOS → Verilog)
```cpp
// Generated glue code in host executable
class InputController {
  uint8_t controller1_state = 0;

  void handleKeyDown(NSEvent* event) {
    switch (event.keyCode) {
      case kVK_Return:
        controller1_state |= (1 << 3);  // Start button
        break;
      // ... more mappings ...
    }
  }

  void updateSimulation() {
    // Write to Metal buffer bound to Verilog input
    input_buffer->controller1 = controller1_state;
  }
};
```

#### Output Path (Verilog → macOS)
```cpp
// Video rendering
class VideoRenderer {
  id<MTLTexture> frameTexture;

  void updateFrame() {
    // Read from Metal buffer bound to Verilog output
    uint32_t* pixels = (uint32_t*)video_buffer->video_rgb;

    // Upload to Metal texture for display
    [frameTexture replaceRegion:region
                    mipmapLevel:0
                      withBytes:pixels
                    bytesPerRow:256 * 4];
  }
};
```

### Clock Management

**Problem:** Verilog clock runs at MHz, macOS display at 60Hz

**Solution:** Time dilation in simulation loop
```cpp
const double NES_CLOCK = 21.477272e6;  // Hz
const double FRAME_RATE = 60.0;        // Hz
const uint64_t CYCLES_PER_FRAME = NES_CLOCK / FRAME_RATE;  // ~357,954

void simulationLoop() {
  while (!shouldStop) {
    @autoreleasepool {
      // Run simulation for one frame's worth of cycles
      runMetalKernel(CYCLES_PER_FRAME);

      // Update display (throttled to 60Hz by CADisplayLink)
      renderFrame();
    }
  }
}
```

### Memory Mapping

**Direct Buffer Access (Unified Memory):**
```cpp
// Verilog `reg [23:0] video_rgb [0:61439]` (256x240 framebuffer)
// Maps to Metal buffer accessible from both CPU and GPU

id<MTLBuffer> videoBuffer = [device newBufferWithLength:256*240*4
                                               options:MTLResourceStorageModeShared];

// GPU writes via Verilog simulation
// CPU reads for texture upload (zero-copy on Apple Silicon)
```

---

## Distribution

### Code Signing
```bash
codesign --deep --force --verify --verbose \
  --sign "Developer ID Application: Your Name" \
  YourCore.app
```

### Notarization
```bash
xcrun notarytool submit YourCore.app.zip \
  --apple-id you@example.com \
  --team-id TEAMID \
  --wait
```

### Distribution Channels
1. **Direct download** - Host `.dmg` on website
2. **Mac App Store** - Requires sandbox entitlements
3. **Homebrew Cask** - `brew install --cask yourcore`
4. **GitHub Releases** - Automated CI/CD builds

---

## Performance Considerations

### GPU Utilization
- **Single instance** - May underutilize GPU (NES ~357k cycles/frame)
- **Multi-instance** - Run 100+ NES cores in parallel for benchmarks
- **Optimization** - Batch multiple frames per kernel dispatch

### Latency
- **Target** - Sub-frame latency (<16ms for 60Hz)
- **Challenge** - GPU→CPU readback adds ~1-2ms
- **Mitigation** - Triple buffering, async command encoding

### Power Efficiency
- **Apple Silicon advantage** - Unified memory = no PCIe bottleneck
- **Dynamic clocking** - Metal adjusts GPU frequency based on load
- **Thermal management** - Monitor via `IOKit` power metrics

---

## Competitive Landscape

### Existing Solutions
- **MAME** - CPU-based, not cycle-accurate for FPGA cores
- **RetroArch** - Plugin architecture, not native Metal
- **Analogue Pocket** - Hardware FPGA, not software distribution
- **MiSTer** - Linux/DE10-Nano, not macOS

### metalfpga Advantages
1. **Cycle-accurate** - It's the actual RTL, not an emulation layer
2. **GPU-native** - Metal performance on Apple Silicon
3. **Distributable** - Standalone apps, no FPGA hardware required
4. **Portable** - Same Verilog runs on actual FPGA or macOS
5. **Debuggable** - VCD export, waveform viewing built-in

---

## Roadmap

### v1.0 - Basic Bundling
- [ ] Generate minimal `.app` structure
- [ ] Embed Metal library and host executable
- [ ] Basic window with Metal view
- [ ] Command-line `--bundle` flag

### v1.5 - Signal Binding
- [ ] YAML binding DSL parser
- [ ] Generate input/output glue code
- [ ] Support keyboard, mouse, gamepad inputs
- [ ] Support video texture and audio buffer outputs

### v2.0 - Templates
- [ ] Emulator template (ROM picker, save states)
- [ ] Debugger template (waveforms, registers)
- [ ] Benchmark template (multi-instance, metrics)
- [ ] Minimal template (just rendering surface)

### v2.5 - Advanced Features
- [ ] Save state snapshot/restore
- [ ] Rewind buffer (ring buffer of states)
- [ ] Network link cable emulation
- [ ] Performance profiling overlay

### v3.0 - Ecosystem
- [ ] App store for community cores
- [ ] Cloud compilation service
- [ ] IDE integration (Xcode plugin)
- [ ] Automated testing harness

---

## Example Gallery

### Potential Apps Built with metalfpga

**Gaming:**
- NES Emulator
- Game Boy / Game Boy Color
- Sega Genesis / Mega Drive
- Neo Geo MVS
- Arcade cores (Pac-Man, Galaga, Donkey Kong, etc.)

**Education:**
- RISC-V Visualizer
- 6502 Debugger
- MIPS Pipeline Explorer
- Custom CPU Architecture Lab

**Research:**
- Neural Network Accelerator Demo
- Custom Crypto Engine Benchmark
- Novel Cache Architecture Simulator

**Professional:**
- ASIC Validation Suite
- IP Core Sales Demo
- Pre-Silicon Co-Verification Tool

---

## Marketing Tagline

> **"From RTL to App Store in one command."**

**metalfpga: Compile hardware descriptions into native macOS applications. Zero emulation. Pure Metal. Cycle-accurate.**

---

## Next Steps

1. **Prototype** - Build minimal NES.app proof-of-concept
2. **Validate** - Confirm Metal kernel can drive 60Hz display
3. **Design DSL** - Finalize signal binding syntax
4. **Generate Templates** - Create SwiftUI scaffolding
5. **Polish UX** - Make bundling seamless and ergonomic

This feature transforms metalfpga from a **compiler** into a **platform** for distributing hardware simulations as consumer software. 🚀
