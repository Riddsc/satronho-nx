#!/bin/bash
# Satronho NX: FFmpeg for Android arm64 with AV1 (libdav1d, and av1_mediacodec for the phone's
# hardware decoder), in the CI package layout CPMUtil's AddCIPackage downloads:
#   artifacts/ffmpeg-android-aarch64-$VERSION.tar.zst (+ .sha512sum)
# Same FFmpeg commit and flags as crueter-ci/FFmpeg 9.0.2 for Android, plus AV1. libdav1d is
# merged into libavcodec.a, so nothing else needs linking.
#
# Needs: NDK (path to the Android NDK), VERSION, FFMPEG_COMMIT, DAV1D_VERSION, API.
set -euo pipefail

: "${NDK:?}" "${VERSION:?}" "${FFMPEG_COMMIT:?}" "${DAV1D_VERSION:?}"
: "${API:=23}"

ROOT="$PWD"
WORK="$ROOT/build-ffmpeg-android"
TC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
CC="$TC/aarch64-linux-android${API}-clang"
CXX="$TC/aarch64-linux-android${API}-clang++"
OUT="$WORK/out"
DAV1D_OUT="$WORK/dav1d-out"

rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK"

fetch() {
    for attempt in 1 2 3 4 5; do
        if curl -sSfL "$1" -o "$2"; then
            return 0
        fi
        echo "Download failed (attempt $attempt): $1"
        sleep 10
    done
    return 1
}

echo "::group::dav1d $DAV1D_VERSION"
fetch "https://code.videolan.org/videolan/dav1d/-/archive/$DAV1D_VERSION/dav1d-$DAV1D_VERSION.tar.gz" dav1d.tar.gz
tar xzf dav1d.tar.gz
cat > cross.txt <<EOF
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$TC/llvm-ar'
strip = '$TC/llvm-strip'

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
EOF
meson setup dav1d-build "dav1d-$DAV1D_VERSION" --cross-file cross.txt --buildtype release \
    --default-library static -Denable_tools=false -Denable_tests=false \
    --prefix "$DAV1D_OUT" --libdir lib
ninja -C dav1d-build install
echo "::endgroup::"

echo "::group::FFmpeg $FFMPEG_COMMIT"
fetch "https://github.com/FFmpeg/FFmpeg/archive/$FFMPEG_COMMIT.tar.gz" ffmpeg.tar.gz
tar xzf ffmpeg.tar.gz
cd "FFmpeg-$FFMPEG_COMMIT"
export PKG_CONFIG_LIBDIR="$DAV1D_OUT/lib/pkgconfig"
export PKG_CONFIG_PATH="$DAV1D_OUT/lib/pkgconfig"
flags=(
    --prefix="$OUT"
    --disable-avdevice --disable-avformat --disable-doc --disable-everything --disable-ffmpeg
    --disable-ffprobe --disable-network
    --enable-static --enable-filter=yadif,scale --enable-small --enable-pic --enable-swresample
    --enable-decoder=h264 --enable-decoder=vp8 --enable-decoder=vp9 --enable-decoder=opus
    --enable-mediacodec --enable-jni
    --enable-decoder=h264_mediacodec --enable-decoder=vp8_mediacodec
    --enable-decoder=vp9_mediacodec --enable-decoder=av1_mediacodec
    --enable-libdav1d --enable-decoder=libdav1d
    --enable-cross-compile --target-os=android --arch=aarch64 --toolchain=llvm
    "--extra-ldflags=-Wl,-z,max-page-size=16384,--hash-style=both"
    --cross-prefix="$TC/"
    --cc="$CC" --cxx="$CXX"
    --pkg-config=pkg-config --pkg-config-flags=--static
)
printf '  %s\n' "${flags[@]}"
./configure "${flags[@]}" || { tail -n 80 ffbuild/config.log; exit 1; }
make -j"$(nproc)"
make install
echo "::endgroup::"

echo "::group::Package"
cd "$OUT/lib"
"$TC/llvm-ar" -M <<EOF
create libavcodec-merged.a
addlib libavcodec.a
addlib $DAV1D_OUT/lib/libdav1d.a
save
end
EOF
mv libavcodec-merged.a libavcodec.a
"$TC/llvm-ranlib" libavcodec.a
# Read the symbols into a file first: with pipefail, grep -q closing the pipe early fails the check.
"$TC/llvm-nm" libavcodec.a > "$WORK/avcodec-symbols.txt" 2>/dev/null || true
for symbol in ff_libdav1d_decoder ff_av1_mediacodec_decoder dav1d_open; do
    if ! grep -qE " [A-TV-Z] $symbol$" "$WORK/avcodec-symbols.txt"; then
        echo "::error::$symbol is missing from libavcodec.a"
        exit 1
    fi
done
rm -rf "$OUT/share" "$OUT/lib/pkgconfig"
cp "$ROOT/.ci/ffmpeg-android/CMakeLists.txt" "$OUT/"

mkdir -p "$ROOT/artifacts"
tarball="ffmpeg-android-aarch64-$VERSION.tar"
cd "$OUT"
tar cf "$ROOT/artifacts/$tarball" ./*
cd "$ROOT/artifacts"
zstd -10 --rm "$tarball"
sha512sum "$tarball.zst" | cut -d " " -f1 | tr -d "\n" > "$tarball.zst.sha512sum"
ls -l "$ROOT/artifacts"
echo "::endgroup::"
