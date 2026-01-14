#!/usr/bin/env bash
# Emit MSL for the IEEE 1364-2005 suite and compile each file with xcrun metal.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TESTS_DIR="${SCRIPT_DIR}/ispras-sv-tests/ieee-1364-2005"
OUT_DIR="${REPO_ROOT}/artifacts/msl/full-suite"
METALFPGA="${METALFPGA_CLI:-}"

usage() {
  cat <<EOF
Usage: $0 [--tests-dir PATH] [--out-dir PATH] [--metalfpga PATH]

Defaults:
  --tests-dir  ${TESTS_DIR}
  --out-dir    ${OUT_DIR}
  --metalfpga  ${REPO_ROOT}/build/metalfpga_cli (or METALFPGA_CLI)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --tests-dir)
      TESTS_DIR="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --metalfpga)
      METALFPGA="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

if [[ -z "${METALFPGA}" ]]; then
  if [[ -x "${REPO_ROOT}/build/metalfpga_cli" ]]; then
    METALFPGA="${REPO_ROOT}/build/metalfpga_cli"
  else
    METALFPGA="metalfpga_cli"
  fi
fi

if ! command -v "${METALFPGA}" &>/dev/null; then
  echo "ERROR: metalfpga not found at: ${METALFPGA}"
  exit 1
fi

if [[ ! -d "${TESTS_DIR}" ]]; then
  echo "ERROR: tests directory not found: ${TESTS_DIR}"
  exit 1
fi

AIR_DIR="${OUT_DIR}/air"
mkdir -p "${OUT_DIR}" "${AIR_DIR}"

: > "${OUT_DIR}/emit_failures.log"
: > "${OUT_DIR}/compile_failures.log"
: > "${OUT_DIR}/compile_ok.log"
: > "${OUT_DIR}/compile_warnings.raw.log"
: > "${OUT_DIR}/compile_warnings.log"
: > "${OUT_DIR}/summary.txt"
: > "${OUT_DIR}/warnings_summary.txt"

TOTAL=0
EMITTED=0
COMPILE_OK=0
EMIT_FAILURES=0
COMPILE_FAILURES=0

while IFS= read -r vfile; do
  base="$(basename "${vfile}" .v)"
  TOTAL=$((TOTAL + 1))
  emit_log="${AIR_DIR}/${base}.emit.log"
  msl="${OUT_DIR}/${base}.msl"
  tmp_msl="${OUT_DIR}/.tmp_${base}.msl"
  if "${METALFPGA}" "${vfile}" --auto --4state --emit-msl "${tmp_msl}" \
      >"${emit_log}" 2>&1; then
    mv -f "${tmp_msl}" "${msl}"
    EMITTED=$((EMITTED + 1))
  else
    rm -f "${tmp_msl}"
    echo "${vfile}" >> "${OUT_DIR}/emit_failures.log"
    EMIT_FAILURES=$((EMIT_FAILURES + 1))
    continue
  fi
  air_out="${AIR_DIR}/${base}.air"
  metal_log="${AIR_DIR}/${base}.log"
  if xcrun metal -c "${msl}" -o "${air_out}" >"${metal_log}" 2>&1; then
    echo "${vfile}" >> "${OUT_DIR}/compile_ok.log"
    COMPILE_OK=$((COMPILE_OK + 1))
  else
    echo "${vfile}" >> "${OUT_DIR}/compile_failures.log"
    COMPILE_FAILURES=$((COMPILE_FAILURES + 1))
  fi
done < <(find "${TESTS_DIR}" -name "*.v" -type f | sort)

if command -v rg >/dev/null 2>&1; then
  rg -n "warning:" "${AIR_DIR}" > "${OUT_DIR}/compile_warnings.raw.log" || true
  rg -v "linker' input unused" "${OUT_DIR}/compile_warnings.raw.log" \
      > "${OUT_DIR}/compile_warnings.log" || true
else
  grep -R -n "warning:" "${AIR_DIR}" > "${OUT_DIR}/compile_warnings.raw.log" || true
  grep -v "linker' input unused" "${OUT_DIR}/compile_warnings.raw.log" \
      > "${OUT_DIR}/compile_warnings.log" || true
fi

RAW_WARNINGS="$(wc -l < "${OUT_DIR}/compile_warnings.raw.log" | tr -d ' ')"
FILTERED_WARNINGS="$(wc -l < "${OUT_DIR}/compile_warnings.log" | tr -d ' ')"
echo "RAW_WARNINGS=${RAW_WARNINGS}" > "${OUT_DIR}/warnings_summary.txt"
echo "FILTERED_WARNINGS=${FILTERED_WARNINGS}" >> "${OUT_DIR}/warnings_summary.txt"

{
  echo "TOTAL=${TOTAL}"
  echo "EMITTED=${EMITTED}"
  echo "COMPILE_OK=${COMPILE_OK}"
  echo "EMIT_FAILURES=${EMIT_FAILURES}"
  echo "COMPILE_FAILURES=${COMPILE_FAILURES}"
} > "${OUT_DIR}/summary.txt"

echo "Done. Summary written to ${OUT_DIR}/summary.txt"
