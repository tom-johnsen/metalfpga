#!/usr/bin/env bash
# IEEE 1800-2012 SystemVerilog Test Suite Runner for metalfpga
# Runs all 77 ISPRAS SystemVerilog compliance tests

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TESTS_DIR="$(cd "${SCRIPT_DIR}/ispras-sv-tests/ieee-1800-2012" && pwd)"
METALFPGA="${METALFPGA_CLI:-}"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="${RESULTS_DIR}/test_run_1800_2012_${TIMESTAMP}.log"
SUMMARY_FILE="${RESULTS_DIR}/summary_1800_2012_${TIMESTAMP}.txt"

# Colors for terminal output
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
VARYING=0

# Test classification arrays
declare -a POSITIVE_TESTS
declare -a NEGATIVE_TESTS
declare -a VARYING_TESTS
declare -a FAILED_TESTS

# Parse command line arguments
MODE="all"
VERBOSE=false
STOP_ON_FAIL=false
FILTER=""
DRY_RUN=false
PARALLEL=false
MAX_JOBS=4
FORCE_4STATE=false
FORCE_2STATE=false
FORCE_AUTO=false

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run IEEE 1800-2012 SystemVerilog compliance tests for metalfpga.

OPTIONS:
    -h, --help              Show this help message
    -v, --verbose           Verbose output (show compilation details)
    -s, --stop-on-fail      Stop on first failure
    -f, --filter PATTERN    Only run tests matching PATTERN (e.g., "test_16_08_*")
    -m, --mode MODE         Test mode: all, positive, negative, varying (default: all)
    -d, --dry-run           Show what would be tested without running
    -p, --parallel          Run tests in parallel (experimental)
    -j, --jobs N            Number of parallel jobs (default: 4)
    --4state                Force --4state for all tests
    --2state                Force 2-state (disable --4state retries)
    --auto                  Pass --auto for include discovery
    --metalfpga PATH        Path to metalfpga_cli binary

EXAMPLES:
    # Run all tests
    $0

    # Run only sequence declaration tests (section 16.8)
    $0 --filter "test_16_08_*"

    # Run only POSITIVE tests verbosely
    $0 --mode positive --verbose

    # Run tests in parallel with 8 jobs
    $0 --parallel --jobs 8

    # Dry run to see what would be tested
    $0 --filter "test_16_12_*" --dry-run

ENVIRONMENT:
    METALFPGA_CLI           Path to metalfpga binary (default: metalfpga_cli)

OUTPUT:
    Results are logged to: ${RESULTS_DIR}/
    - test_run_1800_2012_TIMESTAMP.log     Full test log
    - summary_1800_2012_TIMESTAMP.txt      Summary report
EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -s|--stop-on-fail)
            STOP_ON_FAIL=true
            shift
            ;;
        -f|--filter)
            FILTER="$2"
            shift 2
            ;;
        -m|--mode)
            MODE="$2"
            shift 2
            ;;
        -d|--dry-run)
            DRY_RUN=true
            shift
            ;;
        -p|--parallel)
            PARALLEL=true
            shift
            ;;
        -j|--jobs)
            MAX_JOBS="$2"
            shift 2
            ;;
        --4state)
            FORCE_4STATE=true
            shift
            ;;
        --2state)
            FORCE_2STATE=true
            shift
            ;;
        --auto)
            FORCE_AUTO=true
            shift
            ;;
        --metalfpga)
            METALFPGA="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Setup results directory
mkdir -p "${RESULTS_DIR}"

# Header
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  metalfpga IEEE 1800-2012 SystemVerilog Test Suite Runner${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""
echo "Test directory: ${TESTS_DIR}"
if [[ -z "${METALFPGA}" ]]; then
    if [[ -x "${REPO_ROOT}/build/metalfpga_cli" ]]; then
        METALFPGA="${REPO_ROOT}/build/metalfpga_cli"
    else
        METALFPGA="metalfpga_cli"
    fi
fi
echo "metalfpga CLI:  ${METALFPGA}"
echo "Mode:           ${MODE}"
echo "Filter:         ${FILTER:-none}"
echo "Parallel:       ${PARALLEL}"
[[ "${PARALLEL}" == "true" ]] && echo "Max jobs:       ${MAX_JOBS}"
echo "Results:        ${RESULTS_DIR}"
echo ""

# Check if metalfpga exists
if ! command -v "${METALFPGA}" &> /dev/null; then
    echo -e "${RED}ERROR: metalfpga not found at: ${METALFPGA}${NC}"
    echo "Set METALFPGA_CLI environment variable or use --metalfpga option"
    exit 1
fi

# Check metalfpga version
echo "metalfpga version:"
"${METALFPGA}" --version 2>&1 || echo "  (version not available)"
echo ""

# Get test type from file
get_test_type() {
    local test_file="$1"
    if grep -q "^// ! TYPE: POSITIVE" "$test_file" 2>/dev/null; then
        echo "POSITIVE"
    elif grep -q "^// ! TYPE: NEGATIVE" "$test_file" 2>/dev/null; then
        echo "NEGATIVE"
    elif grep -q "^// ! TYPE: VARYING" "$test_file" 2>/dev/null; then
        echo "VARYING"
    else
        echo "UNKNOWN"
    fi
}

# Get test description from file
get_test_description() {
    local test_file="$1"
    # Extract IEEE section comment (lines starting with "//   " after standard line)
    sed -n '/^\/\/ IEEE Std 1800-2012/,/^\/\/ ! TYPE/p' "$test_file" 2>/dev/null | \
        grep "^//   " | sed 's/^\/\/   //' | tail -1 || echo "No description"
}

HAS_RG=0
if command -v rg >/dev/null 2>&1; then
    HAS_RG=1
fi

file_suggests_4state() {
    local file="$1"
    local pattern="([0-9]+'[bxzBXZ])|\\b(casex|casez|tri0|tri1|triand|trior|trireg|wand|wor|tranif0|tranif1|tran|cmos|bufif|notif|nmos|pmos|rnmos|rpmos|rtran|rtranif|pullup|pulldown)\\b|\\binout\\b"
    if [ "$HAS_RG" -eq 1 ]; then
        if rg -n --no-messages -U -e "$pattern" "$file" >/dev/null; then
            return 0
        fi
    else
        if grep -E -q "$pattern" "$file"; then
            return 0
        fi
    fi
    return 1
}

is_4state_error() {
    local log="$1"
    local pattern="requires --4state|x/z literals require --4state|tristate|switch primitives|net type requires --4state|multiple drivers"
    if [ "$HAS_RG" -eq 1 ]; then
        if rg -n --no-messages -i -e "$pattern" "$log" >/dev/null; then
            return 0
        fi
    else
        if grep -E -i -q "$pattern" "$log"; then
            return 0
        fi
    fi
    return 1
}

is_multi_top_error() {
    local log="$1"
    if [ "$HAS_RG" -eq 1 ]; then
        if rg -n --no-messages -e "multiple top-level modules found" "$log" >/dev/null; then
            return 0
        fi
    else
        if grep -E -q "multiple top-level modules found" "$log"; then
            return 0
        fi
    fi
    return 1
}

get_module_names() {
    local file="$1"
    {
        if [ "$HAS_RG" -eq 1 ]; then
            rg -n --no-messages "^[[:space:]]*module[[:space:]]+[A-Za-z_][A-Za-z0-9_$]*" "$file" \
                2>/dev/null || true
        else
            grep -E "^[[:space:]]*module[[:space:]]+[A-Za-z_][A-Za-z0-9_$]*" "$file" \
                2>/dev/null || true
        fi
    } | sed -E 's/^[^:]*:[[:space:]]*module[[:space:]]+([A-Za-z_][A-Za-z0-9_$]*).*/\1/' \
        | sort -u
}

run_metalfpga() {
    local -a cmd=("$@")
    local output
    local exit_code
    set +e
    output=$("${cmd[@]}" 2>&1)
    exit_code=$?
    set -e
    echo "$output"
    return $exit_code
}

CAPTURE_OUTPUT=""
CAPTURE_STATUS=0

capture_metalfpga() {
    local -a cmd=("$@")
    local output
    local exit_code
    set +e
    output=$(run_metalfpga "${cmd[@]}")
    exit_code=$?
    set -e
    CAPTURE_OUTPUT="$output"
    CAPTURE_STATUS=$exit_code
}

read_lines() {
    local array_name="$1"
    local line
    eval "${array_name}=()"
    while IFS= read -r line; do
        eval "${array_name}+=(\"\$line\")"
    done
}

# Run a single test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .sv)
    if [[ "$test_name" == "$(basename "$test_file")" ]]; then
        test_name=$(basename "$test_file" .v)
    fi
    local test_type=$(get_test_type "$test_file")
    local test_desc=$(get_test_description "$test_file")
    local use_4state=false
    local use_auto="$FORCE_AUTO"

    if [[ "${FORCE_4STATE}" == "true" ]]; then
        use_4state=true
    elif [[ "${FORCE_2STATE}" == "false" ]] && file_suggests_4state "$test_file"; then
        use_4state=true
    fi

    # Apply mode filter
    if [[ "${MODE}" != "all" ]]; then
        local mode_upper=$(echo "${MODE}" | tr '[:lower:]' '[:upper:]')
        if [[ "${test_type}" != "${mode_upper}" ]]; then
            return 0  # Skip this test
        fi
    fi

    ((TOTAL++))

    printf "[%3d] %-30s [%-8s] " "$TOTAL" "$test_name" "$test_type"

    if [[ "${DRY_RUN}" == "true" ]]; then
        echo -e "${BLUE}DRY RUN${NC}"
        return 0
    fi

    # Run metalfpga compilation
    local output
    local exit_code
    local -a cmd=("${METALFPGA}" "$test_file")
    if [[ "${use_auto}" == "true" ]]; then
        cmd+=("--auto")
    fi
    if [[ "${use_4state}" == "true" ]]; then
        cmd+=("--4state")
    fi

    capture_metalfpga "${cmd[@]}"
    output="$CAPTURE_OUTPUT"
    exit_code=$CAPTURE_STATUS
    if [[ $exit_code -ne 0 && "${use_4state}" == "false" && "${FORCE_2STATE}" == "false" ]]; then
        if is_4state_error <(printf '%s\n' "$output"); then
            use_4state=true
            cmd=("${METALFPGA}" "$test_file" "--4state")
            if [[ "${use_auto}" == "true" ]]; then
                cmd+=("--auto")
            fi
            capture_metalfpga "${cmd[@]}"
            output="$CAPTURE_OUTPUT"
            exit_code=$CAPTURE_STATUS
        fi
    fi
    if [[ "${VERBOSE}" == "true" ]]; then
        echo ""
        echo "$output"
    fi

    if [[ $exit_code -ne 0 ]] && [[ "${test_type}" != "NEGATIVE" ]] && is_multi_top_error <(printf '%s\n' "$output"); then
        local module_names
        module_names=$(get_module_names "$test_file")
        if [[ -n "$module_names" ]]; then
            local any_pass=0
            local any_output=""
            for top in $module_names; do
                local -a top_cmd=("${METALFPGA}" "$test_file" "--top" "$top")
                if [[ "${use_auto}" == "true" ]]; then
                    top_cmd+=("--auto")
                fi
                if [[ "${use_4state}" == "true" ]]; then
                    top_cmd+=("--4state")
                fi
                local top_output
                capture_metalfpga "${top_cmd[@]}"
                top_output="$CAPTURE_OUTPUT"
                local top_code=$CAPTURE_STATUS
                any_output="$top_output"
                if [[ $top_code -eq 0 ]]; then
                    any_pass=1
                fi
            done
            if [[ $any_pass -eq 1 ]]; then
                exit_code=0
                output="$any_output"
            else
                exit_code=1
                output="$any_output"
            fi
        fi
    fi

    # Evaluate result based on test type
    local result="UNKNOWN"
    case "${test_type}" in
        POSITIVE)
            if [[ $exit_code -eq 0 ]]; then
                result="PASS"
                ((PASSED++))
                echo -e "${GREEN}PASS${NC}"
            else
                result="FAIL"
                ((FAILED++))
                FAILED_TESTS+=("$test_name [POSITIVE should pass, but failed]")
                echo -e "${RED}FAIL${NC} (expected success, got error)"
                [[ "${VERBOSE}" == "false" ]] && echo "$output" | head -5
            fi
            ;;
        NEGATIVE)
            if [[ $exit_code -ne 0 ]]; then
                result="PASS"
                ((PASSED++))
                echo -e "${GREEN}PASS${NC} (correctly rejected)"
            else
                result="FAIL"
                ((FAILED++))
                FAILED_TESTS+=("$test_name [NEGATIVE should fail, but passed]")
                echo -e "${RED}FAIL${NC} (expected error, but succeeded)"
            fi
            ;;
        VARYING)
            # VARYING tests are informational
            if [[ $exit_code -eq 0 ]]; then
                result="VARY-PASS"
                ((PASSED++))
                ((VARYING++))
                echo -e "${YELLOW}VARY${NC} (implementation-defined, compiled OK)"
            else
                result="VARY-FAIL"
                ((VARYING++))
                echo -e "${YELLOW}VARY${NC} (implementation-defined, failed)"
            fi
            ;;
        *)
            ((SKIPPED++))
            echo -e "${YELLOW}SKIP${NC} (unknown test type)"
            ;;
    esac

    # Log to file
    {
        echo "=========================================="
        echo "Test: $test_name"
        echo "Type: $test_type"
        echo "Desc: $test_desc"
        echo "Result: $result"
        echo "Exit code: $exit_code"
        echo "Output:"
        echo "$output"
        echo ""
    } >> "$LOG_FILE"

    # Stop on failure if requested
    if [[ "${STOP_ON_FAIL}" == "true" && "${result}" == "FAIL" ]]; then
        echo -e "\n${RED}Stopping on first failure (--stop-on-fail)${NC}"
        exit 1
    fi
}

# Find all test files
echo -e "${BLUE}Discovering tests...${NC}"
read_lines TEST_FILES < <(find "$TESTS_DIR" -type f \( -name "test_*.sv" -o -name "test_*.v" \) | sort)

# Apply filter if specified
if [[ -n "${FILTER}" ]]; then
    FILTERED_FILES=()
    for test_file in "${TEST_FILES[@]}"; do
        if printf '%s\n' "$test_file" | grep -q "$FILTER"; then
            FILTERED_FILES+=("$test_file")
        fi
    done
    TEST_FILES=("${FILTERED_FILES[@]}")
fi

echo "Found ${#TEST_FILES[@]} test files"
echo ""

# Run tests
echo -e "${BLUE}Running tests...${NC}"
echo ""

if [[ "${PARALLEL}" == "true" ]]; then
    echo -e "${YELLOW}Parallel mode is experimental${NC}"
    export -f run_test get_test_type get_test_description file_suggests_4state is_4state_error is_multi_top_error get_module_names run_metalfpga capture_metalfpga
    export METALFPGA VERBOSE DRY_RUN LOG_FILE RED GREEN YELLOW BLUE NC
    printf '%s\n' "${TEST_FILES[@]}" | xargs -P "${MAX_JOBS}" -I {} bash -c 'run_test "$@"' _ {}
else
    for test_file in "${TEST_FILES[@]}"; do
        run_test "$test_file"
    done
fi

# Generate summary
echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Test Summary${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""
echo "Total tests:       $TOTAL"
echo -e "${GREEN}Passed:            $PASSED${NC}"
echo -e "${RED}Failed:            $FAILED${NC}"
echo -e "${YELLOW}Varying:           $VARYING${NC}"
echo "Skipped:           $SKIPPED"
echo ""

if [[ $TOTAL -gt 0 ]]; then
    PASS_RATE=$(awk "BEGIN {printf \"%.1f\", ($PASSED / $TOTAL) * 100}")
    echo "Pass rate:         ${PASS_RATE}%"
fi

# List failed tests
if [[ ${FAILED} -gt 0 ]]; then
    echo ""
    echo -e "${RED}Failed tests:${NC}"
    for failed in "${FAILED_TESTS[@]}"; do
        echo "  - $failed"
    done
fi

# Save summary to file
{
    echo "metalfpga IEEE 1800-2012 SystemVerilog Test Run Summary"
    echo "Timestamp: $(date)"
    echo "metalfpga: ${METALFPGA}"
    echo ""
    echo "Total tests:   $TOTAL"
    echo "Passed:        $PASSED"
    echo "Failed:        $FAILED"
    echo "Varying:       $VARYING"
    echo "Skipped:       $SKIPPED"
    echo "Pass rate:     ${PASS_RATE}%"
    echo ""
    if [[ ${FAILED} -gt 0 ]]; then
        echo "Failed tests:"
        for failed in "${FAILED_TESTS[@]}"; do
            echo "  - $failed"
        done
    fi
} > "$SUMMARY_FILE"

echo ""
echo "Full log:     $LOG_FILE"
echo "Summary:      $SUMMARY_FILE"
echo ""

# Exit with appropriate code
if [[ ${FAILED} -gt 0 ]]; then
    exit 1
else
    exit 0
fi
