#!/bin/bash

# MetalFPGA comprehensive test script
# Tests all Verilog files, emits MSL, and validates output

set -e  # Exit on error (can be disabled for full run)

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

echo "MetalFPGA Test Suite" | tee "$LOG_FILE"
echo "====================" | tee -a "$LOG_FILE"
echo "Started at $(date)" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Find all .v files recursively in verilog/
echo -e "${BLUE}Discovering Verilog test files...${NC}" | tee -a "$LOG_FILE"
VERILOG_FILES=$(find verilog -name "*.v" -type f | sort)
TOTAL=$(echo "$VERILOG_FILES" | wc -l | tr -d ' ')
echo -e "${BLUE}Found $TOTAL Verilog files${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Test each file
for vfile in $VERILOG_FILES; do
    # Extract module name from file path
    filename=$(basename "$vfile" .v)
    dirname=$(dirname "$vfile")

    echo -e "${YELLOW}Testing: $vfile${NC}" | tee -a "$LOG_FILE"

    # Output file paths
    msl_file="$MSL_DIR/${filename}.metal"
    host_file="$HOST_DIR/${filename}.mm"

    # Step 1: Try to dump flat (basic parsing/elaboration test)
    FLAGS=""
    echo "  [1/3] Parsing and elaborating..." | tee -a "$LOG_FILE"
    if ! ./build/metalfpga_cli "$vfile" --dump-flat > "$RESULTS_DIR/${filename}_flat.txt" 2>&1; then
        if grep -q "x/z literals require --4state" "$RESULTS_DIR/${filename}_flat.txt"; then
            echo "  Retrying with --4state..." | tee -a "$LOG_FILE"
            if ./build/metalfpga_cli "$vfile" --dump-flat --4state > "$RESULTS_DIR/${filename}_flat.txt" 2>&1; then
                FLAGS="--4state"
            else
                echo -e "  ${RED}✗ FAILED: Parsing/elaboration failed${NC}" | tee -a "$LOG_FILE"
                FAILED=$((FAILED + 1))
                FAILED_FILES+=("$vfile (parsing)")
                cat "$RESULTS_DIR/${filename}_flat.txt" | tee -a "$LOG_FILE"
                echo "" | tee -a "$LOG_FILE"
                continue
            fi
        else
            echo -e "  ${RED}✗ FAILED: Parsing/elaboration failed${NC}" | tee -a "$LOG_FILE"
            FAILED=$((FAILED + 1))
            FAILED_FILES+=("$vfile (parsing)")
            cat "$RESULTS_DIR/${filename}_flat.txt" | tee -a "$LOG_FILE"
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
        cat "$RESULTS_DIR/${filename}_codegen.txt" | tee -a "$LOG_FILE"
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

    # Check for common MSL syntax issues (basic validation)
    if grep -q "kernel void" "$msl_file"; then
        MSL_SIZE=$(wc -c < "$msl_file" | tr -d ' ')
        echo -e "  ${GREEN}✓ PASSED${NC} (MSL: ${MSL_SIZE} bytes)" | tee -a "$LOG_FILE"
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
    echo -e "${RED}Failed files:${NC}" | tee -a "$LOG_FILE"
    for file in "${FAILED_FILES[@]}"; do
        echo "  - $file" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Show skipped files if any
if [ $SKIPPED -gt 0 ]; then
    echo -e "${YELLOW}Skipped files:${NC}" | tee -a "$LOG_FILE"
    for file in "${SKIPPED_FILES[@]}"; do
        echo "  - $file" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Pass rate
if [ $TOTAL -gt 0 ]; then
    PASS_RATE=$((PASSED * 100 / TOTAL))
    echo "Pass rate: ${PASS_RATE}%" | tee -a "$LOG_FILE"
fi

echo "" | tee -a "$LOG_FILE"
echo "Finished at $(date)" | tee -a "$LOG_FILE"
echo "Full log: $LOG_FILE" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# MSL Analysis Section
echo "===============================================" | tee -a "$LOG_FILE"
echo "MSL Output Analysis" | tee -a "$LOG_FILE"
echo "===============================================" | tee -a "$LOG_FILE"

# Count total lines of generated MSL
if [ -d "$MSL_DIR" ] && [ "$(ls -A $MSL_DIR 2>/dev/null)" ]; then
    MSL_FILES=$(find "$MSL_DIR" -name "*.metal" -type f)
    MSL_COUNT=$(echo "$MSL_FILES" | wc -l | tr -d ' ')
    MSL_TOTAL_LINES=$(cat $MSL_FILES 2>/dev/null | wc -l | tr -d ' ')
    MSL_TOTAL_SIZE=$(du -sh "$MSL_DIR" | cut -f1)

    echo "Generated MSL files: $MSL_COUNT" | tee -a "$LOG_FILE"
    echo "Total MSL lines: $MSL_TOTAL_LINES" | tee -a "$LOG_FILE"
    echo "Total MSL size: $MSL_TOTAL_SIZE" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"

    # Find largest MSL files
    echo "Largest MSL files:" | tee -a "$LOG_FILE"
    find "$MSL_DIR" -name "*.metal" -type f -exec wc -l {} \; | sort -rn | head -5 | while read lines file; do
        echo "  $lines lines: $(basename $file)" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Exit with error code if any tests failed
if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
