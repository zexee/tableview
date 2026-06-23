#!/usr/bin/env bash
# Build a static Qt 6 (qtbase only) for tableview, matching the local
# qt6-static layout. Used by CI on Linux and macOS, and reusable locally.
#
# Environment variables:
#   QT_VERSION   Qt version to build (default 6.8.3)
#   QT_PREFIX    install prefix (default <repo>/qt6-static)
#   QT_SRC_DIR   where to download/unpack sources (default <repo>/qt6-src)
#   QT_BUILD_DIR build tree (default <repo>/qt6-build)
set -euo pipefail

QT_VERSION="${QT_VERSION:-6.8.3}"
QT_MAJOR_MINOR="${QT_VERSION%.*}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

QT_PREFIX="${QT_PREFIX:-${REPO_DIR}/qt6-static}"
QT_SRC_DIR="${QT_SRC_DIR:-${REPO_DIR}/qt6-src}"
QT_BUILD_DIR="${QT_BUILD_DIR:-${REPO_DIR}/qt6-build}"

if [ -f "${QT_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    echo "Static Qt ${QT_VERSION} already installed at ${QT_PREFIX}; skipping."
    exit 0
fi

TARBALL="qtbase-everywhere-src-${QT_VERSION}.tar.xz"
if [ ! -f "${REPO_DIR}/${TARBALL}" ]; then
    echo "Downloading ${TARBALL} ..."
    BASE="https://download.qt.io/archive/qt/${QT_MAJOR_MINOR}/${QT_VERSION}/submodules"
    MIRROR="https://mirrors.ocf.berkeley.edu/qt/archive/qt/${QT_MAJOR_MINOR}/${QT_VERSION}/submodules"
    curl -fSL "${BASE}/${TARBALL}" -o "${REPO_DIR}/${TARBALL}" \
        || curl -fSL "${MIRROR}/${TARBALL}" -o "${REPO_DIR}/${TARBALL}"
fi

rm -rf "${QT_SRC_DIR}"
mkdir -p "${QT_SRC_DIR}"
echo "Extracting sources ..."
tar -xf "${REPO_DIR}/${TARBALL}" -C "${QT_SRC_DIR}" --strip-components=1

rm -rf "${QT_BUILD_DIR}"
mkdir -p "${QT_BUILD_DIR}"

echo "Configuring Qt ${QT_VERSION} (static) ..."
(
    cd "${QT_BUILD_DIR}"
    "${QT_SRC_DIR}/configure" \
        -static \
        -release \
        -opensource -confirm-license \
        -prefix "${QT_PREFIX}" \
        -no-opengl \
        -no-icu \
        -no-glib \
        -no-dbus \
        -qt-zlib \
        -qt-libpng \
        -qt-libjpeg \
        -qt-pcre \
        -qt-harfbuzz \
        -qt-freetype \
        -nomake examples \
        -nomake tests \
        -nomake benchmarks
)

echo "Building Qt ${QT_VERSION} ..."
cmake --build "${QT_BUILD_DIR}" --parallel

echo "Installing Qt ${QT_VERSION} to ${QT_PREFIX} ..."
cmake --install "${QT_BUILD_DIR}"

echo "Static Qt ${QT_VERSION} ready at ${QT_PREFIX}"
