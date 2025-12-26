# REV10 - Multi-dimensional array implementation

**Commit:** 7de4f01
**Date:** Thu Dec 25 19:49:46 2025 +0100
**Message:** Added multidim array support and more testing

## Overview

Major feature addition implementing full multi-dimensional array support (2D through 5D+). Arrays can now be declared, indexed, read, and written with multiple subscripts. Includes extensive refactoring of AST, parser, and elaboration to handle arbitrary dimension counts. Six comprehensive test cases demonstrate real-world use cases including matrix operations, convolutions, and video tensor processing.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | Multi-dimensional array parsing with arbitrary dimensions |
| **Elaborate** | ✓ Enhanced | Flattening and linearization of multi-dim arrays |
| **Codegen (2-state)** | ✓ Enhanced | Multi-dim array access in MSL |
| **Codegen (4-state)** | ✓ Enhanced | Multi-dim array access in MSL |
| **Host emission** | ✗ Stubbed only | No changes |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**New Verilog Features:**
- **Multi-dimensional arrays**: `reg [7:0] matrix [0:15][0:15]` (2D)
- **3D arrays**: `reg [7:0] video [0:3][0:63][0:63]` (frames×height×width)
- **5D arrays**: `reg [7:0] tensor [0:3][0:3][0:3][0:7][0:7]` (batched video)
- **Multi-indexing**: `matrix[i][j]`, `tensor[b][f][c][r][c]`
- **Mixed with loops**: Nested loops for multi-dim initialization

**Capabilities:**
- Arbitrary dimension count (2D, 3D, 4D, 5D, etc.)
- Read and write operations on multi-dim arrays
- Combinational and sequential access
- Loop-based initialization patterns

## Architecture Changes

### Frontend: AST Extensions for Multi-Dim Arrays

**File**: `src/frontend/ast.hh` (+31 lines)

**New array dimension structure:**
```cpp
struct ArrayDim {
  int size = 0;                      // Computed size
  std::shared_ptr<Expr> msb_expr;    // Upper bound expression
  std::shared_ptr<Expr> lsb_expr;    // Lower bound expression
};
```

**Updated Net struct:**
```cpp
struct Net {
  // ... existing fields ...

  // OLD (1D only):
  // int array_size = 0;
  // std::shared_ptr<Expr> array_msb_expr;
  // std::shared_ptr<Expr> array_lsb_expr;

  // NEW (multi-dim):
  std::vector<ArrayDim> array_dims;  // Each dimension's bounds
};
```

**Example representations:**

```verilog
// 1D array: reg [7:0] arr [0:7];
array_dims = [{size: 8, msb: 7, lsb: 0}]

// 2D array: reg [7:0] matrix [0:3][0:3];
array_dims = [{size: 4, msb: 3, lsb: 0},
              {size: 4, msb: 3, lsb: 0}]

// 3D array: reg [7:0] video [0:9][0:63][0:63];
array_dims = [{size: 10, msb: 9, lsb: 0},
              {size: 64, msb: 63, lsb: 0},
              {size: 64, msb: 63, lsb: 0}]
```

**Updated SequentialAssign:**
```cpp
struct SequentialAssign {
  std::string lhs;

  // OLD (single index):
  // std::unique_ptr<Expr> lhs_index;

  // NEW (multi-index):
  std::vector<std::unique_ptr<Expr>> lhs_indices;  // All subscripts

  std::unique_ptr<Expr> rhs;
  bool nonblocking = true;
};
```

**New function support (preparatory):**
```cpp
struct FunctionArg {
  std::string name;
  int width = 1;
  bool is_signed = false;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
};

struct Function {
  std::string name;
  int width = 1;                          // Return width
  bool is_signed = false;
  std::shared_ptr<Expr> msb_expr;
  std::shared_ptr<Expr> lsb_expr;
  std::vector<FunctionArg> args;          // Function arguments
  std::unique_ptr<Expr> body_expr;        // Function body expression
};
```

**New expression kind:**
```cpp
enum class ExprKind {
  // ... existing ...
  kCall,  // NEW: Function calls (preparatory)
};
```

### Frontend: Parser Enhancements

**File**: `src/frontend/verilog_parser.cc` (+416 lines!)

This is a major rewrite of array parsing logic.

**Multi-dimensional declaration parsing:**

```verilog
// Verilog: reg [7:0] matrix [0:3][0:7];

// Parsing steps:
// 1. Parse "reg [7:0]" → width = 8
// 2. Parse identifier "matrix"
// 3. Loop while encountering '[':
//    a. Parse range "[0:3]" → dim 0: msb=0, lsb=3, size=4
//    b. Parse range "[0:7]" → dim 1: msb=0, lsb=7, size=8
// 4. Create Net with array_dims = [{4, ...}, {8, ...}]
```

**Implementation:**
```cpp
// In ParseNetDeclaration():
while (Match("[")) {
  ArrayDim dim;
  if (!ParseRange(&dim.size, &dim.msb_expr, &dim.lsb_expr, &had_range)) {
    return false;
  }
  array_dims.push_back(std::move(dim));
  if (!Match("]")) {
    return false;
  }
}
net.array_dims = std::move(array_dims);
```

**Multi-index expression parsing:**

```verilog
// Verilog: matrix[i][j]

// Parsing:
// 1. Parse identifier "matrix"
// 2. Match '[' → parse expression "i", match ']'
// 3. Match '[' → parse expression "j", match ']'
// 4. Create Expr with kind=kIndex, indices=[i, j]
```

**Updated Expr structure:**
```cpp
struct Expr {
  // ... existing fields ...

  // OLD (single index):
  // std::unique_ptr<Expr> index;

  // NEW (multi-index):
  std::vector<std::unique_ptr<Expr>> indices;  // All subscripts
};
```

**Assignment parsing with multi-index:**

```verilog
// Verilog: matrix[i][j] <= value;

// Parsing:
// 1. Parse lhs "matrix"
// 2. Parse all indices: [i], [j]
// 3. Match "<=" (non-blocking)
// 4. Parse rhs "value"
// 5. Create SequentialAssign with lhs_indices = [i, j]
```

### Elaboration: Array Linearization

**File**: `src/core/elaboration.cc` (+408 lines!)

Multi-dimensional arrays are **linearized** into 1D arrays during elaboration.

**Linearization strategy:**

```verilog
// Verilog declaration:
reg [7:0] matrix [0:3][0:7];  // 4×8 = 32 elements

// Internal representation (flattened):
// 1D array of size 32
// Access matrix[i][j] becomes: matrix_flat[i * 8 + j]
```

**Row-major ordering:**
```
matrix[0][0], matrix[0][1], ..., matrix[0][7],  // Row 0
matrix[1][0], matrix[1][1], ..., matrix[1][7],  // Row 1
matrix[2][0], matrix[2][1], ..., matrix[2][7],  // Row 2
matrix[3][0], matrix[3][1], ..., matrix[3][7]   // Row 3
```

**Index calculation:**

```cpp
// For 2D array[i][j] with dimensions [D0][D1]:
flat_index = i * D1 + j

// For 3D array[i][j][k] with dimensions [D0][D1][D2]:
flat_index = i * (D1 * D2) + j * D2 + k

// For 5D array[a][b][c][d][e] with dims [D0][D1][D2][D3][D4]:
flat_index = a * (D1*D2*D3*D4) + b * (D2*D3*D4) + c * (D3*D4) + d * D4 + e
```

**Implementation:**

`FlattenArrayIndex()`:
```cpp
// Converts multi-dimensional index to flat index
// Input: indices = [i, j, k], dims = [D0, D1, D2]
// Output: MSL expression for "i * (D1*D2) + j * D2 + k"

std::string FlattenArrayIndex(
    const std::vector<std::unique_ptr<Expr>>& indices,
    const std::vector<ArrayDim>& dims) {

  // Calculate stride for each dimension
  std::vector<int> strides;
  int stride = 1;
  for (int d = dims.size() - 1; d >= 0; --d) {
    strides[d] = stride;
    stride *= dims[d].size;
  }

  // Build expression: idx[0]*stride[0] + idx[1]*stride[1] + ...
  std::string result;
  for (size_t i = 0; i < indices.size(); ++i) {
    if (i > 0) result += " + ";
    result += "(" + EmitExpr(indices[i]) + ")";
    if (strides[i] > 1) {
      result += " * " + std::to_string(strides[i]);
    }
  }
  return result;
}
```

**Memory allocation:**
```cpp
// For reg [7:0] matrix [0:3][0:7]:
// Total elements = 4 * 8 = 32
// Allocate: uint8_t matrix_flat[32];
```

### Codegen: Multi-Dim Array Access

**File**: `src/codegen/msl_codegen.cc` (+29 lines)

Arrays already flattened during elaboration, so codegen is straightforward.

**Read operation:**
```metal
// Verilog: data = matrix[i][j];
// After elaboration: data = matrix[(i * 8) + j];
// MSL:
data_value = matrix_value[(i * 8u) + j];
data_xz = matrix_xz[(i * 8u) + j];  // If 4-state
```

**Write operation:**
```metal
// Verilog: matrix[i][j] <= value;
// After elaboration: matrix[(i * 8) + j] <= value;
// MSL (in sequential kernel):
uint idx = (i * 8u) + j;
matrix_next_value[idx] = value;
matrix_next_xz[idx] = 0;  // If 4-state
```

**Bounds checking (optional):**
```metal
// Optional safety check (if enabled):
uint idx = (i * 8u) + j;
if (idx >= 32u) {
  // Out of bounds error
  result_xz = 0xFFFFFFFFu;  // Return X
} else {
  result_value = matrix_value[idx];
  result_xz = matrix_xz[idx];
}
```

## Test Coverage

### 2D Array Tests (4 files, 227 lines)

**test_array_2d_basic.v** (37 lines):
```verilog
// Simple 16×16 matrix
reg [7:0] matrix [0:15][0:15];

// Read: data_out = matrix[row][col];
// Write: matrix[row][col] <= data_in;

// Initialization with nested loops:
for (i = 0; i < 4; i = i + 1)
  for (j = 0; j < 4; j = j + 1)
    grid[i][j] = (i * 4) + j;
```

**test_array_2d_write.v** (65 lines):
- Clocked write operations
- Read-modify-write patterns
- Diagonal updates
- Row/column swaps

**test_array_2d_convolution.v** (56 lines):
```verilog
// 3×3 convolution kernel on 8×8 image
reg [7:0] image [0:7][0:7];
reg [7:0] kernel [0:2][0:2];
reg [15:0] output [0:5][0:5];  // 6×6 output (valid convolution)

// Nested loops for convolution:
for (oy = 0; oy < 6; oy = oy + 1)
  for (ox = 0; ox < 6; ox = ox + 1)
    for (ky = 0; ky < 3; ky = ky + 1)
      for (kx = 0; kx < 3; kx = kx + 3)
        accum = accum + image[oy+ky][ox+kx] * kernel[ky][kx];
```

**test_array_2d_matmul.v** (69 lines):
```verilog
// 4×4 matrix multiplication: C = A × B
reg [15:0] A [0:3][0:3];
reg [15:0] B [0:3][0:3];
reg [31:0] C [0:3][0:3];

// Triple nested loop:
for (i = 0; i < 4; i = i + 1)
  for (j = 0; j < 4; j = j + 1)
    for (k = 0; k < 4; k = k + 1)
      C[i][j] = C[i][j] + A[i][k] * B[k][j];
```

### 3D Array Test (1 file, 40 lines)

**test_array_3d.v**:
```verilog
// 3D video tensor: [frames][height][width]
reg [7:0] video [0:9][0:63][0:63];  // 10 frames, 64×64 resolution

// Access: pixel = video[frame][y][x];

// Initialization: all frames to different patterns
for (f = 0; f < 10; f = f + 1)
  for (y = 0; y < 64; y = y + 1)
    for (x = 0; x < 64; x = x + 1)
      video[f][y][x] = (f * 10) + (y ^ x);
```

### 5D Array Test (1 file, 98 lines)

**test_array_5d.v**:
```verilog
// 5D batched video tensor: [batch][frame][channel][row][col]
reg [7:0] video [0:3][0:3][0:3][0:7][0:7];
// 4 batches × 4 frames × 4 channels × 8×8 pixels = 4096 elements

// Access: pixel = video[batch][frame][channel][row][col];

// 5-deep nested loop initialization:
for (iw = 0; iw < 4; iw = iw + 1)
  for (iz = 0; iz < 4; iz = iz + 1)
    for (iy = 0; iy < 4; iy = iy + 1)
      for (ix = 0; ix < 4; ix = ix + 1)
        for (it = 0; it < 4; it = it + 1)
          hyper[iw][iz][iy][ix][it] = /* pattern */;
```

**Use case:** Batched video processing with RGBA channels, demonstrating support for arbitrary dimension counts.

### Tests Moved to pass/ (3 files)

- **test_function.v**: Function declaration tests (now parsing correctly)
- **test_initial_block.v**: Initial block tests (now working with loops)
- **test_multi_dim_array.v**: Original multi-dim test (now superseded by comprehensive tests)

## Implementation Details

### Memory Layout Strategy

**Row-major vs. Column-major:**

metalfpga uses **row-major** ordering (C-style), matching GPU memory access patterns:

```verilog
// Declaration: reg [7:0] A [0:3][0:7];
// Memory: A[0][0..7], A[1][0..7], A[2][0..7], A[3][0..7]

// Good access pattern (sequential):
for (i = 0; i < 4; i++)
  for (j = 0; j < 8; j++)
    read(A[i][j]);  // Sequential addresses → cache-friendly

// Poor access pattern (strided):
for (j = 0; j < 8; j++)
  for (i = 0; i < 4; i++)
    read(A[i][j]);  // Strided addresses → cache misses
```

**GPU optimization implications:**
- Row-major matches Metal/CUDA expectations
- Coalesced memory accesses for contiguous indices
- Better cache utilization

### Stride Calculation Examples

**2D: `array[i][j]` with dims `[4][8]`:**
```
Strides: [8, 1]
Index: i * 8 + j * 1 = i * 8 + j
```

**3D: `array[i][j][k]` with dims `[4][8][16]`:**
```
Strides: [128, 16, 1]  // [8*16, 16, 1]
Index: i * 128 + j * 16 + k
```

**5D: `array[a][b][c][d][e]` with dims `[4][4][4][8][8]`:**
```
Strides: [1024, 256, 64, 8, 1]  // [4*4*8*8, 4*8*8, 8*8, 8, 1]
Index: a * 1024 + b * 256 + c * 64 + d * 8 + e
```

### Dimension Limits

**Practical limits:**
- No hard limit on dimension count (parser handles arbitrary depths)
- Memory limit: Total elements must fit in GPU memory
- 5D example: 4×4×4×8×8 = 4,096 elements ✓
- Large example: 1000×1000×1000 = 1 billion elements (4GB for uint32) - possible but large

## Documentation Updates

**File**: `docs/gpga/verilog_words.md` (+8 lines)

Added notes on:
- Multi-dimensional array syntax
- Index ordering (row-major)
- Memory layout implications

## Known Gaps and Limitations

### Improvements Over REV9

**Now Working:**
- Multi-dimensional arrays (2D, 3D, 4D, 5D+)
- Arbitrary dimension counts
- Multi-index read and write
- Nested loop initialization
- Complex array operations (convolution, matmul)

**Still Missing:**
- Functions/tasks (AST added but not implemented)
- `generate` blocks
- System tasks
- Packed multi-dimensional arrays: `reg [3:0][7:0] data` (array of vectors)
- Dynamic array sizing (all dimensions must be constant)
- Host code emission
- Runtime

### Semantic Notes

**Array semantics:**
- All arrays linearized at elaboration time
- Row-major ordering (C-style, not Fortran column-major)
- Zero-based indexing after elaboration (Verilog ranges respected during parsing)
- Out-of-bounds accesses: Implementation-defined (currently no runtime checking)

**Function support (partial):**
- AST structures added (`Function`, `FunctionArg`, `kCall`)
- Parser prepared but functions not yet elaborated/codegen
- Groundwork for future REV

## Statistics

- **Files changed**: 16
- **Lines added**: 1,161
- **Lines removed**: 123
- **Net change**: +1,038 lines

**Major components:**
- Parser: +416 lines (multi-dim array parsing, refactored indexing)
- Elaboration: +408 lines (array linearization, stride calculation, flattening)
- AST header: +31 lines (ArrayDim, multi-index support, Function prep)
- Codegen: +29 lines (linearized array access)
- Main: +17 lines (pretty-printing multi-dim arrays)
- AST implementation: +10 lines
- Documentation: +8 lines

**Test coverage:**
- 2D arrays: 4 tests (basic, write, convolution, matmul) - 227 lines
- 3D arrays: 1 test (video tensor) - 40 lines
- 5D arrays: 1 test (batched video) - 98 lines
- **Total**: 6 new multi-dim array tests + 3 moved

**Test suite totals:**
- `verilog/pass/`: 102 files (up from 96 in REV9)
- Real-world applications: Image processing, ML tensors, video

This commit transforms metalfpga into a compiler capable of handling complex data structures used in real hardware accelerators (image processors, neural network engines, video codecs).
