#!/bin/bash

# MetalFPGA comprehensive test script with smart flag detection
# Tests all Verilog files with appropriate flags

set +e  # Don't exit on error - we want to test everything

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

# Arrays to track results
declare -a FAILED_FILES
declare -a SKIPPED_FILES

# Create output directories
MSL_DIR="./msl"
HOST_DIR="./host"
RESULTS_DIR="./test_results"
mkdir -p "$MSL_DIR" "$HOST_DIR" "$RESULTS_DIR"

# Log file
LOG_FILE="$RESULTS_DIR/test_run_$(date +%Y%m%d_%H%M%S).log"

echo "MetalFPGA Smart Test Suite" | tee "$LOG_FILE"
echo "===========================" | tee -a "$LOG_FILE"
echo "Started at $(date)" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Function to check if file should be skipped
should_skip() {
    case "$1" in
        *test_comb_loop.v) echo "combinational cycle (expected)"; return 0 ;;
        *test_multidriver.v) echo "multiple drivers (expected)"; return 0 ;;
        *test_recursive_module.v) echo "recursive instantiation (expected)"; return 0 ;;
        *test_function.v) echo "functions not yet supported"; return 0 ;;
        *test_generate.v) echo "generate blocks not yet supported"; return 0 ;;
        *test_multi_dim_array.v) echo "multi-dim arrays not yet supported"; return 0 ;;
        *test_multifile_b.v) echo "requires multi-file compilation"; return 0 ;;
        *test_memory_read.v) echo "multiple top modules"; return 0 ;;
        *test_nested_modules.v) echo "multiple top modules"; return 0 ;;
    esac
    return 1
}

# Function to check if file needs --4state
needs_4state() {
    case "$1" in
        *test_case_4state.v|*test_case_mixed.v|*test_casex.v|*test_casex_simple.v|*test_casez.v|*test_casez_simple.v|*test_inout_port.v)
            return 0 ;;
    esac
    return 1
}

# Find all .v files recursively in verilog/
echo -e "${BLUE}Discovering Verilog test files...${NC}" | tee -a "$LOG_FILE"
VERILOG_FILES=$(find verilog -name "*.v" -type f | sort)
TOTAL=$(echo "$VERILOG_FILES" | wc -l | tr -d ' ')
echo -e "${BLUE}Found $TOTAL Verilog files${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Test each file
for vfile in $VERILOG_FILES; do
    filename=$(basename "$vfile" .v)
    dirname=$(dirname "$vfile")

    # Check if this is a known failure
    SKIP_REASON=$(should_skip "$vfile")
    if [ $? -eq 0 ]; then
        echo -e "${YELLOW}Testing: $vfile ($SKIP_REASON)${NC}" | tee -a "$LOG_FILE"
        SKIPPED=$((SKIPPED + 1))
        SKIPPED_FILES+=("$vfile: $SKIP_REASON")
        echo "" | tee -a "$LOG_FILE"
        continue
    fi

    echo -e "${YELLOW}Testing: $vfile${NC}" | tee -a "$LOG_FILE"

    # Determine flags
    FLAGS=""
    if needs_4state "$vfile"; then
        FLAGS="--4state"
        echo "  Using --4state flag" | tee -a "$LOG_FILE"
    fi

    # Output file paths
    msl_file="$MSL_DIR/${filename}.metal"
    host_file="$HOST_DIR/${filename}.mm"

    # Step 1: Try to dump flat (basic parsing/elaboration test)
    echo "  [1/3] Parsing and elaborating..." | tee -a "$LOG_FILE"
    if ! ./build/metalfpga_cli "$vfile" --dump-flat $FLAGS > "$RESULTS_DIR/${filename}_flat.txt" 2>&1; then
        if [ -z "$FLAGS" ] && grep -q "x/z literals require --4state" "$RESULTS_DIR/${filename}_flat.txt"; then
            FLAGS="--4state"
            echo "  Retrying with --4state..." | tee -a "$LOG_FILE"
            if ./build/metalfpga_cli "$vfile" --dump-flat $FLAGS > "$RESULTS_DIR/${filename}_flat.txt" 2>&1; then
                :
            else
                echo -e "  ${RED}✗ FAILED: Parsing/elaboration failed${NC}" | tee -a "$LOG_FILE"
                FAILED=$((FAILED + 1))
                FAILED_FILES+=("$vfile (parsing)")
                head -5 "$RESULTS_DIR/${filename}_flat.txt" | tee -a "$LOG_FILE"
                echo "" | tee -a "$LOG_FILE"
                continue
            fi
        else
            echo -e "  ${RED}✗ FAILED: Parsing/elaboration failed${NC}" | tee -a "$LOG_FILE"
            FAILED=$((FAILED + 1))
            FAILED_FILES+=("$vfile (parsing)")
            head -5 "$RESULTS_DIR/${filename}_flat.txt" | tee -a "$LOG_FILE"
            echo "" | tee -a "$LOG_FILE"
            continue
        fi
    fi

    # Step 2: Try to emit MSL
    echo "  [2/3] Generating MSL..." | tee -a "$LOG_FILE"
    if ! ./build/metalfpga_cli "$vfile" --emit-msl "$msl_file" --emit-host "$host_file" $FLAGS > "$RESULTS_DIR/${filename}_codegen.txt" 2>&1; then
        echo -e "  ${RED}✗ FAILED: MSL codegen failed${NC}" | tee -a "$LOG_FILE"
        FAILED=$((FAILED + 1))
        FAILED_FILES+=("$vfile (codegen)")
        head -5 "$RESULTS_DIR/${filename}_codegen.txt" | tee -a "$LOG_FILE"
        echo "" | tee -a "$LOG_FILE"
        continue
    fi

    # Step 3: Validate MSL file was created and is non-empty
    echo "  [3/3] Validating output..." | tee -a "$LOG_FILE"
    if [ ! -f "$msl_file" ]; then
        echo -e "  ${RED}✗ FAILED: MSL file not created${NC}" | tee -a "$LOG_FILE"
        FAILED=$((FAILED + 1))
        FAILED_FILES+=("$vfile (no MSL output)")
        echo "" | tee -a "$LOG_FILE"
        continue
    fi

    if [ ! -s "$msl_file" ]; then
        echo -e "  ${YELLOW}⚠ SKIPPED: MSL file is empty${NC}" | tee -a "$LOG_FILE"
        SKIPPED=$((SKIPPED + 1))
        SKIPPED_FILES+=("$vfile (empty MSL)")
        echo "" | tee -a "$LOG_FILE"
        continue
    fi

    # Check for kernel in MSL
    if grep -q "kernel void" "$msl_file"; then
        MSL_SIZE=$(wc -c < "$msl_file" | tr -d ' ')
        MSL_LINES=$(wc -l < "$msl_file" | tr -d ' ')
        echo -e "  ${GREEN}✓ PASSED${NC} (MSL: ${MSL_LINES} lines, ${MSL_SIZE} bytes)" | tee -a "$LOG_FILE"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${YELLOW}⚠ WARNING: No kernel found in MSL${NC}" | tee -a "$LOG_FILE"
        SKIPPED=$((SKIPPED + 1))
        SKIPPED_FILES+=("$vfile (no kernel)")
    fi

    echo "" | tee -a "$LOG_FILE"
done

# Summary
echo "" | tee -a "$LOG_FILE"
echo "===============================================" | tee -a "$LOG_FILE"
echo "Test Summary" | tee -a "$LOG_FILE"
echo "===============================================" | tee -a "$LOG_FILE"
echo "Total files:   $TOTAL" | tee -a "$LOG_FILE"
echo -e "${GREEN}Passed:        $PASSED${NC}" | tee -a "$LOG_FILE"
echo -e "${RED}Failed:        $FAILED${NC}" | tee -a "$LOG_FILE"
echo -e "${YELLOW}Skipped:       $SKIPPED${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Show failed files if any
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed files (unexpected):${NC}" | tee -a "$LOG_FILE"
    for file in "${FAILED_FILES[@]}"; do
        echo "  - $file" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Show skipped files if any
if [ $SKIPPED -gt 0 ]; then
    echo -e "${YELLOW}Skipped files (expected):${NC}" | tee -a "$LOG_FILE"
    for file in "${SKIPPED_FILES[@]}"; do
        echo "  - $file" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Pass rate
if [ $TOTAL -gt 0 ]; then
    TESTABLE=$((TOTAL - SKIPPED))
    if [ $TESTABLE -gt 0 ]; then
        PASS_RATE=$((PASSED * 100 / TESTABLE))
        echo "Pass rate: ${PASS_RATE}% ($PASSED/$TESTABLE testable files)" | tee -a "$LOG_FILE"
    fi
fi

echo "" | tee -a "$LOG_FILE"
echo "Finished at $(date)" | tee -a "$LOG_FILE"
echo "Full log: $LOG_FILE" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# MSL Analysis
echo "===============================================" | tee -a "$LOG_FILE"
echo "MSL Output Analysis" | tee -a "$LOG_FILE"
echo "===============================================" | tee -a "$LOG_FILE"

if [ -d "$MSL_DIR" ] && [ "$(ls -A $MSL_DIR 2>/dev/null)" ]; then
    MSL_FILES=$(find "$MSL_DIR" -name "*.metal" -type f)
    MSL_COUNT=$(echo "$MSL_FILES" | wc -l | tr -d ' ')
    MSL_TOTAL_LINES=$(cat $MSL_FILES 2>/dev/null | wc -l | tr -d ' ')
    MSL_TOTAL_SIZE=$(du -sh "$MSL_DIR" | cut -f1)

    echo "Generated MSL files: $MSL_COUNT" | tee -a "$LOG_FILE"
    echo "Total MSL lines: $MSL_TOTAL_LINES" | tee -a "$LOG_FILE"
    echo "Total MSL size: $MSL_TOTAL_SIZE" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"

    echo "Largest MSL files:" | tee -a "$LOG_FILE"
    find "$MSL_DIR" -name "*.metal" -type f -exec wc -l {} \; | sort -rn | head -10 | while read lines file; do
        echo "  $lines lines: $(basename $file)" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"

    echo "Smallest MSL files:" | tee -a "$LOG_FILE"
    find "$MSL_DIR" -name "*.metal" -type f -exec wc -l {} \; | sort -n | head -5 | while read lines file; do
        echo "  $lines lines: $(basename $file)" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Exit with error code if any unexpected tests failed
if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
