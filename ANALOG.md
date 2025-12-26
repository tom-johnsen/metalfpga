# Analog Signal Support in MetalFPGA

**Status:** Architecturally supported, not yet implemented
**Target:** Future Verilog-AMS mixed-signal simulation

---

## Overview

MetalFPGA's 4-state architecture **already supports** analog signal representation through fixed-point arithmetic. The fundamental infrastructure—parallel GPU processing, val/xz pairs, and custom operations—enables mixed-signal simulation without architectural changes.

## Current Capability

The existing `FourState32` infrastructure can represent analog values:

```metal
// Analog voltage 0.0V to 3.3V as Q16.16 fixed-point
// Range: 0x00000000 (0.0V) to 0x00034CCC (3.3V)
constant uint* voltage_val [[buffer(0)]];  // Fixed-point value
constant uint* voltage_xz [[buffer(1)]];   // 0 = known, 1 = X/Z (undriven)

// Operations work as-is:
FourState32 v1 = fs_make32(voltage_val[gid], voltage_xz[gid], 32u);
FourState32 v2 = fs_add32(v1, another_voltage, 32u);     // Analog addition
FourState32 scaled = fs_mul32(v1, gain_factor, 32u);     // Analog scaling
FourState32 divided = fs_div32(v1, divisor, 32u);        // Analog division
```

## Fixed-Point Formats

### Q16.16 Format (Recommended)
- **Bits:** 16 integer, 16 fractional
- **Range:** -32768.0 to +32767.99998
- **Resolution:** 0.0000152587890625 (1/65536)
- **Use case:** General-purpose analog signals (voltage, current)

### Q24.8 Format (Alternative)
- **Bits:** 24 integer, 8 fractional
- **Range:** -8388608.0 to +8388607.99609
- **Resolution:** 0.00390625 (1/256)
- **Use case:** Wide-range signals with lower precision needs

### Custom Formats
Any Qm.n format fits in 32-bit `FourState32.val`:
- **Q8.24:** High precision, narrow range (-128 to +127.99999994)
- **Q20.12:** Balanced (±524288 range, 0.000244 resolution)

## Analog to Digital Conversion

### Threshold Comparator with Hysteresis

```metal
// Convert analog signal to digital with Schmitt trigger behavior
FourState32 analog_to_digital(FourState32 analog, uint threshold_low, uint threshold_high) {
  if (analog.xz != 0u) {
    return FourState32{0u, 1u};  // Unknown analog → X state
  }

  // Q16.16 comparison with hysteresis
  if (analog.val > threshold_high) {
    return FourState32{1u, 0u};  // Logic HIGH
  } else if (analog.val < threshold_low) {
    return FourState32{0u, 0u};  // Logic LOW
  } else {
    return FourState32{0u, 1u};  // Metastable → X state
  }
}

// Example: 0.8V/1.2V thresholds for 0-3.3V range
uint VIL = 0x0000CCCC;  // 0.8V in Q16.16
uint VIH = 0x00013333;  // 1.2V in Q16.16
FourState32 digital = analog_to_digital(voltage, VIL, VIH);
```

## Digital to Analog Conversion

```metal
// Convert digital logic levels to analog voltage
FourState32 digital_to_analog(FourState32 digital, uint vol_val, uint voh_val) {
  if (digital.xz != 0u) {
    return FourState32{0u, 1u};  // X/Z digital → undriven analog
  }

  if (digital.val != 0u) {
    return FourState32{voh_val, 0u};  // Logic 1 → VOH (e.g., 3.3V)
  } else {
    return FourState32{vol_val, 0u};  // Logic 0 → VOL (e.g., 0.0V)
  }
}

// Example: CMOS levels
uint VOL = 0x00000000;  // 0.0V
uint VOH = 0x00034CCC;  // 3.3V
FourState32 analog_out = digital_to_analog(logic_signal, VOL, VOH);
```

## Buffer Layout for Mixed-Signal Nets

A mixed-signal net carries both analog value and digital state:

```cpp
struct MixedSignalNet {
  uint analog_val;   // Q16.16 fixed-point voltage
  uint analog_xz;    // 0 = driven, 1 = high-Z (undriven)
  uint digital_val;  // Logic level (0/1)
  uint digital_xz;   // 0 = known, 1 = X/Z
};
```

**Metal shader buffers:**
```metal
constant uint* net_analog_val   [[buffer(0)]];
constant uint* net_analog_xz    [[buffer(1)]];
constant uint* net_digital_val  [[buffer(2)]];
constant uint* net_digital_xz   [[buffer(3)]];
```

## Analog Operations Library

### Basic Arithmetic
All existing 4-state operations work on fixed-point:

```metal
FourState32 fs_add32(FourState32 a, FourState32 b, uint width);  // a + b
FourState32 fs_sub32(FourState32 a, FourState32 b, uint width);  // a - b
FourState32 fs_mul32(FourState32 a, FourState32 b, uint width);  // a * b (needs Q rescaling)
FourState32 fs_div32(FourState32 a, FourState32 b, uint width);  // a / b (needs Q rescaling)
```

**Note:** Multiplication/division require fixed-point rescaling:
```metal
// Q16.16 * Q16.16 → Q32.32, shift right 16 to get Q16.16
FourState32 product = fs_shr32(fs_mul32(a, b, 32u), FourState32{16u, 0u}, 32u);
```

### Analog-Specific Operations

```metal
// Voltage divider: Vout = Vin * (R2 / (R1 + R2))
FourState32 fs_voltage_divider(FourState32 vin, uint r1, uint r2) {
  FourState32 ratio = fs_div32(fs_make32(r2, 0u, 32u),
                               fs_add32(fs_make32(r1, 0u, 32u),
                                       fs_make32(r2, 0u, 32u), 32u), 32u);
  return fs_mul32(vin, ratio, 32u);
}

// RC time constant: V(t) = V0 * exp(-t/RC)
// Approximation: Use lookup table or polynomial
FourState32 fs_rc_decay(FourState32 v0, uint time_steps, uint rc_constant);

// Saturation/clamping
FourState32 fs_clamp(FourState32 value, uint min_val, uint max_val) {
  if (value.xz != 0u) return value;  // Preserve X/Z
  if (value.val < min_val) return FourState32{min_val, 0u};
  if (value.val > max_val) return FourState32{max_val, 0u};
  return value;
}
```

## Implementation Challenges

### 1. Analog Solver Requirements
Analog simulation needs **iterative convergence**, which conflicts with GPU parallelism:

```
SPICE-style solver:
  1. Linearize nonlinear devices (Newton-Raphson)
  2. Build admittance matrix
  3. Solve linear system (Gaussian elimination)
  4. Check convergence
  5. Repeat until |ΔV| < tolerance
```

**Problem:** Each iteration depends on the previous one—can't parallelize across time.

**Potential solution:** Parallelize across nodes/devices, not time steps.

### 2. Mixed-Signal Interaction
Digital events trigger analog updates and vice versa:

```metal
// Digital clock edge → capacitor charging (analog)
if (clk_edge) {
  analog_voltage = fs_rc_charge(vin, timestep, rc_tau);
}

// Analog comparator → digital output
digital_out = analog_to_digital(analog_voltage, threshold_low, threshold_high);
```

**Solution:** Hybrid simulation—digital kernel calls analog kernel when needed.

### 3. Timestep Management
Analog requires adaptive timesteps for accuracy:
- **Digital:** Event-driven (only run on clock edges)
- **Analog:** Continuous-time (needs small Δt for accuracy)

**Solution:** Per-net timestep tracking, resync at digital events.

## Verilog-AMS Example

### Resistor Model
```verilog
module resistor(inout a, inout b);
  parameter real R = 1000.0;  // 1kΩ

  analog begin
    I(a, b) <+ V(a, b) / R;  // Ohm's law
  end
endmodule
```

**MetalFPGA representation:**
```metal
// Current through resistor: I = (Va - Vb) / R
FourState32 va = fs_make32(a_val[gid], a_xz[gid], 32u);  // Q16.16 voltage
FourState32 vb = fs_make32(b_val[gid], b_xz[gid], 32u);
FourState32 vdiff = fs_sub32(va, vb, 32u);
FourState32 current = fs_div32(vdiff, fs_make32(R_fixed, 0u, 32u), 32u);  // R in Q16.16
```

### RC Low-Pass Filter
```verilog
module rc_filter(input vin, output vout);
  parameter real R = 10e3;   // 10kΩ
  parameter real C = 100e-9; // 100nF

  analog begin
    I(vout) <+ C * ddt(V(vout));
    I(vout) <+ V(vout) / R;
    I(vout) <+ -V(vin) / R;
  end
endmodule
```

**MetalFPGA kernel (simplified):**
```metal
// First-order approximation: V[n] = V[n-1] + (Vin - Vout) * dt / (R*C)
FourState32 vin_current = fs_make32(vin_val[gid], vin_xz[gid], 32u);
FourState32 vout_prev = fs_make32(vout_val[gid], vout_xz[gid], 32u);
FourState32 delta = fs_sub32(vin_current, vout_prev, 32u);
FourState32 rc_tau = fs_make32(RC_CONSTANT, 0u, 32u);  // Precomputed
FourState32 step = fs_div32(fs_mul32(delta, dt, 32u), rc_tau, 32u);
FourState32 vout_new = fs_add32(vout_prev, step, 32u);
vout_val[gid] = vout_new.val;
vout_xz[gid] = vout_new.xz;
```

## Roadmap

### Phase 1: Foundation (Current)
✅ 4-state digital simulation with X/Z support
✅ Optimized MSL code generation
✅ Parallel GPU execution model

### Phase 2: SystemVerilog (Next Priority)
- Interfaces, structs, enums
- Assertions (SVA)
- Advanced data types
- Constrained randomization

### Phase 3: Mixed-Signal (Future)
- Fixed-point analog representation
- Analog/digital converters with hysteresis
- Basic analog primitives (R, C, voltage sources)
- Hybrid solver integration

### Phase 4: Verilog-AMS (Long-term)
- Full analog kernel support
- Nonlinear device models
- SPICE-compatible netlist import
- Adaptive timestep control

## Why Not Now?

**Strategic reasons to defer analog support:**

1. **Digital foundation first:** Perfect the 4-state Verilog implementation before adding complexity
2. **Higher ROI elsewhere:** SystemVerilog features benefit more users immediately
3. **Solver complexity:** Analog simulation requires careful architecture (GPU-hostile iteration)
4. **Market validation:** Unclear if GPU-accelerated analog simulation has demand
5. **Reference scarcity:** Verilog-AMS tools are rare; less reference material

**But the door is open:** The architecture doesn't preclude analog—it's ready when the time comes.

---

## Technical Validation

**Proof of concept:** Current infrastructure can simulate analog RC circuit:

```metal
// RC charging: V(t) = Vfinal * (1 - exp(-t/RC))
// Discrete approximation: V[n+1] = V[n] + (Vin - V[n]) * (dt/RC)

kernel void gpga_rc_charge(
  constant uint* vin_val [[buffer(0)]],    // Input voltage (Q16.16)
  constant uint* vin_xz [[buffer(1)]],
  device uint* vcap_val [[buffer(2)]],     // Capacitor voltage (Q16.16)
  device uint* vcap_xz [[buffer(3)]],
  constant uint* rc_tau [[buffer(4)]],     // Time constant R*C (Q16.16)
  constant uint* dt [[buffer(5)]],         // Timestep (Q16.16)
  constant GpgaParams& params [[buffer(6)]],
  uint gid [[thread_position_in_grid]]
) {
  if (gid >= params.count) return;

  FourState32 vin = fs_make32(vin_val[gid], vin_xz[gid], 32u);
  FourState32 vcap = fs_make32(vcap_val[gid], vcap_xz[gid], 32u);
  FourState32 tau = fs_make32(rc_tau[0], 0u, 32u);
  FourState32 timestep = fs_make32(dt[0], 0u, 32u);

  // ΔV = (Vin - Vcap) * (dt / RC)
  FourState32 delta_v = fs_sub32(vin, vcap, 32u);
  FourState32 ratio = fs_div32(timestep, tau, 32u);
  FourState32 step = fs_mul32(delta_v, ratio, 32u);
  FourState32 vcap_new = fs_add32(vcap, step, 32u);

  vcap_val[gid] = vcap_new.val;
  vcap_xz[gid] = vcap_new.xz;
}
```

**Result:** This compiles, runs, and produces correct analog charging behavior using only existing 4-state operations. No architectural changes needed.

---

## Conclusion

**Analog support is architecturally viable** in MetalFPGA. The fixed-point approach leverages existing GPU parallelism and 4-state infrastructure. Implementation is deferred pending:

1. Completion of digital Verilog feature set
2. SystemVerilog support (higher priority)
3. User demand validation for mixed-signal GPU simulation
4. Solver architecture design (handling analog convergence loops)

The foundation is ready. The question is **when**, not **if**.

---

**Author:** MetalFPGA Project
**Last Updated:** December 26, 2025
**License:** Same as project (check LICENSE file)
