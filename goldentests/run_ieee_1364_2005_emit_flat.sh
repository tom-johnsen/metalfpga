#!/usr/bin/env bash
# Emit flat maps for all IEEE 1364-2005 Verilog tests.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TESTS_DIR="$(cd "${SCRIPT_DIR}/ispras-sv-tests/ieee-1364-2005" && pwd)"
METALFPGA="${METALFPGA_CLI:-}"
OUT_DIR="${REPO_ROOT}/artifacts/flatmaps"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="${RESULTS_DIR}/flatmap_emit_${TIMESTAMP}.log"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

FILTER=""
DRY_RUN=false
VERBOSE=false
STOP_ON_FAIL=false
DEFAULT_4STATE=true
FORCE_4STATE="${DEFAULT_4STATE}"
FORCE_2STATE=false
FORCE_AUTO=false
RUN_MODE=false
SCHED_VM=false
COMPARE=false
COMPARE_OUT="${REPO_ROOT}/docs/IEEE_1364_2005_FLATMAP_COMPARISON.md"

usage() {
  cat <<EOF
Usage: $0 [OPTIONS]

Emit flat maps for all IEEE 1364-2005 Verilog tests.

OPTIONS:
    -h, --help              Show this help message
    -f, --filter PATTERN    Only run tests matching PATTERN (e.g., "test_05_*")
    -d, --dry-run           Show what would be emitted without running
    -v, --verbose           Show each command
    -s, --stop-on-fail      Stop on first failure
    --out-dir PATH          Output directory (default: ${OUT_DIR})
    --compare               Generate flatmap comparison matrix (Markdown)
    --compare-out PATH      Comparison output path (default: ${COMPARE_OUT})
    --4state                Pass --4state to metalfpga_cli (default)
    --2state                Force 2-state (disable default --4state)
    --auto                  Pass --auto for include discovery
    --sched-vm              Pass --sched-vm to metalfpga_cli
    --run                   Pass --run to metalfpga_cli
    --metalfpga PATH        Path to metalfpga_cli binary

ENVIRONMENT:
    METALFPGA_CLI           Path to metalfpga binary (default: metalfpga_cli)
EOF
}

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

run_metalfpga_cmd() {
  local -a cmd=("$@")
  local run_dir="${REPO_ROOT:-$PWD}"
  (cd "$run_dir" && "${cmd[@]}")
}

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      usage
      exit 0
      ;;
    -f|--filter)
      FILTER="$2"
      shift 2
      ;;
    -d|--dry-run)
      DRY_RUN=true
      shift
      ;;
    -v|--verbose)
      VERBOSE=true
      shift
      ;;
    -s|--stop-on-fail)
      STOP_ON_FAIL=true
      shift
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --compare)
      COMPARE=true
      shift
      ;;
    --compare-out)
      COMPARE_OUT="$2"
      shift 2
      ;;
    --4state)
      FORCE_4STATE=true
      shift
      ;;
    --2state)
      FORCE_2STATE=true
      FORCE_4STATE=false
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

compare_flatmaps() {
  local out_path="$1"
  local tests_dir="$2"
  local flat_dir="$3"
  rm -f "${out_path}"
  PYTHONDONTWRITEBYTECODE=1 python3 - "${out_path}" "${tests_dir}" "${flat_dir}" <<'PY'
import re
import sys
from pathlib import Path

out_path = Path(sys.argv[1])
tests_dir = Path(sys.argv[2])
flat_dir = Path(sys.argv[3])

def parse_flat(path: Path):
    top = ""
    ports = assigns = blocks = namemap = 0
    section = ""
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    except FileNotFoundError:
        return top, ports, assigns, blocks, namemap
    for line in lines:
        if line.startswith("Top:"):
            top = line.split(":", 1)[1].strip()
            continue
        if line.startswith("Ports:"):
            section = "ports"
            continue
        if line.startswith("Nets:"):
            section = "nets"
            continue
        if line.startswith("Assigns:"):
            section = "assigns"
            continue
        if line.startswith("Switches:"):
            section = "switches"
            continue
        if line.startswith("Always blocks:"):
            section = "blocks"
            continue
        if line.startswith("Flat name map:"):
            section = "namemap"
            continue
        if line.startswith("  - "):
            if section == "ports":
                ports += 1
            elif section == "assigns":
                assigns += 1
            elif section == "blocks":
                blocks += 1
            elif section == "namemap":
                namemap += 1
    return top, ports, assigns, blocks, namemap

def preprocess(text: str) -> str:
    lines = text.splitlines()
    out_lines = []
    defined = set()
    stack = []
    active = True

    for line in lines:
        m = re.match(r"\s*`ifdef\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m:
            name = m.group(1)
            cond = name in defined
            stack.append([active, cond, cond])
            active = active and cond
            continue

        m = re.match(r"\s*`ifndef\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m:
            name = m.group(1)
            cond = name not in defined
            stack.append([active, cond, cond])
            active = active and cond
            continue

        m = re.match(r"\s*`elsif\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m and stack:
            name = m.group(1)
            parent_active, _, seen_true = stack[-1]
            cond = (name in defined) and (not seen_true)
            stack[-1][1] = cond
            stack[-1][2] = seen_true or cond
            active = parent_active and cond
            continue

        if re.match(r"\s*`else\b", line):
            if stack:
                parent_active, _, seen_true = stack[-1]
                cond = not seen_true
                stack[-1][1] = cond
                stack[-1][2] = True
                active = parent_active and cond
            continue

        if re.match(r"\s*`endif\b", line):
            if stack:
                active = stack.pop()[0]
            continue

        m = re.match(r"\s*`define\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m:
            if active:
                defined.add(m.group(1))
            continue

        m = re.match(r"\s*`undef\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m:
            if active:
                defined.discard(m.group(1))
            continue

        if active:
            out_lines.append(line)

    return "\n".join(out_lines)

def parse_verilog(path: Path, top: str):
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except FileNotFoundError:
        return 0, 0, 0, 0, 0

    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*", "", text)
    text = preprocess(text)

    module_re = re.compile(r"\bmodule\s+([A-Za-z_][A-Za-z0-9_$]*)\b(.*?)\bendmodule\b", re.S)
    target = None
    for m in module_re.finditer(text):
        if m.group(1) == top:
            target = m.group(2)
            break

    if target is None:
        return 0, 0, 0, 0, 0

    if ";" in target:
        header, body = target.split(";", 1)
    else:
        header, body = target, ""

    def extract_port_list(header_text: str) -> str:
        header_text = re.sub(r"\(\*.*?\*\)", "", header_text, flags=re.S)
        i = 0
        n = len(header_text)
        while i < n and header_text[i].isspace():
            i += 1
        if i < n and header_text[i] == "#":
            i += 1
            while i < n and header_text[i].isspace():
                i += 1
            if i < n and header_text[i] == "(":
                depth = 0
                while i < n:
                    if header_text[i] == "(":
                        depth += 1
                    elif header_text[i] == ")":
                        depth -= 1
                        if depth == 0:
                            i += 1
                            break
                    i += 1
        while i < n and header_text[i].isspace():
            i += 1
        if i < n and header_text[i] == "(":
            depth = 0
            start = i + 1
            i += 1
            while i < n:
                if header_text[i] == "(":
                    depth += 1
                elif header_text[i] == ")":
                    if depth == 0:
                        end = i
                        return header_text[start:end]
                    depth -= 1
                i += 1
        return ""

    port_text = extract_port_list(header).strip()
    if port_text:
        parts = [p.strip() for p in port_text.split(",")]
        port_count = sum(1 for p in parts if p)
    else:
        port_count = 0

    assign_count = len(re.findall(r"\bassign\b", body))
    always_count = len(re.findall(r"\balways\b", body))
    initial_count = len(re.findall(r"\binitial\b", body))

    decl_init_count = 0
    decl_re = re.compile(r"^(reg|integer|real|time|realtime|event)\b")
    assign_tok_re = re.compile(r"(?<![=!<>])=(?!=)")
    for stmt in body.split(";"):
        s = stmt.strip()
        if not s:
            continue
        s = re.sub(r"^\(\*.*?\*\)\s*", "", s, flags=re.S)
        if decl_re.match(s):
            decl_init_count += len(assign_tok_re.findall(s))

    inst_count = 0
    keywords = {
        "assign", "always", "initial", "if", "else", "for", "while", "case",
        "casez", "casex", "endcase", "function", "endfunction", "task", "endtask",
        "module", "endmodule", "input", "output", "inout", "wire", "reg", "tri",
        "integer", "real", "realtime", "time", "event", "parameter", "localparam",
        "generate", "genvar", "endgenerate", "begin", "end", "disable", "fork",
        "join", "specify", "endspecify", "specparam",
    }
    inst_re = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_$]*)\s+([A-Za-z_][A-Za-z0-9_$]*)\s*\(")
    for stmt in body.split(";"):
        s = stmt.strip()
        if not s:
            continue
        s = re.sub(r"^\(\*.*?\*\)\s*", "", s, flags=re.S)
        m = inst_re.match(s)
        if m and m.group(1) not in keywords:
            inst_count += 1

    block_count = always_count + initial_count + decl_init_count
    return 1, port_count, assign_count, block_count, inst_count

rows = []
warn_rows = []
fail_rows = []
total = ok = warn = fail = 0

for flat_file in sorted(flat_dir.glob("test_*.flat")):
    v_file = tests_dir / f"{flat_file.stem}.v"
    test_name = flat_file.stem

    total += 1

    flat_top, flat_ports, flat_assigns, flat_blocks, flat_namemap = parse_flat(flat_file)
    v_found, v_ports, v_assigns, v_blocks, v_insts = parse_verilog(v_file, flat_top)

    status = "OK"
    notes = []

    if not flat_top or v_found == 0:
        status = "FAIL"
        notes = ["missing flat top or matching .v module"]
        fail += 1
        fail_rows.append((test_name, notes))
    else:
        if v_ports != flat_ports:
            if v_ports > 0 and flat_ports == 0:
                notes.append("ports declared but none in flat")
            else:
                notes.append("ports mismatch")
        if v_assigns != flat_assigns:
            if v_assigns > 0 and flat_assigns == 0:
                notes.append("assigns declared but none in flat")
            else:
                notes.append("assigns mismatch")
        if v_blocks != flat_blocks:
            if v_blocks > 0 and flat_blocks == 0:
                notes.append("always/initial declared but none in flat")
            else:
                notes.append("blocks mismatch")
        if flat_namemap == 0 and (v_ports > 0 or v_assigns > 0 or v_blocks > 0 or v_insts > 0):
            notes.append("flat name map empty")

        if notes:
            status = "WARN"
            warn += 1
            warn_rows.append((test_name, notes))
        else:
            ok += 1

    rows.append(
        f"| {test_name} | {status} | {flat_top} | {v_ports}/{flat_ports} | {v_assigns}/{flat_assigns} | {v_blocks}/{flat_blocks} | {v_insts} | {flat_namemap} | {'; '.join(notes)} |"
    )

out_path.parent.mkdir(parents=True, exist_ok=True)
with out_path.open("w", encoding="utf-8") as out:
    out.write("# IEEE 1364-2005 Flatmap Comparison Matrix\n\n")
    out.write("Auto-generated comparison between `artifacts/flatmaps/*.flat` and their corresponding test `.v` files.\n")
    out.write("These checks are structural heuristics (top module, ports, assigns, always/initial blocks, name map coverage) and are not a full semantic proof.\n\n")
    out.write(f"**Total flatmaps:** {total}\n")
    out.write(f"**OK:** {ok}\n")
    out.write(f"**WARN:** {warn}\n")
    out.write(f"**FAIL:** {fail}\n\n")
    out.write("## Legend\n")
    out.write("- `OK`: heuristics match expected structure\n")
    out.write("- `WARN`: possible mismatch or missing coverage; needs manual review\n")
    out.write("- `FAIL`: missing flat top or matching `.v` module\n\n")
    out.write("## Comparison Matrix\n\n")
    out.write("| Test | Status | Top | Ports (v/flat) | Assigns (v/flat) | Blocks (v/flat) | Insts (v) | NameMap | Notes |\n")
    out.write("| --- | --- | --- | --- | --- | --- | --- | --- | --- |\n")
    out.write("\n".join(rows))

print(f"\nComparison summary: OK={ok} WARN={warn} FAIL={fail}")
for name, notes in fail_rows:
    print(f"COMPARE FAIL {name}: {'; '.join(notes)}")
for name, notes in warn_rows:
    print(f"COMPARE WARN {name}: {'; '.join(notes)}")
print(f"\nComparison matrix written to {out_path}")
PY
}
compare_one() {
  local flat_file="$1"
  local v_file="$2"
  PYTHONDONTWRITEBYTECODE=1 python3 - "${flat_file}" "${v_file}" <<'PY'
import re
import sys
from pathlib import Path

flat_path = Path(sys.argv[1])
v_path = Path(sys.argv[2])

def parse_flat(path: Path):
    top = ""
    ports = assigns = blocks = namemap = 0
    section = ""
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    except FileNotFoundError:
        return top, ports, assigns, blocks, namemap
    for line in lines:
        if line.startswith("Top:"):
            top = line.split(":", 1)[1].strip()
            continue
        if line.startswith("Ports:"):
            section = "ports"
            continue
        if line.startswith("Nets:"):
            section = "nets"
            continue
        if line.startswith("Assigns:"):
            section = "assigns"
            continue
        if line.startswith("Switches:"):
            section = "switches"
            continue
        if line.startswith("Always blocks:"):
            section = "blocks"
            continue
        if line.startswith("Flat name map:"):
            section = "namemap"
            continue
        if line.startswith("  - "):
            if section == "ports":
                ports += 1
            elif section == "assigns":
                assigns += 1
            elif section == "blocks":
                blocks += 1
            elif section == "namemap":
                namemap += 1
    return top, ports, assigns, blocks, namemap

def preprocess(text: str) -> str:
    lines = text.splitlines()
    out_lines = []
    defined = set()
    stack = []
    active = True

    for line in lines:
        m = re.match(r"\s*`ifdef\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m:
            name = m.group(1)
            cond = name in defined
            stack.append([active, cond, cond])
            active = active and cond
            continue

        m = re.match(r"\s*`ifndef\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m:
            name = m.group(1)
            cond = name not in defined
            stack.append([active, cond, cond])
            active = active and cond
            continue

        m = re.match(r"\s*`elsif\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m and stack:
            name = m.group(1)
            parent_active, _, seen_true = stack[-1]
            cond = (name in defined) and (not seen_true)
            stack[-1][1] = cond
            stack[-1][2] = seen_true or cond
            active = parent_active and cond
            continue

        if re.match(r"\s*`else\b", line):
            if stack:
                parent_active, _, seen_true = stack[-1]
                cond = not seen_true
                stack[-1][1] = cond
                stack[-1][2] = True
                active = parent_active and cond
            continue

        if re.match(r"\s*`endif\b", line):
            if stack:
                active = stack.pop()[0]
            continue

        m = re.match(r"\s*`define\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m:
            if active:
                defined.add(m.group(1))
            continue

        m = re.match(r"\s*`undef\s+([A-Za-z_][A-Za-z0-9_$]*)", line)
        if m:
            if active:
                defined.discard(m.group(1))
            continue

        if active:
            out_lines.append(line)

    return "\n".join(out_lines)

def parse_verilog(path: Path, top: str):
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except FileNotFoundError:
        return 0, 0, 0, 0, 0

    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*", "", text)
    text = preprocess(text)

    module_re = re.compile(r"\bmodule\s+([A-Za-z_][A-Za-z0-9_$]*)\b(.*?)\bendmodule\b", re.S)
    target = None
    for m in module_re.finditer(text):
        if m.group(1) == top:
            target = m.group(2)
            break

    if target is None:
        return 0, 0, 0, 0, 0

    if ";" in target:
        header, body = target.split(";", 1)
    else:
        header, body = target, ""

    def extract_port_list(header_text: str) -> str:
        header_text = re.sub(r"\(\*.*?\*\)", "", header_text, flags=re.S)
        i = 0
        n = len(header_text)
        while i < n and header_text[i].isspace():
            i += 1
        if i < n and header_text[i] == "#":
            i += 1
            while i < n and header_text[i].isspace():
                i += 1
            if i < n and header_text[i] == "(":
                depth = 0
                while i < n:
                    if header_text[i] == "(":
                        depth += 1
                    elif header_text[i] == ")":
                        depth -= 1
                        if depth == 0:
                            i += 1
                            break
                    i += 1
        while i < n and header_text[i].isspace():
            i += 1
        if i < n and header_text[i] == "(":
            depth = 0
            start = i + 1
            i += 1
            while i < n:
                if header_text[i] == "(":
                    depth += 1
                elif header_text[i] == ")":
                    if depth == 0:
                        end = i
                        return header_text[start:end]
                    depth -= 1
                i += 1
        return ""

    port_text = extract_port_list(header).strip()
    if port_text:
        parts = [p.strip() for p in port_text.split(",")]
        port_count = sum(1 for p in parts if p)
    else:
        port_count = 0

    assign_count = len(re.findall(r"\bassign\b", body))
    always_count = len(re.findall(r"\balways\b", body))
    initial_count = len(re.findall(r"\binitial\b", body))

    decl_init_count = 0
    decl_re = re.compile(r"^(reg|integer|real|time|realtime|event)\b")
    assign_tok_re = re.compile(r"(?<![=!<>])=(?!=)")
    for stmt in body.split(";"):
        s = stmt.strip()
        if not s:
            continue
        s = re.sub(r"^\(\*.*?\*\)\s*", "", s, flags=re.S)
        if decl_re.match(s):
            decl_init_count += len(assign_tok_re.findall(s))

    inst_count = 0
    keywords = {
        "assign", "always", "initial", "if", "else", "for", "while", "case",
        "casez", "casex", "endcase", "function", "endfunction", "task", "endtask",
        "module", "endmodule", "input", "output", "inout", "wire", "reg", "tri",
        "integer", "real", "realtime", "time", "event", "parameter", "localparam",
        "generate", "genvar", "endgenerate", "begin", "end", "disable", "fork",
        "join", "specify", "endspecify", "specparam",
    }
    inst_re = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_$]*)\s+([A-Za-z_][A-Za-z0-9_$]*)\s*\(")
    for stmt in body.split(";"):
        s = stmt.strip()
        if not s:
            continue
        s = re.sub(r"^\(\*.*?\*\)\s*", "", s, flags=re.S)
        m = inst_re.match(s)
        if m and m.group(1) not in keywords:
            inst_count += 1

    block_count = always_count + initial_count + decl_init_count
    return 1, port_count, assign_count, block_count, inst_count

flat_top, flat_ports, flat_assigns, flat_blocks, flat_namemap = parse_flat(flat_path)
if not flat_top:
    print("FAIL|missing flat top")
    raise SystemExit(0)

v_found, v_ports, v_assigns, v_blocks, v_insts = parse_verilog(v_path, flat_top)
if v_found == 0:
    print("FAIL|missing flat top or matching .v module")
    raise SystemExit(0)

notes = []
if v_ports != flat_ports:
    if v_ports > 0 and flat_ports == 0:
        notes.append("ports declared but none in flat")
    else:
        notes.append("ports mismatch")
if v_assigns != flat_assigns:
    if v_assigns > 0 and flat_assigns == 0:
        notes.append("assigns declared but none in flat")
    else:
        notes.append("assigns mismatch")
if v_blocks != flat_blocks:
    if v_blocks > 0 and flat_blocks == 0:
        notes.append("always/initial declared but none in flat")
    else:
        notes.append("blocks mismatch")
if flat_namemap == 0 and (v_ports > 0 or v_assigns > 0 or v_blocks > 0 or v_insts > 0):
    notes.append("flat name map empty")

status = "OK" if not notes else "WARN"
print(f"{status}|{'; '.join(notes)}")
PY
}
mkdir -p "${RESULTS_DIR}"
mkdir -p "${OUT_DIR}"

echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  metalfpga IEEE 1364-2005 Flatmap Emitter${NC}"
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
echo "Output dir:     ${OUT_DIR}"
echo "Filter:         ${FILTER:-none}"
echo "Dry run:        ${DRY_RUN}"
[[ "${SCHED_VM}" == "true" ]] && echo "Sched VM:       enabled"
[[ "${RUN_MODE}" == "true" ]] && echo "Run:            enabled"
echo "Results log:    ${LOG_FILE}"
echo ""

if ! command -v "${METALFPGA}" &> /dev/null; then
  echo -e "${RED}ERROR: metalfpga not found at: ${METALFPGA}${NC}"
  echo "Set METALFPGA_CLI environment variable or use --metalfpga option"
  exit 1
fi

EMIT_FLAT_SUPPORTED=0
emit_help="$("${METALFPGA}" --help 2>&1 || true)"
if echo "${emit_help}" | grep -q -- "--emit-flat"; then
  EMIT_FLAT_SUPPORTED=1
fi
if [[ "${EMIT_FLAT_SUPPORTED}" -eq 1 ]]; then
  echo "Emit mode:      --emit-flat"
else
  echo "Emit mode:      --dump-flat (stdout redirect)"
fi
echo ""

TOTAL=0
EMITTED=0
FAILED=0
SKIPPED=0

echo "Writing log to ${LOG_FILE}"
echo "Flatmap emit run at $(date)" > "${LOG_FILE}"
echo "Tests dir: ${TESTS_DIR}" >> "${LOG_FILE}"
echo "Output dir: ${OUT_DIR}" >> "${LOG_FILE}"
echo "" >> "${LOG_FILE}"

while IFS= read -r test_file; do
  rel_path="${test_file#$TESTS_DIR/}"
  if [[ -n "${FILTER}" ]]; then
    case "${rel_path}" in
      ${FILTER}) ;;
      *) SKIPPED=$((SKIPPED + 1)); continue ;;
    esac
  fi
  TOTAL=$((TOTAL + 1))
  test_name="$(basename "${test_file}" .v)"
  test_type="$(get_test_type "${test_file}")"
  if [[ "${test_type}" == "NEGATIVE" || "${test_type}" == "UNKNOWN" ]]; then
    SKIPPED=$((SKIPPED + 1))
    if [[ "${VERBOSE}" == "true" ]]; then
      echo "[$TOTAL] ${test_name} SKIP (${test_type})"
    else
      printf "[%3d] %-28s " "${TOTAL}" "${test_name}"
      echo -e "${YELLOW}SKIP${NC}"
    fi
    echo "== ${test_file} (SKIP ${test_type})" >> "${LOG_FILE}"
    continue
  fi
  out_path="${OUT_DIR}/${rel_path%.v}.flat"
  mkdir -p "$(dirname "${out_path}")"

  cmd=("${METALFPGA}" "${test_file}")
  if [[ "${FORCE_AUTO}" == "true" ]]; then
    cmd+=("--auto")
  fi
  if [[ "${SCHED_VM}" == "true" ]]; then
    cmd+=("--sched-vm")
  fi
  if [[ "${RUN_MODE}" == "true" ]]; then
    cmd+=("--run")
  fi
  if [[ "${FORCE_4STATE}" == "true" ]]; then
    cmd+=("--4state")
  elif [[ "${FORCE_2STATE}" == "true" ]]; then
    :
  fi
  if [[ "${EMIT_FLAT_SUPPORTED}" -eq 1 ]]; then
    cmd+=("--emit-flat" "${out_path}")
  else
    cmd+=("--dump-flat")
  fi

  if [[ "${VERBOSE}" == "true" ]]; then
    echo "[$TOTAL] ${cmd[*]}"
  else
    printf "[%3d] %-28s " "${TOTAL}" "${test_name}"
  fi

  if [[ "${DRY_RUN}" == "true" ]]; then
    echo "DRY RUN"
    continue
  fi

  echo "== ${test_file} -> ${out_path}" >> "${LOG_FILE}"
  if [[ "${EMIT_FLAT_SUPPORTED}" -eq 1 ]]; then
    if run_metalfpga_cmd "${cmd[@]}" >> "${LOG_FILE}" 2>&1; then
      EMITTED=$((EMITTED + 1))
      if [[ "${VERBOSE}" != "true" ]]; then
        emit_msg="${GREEN}EMIT OK${NC}"
        if [[ "${COMPARE}" == "true" ]]; then
          compare_result="$(compare_one "${out_path}" "${test_file}")"
          if [[ -z "${compare_result}" ]]; then
            compare_status="ERROR"
            compare_notes="compare failed"
          else
            compare_status="${compare_result%%|*}"
            compare_notes="${compare_result#*|}"
          fi
          case "${compare_status}" in
            OK) cmp_msg="${GREEN}COMPARE OK${NC}" ;;
            WARN) cmp_msg="${YELLOW}COMPARE WARN${NC}" ;;
            FAIL) cmp_msg="${RED}COMPARE FAIL${NC}" ;;
            *) cmp_msg="${YELLOW}COMPARE ?${NC}" ;;
          esac
          if [[ -n "${compare_notes}" ]]; then
            cmp_msg="${cmp_msg} ${compare_notes}"
          fi
          echo -e "${emit_msg} ${cmp_msg}"
        else
          echo -e "${emit_msg}"
        fi
      fi
    else
      FAILED=$((FAILED + 1))
      if [[ "${VERBOSE}" != "true" ]]; then
        if [[ "${COMPARE}" == "true" ]]; then
          echo -e "${RED}EMIT FAIL${NC} ${YELLOW}COMPARE SKIP${NC}"
        else
          echo -e "${RED}FAIL${NC}"
        fi
      fi
      if [[ "${STOP_ON_FAIL}" == "true" ]]; then
        echo "Stopping on first failure."
        break
      fi
    fi
  else
    if run_metalfpga_cmd "${cmd[@]}" > "${out_path}" 2>> "${LOG_FILE}"; then
      EMITTED=$((EMITTED + 1))
      if [[ "${VERBOSE}" != "true" ]]; then
        emit_msg="${GREEN}EMIT OK${NC}"
        if [[ "${COMPARE}" == "true" ]]; then
          compare_result="$(compare_one "${out_path}" "${test_file}")"
          if [[ -z "${compare_result}" ]]; then
            compare_status="ERROR"
            compare_notes="compare failed"
          else
            compare_status="${compare_result%%|*}"
            compare_notes="${compare_result#*|}"
          fi
          case "${compare_status}" in
            OK) cmp_msg="${GREEN}COMPARE OK${NC}" ;;
            WARN) cmp_msg="${YELLOW}COMPARE WARN${NC}" ;;
            FAIL) cmp_msg="${RED}COMPARE FAIL${NC}" ;;
            *) cmp_msg="${YELLOW}COMPARE ?${NC}" ;;
          esac
          if [[ -n "${compare_notes}" ]]; then
            cmp_msg="${cmp_msg} ${compare_notes}"
          fi
          echo -e "${emit_msg} ${cmp_msg}"
        else
          echo -e "${emit_msg}"
        fi
      fi
    else
      FAILED=$((FAILED + 1))
      rm -f "${out_path}"
      if [[ "${VERBOSE}" != "true" ]]; then
        if [[ "${COMPARE}" == "true" ]]; then
          echo -e "${RED}EMIT FAIL${NC} ${YELLOW}COMPARE SKIP${NC}"
        else
          echo -e "${RED}FAIL${NC}"
        fi
      fi
      if [[ "${STOP_ON_FAIL}" == "true" ]]; then
        echo "Stopping on first failure."
        break
      fi
    fi
  fi
done < <(find "${TESTS_DIR}" -type f -name "test_*.v" | sort)

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Flatmap Emit Summary${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""
echo "Total tests:    ${TOTAL}"
echo "Emitted:        ${EMITTED}"
echo "Failed:         ${FAILED}"
echo "Skipped:        ${SKIPPED}"
echo "Log:            ${LOG_FILE}"

if [[ "${COMPARE}" == "true" ]]; then
  compare_flatmaps "${COMPARE_OUT}" "${TESTS_DIR}" "${OUT_DIR}"
fi
