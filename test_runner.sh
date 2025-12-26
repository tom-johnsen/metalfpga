#!/bin/bash

# MetalFPGA comprehensive test script with smart flag detection
# Tests all Verilog files with appropriate flags

set +e  # Don't exit on error - we want to test everything

# Script options
FORCE_4STATE=0
FORCE_AUTO=0
for arg in "$@"; do
    case "$arg" in
        --4state) FORCE_4STATE=1 ;;
        --auto) FORCE_AUTO=1 ;;
    esac
done

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
MISSING=0
BUGS=0

# Arrays to track results
declare -a FAILED_FILES
declare -a MISSING_FILES
declare -a BUG_FILES

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

# Prefer rg when available
HAS_RG=0
if command -v rg >/dev/null 2>&1; then
    HAS_RG=1
fi

should_expect_fail() {
    local file="$1"
    local use_4state="${2:-0}"
    case "$file" in
        *test_comb_loop.v) echo "combinational cycle (expected)"; return 0 ;;
        *test_multidriver.v)
            if [ "$use_4state" -eq 0 ]; then
                echo "multiple drivers (expected)"
                return 0
            fi
            ;;
        *test_recursive_module.v) echo "recursive instantiation (expected)"; return 0 ;;
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

# Heuristic check for 4-state constructs
file_suggests_4state() {
    local file="$1"
    local pattern="([0-9]+'[bxzBXZ])|\\b(casex|casez|tri0|tri1|triand|trior|trireg|wand|wor|tranif0|tranif1|tran|cmos|bufif|notif|nmos|pmos|rnmos|rpmos|rtran|rtranif|pullup|pulldown)\\b|\\binout\\b"
    if [ "$HAS_RG" -eq 1 ]; then
        rg -n --no-messages -U -e "$pattern" "$file" >/dev/null
    else
        grep -E -q "$pattern" "$file"
    fi
}

is_4state_error() {
    local log="$1"
    local pattern="requires --4state|x/z literals require --4state|tristate|switch primitives|net type requires --4state|multiple drivers"
    if [ "$HAS_RG" -eq 1 ]; then
        rg -n --no-messages -i -e "$pattern" "$log" >/dev/null
    else
        grep -E -i -q "$pattern" "$log"
    fi
}

is_multi_top_error() {
    local log="$1"
    if [ "$HAS_RG" -eq 1 ]; then
        rg -n --no-messages -e "multiple top-level modules found" "$log" >/dev/null
    else
        grep -E -q "multiple top-level modules found" "$log"
    fi
}

is_missing_error() {
    local log="$1"
    local pattern="unsupported|not supported in v0|unsupported system function|unexpected token|delay control not supported|failed to open include file|unknown module|expected assignment operator|expected identifier"
    if [ "$HAS_RG" -eq 1 ]; then
        rg -n --no-messages -i -e "$pattern" "$log" >/dev/null
    else
        grep -E -i -q "$pattern" "$log"
    fi
}

get_module_names() {
    local file="$1"
    if [ "$HAS_RG" -eq 1 ]; then
        rg -n --no-messages "^[[:space:]]*module[[:space:]]+[A-Za-z_][A-Za-z0-9_$]*" "$file" \
            | sed -E 's/^[^:]*:[[:space:]]*module[[:space:]]+([A-Za-z_][A-Za-z0-9_$]*).*/\1/' \
            | sort -u
    else
        grep -E "^[[:space:]]*module[[:space:]]+[A-Za-z_][A-Za-z0-9_$]*" "$file" \
            | sed -E 's/^[[:space:]]*module[[:space:]]+([A-Za-z_][A-Za-z0-9_$]*).*/\1/' \
            | sort -u
    fi
}

run_test_case() {
    local vfile="$1"
    local top="$2"
    local base="$3"
    local use_4state="$4"
    local use_auto="$5"

    local flat_out="$RESULTS_DIR/${base}_flat.txt"
    local codegen_out="$RESULTS_DIR/${base}_codegen.txt"
    local msl_file="$MSL_DIR/${base}.metal"
    local host_file="$HOST_DIR/${base}.mm"

    local cli=(./build/metalfpga_cli "$vfile")
    if [ -n "$top" ]; then
        cli+=("--top" "$top")
    fi
    if [ "$use_auto" -eq 1 ]; then
        cli+=("--auto")
    fi

    local flags=()
    if [ "$use_4state" -eq 1 ]; then
        flags+=(--4state)
    fi

    echo "  [1/3] Parsing and elaborating..." | tee -a "$LOG_FILE"
    if ! "${cli[@]}" --dump-flat "${flags[@]}" > "$flat_out" 2>&1; then
        if is_multi_top_error "$flat_out"; then
            echo "  Detected multiple top-level modules, retrying per module..." | tee -a "$LOG_FILE"
            return 3
        fi
        if [ "$use_4state" -eq 0 ] && is_4state_error "$flat_out"; then
            echo "  Retrying with --4state..." | tee -a "$LOG_FILE"
            use_4state=1
            flags=(--4state)
            if ! "${cli[@]}" --dump-flat "${flags[@]}" > "$flat_out" 2>&1; then
                if is_multi_top_error "$flat_out"; then
                    echo "  Detected multiple top-level modules, retrying per module..." | tee -a "$LOG_FILE"
                    return 3
                fi
                LAST_FAIL_LOG="$flat_out"
                echo -e "  ${RED}✗ FAILED: Parsing/elaboration failed${NC}" | tee -a "$LOG_FILE"
                head -5 "$flat_out" | tee -a "$LOG_FILE"
                return 1
            fi
        else
            LAST_FAIL_LOG="$flat_out"
            echo -e "  ${RED}✗ FAILED: Parsing/elaboration failed${NC}" | tee -a "$LOG_FILE"
            head -5 "$flat_out" | tee -a "$LOG_FILE"
            return 1
        fi
    fi

    echo "  [2/3] Generating MSL..." | tee -a "$LOG_FILE"
    if ! "${cli[@]}" --emit-msl "$msl_file" --emit-host "$host_file" "${flags[@]}" > "$codegen_out" 2>&1; then
        if [ "$use_4state" -eq 0 ] && is_4state_error "$codegen_out"; then
            echo "  Retrying with --4state..." | tee -a "$LOG_FILE"
            use_4state=1
            flags=(--4state)
            if ! "${cli[@]}" --emit-msl "$msl_file" --emit-host "$host_file" "${flags[@]}" > "$codegen_out" 2>&1; then
                LAST_FAIL_LOG="$codegen_out"
                echo -e "  ${RED}✗ FAILED: MSL codegen failed${NC}" | tee -a "$LOG_FILE"
                head -5 "$codegen_out" | tee -a "$LOG_FILE"
                return 1
            fi
        else
            LAST_FAIL_LOG="$codegen_out"
            echo -e "  ${RED}✗ FAILED: MSL codegen failed${NC}" | tee -a "$LOG_FILE"
            head -5 "$codegen_out" | tee -a "$LOG_FILE"
            return 1
        fi
    fi

    echo "  [3/3] Validating output..." | tee -a "$LOG_FILE"
    if [ ! -f "$msl_file" ]; then
        LAST_FAIL_LOG="$codegen_out"
        echo -e "  ${RED}✗ FAILED: MSL file not created${NC}" | tee -a "$LOG_FILE"
        return 1
    fi

    if [ ! -s "$msl_file" ]; then
        LAST_FAIL_LOG="$codegen_out"
        echo -e "  ${RED}✗ BUG: MSL file is empty${NC}" | tee -a "$LOG_FILE"
        return 2
    fi

    if grep -q "kernel void" "$msl_file"; then
        local msl_size
        local msl_lines
        msl_size=$(wc -c < "$msl_file" | tr -d ' ')
        msl_lines=$(wc -l < "$msl_file" | tr -d ' ')
        echo -e "  ${GREEN}✓ PASSED${NC} (MSL: ${msl_lines} lines, ${msl_size} bytes)" | tee -a "$LOG_FILE"
        return 0
    fi

    LAST_FAIL_LOG="$codegen_out"
    echo -e "  ${RED}✗ BUG: No kernel found in MSL${NC}" | tee -a "$LOG_FILE"
    return 2
}

# Find all .v files recursively in verilog/
echo -e "${BLUE}Discovering Verilog test files...${NC}" | tee -a "$LOG_FILE"
if [ -n "${VERILOG_FILES_OVERRIDE:-}" ]; then
    VERILOG_FILES="$VERILOG_FILES_OVERRIDE"
else
    VERILOG_FILES=$(find verilog -name "*.v" -type f | sort)
fi
TOTAL=$(echo "$VERILOG_FILES" | wc -l | tr -d ' ')
echo -e "${BLUE}Found $TOTAL Verilog files${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Test each file
for vfile in $VERILOG_FILES; do
    filename=$(basename "$vfile" .v)
    dirname=$(dirname "$vfile")

    use_4state=$FORCE_4STATE
    if [ "$use_4state" -eq 0 ]; then
        if needs_4state "$vfile" || file_suggests_4state "$vfile"; then
            use_4state=1
        fi
    fi
    use_auto=$FORCE_AUTO

    # Expected-failure tests count as fail when they fail
    EXPECT_REASON=$(should_expect_fail "$vfile" "$use_4state")
    if [ $? -eq 0 ]; then
        echo -e "${YELLOW}Testing: $vfile ($EXPECT_REASON)${NC}" | tee -a "$LOG_FILE"

        if [ "$use_4state" -eq 1 ]; then
            echo "  Using --4state flag" | tee -a "$LOG_FILE"
        fi
        if [ "$use_auto" -eq 1 ]; then
            echo "  Using --auto flag" | tee -a "$LOG_FILE"
        fi

        flat_out="$RESULTS_DIR/${filename}_flat.txt"
        cli=(./build/metalfpga_cli "$vfile")
        if [ "$use_auto" -eq 1 ]; then
            cli+=("--auto")
        fi
        flags=()
        if [ "$use_4state" -eq 1 ]; then
            flags+=(--4state)
        fi

        echo "  [1/1] Expecting failure..." | tee -a "$LOG_FILE"
        if "${cli[@]}" --dump-flat "${flags[@]}" > "$flat_out" 2>&1; then
            echo -e "  ${RED}✗ BUG: Expected failure but succeeded${NC}" | tee -a "$LOG_FILE"
            BUGS=$((BUGS + 1))
            BUG_FILES+=("$vfile (expected fail)")
        else
            if is_multi_top_error "$flat_out"; then
                echo "  Detected multiple top-level modules, retrying per module..." | tee -a "$LOG_FILE"
                module_names=$(get_module_names "$vfile")
                if [ -z "$module_names" ]; then
                    echo -e "  ${RED}✗ BUG: Unable to discover module names${NC}" | tee -a "$LOG_FILE"
                    BUGS=$((BUGS + 1))
                    BUG_FILES+=("$vfile (expected fail multi-top)")
                else
                    any_pass=0
                    for top in $module_names; do
                        cli_top=(./build/metalfpga_cli "$vfile" --top "$top")
                        if [ "$use_auto" -eq 1 ]; then
                            cli_top+=("--auto")
                        fi
                        if "${cli_top[@]}" --dump-flat "${flags[@]}" > "$flat_out" 2>&1; then
                            any_pass=1
                            break
                        fi
                    done
                    if [ "$any_pass" -eq 1 ]; then
                        echo -e "  ${RED}✗ BUG: Expected failure but succeeded${NC}" | tee -a "$LOG_FILE"
                        BUGS=$((BUGS + 1))
                        BUG_FILES+=("$vfile (expected fail)")
                    else
                        echo -e "  ${GREEN}✓ EXPECTED FAILURE${NC}" | tee -a "$LOG_FILE"
                        FAILED=$((FAILED + 1))
                        FAILED_FILES+=("$vfile (expected fail)")
                        head -3 "$flat_out" | tee -a "$LOG_FILE"
                    fi
                fi
            else
                echo -e "  ${GREEN}✓ EXPECTED FAILURE${NC}" | tee -a "$LOG_FILE"
                FAILED=$((FAILED + 1))
                FAILED_FILES+=("$vfile (expected fail)")
                head -3 "$flat_out" | tee -a "$LOG_FILE"
            fi
        fi
        echo "" | tee -a "$LOG_FILE"
        continue
    fi

    echo -e "${YELLOW}Testing: $vfile${NC}" | tee -a "$LOG_FILE"

    if [ "$use_4state" -eq 1 ]; then
        echo "  Using --4state flag" | tee -a "$LOG_FILE"
    fi
    if [ "$use_auto" -eq 1 ]; then
        echo "  Using --auto flag" | tee -a "$LOG_FILE"
    fi

    status=0
    base="$filename"
    run_test_case "$vfile" "" "$base" "$use_4state" "$use_auto"
    status=$?

    if [ "$status" -eq 3 ]; then
        module_names=$(get_module_names "$vfile")
        if [ -z "$module_names" ]; then
            echo -e "  ${RED}✗ BUG: Unable to discover module names${NC}" | tee -a "$LOG_FILE"
            BUGS=$((BUGS + 1))
            BUG_FILES+=("$vfile (multi-top discovery)")
            echo "" | tee -a "$LOG_FILE"
            continue
        fi
        any_bug=0
        any_missing=0
        for top in $module_names; do
            echo "  Retrying with --top $top..." | tee -a "$LOG_FILE"
            run_test_case "$vfile" "$top" "${filename}__${top}" "$use_4state" "$use_auto"
            status=$?
            if [ "$status" -eq 1 ]; then
                if [ -n "${LAST_FAIL_LOG:-}" ] && is_missing_error "$LAST_FAIL_LOG"; then
                    any_missing=1
                else
                    any_bug=1
                fi
            elif [ "$status" -eq 2 ]; then
                any_bug=1
            fi
        done
        if [ "$any_bug" -eq 1 ]; then
            BUGS=$((BUGS + 1))
            BUG_FILES+=("$vfile (multi-top)")
        elif [ "$any_missing" -eq 1 ]; then
            MISSING=$((MISSING + 1))
            MISSING_FILES+=("$vfile (multi-top)")
        else
            PASSED=$((PASSED + 1))
        fi
        echo "" | tee -a "$LOG_FILE"
        continue
    fi

    if [ "$status" -eq 0 ]; then
        PASSED=$((PASSED + 1))
    elif [ "$status" -eq 2 ]; then
        BUGS=$((BUGS + 1))
        BUG_FILES+=("$vfile (no kernel)")
    else
        if [ -n "${LAST_FAIL_LOG:-}" ] && is_missing_error "$LAST_FAIL_LOG"; then
            MISSING=$((MISSING + 1))
            MISSING_FILES+=("$vfile")
        else
            BUGS=$((BUGS + 1))
            BUG_FILES+=("$vfile")
        fi
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
echo -e "${BLUE}Expected fail: $FAILED${NC}" | tee -a "$LOG_FILE"
echo -e "${YELLOW}Missing:       $MISSING${NC}" | tee -a "$LOG_FILE"
echo -e "${RED}Bugs:          $BUGS${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Show bug files if any
if [ $BUGS -gt 0 ]; then
    echo -e "${RED}Bug files:${NC}" | tee -a "$LOG_FILE"
    for file in "${BUG_FILES[@]}"; do
        echo "  - $file" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Show missing files if any
if [ $MISSING -gt 0 ]; then
    echo -e "${YELLOW}Missing feature files:${NC}" | tee -a "$LOG_FILE"
    for file in "${MISSING_FILES[@]}"; do
        echo "  - $file" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Show expected-fail files if any
if [ $FAILED -gt 0 ]; then
    echo -e "${BLUE}Expected-fail files:${NC}" | tee -a "$LOG_FILE"
    for file in "${FAILED_FILES[@]}"; do
        echo "  - $file" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Pass rate
if [ $TOTAL -gt 0 ]; then
    TESTABLE=$((TOTAL - MISSING))
    if [ $TESTABLE -gt 0 ]; then
        PASS_COUNT=$((PASSED + FAILED))
        PASS_RATE=$((PASS_COUNT * 100 / TESTABLE))
        echo "Pass rate: ${PASS_RATE}% ($PASS_COUNT/$TESTABLE testable files)" | tee -a "$LOG_FILE"
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

# Exit with error code if any bugs were found
if [ $BUGS -gt 0 ]; then
    exit 1
else
    exit 0
fi
