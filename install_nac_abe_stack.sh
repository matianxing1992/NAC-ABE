#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_DIR="$ROOT/dependencies"
OPENABE_PREFIX=""
OPENABE_REPO_URL="${OPENABE_REPO_URL:-https://github.com/matianxing1992/openabe.git}"
BUILD_DIR="$ROOT/build"
INSTALL_PREFIX=""
INSTALL_NAC_ABE=1
BUILD_TESTS=1
BUILD_EXAMPLES=0
INSTALL_SYSTEM_PACKAGES=1
FORCE_OPENABE=0
REGISTER_LDCONFIG=1
CC_BIN="${CC:-}"
CXX_BIN="${CXX:-}"

usage() {
  cat <<'EOF'
Usage: ./install_nac_abe_stack.sh [options]

Build NAC-ABE, installing a private OpenABE dependency first when libopenabe is
not already available.

Options:
  --deps-dir PATH           Clone/build OpenABE under PATH (default: ./dependencies).
  --openabe-prefix PATH     Dedicated OpenABE install prefix (default: /usr/local/openabe).
  --openabe-repo-url URL    OpenABE repository URL (default: https://github.com/matianxing1992/openabe.git).
  --force-openabe           Rebuild OpenABE even if libopenabe is already found.
  --cc PATH                 C compiler for this build only, for example gcc-10.
  --cxx PATH                C++ compiler for this build only, for example g++-10.
  --install-prefix PATH     CMake install prefix for NAC-ABE.
  --build-dir PATH          CMake build directory (default: ./build).
  --with-tests              Configure, build, and run NAC-ABE unit tests (default).
  --no-tests                Skip NAC-ABE unit-test configuration and execution.
  --with-examples           Configure NAC-ABE examples.
  --no-system-packages      Do not install apt build packages.
  --no-install              Build NAC-ABE but do not run cmake --install.
  --no-ldconfig             Do not register the OpenABE prefix with ldconfig.
  --openabe-only            Install/check OpenABE only; skip NAC-ABE configure/build/install.
  -h, --help                Show this help.

Behavior:
  - If libopenabe is found in --openabe-prefix, OpenABE is reused.
  - If libopenabe is missing, OpenABE is cloned and built with its bundled
    private OpenSSL 1.1 dependency. This does not replace the system OpenSSL.
  - OpenABE and its bundled runtime libraries are kept under a dedicated prefix
    and registered through /etc/ld.so.conf.d/openabe.conf by default.
  - NAC-ABE CMake is configured with -DOPENABE_ROOT=<prefix>.
  - --cc/--cxx only set CC/CXX for this script and its child builds. They do
    not change the system default compiler.
EOF
}

run() {
  echo "+ $*"
  "$@"
}

sudo_run() {
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
    run "$@"
  else
    run sudo "$@"
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --deps-dir)
      DEPS_DIR="$2"
      shift 2
      ;;
    --openabe-prefix)
      OPENABE_PREFIX="$2"
      shift 2
      ;;
    --openabe-repo-url)
      OPENABE_REPO_URL="$2"
      shift 2
      ;;
    --force-openabe)
      FORCE_OPENABE=1
      shift
      ;;
    --cc)
      CC_BIN="$2"
      shift 2
      ;;
    --cxx)
      CXX_BIN="$2"
      shift 2
      ;;
    --install-prefix)
      INSTALL_PREFIX="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --with-tests)
      BUILD_TESTS=1
      shift
      ;;
    --no-tests)
      BUILD_TESTS=0
      shift
      ;;
    --with-examples)
      BUILD_EXAMPLES=1
      shift
      ;;
    --no-system-packages)
      INSTALL_SYSTEM_PACKAGES=0
      shift
      ;;
    --no-install)
      INSTALL_NAC_ABE=0
      shift
      ;;
    --no-ldconfig)
      REGISTER_LDCONFIG=0
      shift
      ;;
    --openabe-only)
      INSTALL_NAC_ABE=0
      BUILD_DIR=""
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "$OPENABE_PREFIX" ]]; then
  OPENABE_PREFIX="/usr/local/openabe"
fi

if [[ -n "$CC_BIN" ]]; then
  export CC="$CC_BIN"
fi
if [[ -n "$CXX_BIN" ]]; then
  export CXX="$CXX_BIN"
fi

require_compiler() {
  local kind="$1"
  local value="$2"
  if [[ -z "$value" ]]; then
    return
  fi
  if ! command -v "$value" >/dev/null 2>&1; then
    echo "Requested $kind compiler not found: $value" >&2
    exit 1
  fi
}

has_openabe() {
  [[ -f "$OPENABE_PREFIX/lib/libopenabe.so" ]] && return 0
  ldconfig -p 2>/dev/null | grep -q 'libopenabe\.so' && return 0
  [[ -f /usr/local/lib/libopenabe.so ]] && return 0
  return 1
}

has_openabe_prefix() {
  [[ -f "$OPENABE_PREFIX/lib/libopenabe.so" ]]
}

install_system_packages() {
  if [[ "$INSTALL_SYSTEM_PACKAGES" != "1" ]]; then
    echo "==> Skipping OS package installation"
    return
  fi
  if ! command -v apt-get >/dev/null 2>&1; then
    echo "==> No apt-get found; assuming build tools are installed"
    return
  fi

  echo "==> Installing common OpenABE/NAC-ABE build packages"
  sudo_run apt-get update
  sudo_run env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential git pkg-config cmake python3 wget curl \
    autoconf automake libtool m4 bison flex \
    libgmp-dev libssl-dev libboost-all-dev libsqlite3-dev libpcap-dev
}

ensure_openabe_source() {
  local dir="$DEPS_DIR/openabe"
  mkdir -p "$DEPS_DIR"
  if [[ -d "$dir/.git" ]]; then
    echo "==> Reusing OpenABE source tree: $dir"
  elif [[ -e "$dir" ]]; then
    echo "OpenABE dependency path exists but is not a git repository: $dir" >&2
    exit 1
  else
    echo "==> Cloning OpenABE from $OPENABE_REPO_URL into $dir"
    run git clone "$OPENABE_REPO_URL" "$dir"
  fi
  printf '%s\n' "$dir"
}

build_openabe() {
  if [[ "$FORCE_OPENABE" != "1" ]] && has_openabe; then
    if has_openabe_prefix; then
      echo "==> OpenABE already available under $OPENABE_PREFIX; skipping private OpenABE build"
    else
      echo "==> OpenABE already available from system library paths; skipping private OpenABE build"
    fi
    return
  fi

  install_system_packages

  local dir
  dir="$(ensure_openabe_source | tail -n 1)"
  echo "==> Building OpenABE with private bundled OpenSSL 1.1 dependency"
  run bash -lc "cd '$dir' && . ./env && make -C deps/openssl && make -C deps/relic && make -C deps/gtest && BISON=\$(command -v bison) FLEX=\$(command -v flex) make"
  echo "==> Installing private OpenABE under $OPENABE_PREFIX"
  sudo_run bash -lc "cd '$dir' && . ./env && make INSTALL_PREFIX='$OPENABE_PREFIX' install"
  install_openabe_runtime_deps "$dir"
}

install_openabe_runtime_deps() {
  local dir="$1"
  local deps_root="$dir/deps/root"
  if [[ ! -d "$deps_root/lib" ]]; then
    return
  fi

  echo "==> Installing OpenABE bundled runtime libraries under $OPENABE_PREFIX/deps/root/lib"
  sudo_run install -d "$OPENABE_PREFIX/deps/root/lib"
  find "$deps_root/lib" -maxdepth 1 \( -type f -o -type l \) \
    \( -name 'librelic*.so*' -o -name 'libssl*.so*' -o -name 'libcrypto*.so*' \) \
    -print0 | while IFS= read -r -d '' lib; do
      sudo_run install -m 0644 "$lib" "$OPENABE_PREFIX/deps/root/lib/$(basename "$lib")"
    done
}

register_openabe_ldconfig() {
  if [[ "$REGISTER_LDCONFIG" != "1" ]]; then
    echo "==> Skipping OpenABE ldconfig registration"
    return
  fi

  local conf_file="/etc/ld.so.conf.d/openabe.conf"
  local tmp_file
  tmp_file="$(mktemp)"
  {
    echo "$OPENABE_PREFIX/lib"
    echo "$OPENABE_PREFIX/lib64"
    echo "$OPENABE_PREFIX/deps/root/lib"
    echo "$OPENABE_PREFIX/deps/root/lib64"
  } >"$tmp_file"
  echo "==> Registering OpenABE runtime library paths in $conf_file"
  sudo_run install -m 0644 "$tmp_file" "$conf_file"
  rm -f "$tmp_file"
  sudo_run ldconfig
}

restore_test_assets() {
  if [[ "$BUILD_TESTS" != "1" || -z "$BUILD_DIR" ]]; then
    return
  fi

  local test_src="$ROOT/tests/unit-tests"
  if [[ ! -f "$test_src/trust-schema.conf" || ! -f "$test_src/example-trust-anchor.cert" ]]; then
    return
  fi

  echo "==> Restoring NAC-ABE unit-test trust assets"
  run install -d "$BUILD_DIR"
  run install -m 0644 "$test_src/trust-schema.conf" "$BUILD_DIR/trust-schema.conf"
  run install -m 0644 "$test_src/example-trust-anchor.cert" "$BUILD_DIR/example-trust-anchor.cert"
}

build_nac_abe() {
  if [[ -z "$BUILD_DIR" ]]; then
    return
  fi

  local cmake_args=()
  cmake_args+=("-S" "$ROOT" "-B" "$BUILD_DIR")
  cmake_args+=("-UOPENABE_ROOT" "-UOPENABE_RELIC_ROOT")
  cmake_args+=("-UOPENABE_INCLUDE_DIR" "-UOPENABE_LIBRARY")
  cmake_args+=("-URELIC_LIBRARY" "-URELIC_EC_LIBRARY")
  cmake_args+=("-DHAVE_TESTS=$([[ "$BUILD_TESTS" == "1" ]] && echo True || echo False)")
  cmake_args+=("-DBUILD_EXAMPLES=$([[ "$BUILD_EXAMPLES" == "1" ]] && echo True || echo False)")
  if [[ -n "$INSTALL_PREFIX" ]]; then
    cmake_args+=("-DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX")
  fi
  if [[ -f "$OPENABE_PREFIX/lib/libopenabe.so" ]]; then
    cmake_args+=("-DOPENABE_ROOT=$OPENABE_PREFIX")
  fi
  if [[ -d "$OPENABE_PREFIX/deps/root" ]]; then
    cmake_args+=("-DOPENABE_RELIC_ROOT=$OPENABE_PREFIX/deps/root")
  fi

  echo "==> Configuring NAC-ABE"
  run cmake "${cmake_args[@]}"
  echo "==> Building NAC-ABE"
  run cmake --build "$BUILD_DIR" --clean-first -j"$(nproc)"
  if [[ "$BUILD_TESTS" == "1" ]]; then
    restore_test_assets
    echo "==> Running NAC-ABE unit tests"
    run ctest --test-dir "$BUILD_DIR" --output-on-failure
    restore_test_assets
  fi
  if [[ "$INSTALL_NAC_ABE" == "1" ]]; then
    echo "==> Installing NAC-ABE"
    sudo_run cmake --install "$BUILD_DIR"
    sudo_run ldconfig
  else
    echo "==> Skipping NAC-ABE install"
  fi
}

cd "$ROOT"
echo "==> NAC-ABE install root: $ROOT"
echo "==> OpenABE prefix: $OPENABE_PREFIX"
require_compiler "C" "${CC:-}"
require_compiler "C++" "${CXX:-}"
if [[ -n "${CC:-}" ]]; then
  echo "==> C compiler: $CC"
fi
if [[ -n "${CXX:-}" ]]; then
  echo "==> C++ compiler: $CXX"
fi

build_openabe
if has_openabe_prefix; then
  register_openabe_ldconfig
fi
build_nac_abe
