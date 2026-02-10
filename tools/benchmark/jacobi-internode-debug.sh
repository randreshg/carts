#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="/opt/carts"
BENCH_DIR="$ROOT_DIR/external/carts-benchmarks/polybench/jacobi2d"
ARTS_CFG="${ARTS_CFG:-$ROOT_DIR/docker/arts-docker.cfg}"
RUNS="${RUNS:-10}"
TIMEOUT_SECS="${TIMEOUT_SECS:-20}"
SIZE_TARGET="${SIZE_TARGET:-small}"
CAPTURE_BT="${CAPTURE_BT:-0}"
BT_TIMEOUT_SECS="${BT_TIMEOUT_SECS:-15}"
TIMEOUT_SIGNAL="${TIMEOUT_SIGNAL:-TERM}"

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --arts-cfg <path>      ARTS config file (default: $ARTS_CFG)
  --runs <n>             Number of ARTS runs (default: $RUNS)
  --timeout <seconds>    Timeout for each ARTS run (default: $TIMEOUT_SECS)
  --timeout-signal <sig> Signal sent by timeout (default: $TIMEOUT_SIGNAL)
  --size <small|medium>  Benchmark size target (default: $SIZE_TARGET)
  --capture-bt <0|1>     Capture gdb backtrace on failed runs (default: $CAPTURE_BT)
  --bt-timeout <seconds> Timeout for each gdb capture (default: $BT_TIMEOUT_SECS)
  --help                 Show this message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
  --arts-cfg)
    ARTS_CFG="$2"
    shift 2
    ;;
  --runs)
    RUNS="$2"
    shift 2
    ;;
  --timeout)
    TIMEOUT_SECS="$2"
    shift 2
    ;;
  --timeout-signal)
    TIMEOUT_SIGNAL="$2"
    shift 2
    ;;
  --size)
    SIZE_TARGET="$2"
    shift 2
    ;;
  --capture-bt)
    CAPTURE_BT="$2"
    shift 2
    ;;
  --bt-timeout)
    BT_TIMEOUT_SECS="$2"
    shift 2
    ;;
  --help)
    usage
    exit 0
    ;;
  *)
    echo "Unknown argument: $1" >&2
    usage
    exit 1
    ;;
  esac
done

if [[ ! -f "$ARTS_CFG" ]]; then
  echo "ARTS config not found: $ARTS_CFG" >&2
  exit 1
fi

timestamp="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="/tmp/jacobi-internode-debug-${timestamp}"
mkdir -p "$OUT_DIR"

summary_tsv="$OUT_DIR/summary.tsv"
echo -e "run\texit_code\telapsed_sec\tchecksum\tnotes" >"$summary_tsv"

echo "Output directory: $OUT_DIR"
echo "Building OpenMP + ARTS (${SIZE_TARGET})..."

cd "$BENCH_DIR"
make -j1 clean >/dev/null 2>&1 || true
make -j1 "${SIZE_TARGET}-openmp" >/dev/null
ARTS_CFG="$ARTS_CFG" make -j1 "${SIZE_TARGET}-arts" >/dev/null

echo "Running OpenMP baseline..."
./build/jacobi2d_omp >"$OUT_DIR/openmp.out" 2>&1 || true
openmp_checksum="$(rg -n "checksum:" "$OUT_DIR/openmp.out" 2>/dev/null | tail -n1 | sed 's/.*checksum:[[:space:]]*//' || true)"
echo "OpenMP checksum: ${openmp_checksum:-<missing>}"

for i in $(seq 1 "$RUNS"); do
  run_log="$OUT_DIR/run_${i}.out"
  run_bt="$OUT_DIR/run_${i}.bt"

  start_ns="$(date +%s%N)"
  set +e
  timeout -s "$TIMEOUT_SIGNAL" "${TIMEOUT_SECS}s" env artsConfig="$ARTS_CFG" \
    ./jacobi2d_arts \
    >"$run_log" 2>&1
  code=$?
  set -e
  end_ns="$(date +%s%N)"

  elapsed_sec="$(awk -v s="$start_ns" -v e="$end_ns" 'BEGIN{printf "%.6f", (e-s)/1000000000.0}')"
  checksum="$(rg -n "checksum:" "$run_log" 2>/dev/null | tail -n1 | sed 's/.*checksum:[[:space:]]*//' || true)"
  notes=""

  if [[ -z "$checksum" ]]; then
    notes="missing-checksum"
  fi

  if [[ "$code" -ne 0 ]]; then
    core_file="$(ls -1t core* 2>/dev/null | head -n1 || true)"
    if [[ "$CAPTURE_BT" == "1" && -n "$core_file" ]]; then
      timeout "${BT_TIMEOUT_SECS}s" gdb -batch -ex "set pagination off" \
        -ex "thread apply all bt" ./jacobi2d_arts "$core_file" \
        >"$run_bt" 2>&1 || true
      notes="${notes:+$notes,}core=$core_file"
    fi
  fi

  echo -e "${i}\t${code}\t${elapsed_sec}\t${checksum:-}\t${notes}" >>"$summary_tsv"
  echo "run=${i} exit=${code} elapsed=${elapsed_sec}s checksum=${checksum:-<none>} notes=${notes:-none}"
done

echo
echo "Summary written to: $summary_tsv"
echo "OpenMP output: $OUT_DIR/openmp.out"
echo "ARTS run logs: $OUT_DIR/run_*.out"
