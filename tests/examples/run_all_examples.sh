#!/usr/bin/env bash
# Run carts execute -O3 for all examples in this directory and report status.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

get_args() {
  case "$1" in
    array) echo "100" ;;
    dotproduct) echo "100" ;;
    matrix) echo "100" ;;
    matrixmul) echo "64 16" ;;
    rowdep) echo "64" ;;
    stencil) echo "100" ;;
    *) echo "" ;;
  esac
}

examples=(
  array
  deps
  dotproduct
  matrix
  matrixmul
  parallel
  rowdep
  smith-waterman
  stencil
)

printf "%-16s | %-8s | %s\n" "Example" "Status" "Note"
printf "%-16s-+-%-8s-+-%s\n" "----------------" "--------" "-----------------------------"

for ex in "${examples[@]}"; do
  dir="${ROOT}/${ex}"
  src=("${dir}"/*.c)
  bin="$(basename "${src[0]}" .c)"

  status="PASS"
  note=""

  if ! (cd "${dir}" && carts execute "${bin}.c" -O3 >/dev/null 2>&1); then
    status="FAIL"
    note="carts execute failed"
    printf "%-16s | %-8s | %s\n" "${ex}" "${status}" "${note}"
    continue
  fi

  cmd=( "./${bin}" )
  ex_args="$(get_args "${ex}")"
  if [[ -n "${ex_args}" ]]; then
    # Split arguments for this example.
    read -r -a extra <<< "${ex_args}"
    cmd+=( "${extra[@]}" )
  fi

  run_output="$(
    cd "${dir}" && "${cmd[@]}" 2>&1
  )"
  run_rc=$?

  if [[ ${run_rc} -ne 0 ]]; then
    status="FAIL"
    note="runtime error (rc=${run_rc})"
  elif grep -q "INCORRECT" <<< "${run_output}"; then
    status="FAIL"
    note="reported incorrect result"
  else
    # Keep a short note to help spot unexpected warnings.
    first_line="$(head -n 1 <<< "${run_output}")"
    note="${first_line}"
  fi

  printf "%-16s | %-8s | %s\n" "${ex}" "${status}" "${note}"
done
