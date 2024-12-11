GAY_VERSION="1.0"

set -e

if [ "$(uname -m)" != "x86_64" ]; then
  echo "This script requires an x86_64 (64-bit) machine."
  exit 1
fi

DIR="$(readlink -f .)"
PARENT_DIR="$(readlink -f ${DIR}/..)"
OUTDIR="$(pwd)/out"
BUILD_ARGS="LOCALVERSION=-GayAsf-${GAY_VERSION}-Beta KBUILD_BUILD_USER=Ksawlii KBUILD_BUILD_HOST=GayAsFuck"

# clang and shit
export CROSS_COMPILE="$PARENT_DIR/clang-r536225/bin/aarch64-linux-gnu-"
export CC="$PARENT_DIR/clang-r536225/bin/clang"
export PATH="$PARENT_DIR/build-tools/path/linux-x86:$PARENT_DIR/clang-r536225/bin:$PATH"
export ARCH=arm64
if [ ! -d "$PARENT_DIR/clang-r536225" ]; then
  git clone -j$(nproc --all) https://gitlab.com/crdroidandroid/android_prebuilts_clang_host_linux-x86_clang-r536225.git "$PARENT_DIR/clang-r536225" --depth=1
fi

if [ ! -d "$PARENT_DIR/build-tools" ]; then
    git clone https://android.googlesource.com/platform/prebuilts/build-tools "$PARENT_DIR/build-tools" --depth=1
fi

# build!1!
make -j$(nproc --all) -C $(pwd) CC="$PARENT_DIR/clang-r536225/bin/clang" O=out $BUILD_ARGS gale_defconfig >/dev/null
make -j$(nproc --all) -C $(pwd) CC="$PARENT_DIR/clang-r536225/bin/clang" O=out $BUILD_ARGS >/dev/null
