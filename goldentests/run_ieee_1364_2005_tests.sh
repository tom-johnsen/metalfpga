#!/usr/bin/env bash
# IEEE 1364-2005 Test Suite Runner for metalfpga
# Runs all 355 ISPRAS Verilog-2005 compliance tests

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TESTS_DIR="$(cd "${SCRIPT_DIR}/ispras-sv-tests/ieee-1364-2005" && pwd)"
METALFPGA="${METALFPGA_CLI:-}"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="${RESULTS_DIR}/test_run_${TIMESTAMP}.log"
SUMMARY_FILE="${RESULTS_DIR}/summary_${TIMESTAMP}.txt"

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
declare -a FALLBACK_TESTS

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
RUN_MODE=false
SCHED_VM=false
FALLBACK_DIAG=false
FALLBACK_CAPTURE_DIR=""
FALLBACK_CAPTURED_PATH=""
RUN_SKIP_TESTS=(
    "test_09_09_02_1"
    "test_09_09_02_2"
)

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run IEEE 1364-2005 compliance tests for metalfpga.

OPTIONS:
    -h, --help              Show this help message
    -v, --verbose           Verbose output (show compilation details)
    -s, --stop-on-fail      Stop on first failure
    -f, --filter PATTERN    Only run tests matching PATTERN (e.g., "test_05_*")
    -m, --mode MODE         Test mode: all, positive, negative, varying (default: all)
    -d, --dry-run           Show what would be tested without running
    -p, --parallel          Run tests in parallel (experimental)
    -j, --jobs N            Number of parallel jobs (default: 4)
    --4state                Force --4state for all tests
    --2state                Force 2-state (disable --4state retries)
    --auto                  Pass --auto for include discovery
    --sched-vm              Pass --sched-vm to metalfpga_cli
    --fallback-diag         Emit sched-vm fallback diagnostics and capture fallbacks
    --run                   Pass --run to metalfpga_cli
    --metalfpga PATH        Path to metalfpga_cli binary

EXAMPLES:
    # Run all tests
    $0

    # Run only Chapter 5 (Expressions) tests
    $0 --filter "test_05_*"

    # Run only POSITIVE tests verbosely
    $0 --mode positive --verbose

    # Run tests in parallel with 8 jobs
    $0 --parallel --jobs 8

    # Dry run to see what would be tested
    $0 --filter "test_03_*" --dry-run

ENVIRONMENT:
    METALFPGA_CLI           Path to metalfpga binary (default: metalfpga_cli)

OUTPUT:
    Results are logged to: ${RESULTS_DIR}/
    - test_run_TIMESTAMP.log     Full test log
    - summary_TIMESTAMP.txt      Summary report
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
    --run)
        RUN_MODE=true
        shift
        ;;
    --sched-vm)
        SCHED_VM=true
        shift
        ;;
    --fallback-diag)
        FALLBACK_DIAG=true
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

# Fallback diagnostics are written to a fixed filename, so avoid parallel races.
if [[ "${FALLBACK_DIAG}" == "true" && "${PARALLEL}" == "true" ]]; then
    echo -e "${YELLOW}Fallback diag capture is not compatible with parallel mode; disabling parallel.${NC}"
    PARALLEL=false
fi

# Header
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  metalfpga IEEE 1364-2005 Test Suite Runner${NC}"
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
[[ "${SCHED_VM}" == "true" ]] && echo "Sched VM:       enabled"
[[ "${FALLBACK_DIAG}" == "true" ]] && echo "Fallback diag:  enabled"
[[ "${RUN_MODE}" == "true" ]] && echo "Run:            enabled"
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
    # Extract IEEE section comment (usually starts with "//   ")
    grep "^//   [0-9]" "$test_file" 2>/dev/null | head -1 | sed 's/^\/\/   //' || echo "No description"
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
    local run_dir="${REPO_ROOT:-$PWD}"
    set +e
    output=$(cd "$run_dir" && "${cmd[@]}" 2>&1)
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

sanitize_label() {
    local text="$1"
    printf '%s' "$text" | LC_ALL=C tr -c 'A-Za-z0-9_.-' '_'
}

should_skip_run_test() {
    local name="$1"
    for skip in "${RUN_SKIP_TESTS[@]}"; do
        if [[ "$name" == "$skip" ]]; then
            return 0
        fi
    done
    return 1
}

extract_fallback_diag_path() {
    local output="$1"
    printf '%s\n' "$output" | sed -n 's/.*wrote fallback diag to //p' | tail -1
}

fallback_diag_detected() {
    local diag_path="$1"
    awk '
      $1=="count:" && $2+0>0 {found=1}
      $1 ~ /^(assign|delay|force|release|service|service_ret)\[/ {found=1}
      $1=="pids:" && $2!="none" {found=1}
      END {exit found?0:1}
    ' "$diag_path"
}

capture_fallback_diag() {
    local test_name="$1"
    local label="$2"
    local diag_path="$3"
    FALLBACK_CAPTURED_PATH=""
    if [[ -z "$diag_path" || ! -f "$diag_path" ]]; then
        return 1
    fi
    if ! fallback_diag_detected "$diag_path"; then
        return 1
    fi
    if [[ -z "$FALLBACK_CAPTURE_DIR" ]]; then
        FALLBACK_CAPTURE_DIR="${RESULTS_DIR}/fallback_${TIMESTAMP}"
        mkdir -p "$FALLBACK_CAPTURE_DIR"
    fi
    local safe_test
    safe_test="$(sanitize_label "$test_name")"
    local safe_label=""
    if [[ -n "$label" ]]; then
        safe_label="_$(sanitize_label "$label")"
    fi
    local dest="${FALLBACK_CAPTURE_DIR}/${safe_test}${safe_label}.txt"
    cp "$diag_path" "$dest"
    FALLBACK_CAPTURED_PATH="$dest"
    FALLBACK_TESTS+=("${test_name}${label:+ (${label})}")
    return 0
}

# Run a single test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .v)
    local test_type=$(get_test_type "$test_file")
    local test_desc=$(get_test_description "$test_file")
    local use_4state=false
    local use_auto="$FORCE_AUTO"
    local diag_label=""

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

    printf "[%3d] %-25s [%-8s] " "$TOTAL" "$test_name" "$test_type"

    if [[ "${RUN_MODE}" == "true" ]] && should_skip_run_test "$test_name"; then
        ((SKIPPED++))
        echo -e "${YELLOW}SKIP${NC} (infinite loop under --run)"
        {
            echo "=========================================="
            echo "Test: $test_name"
            echo "Type: $test_type"
            echo "Desc: $test_desc"
            echo "Result: SKIP"
            echo "Exit code: 0"
            if [[ "${FALLBACK_DIAG}" == "true" ]]; then
                echo "Fallbacks: no"
            fi
            echo "Output:"
            echo "Skipped in --run mode (zero-delay/infinite loop)"
            echo ""
        } >> "$LOG_FILE"
        return 0
    fi

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
    if [[ "${SCHED_VM}" == "true" ]]; then
        cmd+=("--sched-vm")
    fi
    if [[ "${FALLBACK_DIAG}" == "true" ]]; then
        cmd+=("--fallback-diag")
    fi
    if [[ "${RUN_MODE}" == "true" ]]; then
        cmd+=("--run")
    fi
    if [[ "${use_4state}" == "true" ]]; then
        cmd+=("--4state")
    fi
    if [[ "${use_4state}" == "true" ]]; then
        diag_label="4state"
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
            if [[ "${SCHED_VM}" == "true" ]]; then
                cmd+=("--sched-vm")
            fi
            if [[ "${FALLBACK_DIAG}" == "true" ]]; then
                cmd+=("--fallback-diag")
            fi
            if [[ "${RUN_MODE}" == "true" ]]; then
                cmd+=("--run")
            fi
            capture_metalfpga "${cmd[@]}"
            output="$CAPTURE_OUTPUT"
            exit_code=$CAPTURE_STATUS
            diag_label="4state"
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
                if [[ "${SCHED_VM}" == "true" ]]; then
                    top_cmd+=("--sched-vm")
                fi
                if [[ "${FALLBACK_DIAG}" == "true" ]]; then
                    top_cmd+=("--fallback-diag")
                fi
                if [[ "${RUN_MODE}" == "true" ]]; then
                    top_cmd+=("--run")
                fi
                if [[ "${use_4state}" == "true" ]]; then
                    top_cmd+=("--4state")
                fi
                local top_output
                capture_metalfpga "${top_cmd[@]}"
                top_output="$CAPTURE_OUTPUT"
                local top_code=$CAPTURE_STATUS
                any_output="$top_output"
                diag_label="top_${top}"
                if [[ "${use_4state}" == "true" ]]; then
                    diag_label="${diag_label}_4state"
                fi
                if [[ $top_code -eq 0 ]]; then
                    any_pass=1
                else
                    any_fail=1
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

    local fallback_suffix=""
    local fallback_path=""
    if [[ "${FALLBACK_DIAG}" == "true" ]]; then
        local diag_path
        diag_path="$(extract_fallback_diag_path "$output")"
        if capture_fallback_diag "$test_name" "$diag_label" "$diag_path"; then
            fallback_suffix=" (fallback)"
            fallback_path="$FALLBACK_CAPTURED_PATH"
        fi
    fi

    # Evaluate result based on test type
    local result="UNKNOWN"
    case "${test_type}" in
        POSITIVE)
            if [[ $exit_code -eq 0 ]]; then
                result="PASS"
                ((PASSED++))
                echo -e "${GREEN}PASS${NC}${fallback_suffix}"
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
                echo -e "${GREEN}PASS${NC} (correctly rejected)${fallback_suffix}"
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
                echo -e "${YELLOW}VARY${NC} (implementation-defined, compiled OK)${fallback_suffix}"
            else
                result="VARY-FAIL"
                ((VARYING++))
                echo -e "${YELLOW}VARY${NC} (implementation-defined, failed)${fallback_suffix}"
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
        if [[ "${FALLBACK_DIAG}" == "true" ]]; then
            if [[ -n "$fallback_path" ]]; then
                echo "Fallbacks: yes"
                echo "Fallback diag: $fallback_path"
            else
                echo "Fallbacks: no"
            fi
        fi
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
read_lines TEST_FILES < <(find "$TESTS_DIR" -type f -name "test_*.v" | sort)

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
    export -f run_test get_test_type get_test_description file_suggests_4state is_4state_error is_multi_top_error get_module_names run_metalfpga capture_metalfpga sanitize_label extract_fallback_diag_path fallback_diag_detected capture_fallback_diag
    export METALFPGA VERBOSE DRY_RUN LOG_FILE RED GREEN YELLOW BLUE NC REPO_ROOT FALLBACK_DIAG RESULTS_DIR TIMESTAMP FALLBACK_CAPTURE_DIR
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

if [[ "${FALLBACK_DIAG}" == "true" ]]; then
    echo ""
    echo "Fallbacks detected: ${#FALLBACK_TESTS[@]}"
    if [[ ${#FALLBACK_TESTS[@]} -gt 0 ]]; then
        echo "Fallback diags: ${FALLBACK_CAPTURE_DIR}"
        for fallback in "${FALLBACK_TESTS[@]}"; do
            echo "  - $fallback"
        done
    fi
fi

# Save summary to file
{
    echo "metalfpga IEEE 1364-2005 Test Run Summary"
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
    if [[ "${FALLBACK_DIAG}" == "true" ]]; then
        echo ""
        echo "Fallbacks detected: ${#FALLBACK_TESTS[@]}"
        if [[ ${#FALLBACK_TESTS[@]} -gt 0 ]]; then
            echo "Fallback diags: ${FALLBACK_CAPTURE_DIR}"
            for fallback in "${FALLBACK_TESTS[@]}"; do
                echo "  - $fallback"
            done
        fi
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
