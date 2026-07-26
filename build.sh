#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${ROOT}/build"

export PATH="/opt/trinity/bin:${PATH:-}"

mkdir -p "${BUILD}"
cd "${BUILD}"

cmake "${ROOT}" "$@"
make -j"$(nproc)"

echo ""
echo "Built:"
echo "  ${BUILD}/src/nm-tray-tde"
