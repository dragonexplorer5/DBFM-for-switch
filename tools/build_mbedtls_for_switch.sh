#!/usr/bin/env bash
# Cross-build helper for mbedTLS for devkitPro / Nintendo Switch (MSYS2)
# Usage (in MSYS2 / devkitPro environment):
#   1) Edit DEVKITPRO if needed or export it in your session (default /opt/devkitpro)
#   2) Provide a toolchain file path (if you have one) or run interactively.
#   3) ./build_mbedtls_for_switch.sh /path/to/your/toolchain.cmake

set -euo pipefail

# Default devkitPro install root inside MSYS2
DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
# Default install prefix into portlibs/switch (headers -> include, libs -> lib)
INSTALL_PREFIX=${INSTALL_PREFIX:-${DEVKITPRO}/portlibs/switch}

TOOLCHAIN_FILE=${1:-}

echo "DEVKITPRO=${DEVKITPRO}"
echo "INSTALL_PREFIX=${INSTALL_PREFIX}"

if [ -z "$TOOLCHAIN_FILE" ]; then
  echo "No CMake toolchain file provided as first argument."
  echo "If you don't have a devkitPro-compatible CMake toolchain file, set TOOLCHAIN_FILE to point to it." 
  echo "Example toolchain file locations vary depending on your devkitPro setup."
  echo "You can re-run this script with the path: ./build_mbedtls_for_switch.sh /path/to/devkitpro.toolchain.cmake"
  read -p "Continue without toolchain file (attempt native build)? (y/N): " yn
  case $yn in
    [Yy]* ) TOOLCHAIN_FILE="" ;;
    * ) echo "Aborting. Provide a toolchain file for cross-compilation."; exit 1 ;;
  esac
fi

# Clone mbedTLS (will reuse existing folder if present)
MBEDTLS_DIR=mbedtls-src
if [ ! -d "$MBEDTLS_DIR" ]; then
  echo "Cloning mbed TLS..."
  git clone --depth 1 https://github.com/ARMmbed/mbedtls.git "$MBEDTLS_DIR"
else
  echo "Using existing $MBEDTLS_DIR (pulling latest)..."
  (cd "$MBEDTLS_DIR" && git pull --ff-only) || true
fi

BUILD_DIR=${MBEDTLS_DIR}/build-switch
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

CMAKE_ARGS=("-DCMAKE_BUILD_TYPE=Release" "-DENABLE_TESTING=OFF" "-DENABLE_PROGRAMS=OFF" "-DENABLE_STATIC_LIBRARY=ON" "-DENABLE_SHARED_LIBRARY=OFF")

if [ -n "$TOOLCHAIN_FILE" ]; then
  CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
  echo "Using toolchain file: ${TOOLCHAIN_FILE}"
fi

CMAKE_ARGS+=("-DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}")

echo "Running cmake with: ${CMAKE_ARGS[*]}"
cmake .. "${CMAKE_ARGS[@]}"

# Build and install
make -j$(nproc)
make install

echo "mbedTLS build/install complete. Installed to ${INSTALL_PREFIX}"

echo "Next steps:
 - Ensure your Makefile uses: CFLAGS += -I${INSTALL_PREFIX}/include
 - LDFLAGS += -L${INSTALL_PREFIX}/lib -lmbedtls -lmbedx509 -lmbedcrypto
 - Build with: make USE_MBEDTLS=1
If that fails, tell me and I'll automatically switch to option 3 (guard/disable libcurl-dependent features)."
