#!/usr/bin/env bash
# Ensure the ARTS production RDMA provider dev packages are present, installing
# them automatically on Debian/Ubuntu when possible.
#
# Single source of truth for the RDMA provider dependency check: invoked both by
# the Makefile `arts:` target (covers `dekk carts install`/`setup`/`make arts`)
# and by `dekk carts build --arts` (tools/scripts/build.py) for an early check.
#
# Probes pkg-config modules and maps them to apt dev packages. Missing packages
# are installed with non-interactive sudo so a missing sudo rule fails fast into
# manual instructions instead of blocking the build on a password prompt.
#
# Exit 0 when all providers are present (or successfully installed); exit 1 with
# guidance otherwise. The `--no-rdma` TCP developer build needs none of this.
set -u

# pkg-config module -> apt dev package.
MODULES=("librdmacm" "ucx" "libfabric")
PACKAGES=("librdmacm-dev" "libucx-dev" "libfabric-dev")

missing_packages() {
  local out=()
  if ! command -v pkg-config >/dev/null 2>&1; then
    # Cannot probe without pkg-config; treat the whole stack (and pkg-config) as
    # missing so the installer pulls everything in.
    printf '%s\n' "pkg-config" "${PACKAGES[@]}"
    return
  fi
  local i
  for i in "${!MODULES[@]}"; do
    if ! pkg-config --exists "${MODULES[$i]}" >/dev/null 2>&1; then
      out+=("${PACKAGES[$i]}")
    fi
  done
  printf '%s\n' "${out[@]}"
}

try_apt_install() {
  # $@ = packages
  command -v apt-get >/dev/null 2>&1 || return 1
  local cmd=(apt-get install -y "$@")
  if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || return 1
    cmd=(sudo -n "${cmd[@]}")
  fi
  echo "Installing ARTS RDMA provider dependencies: $*"
  "${cmd[@]}"
}

mapfile -t RAW < <(missing_packages)
MISSING=()
for p in "${RAW[@]}"; do [ -n "$p" ] && MISSING+=("$p"); done

if [ "${#MISSING[@]}" -eq 0 ]; then
  exit 0
fi

if try_apt_install "${MISSING[@]}"; then
  mapfile -t RAW < <(missing_packages)
  MISSING=()
  for p in "${RAW[@]}"; do [ -n "$p" ] && MISSING+=("$p"); done
  if [ "${#MISSING[@]}" -eq 0 ]; then
    echo "ARTS RDMA provider dependencies installed."
    exit 0
  fi
fi

echo "ERROR: RDMA production dependencies are missing: ${MISSING[*]}." >&2
echo "Automatic install was unavailable or incomplete (needs apt-get with" >&2
echo "passwordless sudo). Install them manually, or build ARTS with --no-rdma" >&2
echo "for a TCP-only developer runtime." >&2
exit 1
