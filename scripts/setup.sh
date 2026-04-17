#!/usr/bin/env bash

set -euo pipefail

USE_SUDO="true"
if [[ "${1:-}" == "--no-sudo" ]]; then
  USE_SUDO="false"
fi

SUDO_CMD=""
if [[ "${USE_SUDO}" == "true" ]] && [[ "$(id -u)" -ne 0 ]]; then
  SUDO_CMD="sudo"
fi

PIP_ARGS=()
if [[ "$(id -u)" -ne 0 ]]; then
  PIP_ARGS+=(--user)
fi

PACKAGES=(
  ca-certificates
  clang
  clang-format
  clang-tidy
  cmake
  curl
  g++
  gcc
  gdb
  git
  make
  ninja-build
  python3
  python3-pip
  tar
  unzip
  zip
)

${SUDO_CMD} apt-get update
${SUDO_CMD} apt-get install -y --no-install-recommends "${PACKAGES[@]}"
${SUDO_CMD} rm -rf /var/lib/apt/lists/*

python3 -m pip install --upgrade "${PIP_ARGS[@]}" pip
python3 -m pip install --upgrade "${PIP_ARGS[@]}" conan

